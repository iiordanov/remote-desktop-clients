/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <math.h>

#include <spice/macros.h>
#include "log.h"
#include "quic.h"
#include "lz.h"
#include "canvas_base.h"
#include "pixman_utils.h"
#include "canvas_utils.h"
#include "rect.h"
#include "lines.h"
#include "rop3.h"
#include "mem.h"
#include "macros.h"
#include "mutex.h"

#define ROUND(_x) ((int)floor((_x) + 0.5))

#define IS_IMAGE_LOSSY(descriptor)                         \
    (((descriptor)->type == SPICE_IMAGE_TYPE_JPEG) ||      \
    ((descriptor)->type == SPICE_IMAGE_TYPE_JPEG_ALPHA))

 static inline int fix_to_int(SPICE_FIXED28_4 fixed)
{
    int val, rem;

    rem = fixed & 0x0f;
    val = fixed >> 4;
    if (rem > 8) {
        val++;
    }
    return val;
}

 static inline SPICE_FIXED28_4  int_to_fix(int v)
{
    return v << 4;
}

static inline double fix_to_double(SPICE_FIXED28_4 fixed)
{
    return (double)(fixed & 0x0f) / 0x0f + (fixed >> 4);
}

static inline uint16_t rgb_32_to_16_555(uint32_t color)
{
    return
        (((color) >> 3) & 0x001f) |
        (((color) >> 6) & 0x03e0) |
        (((color) >> 9) & 0x7c00);
}
static inline uint16_t rgb_32_to_16_565(uint32_t color)
{
    return
        (((color) >> 3) & 0x001f) |
        (((color) >> 5) & 0x07e0) |
        (((color) >> 8) & 0xf800);
}

static inline uint32_t canvas_16bpp_to_32bpp(uint32_t color)
{
    uint32_t ret;

    ret = ((color & 0x001f) << 3) | ((color & 0x001c) >> 2);
    ret |= ((color & 0x03e0) << 6) | ((color & 0x0380) << 1);
    ret |= ((color & 0x7c00) << 9) | ((color & 0x7000) << 4);

    return ret;
}
#if defined(WIN32) && defined(GDI_CANVAS)
static HDC create_compatible_dc()
{
    HDC dc = CreateCompatibleDC(NULL);

    spice_return_val_if_fail(dc != NULL, NULL);

    return dc;
}

#endif

typedef struct LzData {
    LzUsrContext usr;
    LzContext *lz;
    LzDecodeUsrData decode_data;
    jmp_buf jmp_env;
    char message_buf[512];
} LzData;

typedef struct GlzData {
    SpiceGlzDecoder *decoder;
    LzDecodeUsrData decode_data;
} GlzData;

typedef struct QuicData {
    QuicUsrContext usr;
    QuicContext *quic;
    jmp_buf jmp_env;
    char message_buf[512];
    SpiceChunks *chunks;
    uint32_t current_chunk;
} QuicData;

typedef struct CanvasBase {
    SpiceCanvas parent;
    uint32_t color_shift;
    uint32_t color_mask;
    QuicData quic_data;

    uint32_t format;
    int width;
    int height;
    pixman_region32_t canvas_region;

#if defined(SW_CANVAS_CACHE) || defined(SW_CANVAS_IMAGE_CACHE)
    SpiceImageCache *bits_cache;
#endif
#ifdef SW_CANVAS_CACHE
    SpicePaletteCache *palette_cache;
#endif
#ifdef WIN32
    HDC dc;
#endif

    SpiceImageSurfaces *surfaces;

    LzData lz_data;
    GlzData glz_data;
    SpiceJpegDecoder* jpeg;
    SpiceZlibDecoder* zlib;

    void *usr_data;
    spice_destroy_fn_t usr_data_destroy;
} CanvasBase;

typedef enum {
    ROP_INPUT_SRC,
    ROP_INPUT_BRUSH,
    ROP_INPUT_DEST
} ROPInput;

static SpiceROP ropd_descriptor_to_rop(int desc,
                                       ROPInput src_input,
                                       ROPInput dest_input)
{
    int old;
    int invert_masks[] = {
        SPICE_ROPD_INVERS_SRC,
        SPICE_ROPD_INVERS_BRUSH,
        SPICE_ROPD_INVERS_DEST
    };

    old = desc;

    desc &= ~(SPICE_ROPD_INVERS_SRC | SPICE_ROPD_INVERS_DEST);
    if (old & invert_masks[src_input]) {
        desc |= SPICE_ROPD_INVERS_SRC;
    }

    if (old & invert_masks[dest_input]) {
        desc |= SPICE_ROPD_INVERS_DEST;
    }

    if (desc & SPICE_ROPD_OP_PUT) {
        if (desc & SPICE_ROPD_INVERS_SRC) {
            if (desc & SPICE_ROPD_INVERS_RES) {
                return SPICE_ROP_COPY;
            }
            return SPICE_ROP_COPY_INVERTED;
        } else {
            if (desc & SPICE_ROPD_INVERS_RES) {
                return SPICE_ROP_COPY_INVERTED;
            }
            return SPICE_ROP_COPY;
        }
    } else if (desc & SPICE_ROPD_OP_OR) {

        if (desc & SPICE_ROPD_INVERS_RES) {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(!src or !dest) == src and dest*/
                    return SPICE_ROP_AND;
                } else {
                    /* ! (!src or dest) = src and !dest*/
                    return SPICE_ROP_AND_REVERSE;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(src or !dest) == !src and dest */
                    return SPICE_ROP_AND_INVERTED;
                } else {
                    /* !(src or dest) */
                    return SPICE_ROP_NOR;
                }
            }
        } else {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !src or !dest == !(src and dest)*/
                    return SPICE_ROP_NAND;
                } else {
                    /* !src or dest */
                    return SPICE_ROP_OR_INVERTED;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* src or !dest */
                    return SPICE_ROP_OR_REVERSE;
                } else {
                    /* src or dest */
                    return SPICE_ROP_OR;
                }
            }
        }

    } else if (desc & SPICE_ROPD_OP_AND) {

        if (desc & SPICE_ROPD_INVERS_RES) {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(!src and !dest) == src or dest*/
                    return SPICE_ROP_OR;
                } else {
                    /* ! (!src and dest) = src or !dest*/
                    return SPICE_ROP_OR_REVERSE;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(src and !dest) == !src or dest */
                    return SPICE_ROP_OR_INVERTED;
                } else {
                    /* !(src and dest) */
                    return SPICE_ROP_NAND;
                }
            }
        } else {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !src and !dest == !(src or dest)*/
                    return SPICE_ROP_NOR;
                } else {
                    /* !src and dest */
                    return SPICE_ROP_AND_INVERTED;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* src and !dest */
                    return SPICE_ROP_AND_REVERSE;
                } else {
                    /* src and dest */
                    return SPICE_ROP_AND;
                }
            }
        }

    } else if (desc & SPICE_ROPD_OP_XOR) {

        if (desc & SPICE_ROPD_INVERS_RES) {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(!src xor !dest) == !src xor dest */
                    return SPICE_ROP_EQUIV;
                } else {
                    /* ! (!src xor dest) = src xor dest*/
                    return SPICE_ROP_XOR;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !(src xor !dest) == src xor dest */
                    return SPICE_ROP_XOR;
                } else {
                    /* !(src xor dest) */
                    return SPICE_ROP_EQUIV;
                }
            }
        } else {
            if (desc & SPICE_ROPD_INVERS_SRC) {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* !src xor !dest == src xor dest */
                    return SPICE_ROP_XOR;
                } else {
                    /* !src xor dest */
                    return SPICE_ROP_EQUIV;
                }
            } else {
                if (desc & SPICE_ROPD_INVERS_DEST) {
                    /* src xor !dest */
                    return SPICE_ROP_EQUIV;
                } else {
                    /* src xor dest */
                    return SPICE_ROP_XOR;
                }
            }
        }

    } else if (desc & SPICE_ROPD_OP_BLACKNESS) {
        return SPICE_ROP_CLEAR;
    } else if (desc & SPICE_ROPD_OP_WHITENESS) {
        return SPICE_ROP_SET;
    } else if (desc & SPICE_ROPD_OP_INVERS) {
        return SPICE_ROP_INVERT;
    }
    return SPICE_ROP_COPY;
}

//#define DEBUG_DUMP_COMPRESS
#ifdef DEBUG_DUMP_COMPRESS
static void dump_surface(pixman_image_t *surface, int cache);
#endif


static pixman_format_code_t canvas_get_target_format(CanvasBase *canvas,
                                                     int source_has_alpha)
{
    pixman_format_code_t format;

    /* Convert to target surface format */
    format = spice_surface_format_to_pixman (canvas->format);

    if (source_has_alpha) {
        /* Even though the destination has no alpha, we make the source
         * remember there are alpha bits instead of throwing away this
         * information. The results are the same if alpha is not
         * interpreted, and if need to interpret alpha, don't use
         * conversion to target format.
         * This is needed for instance when doing the final
         * canvas_get_target_format() in canvas_get_image_internal
         * as otherwise we wouldn't know if the bitmap source
         * really had alpha.
         */
        if (format == PIXMAN_x8r8g8b8) {
            format = PIXMAN_a8r8g8b8;
        }
    } else { /* !source_has_alpha */
        /* If the source doesn't have alpha, but the destination has,
           don't convert to alpha, since that would just do an unnecessary
           copy to fill the alpha bytes with 0xff which is not expected if
           we just use the raw bits, (and handled implicitly by pixman if
           we're interpreting data) */
        if (format == PIXMAN_a8r8g8b8) {
            format = PIXMAN_x8r8g8b8;
        }
    }

    return format;
}

static pixman_image_t *canvas_get_quic(CanvasBase *canvas, SpiceImage *image,
                                       int invers, int want_original)
{
    pixman_image_t *surface = NULL;
    QuicData *quic_data = &canvas->quic_data;
    QuicImageType type, as_type;
    pixman_format_code_t pixman_format;
    uint8_t *dest;
    int stride;
    int width;
    int height;

    if (setjmp(quic_data->jmp_env)) {
        pixman_image_unref(surface);
        spice_warning("%s", quic_data->message_buf);
        return NULL;
    }

    quic_data->chunks = image->u.quic.data;
    quic_data->current_chunk = 0;

    if (quic_decode_begin(quic_data->quic,
                          (uint32_t *)image->u.quic.data->chunk[0].data,
                          image->u.quic.data->chunk[0].len >> 2,
                          &type, &width, &height) == QUIC_ERROR) {
        spice_warning("quic decode begin failed");
        return NULL;
    }

    switch (type) {
    case QUIC_IMAGE_TYPE_RGBA:
        as_type = QUIC_IMAGE_TYPE_RGBA;
        pixman_format = PIXMAN_a8r8g8b8;
        break;
    case QUIC_IMAGE_TYPE_RGB32:
    case QUIC_IMAGE_TYPE_RGB24:
        as_type = QUIC_IMAGE_TYPE_RGB32;
        pixman_format = PIXMAN_x8r8g8b8;
        break;
    case QUIC_IMAGE_TYPE_RGB16:
        if (!want_original &&
            (canvas->format == SPICE_SURFACE_FMT_32_xRGB ||
             canvas->format == SPICE_SURFACE_FMT_32_ARGB)) {
            as_type = QUIC_IMAGE_TYPE_RGB32;
            pixman_format = PIXMAN_x8r8g8b8;
        } else {
            as_type = QUIC_IMAGE_TYPE_RGB16;
            pixman_format = PIXMAN_x1r5g5b5;
        }
        break;
    case QUIC_IMAGE_TYPE_INVALID:
    case QUIC_IMAGE_TYPE_GRAY:
    default:
        spice_warn_if_reached();
        return NULL;
    }

    spice_return_val_if_fail((uint32_t)width == image->descriptor.width, NULL);
    spice_return_val_if_fail((uint32_t)height == image->descriptor.height, NULL);

    surface = surface_create(
#ifdef WIN32
                             canvas->dc,
#endif
                             pixman_format,
                             width, height, FALSE);

    spice_return_val_if_fail(surface != NULL, NULL);

    dest = (uint8_t *)pixman_image_get_data(surface);
    stride = pixman_image_get_stride(surface);
    if (quic_decode(quic_data->quic, as_type,
                    dest, stride) == QUIC_ERROR) {
        pixman_image_unref(surface);
        spice_warning("quic decode failed");
        return NULL;
    }

    if (invers) {
        uint8_t *end = dest + height * stride;
        for (; dest != end; dest += stride) {
            uint32_t *pix;
            uint32_t *end_pix;

            pix = (uint32_t *)dest;
            end_pix = pix + width;
            for (; pix < end_pix; pix++) {
                *pix ^= 0xffffffff;
            }
        }
    }

#ifdef DEBUG_DUMP_COMPRESS
    dump_surface(surface, 0);
#endif
    return surface;
}


//#define DUMP_JPEG
#ifdef DUMP_JPEG
static int jpeg_id = 0;
static void dump_jpeg(uint8_t* data, int data_size)
{
    char file_str[200];
    uint32_t id = ++jpeg_id;

#ifdef WIN32
    sprintf(file_str, "c:\\tmp\\spice_dump\\%u.jpg", id);
#else
    sprintf(file_str, "/tmp/spice_dump/%u.jpg", id);
#endif

    FILE *f = fopen(file_str, "wb");
    if (!f) {
        return;
    }

    fwrite(data, 1, data_size, f);
    fclose(f);
}
#endif

static pixman_image_t *canvas_get_jpeg(CanvasBase *canvas, SpiceImage *image, int invers)
{
    pixman_image_t *surface = NULL;
    int stride;
    int width;
    int height;
    uint8_t *dest;

    spice_return_val_if_fail(image->u.jpeg.data->num_chunks == 1, NULL);
    canvas->jpeg->ops->begin_decode(canvas->jpeg, image->u.jpeg.data->chunk[0].data, image->u.jpeg.data->chunk[0].len,
                                    &width, &height);
    spice_return_val_if_fail((uint32_t)width == image->descriptor.width, NULL);
    spice_return_val_if_fail((uint32_t)height == image->descriptor.height, NULL);

    surface = surface_create(
#ifdef WIN32
                             canvas->dc,
#endif
                             PIXMAN_x8r8g8b8,
                             width, height, FALSE);
    if (surface == NULL) {
        spice_warning("create surface failed");
        return NULL;
    }

    dest = (uint8_t *)pixman_image_get_data(surface);
    stride = pixman_image_get_stride(surface);

    canvas->jpeg->ops->decode(canvas->jpeg, dest, stride, SPICE_BITMAP_FMT_32BIT);

    if (invers) {
        uint8_t *end = dest + height * stride;
        for (; dest != end; dest += stride) {
            uint32_t *pix;
            uint32_t *end_pix;

            pix = (uint32_t *)dest;
            end_pix = pix + width;
            for (; pix < end_pix; pix++) {
                *pix ^= 0x00ffffff;
            }
        }
    }
#ifdef DUMP_JPEG
    dump_jpeg(image->u.jpeg.data, image->u.jpeg.data_size);
#endif
    return surface;
}

static pixman_image_t *canvas_get_jpeg_alpha(CanvasBase *canvas,
                                             SpiceImage *image, int invers)
{
    pixman_image_t *surface = NULL;
    int stride;
    int width;
    int height;
    uint8_t *dest;
    int alpha_top_down = FALSE;
    LzData *lz_data = &canvas->lz_data;
    LzImageType lz_alpha_type;
    uint8_t *comp_alpha_buf = NULL;
    uint8_t *decomp_alpha_buf = NULL;
    int alpha_size;
    int lz_alpha_width, lz_alpha_height, n_comp_pixels, lz_alpha_top_down;

    spice_return_val_if_fail(image->u.jpeg_alpha.data->num_chunks == 1, NULL);
    canvas->jpeg->ops->begin_decode(canvas->jpeg,
                                    image->u.jpeg_alpha.data->chunk[0].data,
                                    image->u.jpeg_alpha.jpeg_size,
                                    &width, &height);
    spice_return_val_if_fail((uint32_t)width == image->descriptor.width, NULL);
    spice_return_val_if_fail((uint32_t)height == image->descriptor.height, NULL);

    if (image->u.jpeg_alpha.flags & SPICE_JPEG_ALPHA_FLAGS_TOP_DOWN) {
        alpha_top_down = TRUE;
    }

#ifdef WIN32
    lz_data->decode_data.dc = canvas->dc;
#endif
    surface = alloc_lz_image_surface(&lz_data->decode_data, PIXMAN_a8r8g8b8,
                                     width, height, width*height, alpha_top_down);

    if (surface == NULL) {
        spice_warning("create surface failed");
        return NULL;
    }

    dest = (uint8_t *)pixman_image_get_data(surface);
    stride = pixman_image_get_stride(surface);

    canvas->jpeg->ops->decode(canvas->jpeg, dest, stride, SPICE_BITMAP_FMT_32BIT);

    comp_alpha_buf = image->u.jpeg_alpha.data->chunk[0].data + image->u.jpeg_alpha.jpeg_size;
    alpha_size = image->u.jpeg_alpha.data_size - image->u.jpeg_alpha.jpeg_size;

    lz_decode_begin(lz_data->lz, comp_alpha_buf, alpha_size, &lz_alpha_type,
                    &lz_alpha_width, &lz_alpha_height, &n_comp_pixels,
                    &lz_alpha_top_down, NULL);
    spice_return_val_if_fail(lz_alpha_type == LZ_IMAGE_TYPE_XXXA, NULL);
    spice_return_val_if_fail(!!lz_alpha_top_down == !!alpha_top_down, NULL);
    spice_return_val_if_fail(lz_alpha_width == width, NULL);
    spice_return_val_if_fail(lz_alpha_height == height, NULL);
    spice_return_val_if_fail(n_comp_pixels == width * height, NULL);

    if (!alpha_top_down) {
        decomp_alpha_buf = dest + stride * (height - 1);
    } else {
        decomp_alpha_buf = dest;
    }
    lz_decode(lz_data->lz, LZ_IMAGE_TYPE_XXXA, decomp_alpha_buf);

    if (invers) {
        uint8_t *end = dest + height * stride;
        for (; dest != end; dest += stride) {
            uint32_t *pix;
            uint32_t *end_pix;

            pix = (uint32_t *)dest;
            end_pix = pix + width;
            for (; pix < end_pix; pix++) {
                *pix ^= 0x00ffffff;
            }
        }
    }
#ifdef DUMP_JPEG
    dump_jpeg(image->u.jpeg_alpha.data, image->u.jpeg_alpha.jpeg_size);
#endif
    return surface;
}

static pixman_image_t *canvas_bitmap_to_surface(CanvasBase *canvas, SpiceBitmap* bitmap,
                                                SpicePalette *palette, int want_original)
{
    uint8_t* src;
    pixman_image_t *image;
    pixman_format_code_t format;

    spice_chunks_linearize(bitmap->data);

    src = bitmap->data->chunk[0].data;

    if (want_original) {
        format = spice_bitmap_format_to_pixman(bitmap->format, canvas->format);
    } else {
        format = canvas_get_target_format(canvas,
                                          bitmap->format == SPICE_BITMAP_FMT_RGBA);
    }

    image = surface_create(
#ifdef WIN32
                           canvas->dc,
#endif
                           format,
                           bitmap->x, bitmap->y, FALSE);
    if (image == NULL) {
        spice_warning("create surface failed");
        return NULL;
    }

    spice_bitmap_convert_to_pixman(format, image,
                                   bitmap->format,
                                   bitmap->flags,
                                   bitmap->x, bitmap->y,
                                   src, bitmap->stride,
                                   canvas->format, palette);
    return image;
}


#ifdef SW_CANVAS_CACHE

static inline SpicePalette *canvas_get_palette(CanvasBase *canvas, SpicePalette *base_palette, uint64_t palette_id, uint8_t flags)
{
    SpicePalette *palette;

    if (flags & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE) {
        palette = canvas->palette_cache->ops->get(canvas->palette_cache, palette_id);
    } else {
        palette = base_palette;
        if (palette != NULL && flags & SPICE_BITMAP_FLAGS_PAL_CACHE_ME) {
            canvas->palette_cache->ops->put(canvas->palette_cache, palette);
        }
    }
    return palette;
}

static inline SpicePalette *canvas_get_localized_palette(CanvasBase *canvas, SpicePalette *base_palette, uint64_t palette_id, uint8_t flags, int *free_palette)
{
    SpicePalette *palette = canvas_get_palette(canvas, base_palette, palette_id, flags);
    SpicePalette *copy;
    uint32_t *now, *end;
    size_t size;

    if (canvas->format == SPICE_SURFACE_FMT_32_xRGB ||
        canvas->format == SPICE_SURFACE_FMT_32_ARGB) {
        return palette;
    }

    size = sizeof(SpicePalette) + palette->num_ents * 4;
    copy = (SpicePalette *)spice_malloc(size);
    memcpy(copy, palette, size);

    switch (canvas->format) {
    case SPICE_SURFACE_FMT_32_xRGB:
    case SPICE_SURFACE_FMT_32_ARGB:
        /* Won't happen */
        break;
    case SPICE_SURFACE_FMT_16_555:
        now = copy->ents;
        end = now + copy->num_ents;
        for (; now < end; now++) {
            *now = canvas_16bpp_to_32bpp(*now);
        }
        break;
    case SPICE_SURFACE_FMT_16_565:
    default:
        spice_warn_if_reached();
        return NULL;
    }
    *free_palette = TRUE;
    return copy;
}

static pixman_image_t *canvas_get_lz(CanvasBase *canvas, SpiceImage *image, int invers,
                                     int want_original)
{
    LzData *lz_data = &canvas->lz_data;
    uint8_t *comp_buf = NULL;
    int comp_size;
    uint8_t    *decomp_buf = NULL;
    uint8_t    *src;
    pixman_format_code_t pixman_format;
    LzImageType type, as_type;
    SpicePalette *palette;
    int n_comp_pixels;
    int width;
    int height;
    int top_down;
    int stride;
    int free_palette;

    if (setjmp(lz_data->jmp_env)) {
        free(decomp_buf);
        spice_warning("%s", lz_data->message_buf);
        return NULL;
    }

    free_palette = FALSE;
    if (image->descriptor.type == SPICE_IMAGE_TYPE_LZ_RGB) {
        spice_return_val_if_fail(image->u.lz_rgb.data->num_chunks == 1, NULL); /* TODO: Handle chunks */
        comp_buf = image->u.lz_rgb.data->chunk[0].data;
        comp_size = image->u.lz_rgb.data->chunk[0].len;
        palette = NULL;
    } else if (image->descriptor.type == SPICE_IMAGE_TYPE_LZ_PLT) {
        spice_return_val_if_fail(image->u.lz_plt.data->num_chunks == 1, NULL); /* TODO: Handle chunks */
        comp_buf = image->u.lz_plt.data->chunk[0].data;
        comp_size = image->u.lz_plt.data->chunk[0].len;
        palette = canvas_get_localized_palette(canvas, image->u.lz_plt.palette, image->u.lz_plt.palette_id, image->u.lz_plt.flags, &free_palette);
    } else {
        spice_warn_if_reached();
        return NULL;
    }

    lz_decode_begin(lz_data->lz, comp_buf, comp_size, &type,
                    &width, &height, &n_comp_pixels, &top_down, palette);

    switch (type) {
    case LZ_IMAGE_TYPE_RGBA:
        as_type = LZ_IMAGE_TYPE_RGBA;
        pixman_format = PIXMAN_a8r8g8b8;
        break;
    case LZ_IMAGE_TYPE_RGB32:
    case LZ_IMAGE_TYPE_RGB24:
    case LZ_IMAGE_TYPE_PLT1_LE:
    case LZ_IMAGE_TYPE_PLT1_BE:
    case LZ_IMAGE_TYPE_PLT4_LE:
    case LZ_IMAGE_TYPE_PLT4_BE:
    case LZ_IMAGE_TYPE_PLT8:
        as_type = LZ_IMAGE_TYPE_RGB32;
        pixman_format = PIXMAN_x8r8g8b8;
        break;
    case LZ_IMAGE_TYPE_A8:
        as_type = LZ_IMAGE_TYPE_A8;
        pixman_format = PIXMAN_a8;
        break;
    case LZ_IMAGE_TYPE_RGB16:
        if (!want_original &&
            (canvas->format == SPICE_SURFACE_FMT_32_xRGB ||
             canvas->format == SPICE_SURFACE_FMT_32_ARGB)) {
            as_type = LZ_IMAGE_TYPE_RGB32;
            pixman_format = PIXMAN_x8r8g8b8;
        } else {
            as_type = LZ_IMAGE_TYPE_RGB16;
            pixman_format = PIXMAN_x1r5g5b5;
        }
        break;
    default:
        spice_warn_if_reached();
        return NULL;
    }

    spice_return_val_if_fail((unsigned)width == image->descriptor.width, NULL);
    spice_return_val_if_fail((unsigned)height == image->descriptor.height, NULL);

    spice_return_val_if_fail((image->descriptor.type == SPICE_IMAGE_TYPE_LZ_PLT) || (n_comp_pixels == width * height), NULL);
#ifdef WIN32
    lz_data->decode_data.dc = canvas->dc;
#endif


    alloc_lz_image_surface(&lz_data->decode_data, pixman_format,
                           width, height, n_comp_pixels, top_down);

    src = (uint8_t *)pixman_image_get_data(lz_data->decode_data.out_surface);

    stride = (n_comp_pixels / height) * 4;
    if (!top_down) {
        stride = -stride;
        decomp_buf = src + stride * (height - 1);
    } else {
        decomp_buf = src;
    }

    lz_decode(lz_data->lz, as_type, decomp_buf);

    if (invers) {
        uint8_t *line = src;
        uint8_t *end = src + height * stride;
        for (; line != end; line += stride) {
            uint32_t *pix;
            uint32_t *end_pix;

            pix = (uint32_t *)line;
            end_pix = pix + width;
            for (; pix < end_pix; pix++) {
                *pix ^= 0xffffffff;
            }
        }
    }

    if (free_palette)  {
        free(palette);
    }

    return lz_data->decode_data.out_surface;
}

static pixman_image_t *canvas_get_glz_rgb_common(CanvasBase *canvas, uint8_t *data,
                                                 int want_original)
{
    spice_return_val_if_fail(canvas->glz_data.decoder != NULL, NULL);

    canvas->glz_data.decoder->ops->decode(canvas->glz_data.decoder,
                                          data, NULL,
                                          &canvas->glz_data.decode_data);

    /* global_decode calls alloc_lz_image, which sets canvas->glz_data.surface */
    return (canvas->glz_data.decode_data.out_surface);
}

// don't handle plts since bitmaps with plt can be decoded globally to RGB32 (because
// same byte sequence can be transformed to different RGB pixels by different plts)
static pixman_image_t *canvas_get_glz(CanvasBase *canvas, SpiceImage *image,
                                      int want_original)
{
    spice_return_val_if_fail(image->descriptor.type == SPICE_IMAGE_TYPE_GLZ_RGB, NULL);
#ifdef WIN32
    canvas->glz_data.decode_data.dc = canvas->dc;
#endif

    spice_return_val_if_fail(image->u.lz_rgb.data->num_chunks == 1, NULL); /* TODO: Handle chunks */
    return canvas_get_glz_rgb_common(canvas, image->u.lz_rgb.data->chunk[0].data, want_original);
}

static pixman_image_t *canvas_get_zlib_glz_rgb(CanvasBase *canvas, SpiceImage *image,
                                               int want_original)
{
    uint8_t *glz_data;
    pixman_image_t *surface;

    spice_return_val_if_fail(canvas->zlib != NULL, NULL);

    spice_return_val_if_fail(image->u.zlib_glz.data->num_chunks == 1, NULL); /* TODO: Handle chunks */
    glz_data = (uint8_t*)spice_malloc(image->u.zlib_glz.glz_data_size);
    canvas->zlib->ops->decode(canvas->zlib, image->u.zlib_glz.data->chunk[0].data,
                              image->u.zlib_glz.data->chunk[0].len,
                              glz_data, image->u.zlib_glz.glz_data_size);
    surface = canvas_get_glz_rgb_common(canvas, glz_data, want_original);
    free(glz_data);
    return surface;
}

//#define DEBUG_DUMP_BITMAP

#ifdef DEBUG_DUMP_BITMAP
static void dump_bitmap(SpiceBitmap *bitmap, SpicePalette *palette)
{
    uint8_t* data = (uint8_t *)SPICE_GET_ADDRESS(bitmap->data);
    static uint32_t file_id = 0;
    uint32_t i, j;
    char file_str[200];
    uint32_t id = ++file_id;

#ifdef WIN32
    sprintf(file_str, "c:\\tmp\\spice_dump\\%u.%ubpp", id, bitmap->format);
#else
    sprintf(file_str, "/tmp/spice_dump/%u.%ubpp", id, bitmap->format);
#endif
    FILE *f = fopen(file_str, "wb");
    if (!f) {
        return;
    }

    fprintf(f, "%d\n", bitmap->format);                          // 1_LE,1_BE,....
    fprintf(f, "%d %d\n", bitmap->x, bitmap->y);     // width and height
    fprintf(f, "%d\n", palette->num_ents);               // #plt entries
    for (i = 0; i < palette->num_ents; i++) {
        fwrite(&(palette->ents[i]), 4, 1, f);
    }
    fprintf(f, "\n");

    for (i = 0; i < bitmap->y; i++, data += bitmap->stride) {
        uint8_t *now = data;
        for (j = 0; j < bitmap->x; j++) {
            fwrite(now, 1, 1, f);
            now++;
        }
    }
}

#endif

static pixman_image_t *canvas_get_bits(CanvasBase *canvas, SpiceBitmap *bitmap,
                                       int want_original)
{
    pixman_image_t* surface;
    SpicePalette *palette;

    palette = canvas_get_palette(canvas, bitmap->palette, bitmap->palette_id, bitmap->flags);
#ifdef DEBUG_DUMP_BITMAP
    if (palette) {
        dump_bitmap(bitmap, palette);
    }
#endif

    surface = canvas_bitmap_to_surface(canvas, bitmap, palette, want_original);

    if (palette && (bitmap->flags & SPICE_BITMAP_FLAGS_PAL_FROM_CACHE)) {
        canvas->palette_cache->ops->release(canvas->palette_cache, palette);
    }

    return surface;
}

#else


static pixman_image_t *canvas_get_bits(CanvasBase *canvas, SpiceBitmap *bitmap,
                                       int want_original)
{
    SpicePalette *palette;

    if (!bitmap->palette) {
        return canvas_bitmap_to_surface(canvas, bitmap, NULL, want_original);
    }
    palette = (SpicePalette *)SPICE_GET_ADDRESS(bitmap->palette);
    return canvas_bitmap_to_surface(canvas, bitmap, palette, want_original);
}

#endif



// caution: defining DEBUG_DUMP_SURFACE will dump both cached & non-cached
//          images to disk. it will reduce performance dramatically & eat
//          disk space rapidly. use it only for debugging.
//#define DEBUG_DUMP_SURFACE

#if defined(DEBUG_DUMP_SURFACE) || defined(DEBUG_DUMP_COMPRESS)

static void dump_surface(pixman_image_t *surface, int cache)
{
    static uint32_t file_id = 0;
    int i, j;
    char file_str[200];
    int depth = pixman_image_get_depth(surface);

    if (depth != 24 && depth != 32) {
        return;
    }

    uint8_t *data = (uint8_t *)pixman_image_get_data(surface);
    int width = pixman_image_get_width(surface);
    int height = pixman_image_get_height(surface);
    int stride = pixman_image_surface_get_stride(surface);

    uint32_t id = ++file_id;
#ifdef WIN32
    sprintf(file_str, "c:\\tmp\\spice_dump\\%d\\%u.ppm", cache, id);
#else
    sprintf(file_str, "/tmp/spice_dump/%u.ppm", id);
#endif
    FILE *f = fopen(file_str, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n");
    fprintf(f, "%d %d\n", width, height);
    fprintf(f, "#spicec dump\n");
    fprintf(f, "255\n");
    for (i = 0; i < height; i++, data += stride) {
        uint8_t *now = data;
        for (j = 0; j < width; j++) {
            fwrite(&now[2], 1, 1, f);
            fwrite(&now[1], 1, 1, f);
            fwrite(&now[0], 1, 1, f);
            now += 4;
        }
    }
    fclose(f);
}

#endif

static SpiceCanvas *canvas_get_surface_internal(CanvasBase *canvas, SpiceImage *image)
{
    if (image->descriptor.type == SPICE_IMAGE_TYPE_SURFACE) {
        SpiceSurface *surface = &image->u.surface;
        return canvas->surfaces->ops->get(canvas->surfaces, surface->surface_id);
    }
    return NULL;
}

static SpiceCanvas *canvas_get_surface_mask_internal(CanvasBase *canvas, SpiceImage *image)
{
    if (image->descriptor.type == SPICE_IMAGE_TYPE_SURFACE) {
        SpiceSurface *surface = &image->u.surface;
        return canvas->surfaces->ops->get(canvas->surfaces, surface->surface_id);
    }
    return NULL;
}


#if defined(SW_CANVAS_CACHE)
static int image_has_palette_to_cache(SpiceImage *image)
{
    SpiceImageDescriptor *descriptor = &image->descriptor;

    if (descriptor->type == SPICE_IMAGE_TYPE_BITMAP) {
        return image->u.bitmap.palette &&
               (image->u.bitmap.flags & SPICE_BITMAP_FLAGS_PAL_CACHE_ME);
    } else if (descriptor->type == SPICE_IMAGE_TYPE_LZ_PLT) {
        return image->u.lz_plt.palette &&
               (image->u.lz_plt.flags & SPICE_BITMAP_FLAGS_PAL_CACHE_ME);
    }
    return FALSE;
}
#endif

#if defined(SW_CANVAS_CACHE) || defined(SW_CANVAS_IMAGE_CACHE)
//#define DEBUG_LZ

/* If real get is FALSE, then only do whatever is needed but don't return an image. For instance,
 *  if we need to read it to cache it we do.
 *
 * This generally converts the image to the right type for the canvas.
 * However, if want_original is set the real source format is returned, and
 * you have to be able to handle any image format. This is useful to avoid
 * e.g. losing alpha when blending a argb32 image on a rgb16 surface.
 */
static pixman_image_t *canvas_get_image_internal(CanvasBase *canvas, SpiceImage *image,
                                                 int want_original, int real_get)
{
    SpiceImageDescriptor *descriptor = &image->descriptor;
    pixman_image_t *surface, *converted;
    pixman_format_code_t wanted_format, surface_format;
    int saved_want_original;

    /* When touching, only really allocate if we need to cache, or
     * if we're loading a GLZ stream (since those need inter-thread communication
     * to happen which breaks if we don't. */
    if (!real_get &&
        !(descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_ME) &&
#ifdef SW_CANVAS_CACHE
        !(descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_REPLACE_ME) &&
        !image_has_palette_to_cache(image) &&
#endif
        (descriptor->type != SPICE_IMAGE_TYPE_GLZ_RGB) &&
        (descriptor->type != SPICE_IMAGE_TYPE_ZLIB_GLZ_RGB)) {
        return NULL;
    }

    saved_want_original = want_original;
    if (descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_ME
#ifdef SW_CANVAS_CACHE
        || descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_REPLACE_ME
#endif
       ) {
        want_original = TRUE;
    }

    switch (descriptor->type) {
    case SPICE_IMAGE_TYPE_QUIC: {
        surface = canvas_get_quic(canvas, image, 0, want_original);
        break;
    }
#if defined(SW_CANVAS_CACHE)
    case SPICE_IMAGE_TYPE_LZ_PLT: {
        surface = canvas_get_lz(canvas, image, 0, want_original);
        break;
    }
    case SPICE_IMAGE_TYPE_LZ_RGB: {
        surface = canvas_get_lz(canvas, image, 0, want_original);
        break;
    }
#endif
    case SPICE_IMAGE_TYPE_JPEG: {
        surface = canvas_get_jpeg(canvas, image, 0);
        break;
    }
    case SPICE_IMAGE_TYPE_JPEG_ALPHA: {
        surface = canvas_get_jpeg_alpha(canvas, image, 0);
        break;
    }
#if defined(SW_CANVAS_CACHE)
    case SPICE_IMAGE_TYPE_GLZ_RGB: {
        surface = canvas_get_glz(canvas, image, want_original);
        break;
    }
    case SPICE_IMAGE_TYPE_ZLIB_GLZ_RGB: {
        surface = canvas_get_zlib_glz_rgb(canvas, image, want_original);
        break;
    }
#endif
    case SPICE_IMAGE_TYPE_FROM_CACHE:
        surface = canvas->bits_cache->ops->get(canvas->bits_cache, descriptor->id);
        break;
#ifdef SW_CANVAS_CACHE
    case SPICE_IMAGE_TYPE_FROM_CACHE_LOSSLESS:
        surface = canvas->bits_cache->ops->get_lossless(canvas->bits_cache, descriptor->id);
        break;
#endif
    case SPICE_IMAGE_TYPE_BITMAP: {
        surface = canvas_get_bits(canvas, &image->u.bitmap, want_original);
        break;
    }
    default:
        spice_warn_if_reached();
        return NULL;
    }

    spice_return_val_if_fail(surface != NULL, NULL);
    spice_return_val_if_fail(spice_pixman_image_get_format(surface, &surface_format), NULL);

    if (descriptor->flags & SPICE_IMAGE_FLAGS_HIGH_BITS_SET &&
        descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE &&
#ifdef SW_CANVAS_CACHE
        descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE_LOSSLESS &&
#endif
        surface_format == PIXMAN_x8r8g8b8) {
        spice_pixman_fill_rect_rop(surface,
                                   0, 0,
                                   pixman_image_get_width(surface),
                                   pixman_image_get_height(surface),
                                   0xff000000U, SPICE_ROP_OR);
    }

    if (descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_ME &&
#ifdef SW_CANVAS_CACHE
        descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE_LOSSLESS &&
#endif
        descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE ) {
#ifdef SW_CANVAS_CACHE
        if (!IS_IMAGE_LOSSY(descriptor)) {
            canvas->bits_cache->ops->put(canvas->bits_cache, descriptor->id, surface);
        } else {
            canvas->bits_cache->ops->put_lossy(canvas->bits_cache, descriptor->id, surface);
        }
#else
        canvas->bits_cache->ops->put(canvas->bits_cache, descriptor->id, surface);
#endif
#ifdef DEBUG_DUMP_SURFACE
        dump_surface(surface, 1);
#endif
#ifdef SW_CANVAS_CACHE
    } else if (descriptor->flags & SPICE_IMAGE_FLAGS_CACHE_REPLACE_ME) {
        if (IS_IMAGE_LOSSY(descriptor)) {
            spice_warning("invalid cache replace request: the image is lossy");
            return NULL;
        }
        canvas->bits_cache->ops->replace_lossy(canvas->bits_cache, descriptor->id, surface);
#ifdef DEBUG_DUMP_SURFACE
        dump_surface(surface, 1);
#endif
#endif
#ifdef DEBUG_DUMP_SURFACE
    } else if (descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE
#ifdef SW_CANVAS_CACHE
               && descriptor->type != SPICE_IMAGE_TYPE_FROM_CACHE_LOSSLESS
#endif
    ) {

        dump_surface(surface, 0);
#endif
    }

    if (!real_get) {
        pixman_image_unref(surface);
        return NULL;
    }

    if (!saved_want_original) {
        /* Conversion to canvas format was requested, but maybe it didn't
           happen above (due to save/load to cache for instance, or
           maybe the reader didn't support conversion).
           If so we convert here. */

        wanted_format = canvas_get_target_format(canvas,
                                                 surface_format == PIXMAN_a8r8g8b8);

        if (surface_format != wanted_format) {
            converted = surface_create(
#ifdef WIN32
                                       canvas->dc,
#endif
                                       wanted_format,
                                       pixman_image_get_width(surface),
                                       pixman_image_get_height(surface),
                                       TRUE);
            pixman_image_composite32 (PIXMAN_OP_SRC,
                                      surface, NULL, converted,
                                      0, 0,
                                      0, 0,
                                      0, 0,
                                      pixman_image_get_width(surface),
                                      pixman_image_get_height(surface));
            pixman_image_unref (surface);
            surface = converted;
        }
    }

    return surface;
}

#else

static pixman_image_t *canvas_get_image_internal(CanvasBase *canvas, SpiceImage *image,
                                                 int want_original, int real_get)
{
    SpiceImageDescriptor *descriptor = &image->descriptor;
    pixman_format_code_t format;

    /* When touching, never load image. */
    if (!real_get) {
        return NULL;
    }

    switch (descriptor->type) {
    case SPICE_IMAGE_TYPE_QUIC: {
        return canvas_get_quic(canvas, image, 0);
    }
    case SPICE_IMAGE_TYPE_BITMAP: {
        return canvas_get_bits(canvas, &image->u.bitmap, want_original, &format);
    }
    default:
        spice_warn_if_reached();
        return NULL;
    }

    return NULL;
}

#endif

static SpiceCanvas *canvas_get_surface_mask(CanvasBase *canvas, SpiceImage *image)
{
    return canvas_get_surface_mask_internal(canvas, image);
}

static SpiceCanvas *canvas_get_surface(CanvasBase *canvas, SpiceImage *image)
{
    return canvas_get_surface_internal(canvas, image);
}

static pixman_image_t *canvas_get_image(CanvasBase *canvas, SpiceImage *image,
                                        int want_original)
{
    return canvas_get_image_internal(canvas, image, want_original, TRUE);
}

static void canvas_touch_image(CanvasBase *canvas, SpiceImage *image)
{
    canvas_get_image_internal(canvas, image, TRUE, FALSE);
}

static pixman_image_t* canvas_get_image_from_self(SpiceCanvas *canvas,
                                                  int x, int y,
                                                  int32_t width, int32_t height,
                                                  int force_opaque)
{
    CanvasBase *canvas_base = (CanvasBase *)canvas;
    pixman_image_t *surface;
    uint8_t *dest;
    int dest_stride;
    SpiceRect area;
    pixman_format_code_t format;

    format = spice_surface_format_to_pixman (canvas_base->format);
    if (force_opaque)
    {
        /* Set alpha bits of the format to 0 */
        format = (pixman_format_code_t)(((uint32_t)format) & ~(0xf << 12));

        spice_return_val_if_fail (
            pixman_format_supported_destination (format), NULL);
    }

    surface = pixman_image_create_bits(spice_surface_format_to_pixman (canvas_base->format),
                                       width, height, NULL, 0);
    spice_return_val_if_fail(surface != NULL, NULL);

    dest = (uint8_t *)pixman_image_get_data(surface);
    dest_stride = pixman_image_get_stride(surface);

    area.left = x;
    area.top = y;
    area.right = x + width;
    area.bottom = y + height;

    canvas->ops->read_bits(canvas, dest, dest_stride, &area);

    return surface;
}

static inline uint8_t revers_bits(uint8_t byte)
{
    uint8_t ret = 0;
    int i;

    for (i = 0; i < 4; i++) {
        int shift = 7 - i * 2;
        ret |= (byte & (1 << i)) << shift;
        ret |= (byte & (0x80 >> i)) >> shift;
    }
    return ret;
}

static pixman_image_t *canvas_get_bitmap_mask(CanvasBase *canvas, SpiceBitmap* bitmap, int invers)
{
    pixman_image_t *surface;
    uint8_t *src_line;
    uint8_t *end_line;
    uint8_t *dest_line;
    int src_stride;
    int line_size;
    int dest_stride;

    surface = surface_create(
#ifdef WIN32
            canvas->dc,
#endif
            PIXMAN_a1, bitmap->x, bitmap->y, TRUE);
    spice_return_val_if_fail(surface != NULL, NULL);

    spice_chunks_linearize(bitmap->data);
    src_line = bitmap->data->chunk[0].data;
    src_stride = bitmap->stride;
    end_line = src_line + (bitmap->y * src_stride);
    line_size = SPICE_ALIGN(bitmap->x, 8) >> 3;

    dest_stride = pixman_image_get_stride(surface);
    dest_line = (uint8_t *)pixman_image_get_data(surface);
#if defined(GL_CANVAS)
    if ((bitmap->flags & SPICE_BITMAP_FLAGS_TOP_DOWN)) {
#else
    if (!(bitmap->flags & SPICE_BITMAP_FLAGS_TOP_DOWN)) {
#endif
        spice_return_val_if_fail(bitmap->y > 0, NULL);
        dest_line += dest_stride * ((int)bitmap->y - 1);
        dest_stride = -dest_stride;
    }

    if (invers) {
        switch (bitmap->format) {
#if defined(GL_CANVAS) || defined(GDI_CANVAS)
        case SPICE_BITMAP_FMT_1BIT_BE:
#else
        case SPICE_BITMAP_FMT_1BIT_LE:
#endif
            for (; src_line != end_line; src_line += src_stride, dest_line += dest_stride) {
                uint8_t *dest = dest_line;
                uint8_t *now = src_line;
                uint8_t *end = now + line_size;
                while (now < end) {
                    *(dest++) = ~*(now++);
                }
            }
            break;
#if defined(GL_CANVAS) || defined(GDI_CANVAS)
        case SPICE_BITMAP_FMT_1BIT_LE:
#else
        case SPICE_BITMAP_FMT_1BIT_BE:
#endif
            for (; src_line != end_line; src_line += src_stride, dest_line += dest_stride) {
                uint8_t *dest = dest_line;
                uint8_t *now = src_line;
                uint8_t *end = now + line_size;

                while (now < end) {
                    *(dest++) = ~revers_bits(*(now++));
                }
            }
            break;
        default:
            pixman_image_unref(surface);
            surface = NULL;
            spice_warn_if_reached();
            return NULL;
        }
    } else {
        switch (bitmap->format) {
#if defined(GL_CANVAS) || defined(GDI_CANVAS)
        case SPICE_BITMAP_FMT_1BIT_BE:
#else
        case SPICE_BITMAP_FMT_1BIT_LE:
#endif
            for (; src_line != end_line; src_line += src_stride, dest_line += dest_stride) {
                memcpy(dest_line, src_line, line_size);
            }
            break;
#if defined(GL_CANVAS) || defined(GDI_CANVAS)
        case SPICE_BITMAP_FMT_1BIT_LE:
#else
        case SPICE_BITMAP_FMT_1BIT_BE:
#endif
            for (; src_line != end_line; src_line += src_stride, dest_line += dest_stride) {
                uint8_t *dest = dest_line;
                uint8_t *now = src_line;
                uint8_t *end = now + line_size;

                while (now < end) {
                    *(dest++) = revers_bits(*(now++));
                }
            }
            break;
        default:
            pixman_image_unref(surface);
            surface = NULL;
            spice_warn_if_reached();
            return NULL;
        }
    }
    return surface;
}

static inline pixman_image_t *canvas_A1_invers(pixman_image_t *src_surf)
{
    int width = pixman_image_get_width(src_surf);
    int height = pixman_image_get_height(src_surf);
    pixman_image_t * invers;
    uint8_t *src_line, *end_line,  *dest_line;
    int src_stride, line_size, dest_stride;

    spice_return_val_if_fail(pixman_image_get_depth(src_surf) == 1, NULL);

    invers = pixman_image_create_bits(PIXMAN_a1, width, height, NULL, 0);
    spice_return_val_if_fail(invers != NULL, NULL);

    src_line = (uint8_t *)pixman_image_get_data(src_surf);
    src_stride = pixman_image_get_stride(src_surf);
    end_line = src_line + (height * src_stride);
    line_size = SPICE_ALIGN(width, 8) >> 3;
    dest_line = (uint8_t *)pixman_image_get_data(invers);
    dest_stride = pixman_image_get_stride(invers);

    for (; src_line != end_line; src_line += src_stride, dest_line += dest_stride) {
        uint8_t *dest = dest_line;
        uint8_t *now = src_line;
        uint8_t *end = now + line_size;
        while (now < end) {
            *(dest++) = ~*(now++);
        }
    }
    return invers;
}

static pixman_image_t *canvas_get_mask(CanvasBase *canvas, SpiceQMask *mask, int *needs_invert_out)
{
    SpiceImage *image;
    pixman_image_t *surface;
    int need_invers;
    int is_invers;
    int cache_me;

    if (needs_invert_out) {
        *needs_invert_out = 0;
    }

    image = mask->bitmap;
    need_invers = mask->flags & SPICE_MASK_FLAGS_INVERS;

#ifdef SW_CANVAS_CACHE
    cache_me = image->descriptor.flags & SPICE_IMAGE_FLAGS_CACHE_ME;
#else
    cache_me = 0;
#endif

    switch (image->descriptor.type) {
    case SPICE_IMAGE_TYPE_BITMAP: {
        is_invers = need_invers && !cache_me;
        surface = canvas_get_bitmap_mask(canvas, &image->u.bitmap, is_invers);
        break;
    }
#if defined(SW_CANVAS_CACHE) || defined(SW_CANVAS_IMAGE_CACHE)
    case SPICE_IMAGE_TYPE_FROM_CACHE:
        surface = canvas->bits_cache->ops->get(canvas->bits_cache, image->descriptor.id);
        is_invers = 0;
        break;
#endif
#ifdef SW_CANVAS_CACHE
    case SPICE_IMAGE_TYPE_FROM_CACHE_LOSSLESS:
        surface = canvas->bits_cache->ops->get_lossless(canvas->bits_cache, image->descriptor.id);
        is_invers = 0;
        break;
#endif
    default:
        spice_warn_if_reached();
        return NULL;
    }

#if defined(SW_CANVAS_CACHE) || defined(SW_CANVAS_IMAGE_CACHE)
    if (cache_me) {
        canvas->bits_cache->ops->put(canvas->bits_cache, image->descriptor.id, surface);
    }

    if (need_invers && !is_invers) { // surface is in cache
        if (needs_invert_out != NULL) {
            *needs_invert_out = TRUE;
        } else {
            pixman_image_t *inv_surf;
            inv_surf = canvas_A1_invers(surface);
            pixman_image_unref(surface);
            surface = inv_surf;
        }
    }
#endif
    return surface;
}

static inline void canvas_raster_glyph_box(const SpiceRasterGlyph *glyph, SpiceRect *r)
{
    spice_return_if_fail(r != NULL);

    r->top = glyph->render_pos.y + glyph->glyph_origin.y;
    r->bottom = r->top + glyph->height;
    r->left = glyph->render_pos.x + glyph->glyph_origin.x;
    r->right = r->left + glyph->width;
}

#ifdef GL_CANVAS
static inline void __canvas_put_bits(uint8_t *dest, int offset, uint8_t val, int n)
{
    uint8_t mask;
    int now;

    dest = dest + (offset >> 3);
    offset &= 0x07;
    now = MIN(8 - offset, n);

    mask = ~((1 << (8 - now)) - 1);
    mask >>= offset;
    *dest = ((val >> offset) & mask) | *dest;

    if ((n = n - now)) {
        mask = ~((1 << (8 - n)) - 1);
        dest++;
        *dest = ((val << now) & mask) | *dest;
    }
}

#else
static inline void __canvas_put_bits(uint8_t *dest, int offset, uint8_t val, int n)
{
    uint8_t mask;
    int now;

    dest = dest + (offset >> 3);
    offset &= 0x07;

    now = MIN(8 - offset, n);

    mask = (1 << now) - 1;
    mask <<= offset;
    val = revers_bits(val);
    *dest = ((val << offset) & mask) | *dest;

    if ((n = n - now)) {
        mask = (1 << n) - 1;
        dest++;
        *dest = ((val >> now) & mask) | *dest;
    }
}

#endif

static inline void canvas_put_bits(uint8_t *dest, int dest_offset, uint8_t *src, int n)
{
    while (n) {
        int now = MIN(n, 8);

        n -= now;
        __canvas_put_bits(dest, dest_offset, *src, now);
        dest_offset += now;
        src++;
    }
}

static void canvas_put_glyph_bits(SpiceRasterGlyph *glyph, int bpp, uint8_t *dest, int dest_stride,
                                  SpiceRect *bounds)
{
    SpiceRect glyph_box;
    uint8_t *src;
    int lines;
    int width;

    //todo: support SPICE_STRING_FLAGS_RASTER_TOP_DOWN
    canvas_raster_glyph_box(glyph, &glyph_box);
    spice_return_if_fail(glyph_box.top >= bounds->top && glyph_box.bottom <= bounds->bottom);
    spice_return_if_fail(glyph_box.left >= bounds->left && glyph_box.right <= bounds->right);
    rect_offset(&glyph_box, -bounds->left, -bounds->top);

    dest += glyph_box.top * dest_stride;
    src = glyph->data;
    lines = glyph_box.bottom - glyph_box.top;
    width = glyph_box.right - glyph_box.left;
    switch (bpp) {
    case 1: {
        int src_stride = SPICE_ALIGN(width, 8) >> 3;
        int i;

        src += src_stride * (lines);
        for (i = 0; i < lines; i++) {
            src -= src_stride;
            canvas_put_bits(dest, glyph_box.left, src, width);
            dest += dest_stride;
        }
        break;
    }
    case 4: {
        uint8_t *end;
        int src_stride = SPICE_ALIGN(width * 4, 8) >> 3;

        src += src_stride * lines;
        dest += glyph_box.left;
        end = dest + dest_stride * lines;
        for (; dest != end; dest += dest_stride) {
            int i = 0;
            uint8_t *now;

            src -= src_stride;
            now = src;
            while (i < (width & ~1)) {
                dest[i] = MAX(dest[i], *now & 0xf0);
                dest[i + 1] = MAX(dest[i + 1], *now << 4);
                i += 2;
                now++;
            }
            if (i < width) {
                dest[i] = MAX(dest[i], *now & 0xf0);
                now++;
            }
        }
        break;
    }
    case 8: {
        uint8_t *end;
        src += width * lines;
        dest += glyph_box.left;
        end = dest + dest_stride * lines;
        for (; dest != end; dest += dest_stride, src -= width) {
            int i;

            for (i = 0; i < width; i++) {
                dest[i] = MAX(dest[i], src[i]);
            }
        }
        break;
    }
    default:
        spice_warn_if_reached();
        return;
    }
}

static pixman_image_t *canvas_get_str_mask(CanvasBase *canvas, SpiceString *str, int bpp, SpicePoint *pos)
{
    SpiceRasterGlyph *glyph;
    SpiceRect bounds;
    pixman_image_t *str_mask;
    uint8_t *dest;
    int dest_stride;
    int i;

    spice_return_val_if_fail(str->length > 0, NULL);

    glyph = str->glyphs[0];
    canvas_raster_glyph_box(glyph, &bounds);

    for (i = 1; i < str->length; i++) {
        SpiceRect glyph_box;

        canvas_raster_glyph_box(str->glyphs[i], &glyph_box);
        rect_union(&bounds, &glyph_box);
    }

    str_mask = pixman_image_create_bits((bpp == 1) ? PIXMAN_a1 : PIXMAN_a8,
                                        bounds.right - bounds.left,
                                        bounds.bottom - bounds.top, NULL, 0);
    spice_return_val_if_fail(str_mask != NULL, NULL);

    dest = (uint8_t *)pixman_image_get_data(str_mask);
    dest_stride = pixman_image_get_stride(str_mask);
    for (i = 0; i < str->length; i++) {
        glyph = str->glyphs[i];
#if defined(GL_CANVAS)
        canvas_put_glyph_bits(glyph, bpp, dest + (bounds.bottom - bounds.top - 1) * dest_stride,
                              -dest_stride, &bounds);
#else
        canvas_put_glyph_bits(glyph, bpp, dest, dest_stride, &bounds);
#endif
    }

    pos->x = bounds.left;
    pos->y = bounds.top;
    return str_mask;
}

static pixman_image_t *canvas_scale_surface(pixman_image_t *src, const SpiceRect *src_area, int width,
                                            int height, int scale_mode)
{
    pixman_image_t *surface;
    pixman_transform_t transform;
    pixman_format_code_t format;
    double sx, sy;

    spice_return_val_if_fail(spice_pixman_image_get_format (src, &format), NULL);

    surface = pixman_image_create_bits(format, width, height, NULL, 0);
    spice_return_val_if_fail(surface != NULL, NULL);

    sx = (double)(src_area->right - src_area->left) / width;
    sy = (double)(src_area->bottom - src_area->top) / height;

    pixman_transform_init_scale(&transform, pixman_double_to_fixed(sx), pixman_double_to_fixed(sy));

    pixman_image_set_transform (src, &transform);
    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);
    spice_return_val_if_fail(scale_mode == SPICE_IMAGE_SCALE_MODE_INTERPOLATE || scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST, NULL);
    pixman_image_set_filter(src,
                            (scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST) ?PIXMAN_FILTER_NEAREST : PIXMAN_FILTER_GOOD,
                            NULL, 0);

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src, NULL, surface,
                             ROUND(src_area->left / sx), ROUND (src_area->top / sy),
                             0, 0, /* mask */
                             0, 0, /* dst */
                             width, height);

    pixman_transform_init_identity(&transform);
    pixman_image_set_transform(src, &transform);

    return surface;
}

SPICE_ATTR_NORETURN
SPICE_ATTR_PRINTF(2, 3) static void quic_usr_error(QuicUsrContext *usr, const char *fmt, ...)
{
    QuicData *usr_data = (QuicData *)usr;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(usr_data->message_buf, sizeof(usr_data->message_buf), fmt, ap);
    va_end(ap);

    longjmp(usr_data->jmp_env, 1);
}

SPICE_ATTR_PRINTF(2, 3) static void quic_usr_warn(QuicUsrContext *usr, const char *fmt, ...)
{
    QuicData *usr_data = (QuicData *)usr;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(usr_data->message_buf, sizeof(usr_data->message_buf), fmt, ap);
    va_end(ap);
}

static void *quic_usr_malloc(QuicUsrContext *usr, int size)
{
    return spice_malloc(size);
}

static void quic_usr_free(QuicUsrContext *usr, void *ptr)
{
    free(ptr);
}

SPICE_ATTR_PRINTF(2, 3) static void lz_usr_warn(LzUsrContext *usr, const char *fmt, ...)
{
    LzData *usr_data = (LzData *)usr;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(usr_data->message_buf, sizeof(usr_data->message_buf), fmt, ap);
    va_end(ap);
}

SPICE_ATTR_NORETURN
SPICE_ATTR_PRINTF(2, 3) static void lz_usr_error(LzUsrContext *usr, const char *fmt, ...)
{
    LzData *usr_data = (LzData *)usr;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(usr_data->message_buf, sizeof(usr_data->message_buf), fmt, ap);
    va_end(ap);

    longjmp(usr_data->jmp_env, 1);
}

static void *lz_usr_malloc(LzUsrContext *usr, int size)
{
    return spice_malloc(size);
}

static void lz_usr_free(LzUsrContext *usr, void *ptr)
{
    free(ptr);
}

static int lz_usr_more_space(LzUsrContext *usr, uint8_t **io_ptr)
{
    return 0;
}

static int lz_usr_more_lines(LzUsrContext *usr, uint8_t **lines)
{
    return 0;
}

static int quic_usr_more_space(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed)
{
    QuicData *quic_data = (QuicData *)usr;

    if (quic_data->current_chunk == quic_data->chunks->num_chunks -1) {
        return 0;
    }
    quic_data->current_chunk++;

    *io_ptr = (uint32_t *)quic_data->chunks->chunk[quic_data->current_chunk].data;
    return quic_data->chunks->chunk[quic_data->current_chunk].len >> 2;
}


static int quic_usr_more_lines(QuicUsrContext *usr, uint8_t **lines)
{
    return 0;
}

static void canvas_base_destroy(CanvasBase *canvas)
{
    quic_destroy(canvas->quic_data.quic);
    lz_destroy(canvas->lz_data.lz);
#ifdef GDI_CANVAS
    DeleteDC(canvas->dc);
#endif

    if (canvas->usr_data && canvas->usr_data_destroy) {
        canvas->usr_data_destroy (canvas->usr_data);
        canvas->usr_data = NULL;
    }
}

/* This is kind of lame, but it protects against multiple
   instances of these functions. We really should stop including
   canvas_base.c and build it separately instead */
#ifdef  CANVAS_SINGLE_INSTANCE

void spice_canvas_set_usr_data(SpiceCanvas *spice_canvas,
                               void *data,
                               spice_destroy_fn_t destroy_fn)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    if (canvas->usr_data && canvas->usr_data_destroy) {
        canvas->usr_data_destroy (canvas->usr_data);
    }
    canvas->usr_data = data;
    canvas->usr_data_destroy = destroy_fn;
}

void *spice_canvas_get_usr_data(SpiceCanvas *spice_canvas)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    return  canvas->usr_data;
}
#endif


static void canvas_clip_pixman(CanvasBase *canvas,
                               pixman_region32_t *dest_region,
                               SpiceClip *clip)
{
    pixman_region32_intersect(dest_region, dest_region, &canvas->canvas_region);

    switch (clip->type) {
    case SPICE_CLIP_TYPE_NONE:
        break;
    case SPICE_CLIP_TYPE_RECTS: {
        uint32_t n = clip->rects->num_rects;
        SpiceRect *now = clip->rects->rects;

        pixman_region32_t clip;

        if (spice_pixman_region32_init_rects(&clip, now, n)) {
            pixman_region32_intersect(dest_region, dest_region, &clip);
            pixman_region32_fini(&clip);
        }

        break;
    }
    default:
        spice_warn_if_reached();
        return;
    }
}

static void canvas_mask_pixman(CanvasBase *canvas,
                               pixman_region32_t *dest_region,
                               SpiceQMask *mask, int x, int y)
{
    SpiceCanvas *surface_canvas;
    pixman_image_t *image, *subimage;
    int needs_invert;
    pixman_region32_t mask_region;
    uint32_t *mask_data;
    int mask_x, mask_y;
    int mask_width, mask_height, mask_stride;
    pixman_box32_t extents;

    if (!mask->bitmap) {
        return;
    }

    surface_canvas = canvas_get_surface_mask(canvas, mask->bitmap);
    if (surface_canvas) {
        needs_invert = mask->flags & SPICE_MASK_FLAGS_INVERS;
        image = surface_canvas->ops->get_image(surface_canvas, FALSE);
    } else {
        needs_invert = FALSE;
        image = canvas_get_mask(canvas,
                                mask,
                                &needs_invert);
    }

    mask_data = pixman_image_get_data(image);
    mask_width = pixman_image_get_width(image);
    mask_height = pixman_image_get_height(image);
    mask_stride = pixman_image_get_stride(image);

    mask_x = mask->pos.x;
    mask_y = mask->pos.y;

    /* We need to subset the area of the mask that we turn into a region,
       because a cached mask may be much larger than what is used for
       the clip operation. */
    extents = *pixman_region32_extents(dest_region);

    /* convert from destination pixels to mask pixels */
    extents.x1 -= x - mask_x;
    extents.y1 -= y - mask_y;
    extents.x2 -= x - mask_x;
    extents.y2 -= y - mask_y;

    /* clip to mask size */
    if (extents.x1 < 0) {
        extents.x1 = 0;
    }
    if (extents.x2 >= mask_width) {
        extents.x2 = mask_width;
    }
    if (extents.x2 < extents.x1) {
        extents.x2 = extents.x1;
    }
    if (extents.y1 < 0) {
        extents.y1 = 0;
    }
    if (extents.y2 >= mask_height) {
        extents.y2 = mask_height;
    }
    if (extents.y2 < extents.y1) {
        extents.y2 = extents.y1;
    }

    /* round down X to even 32 pixels (i.e. uint32_t) */
    extents.x1 = extents.x1 & ~(0x1f);

    mask_data = (uint32_t *)((uint8_t *)mask_data + mask_stride * extents.y1 + extents.x1 / 32);
    mask_x -= extents.x1;
    mask_y -= extents.y1;
    mask_width = extents.x2 - extents.x1;
    mask_height = extents.y2 - extents.y1;

    subimage = pixman_image_create_bits(PIXMAN_a1, mask_width, mask_height,
                                        mask_data, mask_stride);
    pixman_region32_init_from_image(&mask_region,
                                    subimage);
    pixman_image_unref(subimage);

    if (needs_invert) {
        pixman_box32_t rect;

        rect.x1 = rect.y1 = 0;
        rect.x2 = mask_width;
        rect.y2 = mask_height;

        pixman_region32_inverse(&mask_region, &mask_region, &rect);
    }

    pixman_region32_translate(&mask_region,
                              -mask_x + x, -mask_y + y);

    pixman_region32_intersect(dest_region, dest_region, &mask_region);
    pixman_region32_fini(&mask_region);

    pixman_image_unref(image);
}

static void draw_brush(SpiceCanvas *canvas,
                       pixman_region32_t *region,
                       SpiceBrush *brush,
                       SpiceROP rop)
{
    CanvasBase *canvas_base = (CanvasBase *)canvas;
    uint32_t color;
    SpicePattern *pattern;
    pixman_image_t *tile;
    int offset_x, offset_y;
    pixman_box32_t *rects;
    int n_rects;

    rects = pixman_region32_rectangles(region, &n_rects);

   switch (brush->type) {
    case SPICE_BRUSH_TYPE_SOLID:
        color = brush->u.color;
        if (rop == SPICE_ROP_COPY) {
            canvas->ops->fill_solid_rects(canvas, rects, n_rects, color);
        } else {
            canvas->ops->fill_solid_rects_rop(canvas, rects, n_rects, color, rop);
        }
        break;
        case SPICE_BRUSH_TYPE_PATTERN: {
        SpiceCanvas *surface_canvas;

        pattern = &brush->u.pattern;
        offset_x = pattern->pos.x;
        offset_y = pattern->pos.y;

        surface_canvas = canvas_get_surface(canvas_base, pattern->pat);
        if (surface_canvas) {
            if (rop == SPICE_ROP_COPY) {
                canvas->ops->fill_tiled_rects_from_surface(canvas, rects, n_rects, surface_canvas,
                                                           offset_x, offset_y);
            } else {
                canvas->ops->fill_tiled_rects_rop_from_surface(canvas, rects, n_rects,
                                                               surface_canvas, offset_x, offset_y,
                                                               rop);
            }
        } else {
            tile = canvas_get_image(canvas_base, pattern->pat, FALSE);
            spice_return_if_fail(tile != NULL);

            if (rop == SPICE_ROP_COPY) {
                canvas->ops->fill_tiled_rects(canvas, rects, n_rects, tile, offset_x, offset_y);
            } else {
                canvas->ops->fill_tiled_rects_rop(canvas, rects, n_rects,
                                                  tile, offset_x, offset_y, rop);
            }
            pixman_image_unref(tile);
        }
        break;
    }
    case SPICE_BRUSH_TYPE_NONE:
        /* Still need to do *something* here, because rop could be e.g invert dest */
        canvas->ops->fill_solid_rects_rop(canvas, rects, n_rects, 0, rop);
        break;
    default:
        spice_warn_if_reached();
        return;
    }
}

/* If we're exiting early we may still have to load an image in case
   it has to be cached or something */
static void touch_brush(CanvasBase *canvas, SpiceBrush *brush)
{
    SpicePattern *pattern;

    if (brush->type == SPICE_BRUSH_TYPE_PATTERN) {
        pattern = &brush->u.pattern;
        canvas_touch_image(canvas, pattern->pat);
    }
}

static void canvas_draw_fill(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceFill *fill)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    SpiceROP rop;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);


    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &fill->mask,
                       bbox->left, bbox->top);

    rop = ropd_descriptor_to_rop(fill->rop_descriptor,
                                 ROP_INPUT_BRUSH,
                                 ROP_INPUT_DEST);

    if (rop == SPICE_ROP_NOOP || !pixman_region32_not_empty(&dest_region)) {
        touch_brush(canvas, &fill->brush);
        pixman_region32_fini(&dest_region);
        return;
    }

    draw_brush(spice_canvas, &dest_region, &fill->brush, rop);

    pixman_region32_fini(&dest_region);
}

static void canvas_draw_copy(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceCopy *copy)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    SpiceCanvas *surface_canvas;
    pixman_image_t *src_image;
    SpiceROP rop;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &copy->mask,
                       bbox->left, bbox->top);

    rop = ropd_descriptor_to_rop(copy->rop_descriptor,
                                 ROP_INPUT_SRC,
                                 ROP_INPUT_DEST);

    if (rop == SPICE_ROP_NOOP || !pixman_region32_not_empty(&dest_region)) {
        canvas_touch_image(canvas, copy->src_bitmap);
        pixman_region32_fini(&dest_region);
        return;
    }

    surface_canvas = canvas_get_surface(canvas, copy->src_bitmap);
    if (surface_canvas) {
        if (rect_is_same_size(bbox, &copy->src_area)) {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->blit_image_from_surface(spice_canvas, &dest_region,
                                                           surface_canvas,
                                                           bbox->left - copy->src_area.left,
                                                           bbox->top - copy->src_area.top);
            } else {
                spice_canvas->ops->blit_image_rop_from_surface(spice_canvas, &dest_region,
                                                               surface_canvas,
                                                               bbox->left - copy->src_area.left,
                                                               bbox->top - copy->src_area.top,
                                                               rop);
            }
        } else {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->scale_image_from_surface(spice_canvas, &dest_region,
                                                            surface_canvas,
                                                            copy->src_area.left,
                                                            copy->src_area.top,
                                                            copy->src_area.right - copy->src_area.left,
                                                            copy->src_area.bottom - copy->src_area.top,
                                                            bbox->left,
                                                            bbox->top,
                                                            bbox->right - bbox->left,
                                                            bbox->bottom - bbox->top,
                                                            copy->scale_mode);
            } else {
                spice_canvas->ops->scale_image_rop_from_surface(spice_canvas, &dest_region,
                                                                surface_canvas,
                                                                copy->src_area.left,
                                                                copy->src_area.top,
                                                                copy->src_area.right - copy->src_area.left,
                                                                copy->src_area.bottom - copy->src_area.top,
                                                                bbox->left,
                                                                bbox->top,
                                                                bbox->right - bbox->left,
                                                                bbox->bottom - bbox->top,
                                                                copy->scale_mode,
                                                                rop);
            }
        }
    } else {
        src_image = canvas_get_image(canvas, copy->src_bitmap, FALSE);
        spice_return_if_fail(src_image != NULL);

        if (rect_is_same_size(bbox, &copy->src_area)) {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->blit_image(spice_canvas, &dest_region,
                                              src_image,
                                              bbox->left - copy->src_area.left,
                                              bbox->top - copy->src_area.top);
            } else {
                spice_canvas->ops->blit_image_rop(spice_canvas, &dest_region,
                                                  src_image,
                                                  bbox->left - copy->src_area.left,
                                                  bbox->top - copy->src_area.top,
                                                  rop);
            }
        } else {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->scale_image(spice_canvas, &dest_region,
                                               src_image,
                                               copy->src_area.left,
                                               copy->src_area.top,
                                               copy->src_area.right - copy->src_area.left,
                                               copy->src_area.bottom - copy->src_area.top,
                                               bbox->left,
                                               bbox->top,
                                               bbox->right - bbox->left,
                                               bbox->bottom - bbox->top,
                                               copy->scale_mode);
            } else {
                spice_canvas->ops->scale_image_rop(spice_canvas, &dest_region,
                                                   src_image,
                                                   copy->src_area.left,
                                                   copy->src_area.top,
                                                   copy->src_area.right - copy->src_area.left,
                                                   copy->src_area.bottom - copy->src_area.top,
                                                   bbox->left,
                                                   bbox->top,
                                                   bbox->right - bbox->left,
                                                   bbox->bottom - bbox->top,
                                                   copy->scale_mode,
                                                   rop);
            }
        }
        pixman_image_unref(src_image);
    }
    pixman_region32_fini(&dest_region);
}

static void canvas_draw_transparent(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceTransparent* transparent)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    SpiceCanvas *surface_canvas;
    pixman_image_t *src_image;
    pixman_region32_t dest_region;
    uint32_t transparent_color;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);

    if (pixman_region32_n_rects (&dest_region) == 0) {
        canvas_touch_image(canvas, transparent->src_bitmap);
        pixman_region32_fini(&dest_region);
        return;
    }

    switch (canvas->format) {
    case SPICE_SURFACE_FMT_32_xRGB:
    case SPICE_SURFACE_FMT_32_ARGB:
        transparent_color = transparent->true_color;
        break;
    case SPICE_SURFACE_FMT_16_555:
        transparent_color = rgb_32_to_16_555(transparent->true_color);
        break;
    case SPICE_SURFACE_FMT_16_565:
        transparent_color = rgb_32_to_16_565(transparent->true_color);
        break;
    default:
        transparent_color = 0;
    }

    surface_canvas = canvas_get_surface(canvas, transparent->src_bitmap);
    if (surface_canvas) {
        if (rect_is_same_size(bbox, &transparent->src_area)) {
            spice_canvas->ops->colorkey_image_from_surface(spice_canvas, &dest_region,
                                                           surface_canvas,
                                                           bbox->left - transparent->src_area.left,
                                                           bbox->top - transparent->src_area.top,
                                                           transparent_color);
        } else {
            spice_canvas->ops->colorkey_scale_image_from_surface(spice_canvas, &dest_region,
                                                                 surface_canvas,
                                                                 transparent->src_area.left,
                                                                 transparent->src_area.top,
                                                                 transparent->src_area.right - transparent->src_area.left,
                                                                 transparent->src_area.bottom - transparent->src_area.top,
                                                                 bbox->left,
                                                                 bbox->top,
                                                                 bbox->right - bbox->left,
                                                                 bbox->bottom - bbox->top,
                                                                 transparent_color);
        }
    } else {
        src_image = canvas_get_image(canvas, transparent->src_bitmap, FALSE);
        spice_return_if_fail(src_image != NULL);

        if (rect_is_same_size(bbox, &transparent->src_area)) {
            spice_canvas->ops->colorkey_image(spice_canvas, &dest_region,
                                              src_image,
                                              bbox->left - transparent->src_area.left,
                                              bbox->top - transparent->src_area.top,
                                              transparent_color);
        } else {
            spice_canvas->ops->colorkey_scale_image(spice_canvas, &dest_region,
                                                    src_image,
                                                    transparent->src_area.left,
                                                    transparent->src_area.top,
                                                    transparent->src_area.right - transparent->src_area.left,
                                                    transparent->src_area.bottom - transparent->src_area.top,
                                                    bbox->left,
                                                    bbox->top,
                                                    bbox->right - bbox->left,
                                                    bbox->bottom - bbox->top,
                                                    transparent_color);
        }
        pixman_image_unref(src_image);
    }
    pixman_region32_fini(&dest_region);
}

static void canvas_draw_alpha_blend(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceAlphaBlend* alpha_blend)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    SpiceCanvas *surface_canvas;
    pixman_image_t *src_image;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);

    if (alpha_blend->alpha == 0 ||
        !pixman_region32_not_empty(&dest_region)) {
        canvas_touch_image(canvas, alpha_blend->src_bitmap);
        pixman_region32_fini(&dest_region);
        return;
    }

    surface_canvas = canvas_get_surface(canvas, alpha_blend->src_bitmap);
    if (surface_canvas) {
        if (rect_is_same_size(bbox, &alpha_blend->src_area)) {
            spice_canvas->ops->blend_image_from_surface(spice_canvas, &dest_region,
                                                        alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_DEST_HAS_ALPHA,
                                                        surface_canvas,
                                                        alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_SRC_SURFACE_HAS_ALPHA,
                                                        alpha_blend->src_area.left,
                                                        alpha_blend->src_area.top,
                                                        bbox->left,
                                                        bbox->top,
                                                        bbox->right - bbox->left,
                                                        bbox->bottom - bbox->top,
                                                        alpha_blend->alpha);
        } else {
            spice_canvas->ops->blend_scale_image_from_surface(spice_canvas, &dest_region,
                                                              alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_DEST_HAS_ALPHA,
                                                              surface_canvas,
                                                              alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_SRC_SURFACE_HAS_ALPHA,
                                                              alpha_blend->src_area.left,
                                                              alpha_blend->src_area.top,
                                                              alpha_blend->src_area.right - alpha_blend->src_area.left,
                                                              alpha_blend->src_area.bottom - alpha_blend->src_area.top,
                                                              bbox->left,
                                                              bbox->top,
                                                              bbox->right - bbox->left,
                                                              bbox->bottom - bbox->top,
                                                              SPICE_IMAGE_SCALE_MODE_NEAREST,
                                                              alpha_blend->alpha);
        }
     } else {
        src_image = canvas_get_image(canvas, alpha_blend->src_bitmap, TRUE);
        spice_return_if_fail(src_image != NULL);

        if (rect_is_same_size(bbox, &alpha_blend->src_area)) {
            spice_canvas->ops->blend_image(spice_canvas, &dest_region,
                                           alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_DEST_HAS_ALPHA,
                                           src_image,
                                           alpha_blend->src_area.left,
                                           alpha_blend->src_area.top,
                                           bbox->left,
                                           bbox->top,
                                           bbox->right - bbox->left,
                                           bbox->bottom - bbox->top,
                                           alpha_blend->alpha);
        } else {
            spice_canvas->ops->blend_scale_image(spice_canvas, &dest_region,
                                                 alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_DEST_HAS_ALPHA,
                                                 src_image,
                                                 alpha_blend->src_area.left,
                                                 alpha_blend->src_area.top,
                                                 alpha_blend->src_area.right - alpha_blend->src_area.left,
                                                 alpha_blend->src_area.bottom - alpha_blend->src_area.top,
                                                 bbox->left,
                                                 bbox->top,
                                                 bbox->right - bbox->left,
                                                 bbox->bottom - bbox->top,
                                                 SPICE_IMAGE_SCALE_MODE_NEAREST,
                                                 alpha_blend->alpha);
        }

        pixman_image_unref(src_image);
    }

    pixman_region32_fini(&dest_region);
}

static void canvas_draw_opaque(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceOpaque *opaque)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_image_t *src_image;
    pixman_region32_t dest_region;
    SpiceCanvas *surface_canvas;
    SpiceROP rop;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &opaque->mask,
                       bbox->left, bbox->top);

    rop = ropd_descriptor_to_rop(opaque->rop_descriptor,
                                 ROP_INPUT_BRUSH,
                                 ROP_INPUT_SRC);

    if (rop == SPICE_ROP_NOOP || !pixman_region32_not_empty(&dest_region)) {
        canvas_touch_image(canvas, opaque->src_bitmap);
        touch_brush(canvas, &opaque->brush);
        pixman_region32_fini(&dest_region);
        return;
    }

    surface_canvas = canvas_get_surface(canvas, opaque->src_bitmap);
    if (surface_canvas) {
        if (rect_is_same_size(bbox, &opaque->src_area)) {
            spice_canvas->ops->blit_image_from_surface(spice_canvas, &dest_region,
                                                       surface_canvas,
                                                       bbox->left - opaque->src_area.left,
                                                       bbox->top - opaque->src_area.top);
        } else {
            spice_canvas->ops->scale_image_from_surface(spice_canvas, &dest_region,
                                                        surface_canvas,
                                                        opaque->src_area.left,
                                                        opaque->src_area.top,
                                                        opaque->src_area.right - opaque->src_area.left,
                                                        opaque->src_area.bottom - opaque->src_area.top,
                                                        bbox->left,
                                                        bbox->top,
                                                        bbox->right - bbox->left,
                                                        bbox->bottom - bbox->top,
                                                        opaque->scale_mode);
        }
    } else {
        src_image = canvas_get_image(canvas, opaque->src_bitmap, FALSE);
        spice_return_if_fail(src_image != NULL);

        if (rect_is_same_size(bbox, &opaque->src_area)) {
            spice_canvas->ops->blit_image(spice_canvas, &dest_region,
                                          src_image,
                                          bbox->left - opaque->src_area.left,
                                          bbox->top - opaque->src_area.top);
        } else {
            spice_canvas->ops->scale_image(spice_canvas, &dest_region,
                                           src_image,
                                           opaque->src_area.left,
                                           opaque->src_area.top,
                                           opaque->src_area.right - opaque->src_area.left,
                                           opaque->src_area.bottom - opaque->src_area.top,
                                           bbox->left,
                                           bbox->top,
                                           bbox->right - bbox->left,
                                           bbox->bottom - bbox->top,
                                           opaque->scale_mode);
        }
        pixman_image_unref(src_image);
    }

    draw_brush(spice_canvas, &dest_region, &opaque->brush, rop);

    pixman_region32_fini(&dest_region);
}

static void canvas_draw_blend(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlend *blend)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    SpiceCanvas *surface_canvas;
    pixman_image_t *src_image;
    pixman_region32_t dest_region;
    SpiceROP rop;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &blend->mask,
                       bbox->left, bbox->top);

    rop = ropd_descriptor_to_rop(blend->rop_descriptor,
                                 ROP_INPUT_SRC,
                                 ROP_INPUT_DEST);

    if (rop == SPICE_ROP_NOOP || !pixman_region32_not_empty(&dest_region)) {
        canvas_touch_image(canvas, blend->src_bitmap);
        pixman_region32_fini(&dest_region);
        return;
    }

    surface_canvas = canvas_get_surface(canvas, blend->src_bitmap);
    if (surface_canvas) {
        if (rect_is_same_size(bbox, &blend->src_area)) {
            if (rop == SPICE_ROP_COPY)
                spice_canvas->ops->blit_image_from_surface(spice_canvas, &dest_region,
                                                           surface_canvas,
                                                           bbox->left - blend->src_area.left,
                                                           bbox->top - blend->src_area.top);
            else
                spice_canvas->ops->blit_image_rop_from_surface(spice_canvas, &dest_region,
                                                               surface_canvas,
                                                               bbox->left - blend->src_area.left,
                                                               bbox->top - blend->src_area.top,
                                                               rop);
        } else {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->scale_image_from_surface(spice_canvas, &dest_region,
                                                            surface_canvas,
                                                            blend->src_area.left,
                                                            blend->src_area.top,
                                                            blend->src_area.right - blend->src_area.left,
                                                            blend->src_area.bottom - blend->src_area.top,
                                                            bbox->left,
                                                            bbox->top,
                                                            bbox->right - bbox->left,
                                                            bbox->bottom - bbox->top,
                                                            blend->scale_mode);
            } else {
                spice_canvas->ops->scale_image_rop_from_surface(spice_canvas, &dest_region,
                                                                surface_canvas,
                                                                blend->src_area.left,
                                                                blend->src_area.top,
                                                                blend->src_area.right - blend->src_area.left,
                                                                blend->src_area.bottom - blend->src_area.top,
                                                                bbox->left,
                                                                bbox->top,
                                                                bbox->right - bbox->left,
                                                                bbox->bottom - bbox->top,
                                                                blend->scale_mode, rop);
            }
        }
    } else {
        src_image = canvas_get_image(canvas, blend->src_bitmap, FALSE);
        spice_return_if_fail(src_image != NULL);

        if (rect_is_same_size(bbox, &blend->src_area)) {
            if (rop == SPICE_ROP_COPY)
                spice_canvas->ops->blit_image(spice_canvas, &dest_region,
                                              src_image,
                                              bbox->left - blend->src_area.left,
                                              bbox->top - blend->src_area.top);
            else
                spice_canvas->ops->blit_image_rop(spice_canvas, &dest_region,
                                                  src_image,
                                                  bbox->left - blend->src_area.left,
                                                  bbox->top - blend->src_area.top,
                                                  rop);
        } else {
            if (rop == SPICE_ROP_COPY) {
                spice_canvas->ops->scale_image(spice_canvas, &dest_region,
                                               src_image,
                                               blend->src_area.left,
                                               blend->src_area.top,
                                               blend->src_area.right - blend->src_area.left,
                                               blend->src_area.bottom - blend->src_area.top,
                                               bbox->left,
                                               bbox->top,
                                               bbox->right - bbox->left,
                                               bbox->bottom - bbox->top,
                                               blend->scale_mode);
            } else {
                spice_canvas->ops->scale_image_rop(spice_canvas, &dest_region,
                                                   src_image,
                                                   blend->src_area.left,
                                                   blend->src_area.top,
                                                   blend->src_area.right - blend->src_area.left,
                                                   blend->src_area.bottom - blend->src_area.top,
                                                   bbox->left,
                                                   bbox->top,
                                                   bbox->right - bbox->left,
                                                   bbox->bottom - bbox->top,
                                                   blend->scale_mode, rop);
            }
        }
        pixman_image_unref(src_image);
    }
    pixman_region32_fini(&dest_region);
}

static void canvas_draw_blackness(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlackness *blackness)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    pixman_box32_t *rects;
    int n_rects;

   pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);


    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &blackness->mask,
                       bbox->left, bbox->top);

    if (!pixman_region32_not_empty(&dest_region)) {
        pixman_region32_fini (&dest_region);
        return;
    }

    rects = pixman_region32_rectangles(&dest_region, &n_rects);

    spice_canvas->ops->fill_solid_rects(spice_canvas, rects, n_rects, 0x000000);

    pixman_region32_fini(&dest_region);
}

static void canvas_draw_whiteness(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceWhiteness *whiteness)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    pixman_box32_t *rects;
    int n_rects;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);


    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &whiteness->mask,
                       bbox->left, bbox->top);

    if (!pixman_region32_not_empty(&dest_region)) {
        pixman_region32_fini(&dest_region);
        return;
    }

    rects = pixman_region32_rectangles(&dest_region, &n_rects);
    spice_canvas->ops->fill_solid_rects(spice_canvas, rects, n_rects, 0xffffffff);

    pixman_region32_fini(&dest_region);
}

static void canvas_draw_invers(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceInvers *invers)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    pixman_box32_t *rects;
    int n_rects;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);


    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &invers->mask,
                       bbox->left, bbox->top);

    if (!pixman_region32_not_empty(&dest_region)) {
        pixman_region32_fini(&dest_region);
        return;
    }

    rects = pixman_region32_rectangles(&dest_region, &n_rects);
    spice_canvas->ops->fill_solid_rects_rop(spice_canvas, rects, n_rects, 0x00000000,
                                            SPICE_ROP_INVERT);

    pixman_region32_fini(&dest_region);
}

typedef struct {
    lineGC base;
    SpiceCanvas *canvas;
    pixman_region32_t dest_region;
    SpiceROP fore_rop;
    SpiceROP back_rop;
    int solid;
    uint32_t color;
    int use_surface_canvas;
    union {
        SpiceCanvas *surface_canvas;
        pixman_image_t *tile;
    };
    int tile_offset_x;
    int tile_offset_y;
} StrokeGC;

static void stroke_fill_spans(lineGC * pGC,
                              int num_spans,
                              SpicePoint *points,
                              int *widths,
                              int sorted,
                              int foreground)
{
    SpiceCanvas *canvas;
    StrokeGC *strokeGC;
    int i;
    SpiceROP rop;

    strokeGC = (StrokeGC *)pGC;
    canvas = strokeGC->canvas;

    num_spans = spice_canvas_clip_spans(&strokeGC->dest_region,
                                        points, widths, num_spans,
                                        points, widths, sorted);

    if (foreground) {
        rop = strokeGC->fore_rop;
    } else {
        rop = strokeGC->back_rop;
    }

    if (strokeGC->solid) {
        if (rop == SPICE_ROP_COPY) {
            canvas->ops->fill_solid_spans(canvas, points, widths, num_spans,
                                          strokeGC->color);
        } else {
            for (i = 0; i < num_spans; i++) {
                pixman_box32_t r;
                r.x1 = points[i].x;
                r.y1 = points[i].y;
                r.x2 = points[i].x + widths[i];
                r.y2 = r.y1 + 1;
                canvas->ops->fill_solid_rects_rop(canvas, &r, 1,
                                                  strokeGC->color, rop);
            }
        }
    } else {
        if (rop == SPICE_ROP_COPY) {
            for (i = 0; i < num_spans; i++) {
                pixman_box32_t r;
                r.x1 = points[i].x;
                r.y1 = points[i].y;
                r.x2 = points[i].x + widths[i];
                r.y2 = r.y1 + 1;
                canvas->ops->fill_tiled_rects(canvas, &r, 1,
                                              strokeGC->tile,
                                              strokeGC->tile_offset_x,
                                              strokeGC->tile_offset_y);
            }
        } else {
            for (i = 0; i < num_spans; i++) {
                pixman_box32_t r;
                r.x1 = points[i].x;
                r.y1 = points[i].y;
                r.x2 = points[i].x + widths[i];
                r.y2 = r.y1 + 1;
                canvas->ops->fill_tiled_rects_rop(canvas, &r, 1,
                                                  strokeGC->tile,
                                                  strokeGC->tile_offset_x,
                                                  strokeGC->tile_offset_y, rop);
            }
        }
    }
}

static void stroke_fill_rects(lineGC * pGC,
                              int num_rects,
                              pixman_rectangle32_t *rects,
                              int foreground)
{
    SpiceCanvas *canvas;
    pixman_region32_t area;
    pixman_box32_t *boxes;
    StrokeGC *strokeGC;
    SpiceROP rop;
    int i;
    pixman_box32_t *area_rects;
    int n_area_rects;

    strokeGC = (StrokeGC *)pGC;
    canvas = strokeGC->canvas;

    if (foreground) {
        rop = strokeGC->fore_rop;
    } else {
        rop = strokeGC->back_rop;
    }

    /* TODO: We can optimize this for more common cases where
       dest is one rect */

    boxes = spice_new(pixman_box32_t, num_rects);
    for (i = 0; i < num_rects; i++) {
        boxes[i].x1 = rects[i].x;
        boxes[i].y1 = rects[i].y;
        boxes[i].x2 = rects[i].x + rects[i].width;
        boxes[i].y2 = rects[i].y + rects[i].height;
    }
    pixman_region32_init_rects(&area, boxes, num_rects);
    pixman_region32_intersect(&area, &area, &strokeGC->dest_region);
    free(boxes);

    area_rects = pixman_region32_rectangles(&area, &n_area_rects);

    if (strokeGC->solid) {
        if (rop == SPICE_ROP_COPY) {
            canvas->ops->fill_solid_rects(canvas, area_rects, n_area_rects,
                                          strokeGC->color);
        } else {
            canvas->ops->fill_solid_rects_rop(canvas, area_rects, n_area_rects,
                                              strokeGC->color, rop);
        }
    } else {
        if (rop == SPICE_ROP_COPY) {
            if (strokeGC->use_surface_canvas) {
                canvas->ops->fill_tiled_rects_from_surface(canvas, area_rects, n_area_rects,
                                                           strokeGC->surface_canvas,
                                                           strokeGC->tile_offset_x,
                                                           strokeGC->tile_offset_y);
            } else {
                canvas->ops->fill_tiled_rects(canvas, area_rects, n_area_rects,
                                              strokeGC->tile,
                                              strokeGC->tile_offset_x,
                                              strokeGC->tile_offset_y);
            }
        } else {
            if (strokeGC->use_surface_canvas) {
                canvas->ops->fill_tiled_rects_rop_from_surface(canvas, area_rects, n_area_rects,
                                                               strokeGC->surface_canvas,
                                                               strokeGC->tile_offset_x,
                                                               strokeGC->tile_offset_y,
                                                               rop);
            } else {
                canvas->ops->fill_tiled_rects_rop(canvas, area_rects, n_area_rects,
                                                  strokeGC->tile,
                                                  strokeGC->tile_offset_x,
                                                  strokeGC->tile_offset_y,
                                                  rop);
            }
        }
    }

   pixman_region32_fini(&area);
}

typedef struct {
    SpicePoint *points;
    int num_points;
    int size;
} StrokeLines;

static void stroke_lines_init(StrokeLines *lines)
{
    lines->points = spice_new(SpicePoint, 10);
    lines->size = 10;
    lines->num_points = 0;
}

static void stroke_lines_free(StrokeLines *lines)
{
    free(lines->points);
}

static void stroke_lines_append(StrokeLines *lines,
                                int x, int y)
{
    if (lines->num_points == lines->size) {
        lines->size *= 2;
        lines->points = spice_renew(SpicePoint, lines->points, lines->size);
    }
    lines->points[lines->num_points].x = x;
    lines->points[lines->num_points].y = y;
    lines->num_points++;
}

static void stroke_lines_append_fix(StrokeLines *lines,
                                    SpicePointFix *point)
{
    stroke_lines_append(lines,
                        fix_to_int(point->x),
                        fix_to_int(point->y));
}

static inline int64_t dot(SPICE_FIXED28_4 x1,
                          SPICE_FIXED28_4 y1,
                          SPICE_FIXED28_4 x2,
                          SPICE_FIXED28_4 y2)
{
    return (((int64_t)x1) *((int64_t)x2) +
            ((int64_t)y1) *((int64_t)y2)) >> 4;
}

static inline int64_t dot2(SPICE_FIXED28_4 x,
                           SPICE_FIXED28_4 y)
{
    return (((int64_t)x) *((int64_t)x) +
            ((int64_t)y) *((int64_t)y)) >> 4;
}

static void subdivide_bezier(StrokeLines *lines,
                             SpicePointFix point0,
                             SpicePointFix point1,
                             SpicePointFix point2,
                             SpicePointFix point3)
{
    int64_t A2, B2, C2, AB, CB, h1, h2;

    A2 = dot2(point1.x - point0.x,
              point1.y - point0.y);
    B2 = dot2(point3.x - point0.x,
              point3.y - point0.y);
    C2 = dot2(point2.x - point3.x,
              point2.y - point3.y);

    AB = dot(point1.x - point0.x,
             point1.y - point0.y,
             point3.x - point0.x,
             point3.y - point0.y);

    CB = dot(point2.x - point3.x,
             point2.y - point3.y,
             point0.x - point3.x,
             point0.y - point3.y);

    h1 = (A2*B2 - AB*AB) >> 3;
    h2 = (C2*B2 - CB*CB) >> 3;

    if (h1 < B2 && h2 < B2) {
        /* deviation squared less than half a pixel, use straight line */
        stroke_lines_append_fix(lines, &point3);
    } else {
        SpicePointFix point01, point23, point12, point012, point123, point0123;

        point01.x = (point0.x + point1.x) / 2;
        point01.y = (point0.y + point1.y) / 2;
        point12.x = (point1.x + point2.x) / 2;
        point12.y = (point1.y + point2.y) / 2;
        point23.x = (point2.x + point3.x) / 2;
        point23.y = (point2.y + point3.y) / 2;
        point012.x = (point01.x + point12.x) / 2;
        point012.y = (point01.y + point12.y) / 2;
        point123.x = (point12.x + point23.x) / 2;
        point123.y = (point12.y + point23.y) / 2;
        point0123.x = (point012.x + point123.x) / 2;
        point0123.y = (point012.y + point123.y) / 2;

        subdivide_bezier(lines, point0, point01, point012, point0123);
        subdivide_bezier(lines, point0123, point123, point23, point3);
    }
}

static void stroke_lines_append_bezier(StrokeLines *lines,
                                       SpicePointFix *point1,
                                       SpicePointFix *point2,
                                       SpicePointFix *point3)
{
    SpicePointFix point0;

    point0.x = int_to_fix(lines->points[lines->num_points-1].x);
    point0.y = int_to_fix(lines->points[lines->num_points-1].y);

    subdivide_bezier(lines, point0, *point1, *point2, *point3);
}

static void stroke_lines_draw(StrokeLines *lines,
                              lineGC *gc,
                              int dashed)
{
    if (lines->num_points != 0) {
        if (dashed) {
            spice_canvas_zero_dash_line(gc, CoordModeOrigin,
                                        lines->num_points, lines->points);
        } else {
            spice_canvas_zero_line(gc, CoordModeOrigin,
                                   lines->num_points, lines->points);
        }
        lines->num_points = 0;
    }
}


static void canvas_draw_stroke(SpiceCanvas *spice_canvas, SpiceRect *bbox,
                               SpiceClip *clip, SpiceStroke *stroke)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    SpiceCanvas *surface_canvas = NULL;
    StrokeGC gc;
    lineGCOps ops = {
        stroke_fill_spans,
        stroke_fill_rects
    };
    StrokeLines lines;
    unsigned int i;
    int dashed;

    memset(&gc, 0, sizeof(gc));

    pixman_region32_init_rect(&gc.dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &gc.dest_region, clip);

    if (!pixman_region32_not_empty(&gc.dest_region)) {
        touch_brush(canvas, &stroke->brush);
        pixman_region32_fini(&gc.dest_region);
        return;
    }

    gc.canvas = spice_canvas;
    gc.fore_rop = ropd_descriptor_to_rop(stroke->fore_mode,
                                         ROP_INPUT_BRUSH,
                                         ROP_INPUT_DEST);
    gc.back_rop = ropd_descriptor_to_rop(stroke->back_mode,
                                         ROP_INPUT_BRUSH,
                                         ROP_INPUT_DEST);

    gc.base.width = canvas->width;
    gc.base.height = canvas->height;
    gc.base.alu = gc.fore_rop;
    gc.base.lineWidth = 0;

    /* dash */
    gc.base.dashOffset = 0;
    gc.base.dash = NULL;
    gc.base.numInDashList = 0;
    gc.base.lineStyle = LineSolid;
    /* win32 cosmetic lines are endpoint-exclusive, so use CapNotLast */
    gc.base.capStyle = CapNotLast;
    gc.base.joinStyle = JoinMiter;
    gc.base.ops = &ops;

    dashed = 0;
    if (stroke->attr.flags & SPICE_LINE_FLAGS_STYLED) {
        SPICE_FIXED28_4 *style = stroke->attr.style;
        int nseg;

        dashed = 1;

        nseg = stroke->attr.style_nseg;

        /* To truly handle back_mode we should use LineDoubleDash here
           and treat !foreground as back_rop using the same brush.
           However, using the same brush for that seems wrong.
           The old cairo backend was stroking the non-dashed line with
           rop_mode before enabling dashes for the foreground which is
           not right either. The gl an gdi backend don't use back_mode
           at all */
        gc.base.lineStyle = LineOnOffDash;
        gc.base.dash = (unsigned char *)spice_malloc(nseg);
        gc.base.numInDashList = nseg;

        if (stroke->attr.flags & SPICE_LINE_FLAGS_START_WITH_GAP) {
            gc.base.dash[stroke->attr.style_nseg - 1] = fix_to_int(style[0]);
            for (i = 0; i < (unsigned int)(stroke->attr.style_nseg - 1); i++) {
                gc.base.dash[i] = fix_to_int(style[i+1]);
            }
            gc.base.dashOffset = gc.base.dash[0];
        } else {
            for (i = 0; i < stroke->attr.style_nseg; i++) {
                gc.base.dash[i] = fix_to_int(style[i]);
            }
        }
    }

    switch (stroke->brush.type) {
    case SPICE_BRUSH_TYPE_NONE:
        gc.solid = TRUE;
        gc.color = 0;
        break;
    case SPICE_BRUSH_TYPE_SOLID:
        gc.solid = TRUE;
        gc.color = stroke->brush.u.color;
        break;
    case SPICE_BRUSH_TYPE_PATTERN:
        gc.solid = FALSE;
        surface_canvas = canvas_get_surface(canvas,
                                            stroke->brush.u.pattern.pat);
        if (surface_canvas) {
            gc.use_surface_canvas = TRUE;
            gc.surface_canvas = surface_canvas;
        } else {
            gc.use_surface_canvas = FALSE;
            gc.tile = canvas_get_image(canvas,
                                       stroke->brush.u.pattern.pat,
                                       FALSE);
        }
        gc.tile_offset_x = stroke->brush.u.pattern.pos.x;
        gc.tile_offset_y = stroke->brush.u.pattern.pos.y;
        break;
    default:
        spice_warn_if_reached();
        return;
    }

    stroke_lines_init(&lines);

    for (i = 0; i < stroke->path->num_segments; i++) {
        SpicePathSeg *seg = stroke->path->segments[i];
        SpicePointFix* point, *end_point;

        point = seg->points;
        end_point = point + seg->count;

        if (seg->flags & SPICE_PATH_BEGIN) {
            stroke_lines_draw(&lines, (lineGC *)&gc, dashed);
            stroke_lines_append_fix(&lines, point);
            point++;
        }

        if (seg->flags & SPICE_PATH_BEZIER) {
            spice_return_if_fail((point - end_point) % 3 == 0);
            for (; point + 2 < end_point; point += 3) {
                stroke_lines_append_bezier(&lines,
                                           &point[0],
                                           &point[1],
                                           &point[2]);
            }
        } else
            {
            for (; point < end_point; point++) {
                stroke_lines_append_fix(&lines, point);
            }
        }
        if (seg->flags & SPICE_PATH_END) {
            if (seg->flags & SPICE_PATH_CLOSE) {
                stroke_lines_append(&lines,
                                    lines.points[0].x, lines.points[0].y);
            }
            stroke_lines_draw(&lines, (lineGC *)&gc, dashed);
        }
    }

    stroke_lines_draw(&lines, (lineGC *)&gc, dashed);

    free(gc.base.dash);
    stroke_lines_free(&lines);

    if (!gc.solid && gc.tile && !surface_canvas) {
        pixman_image_unref(gc.tile);
    }

    pixman_region32_fini(&gc.dest_region);
}


//need surfaces handling here !!!
static void canvas_draw_rop3(SpiceCanvas *spice_canvas, SpiceRect *bbox,
                             SpiceClip *clip, SpiceRop3 *rop3)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    SpiceCanvas *surface_canvas;
    pixman_region32_t dest_region;
    pixman_image_t *d;
    pixman_image_t *s;
    SpicePoint src_pos;
    int width;
    int heigth;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);
    canvas_mask_pixman(canvas, &dest_region, &rop3->mask,
                       bbox->left, bbox->top);

    width = bbox->right - bbox->left;
    heigth = bbox->bottom - bbox->top;

    d = canvas_get_image_from_self(spice_canvas, bbox->left, bbox->top, width, heigth, FALSE);
    surface_canvas = canvas_get_surface(canvas, rop3->src_bitmap);
    if (surface_canvas) {
        s = surface_canvas->ops->get_image(surface_canvas, FALSE);
    } else {
        s = canvas_get_image(canvas, rop3->src_bitmap, FALSE);
    }

    if (!rect_is_same_size(bbox, &rop3->src_area)) {
        pixman_image_t *scaled_s = canvas_scale_surface(s, &rop3->src_area, width, heigth,
                                                        rop3->scale_mode);
        pixman_image_unref(s);
        s = scaled_s;
        src_pos.x = 0;
        src_pos.y = 0;
    } else {
        src_pos.x = rop3->src_area.left;
        src_pos.y = rop3->src_area.top;
    }
    if (pixman_image_get_width(s) - src_pos.x < width ||
        pixman_image_get_height(s) - src_pos.y < heigth) {
        spice_critical("bad src bitmap size");
        return;
    }
    if (rop3->brush.type == SPICE_BRUSH_TYPE_PATTERN) {
        SpiceCanvas *_surface_canvas;
        pixman_image_t *p;

        _surface_canvas = canvas_get_surface(canvas, rop3->brush.u.pattern.pat);
        if (_surface_canvas) {
            p = _surface_canvas->ops->get_image(_surface_canvas, FALSE);
        } else {
            p = canvas_get_image(canvas, rop3->brush.u.pattern.pat, FALSE);
        }
        SpicePoint pat_pos;

        pat_pos.x = (bbox->left - rop3->brush.u.pattern.pos.x) % pixman_image_get_width(p);
        pat_pos.y = (bbox->top - rop3->brush.u.pattern.pos.y) % pixman_image_get_height(p);
        do_rop3_with_pattern(rop3->rop3, d, s, &src_pos, p, &pat_pos);
        pixman_image_unref(p);
    } else {
        do_rop3_with_color(rop3->rop3, d, s, &src_pos, rop3->brush.u.color);
    }
    pixman_image_unref(s);

    spice_canvas->ops->blit_image(spice_canvas, &dest_region, d,
                                  bbox->left,
                                  bbox->top);

    pixman_image_unref(d);

    pixman_region32_fini(&dest_region);
}

static void transform_to_pixman_transform(SpiceTransform *transform,
                                          pixman_transform_t *p)
{
    p->matrix[0][0] = transform->t00;
    p->matrix[0][1] = transform->t01;
    p->matrix[0][2] = transform->t02;
    p->matrix[1][0] = transform->t10;
    p->matrix[1][1] = transform->t11;
    p->matrix[1][2] = transform->t12;
    p->matrix[2][0] = 0;
    p->matrix[2][1] = 0;
    p->matrix[2][2] = pixman_fixed_1;
}

#define MASK(lo, hi)                                                    \
    (((1U << (hi)) - 1) - (((1U << (lo))) - 1))

#define EXTRACT(v, lo, hi)                                              \
    ((v & MASK(lo, hi)) >> lo)

static void canvas_draw_composite(SpiceCanvas *spice_canvas, SpiceRect *bbox,
                                  SpiceClip *clip, SpiceComposite *composite)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    SpiceCanvas *surface_canvas;
    pixman_region32_t dest_region;
    pixman_image_t *d;
    pixman_image_t *s;
    pixman_image_t *m;
    pixman_repeat_t src_repeat;
    pixman_filter_t src_filter;
    pixman_op_t op;
    pixman_transform_t transform;
    int width, height;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);

    width = bbox->right - bbox->left;
    height = bbox->bottom - bbox->top;

    /* Dest */
    d = canvas_get_image_from_self(spice_canvas, bbox->left, bbox->top, width, height,
                                   (composite->flags & SPICE_COMPOSITE_DEST_OPAQUE));

    /* Src */
    surface_canvas = canvas_get_surface(canvas, composite->src_bitmap);
    if (surface_canvas) {
        s = surface_canvas->ops->get_image(surface_canvas,
                                           (composite->flags & SPICE_COMPOSITE_SOURCE_OPAQUE));
    } else {
        s = canvas_get_image(canvas, composite->src_bitmap, FALSE);
    }
    if (composite->flags & SPICE_COMPOSITE_HAS_SRC_TRANSFORM)
    {
        transform_to_pixman_transform (&composite->src_transform, &transform);
        pixman_image_set_transform (s, &transform);
    }
    src_filter = (pixman_filter_t) EXTRACT (composite->flags, 8, 11);
    src_repeat = (pixman_repeat_t) EXTRACT (composite->flags, 14, 16);
    pixman_image_set_filter (s, src_filter, NULL, 0);
    pixman_image_set_repeat (s, src_repeat);

    /* Mask */
    m = NULL;
    if (composite->flags & SPICE_COMPOSITE_HAS_MASK) {
        pixman_filter_t mask_filter = (pixman_filter_t) EXTRACT (composite->flags, 11, 14);
        pixman_repeat_t mask_repeat = (pixman_repeat_t) EXTRACT (composite->flags, 16, 18);
        pixman_bool_t component_alpha = EXTRACT (composite->flags, 18, 19);

        surface_canvas = canvas_get_surface(canvas, composite->mask_bitmap);
        if (surface_canvas) {
            m = surface_canvas->ops->get_image(surface_canvas, FALSE);
        } else {
            m = canvas_get_image(canvas, composite->mask_bitmap, FALSE);
        }

        if (composite->flags & SPICE_COMPOSITE_HAS_MASK_TRANSFORM) {
            transform_to_pixman_transform (&composite->mask_transform, &transform);
            pixman_image_set_transform (m, &transform);
        }

        pixman_image_set_repeat (m, mask_repeat);
        pixman_image_set_filter (m, mask_filter, NULL, 0);
        pixman_image_set_component_alpha (m, component_alpha);
    }

    op = (pixman_op_t) EXTRACT (composite->flags, 0, 8);

    pixman_image_composite32 (op, s, m, d,
                              composite->src_origin.x, composite->src_origin.y,
                              composite->mask_origin.x, composite->mask_origin.y,
                              0, 0, width, height);

    pixman_image_unref(s);
    if (m)
        pixman_image_unref(m);

    spice_canvas->ops->blit_image(spice_canvas, &dest_region, d,
                                  bbox->left,
                                  bbox->top);

    pixman_image_unref(d);

    pixman_region32_fini(&dest_region);
}

static void canvas_copy_bits(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpicePoint *src_pos)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_t dest_region;
    int dx, dy;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(canvas, &dest_region, clip);

    dx = bbox->left - src_pos->x;
    dy = bbox->top - src_pos->y;

    if (dx != 0 || dy != 0) {
        pixman_region32_t src_region;

        /* Clip so we don't read outside canvas */
        pixman_region32_init_rect(&src_region,
                                  dx, dy,
                                  canvas->width,
                                  canvas->height);
        pixman_region32_intersect(&dest_region, &dest_region, &src_region);
        pixman_region32_fini(&src_region);

        spice_canvas->ops->copy_region(spice_canvas, &dest_region, dx, dy);
    }

    pixman_region32_fini(&dest_region);
}



static void canvas_base_group_start(SpiceCanvas *spice_canvas, QRegion *region)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_fini(&canvas->canvas_region);

    /* Make sure we always clip to canvas size */
    pixman_region32_init_rect(&canvas->canvas_region,
                              0, 0,
                              canvas->width,
                              canvas->height);

    pixman_region32_intersect(&canvas->canvas_region, &canvas->canvas_region, region);
}

static void canvas_base_group_end(SpiceCanvas *spice_canvas)
{
    CanvasBase *canvas = (CanvasBase *)spice_canvas;
    pixman_region32_fini(&canvas->canvas_region);
    pixman_region32_init_rect(&canvas->canvas_region,
                              0, 0,
                              canvas->width,
                              canvas->height);
}


static void unimplemented_op(SpiceCanvas *canvas)
{
    spice_critical("unimplemented canvas operation");
}

inline static void canvas_base_init_ops(SpiceCanvasOps *ops)
{
    void **ops_cast;
    unsigned i;

    ops_cast = (void **)ops;
    for (i = 0; i < sizeof(SpiceCanvasOps) / sizeof(void *); i++) {
        ops_cast[i] = (void *) unimplemented_op;
    }

    ops->draw_fill = canvas_draw_fill;
    ops->draw_copy = canvas_draw_copy;
    ops->draw_opaque = canvas_draw_opaque;
    ops->copy_bits = canvas_copy_bits;
    ops->draw_blend = canvas_draw_blend;
    ops->draw_blackness = canvas_draw_blackness;
    ops->draw_whiteness = canvas_draw_whiteness;
    ops->draw_invers = canvas_draw_invers;
    ops->draw_transparent = canvas_draw_transparent;
    ops->draw_alpha_blend = canvas_draw_alpha_blend;
    ops->draw_stroke = canvas_draw_stroke;
    ops->draw_rop3 = canvas_draw_rop3;
    ops->draw_composite = canvas_draw_composite;
    ops->group_start = canvas_base_group_start;
    ops->group_end = canvas_base_group_end;
}

static int canvas_base_init(CanvasBase *canvas, SpiceCanvasOps *ops,
                            int width, int height, uint32_t format
#ifdef SW_CANVAS_CACHE
                            , SpiceImageCache *bits_cache
                            , SpicePaletteCache *palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                            , SpiceImageCache *bits_cache
#endif
                            , SpiceImageSurfaces *surfaces
                            , SpiceGlzDecoder *glz_decoder
                            , SpiceJpegDecoder *jpeg_decoder
                            , SpiceZlibDecoder *zlib_decoder
                            )
{
    canvas->parent.ops = ops;
    canvas->quic_data.usr.error = quic_usr_error;
    canvas->quic_data.usr.warn = quic_usr_warn;
    canvas->quic_data.usr.info = quic_usr_warn;
    canvas->quic_data.usr.malloc = quic_usr_malloc;
    canvas->quic_data.usr.free = quic_usr_free;
    canvas->quic_data.usr.more_space = quic_usr_more_space;
    canvas->quic_data.usr.more_lines = quic_usr_more_lines;
    if (!(canvas->quic_data.quic = quic_create(&canvas->quic_data.usr))) {
            return 0;
    }

    canvas->lz_data.usr.error = lz_usr_error;
    canvas->lz_data.usr.warn = lz_usr_warn;
    canvas->lz_data.usr.info = lz_usr_warn;
    canvas->lz_data.usr.malloc = lz_usr_malloc;
    canvas->lz_data.usr.free = lz_usr_free;
    canvas->lz_data.usr.more_space = lz_usr_more_space;
    canvas->lz_data.usr.more_lines = lz_usr_more_lines;
    if (!(canvas->lz_data.lz = lz_create(&canvas->lz_data.usr))) {
            return 0;
    }

    canvas->surfaces = surfaces;
    canvas->glz_data.decoder = glz_decoder;
    canvas->jpeg = jpeg_decoder;
    canvas->zlib = zlib_decoder;

    canvas->format = format;

    /* TODO: This is all wrong now */
    if (SPICE_SURFACE_FMT_DEPTH(format) == 16) {
        canvas->color_shift = 5;
        canvas->color_mask = 0x1f;
    } else {
        canvas->color_shift = 8;
        canvas->color_mask = 0xff;
    }

    canvas->width = width;
    canvas->height = height;
    pixman_region32_init_rect(&canvas->canvas_region,
                              0, 0,
                              canvas->width,
                              canvas->height);

#if defined(SW_CANVAS_CACHE) || defined(SW_CANVAS_IMAGE_CACHE)
    canvas->bits_cache = bits_cache;
#endif
#ifdef SW_CANVAS_CACHE
    canvas->palette_cache = palette_cache;
#endif

#ifdef WIN32
    canvas->dc = NULL;
#endif

#ifdef GDI_CANVAS
    canvas->dc = create_compatible_dc();
    if (!canvas->dc) {
        lz_destroy(canvas->lz_data.lz);
        return 0;
    }
#endif
    return 1;
}
