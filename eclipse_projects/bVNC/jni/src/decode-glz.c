/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include <glib.h>

#include "gio-coroutine.h"
#include "spice-util.h"
#include "decode.h"

#include "common/canvas_utils.h"

struct glz_image_hdr {
    uint64_t                id;
    LzImageType             type;
    uint32_t                width;
    uint32_t                height;
    uint32_t                gross_pixels;
    bool                    top_down;
    uint32_t                win_head_dist;
};

struct glz_image {
    struct glz_image_hdr    hdr;
    pixman_image_t          *surface;
    uint8_t                 *data;
};

static struct glz_image *glz_image_new(struct glz_image_hdr *hdr,
                                       int type, void *opaque)
{
    struct glz_image *img;

    g_return_val_if_fail(type == LZ_IMAGE_TYPE_RGB32 || type == LZ_IMAGE_TYPE_RGBA, NULL);

    img = spice_new0(struct glz_image, 1);
    img->hdr = *hdr;
    img->surface = alloc_lz_image_surface
        (opaque, type == LZ_IMAGE_TYPE_RGBA ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
         img->hdr.width, img->hdr.height, img->hdr.gross_pixels, img->hdr.top_down);
    pixman_image_ref(img->surface);
    img->data = (uint8_t *)pixman_image_get_data(img->surface);
    if (!img->hdr.top_down) {
        img->data = img->data - img->hdr.width * (img->hdr.height - 1) * 4;
    }
    return img;
}

static void glz_image_destroy(struct glz_image *img)
{
    if (img == NULL)
        return;

    pixman_image_unref(img->surface);
    free(img);
}

/* ------------------------------------------------------------------ */

#define INIT_IMAGES_CAPACITY 100
#define WIN_OVERFLOW_FACTOR 1.5
#define WIN_REALLOC_FACTOR 1.5

struct SpiceGlzDecoderWindow {
    struct glz_image        **images;
    uint32_t                nimages;
    uint64_t                oldest;
    uint64_t                tail_gap;
};

static void glz_decoder_window_resize(SpiceGlzDecoderWindow *w)
{
    struct glz_image  **new_images;
    int i, new_slot;

    SPICE_DEBUG("%s: array resize %d -> %d", __FUNCTION__,
                w->nimages, w->nimages * 2);
    new_images = spice_new0(struct glz_image*, w->nimages * 2);
    for (i = 0; i < w->nimages; i++) {
        if (w->images[i] == NULL) {
            /*
             * We can have empty slots when images come in out of order, this
             * can happen when a vm has multiple displays, since each display
             * uses its own socket there is no guarantee that images
             * originating from different displays are received in id order.
             */
            continue;
        }
        new_slot = w->images[i]->hdr.id % (w->nimages * 2);
        new_images[new_slot] = w->images[i];
    }
    free(w->images);
    w->images = new_images;
    w->nimages *= 2;
}

static void glz_decoder_window_add(SpiceGlzDecoderWindow *w,
                                   struct glz_image *img)
{
    int slot = img->hdr.id % w->nimages;

    if (w->images[slot]) {
        /* need more space */
        glz_decoder_window_resize(w);
        slot = img->hdr.id % w->nimages;
    }

    w->images[slot] = img;

    /* close the gap */
    while (w->tail_gap <= img->hdr.id && w->images[w->tail_gap % w->nimages] != NULL)
        w->tail_gap++;
}

struct wait_for_image_data {
    SpiceGlzDecoderWindow     *window;
    uint64_t                   id;
};

static gboolean wait_for_image(gpointer data)
{
    struct wait_for_image_data *wait = data;
    int slot = wait->id % wait->window->nimages;
    struct glz_image *image = wait->window->images[slot];
    gboolean ready = image && image->hdr.id == wait->id;

    return ready;
}

static void *glz_decoder_window_bits(SpiceGlzDecoderWindow *w, uint64_t id,
                                     uint32_t dist, uint32_t offset)
{
    struct wait_for_image_data data = {
        .window = w,
        .id = id - dist,
    };

    if (!g_coroutine_condition_wait(g_coroutine_self(), wait_for_image, &data))
        SPICE_DEBUG("wait for image cancelled");

    int slot = (id - dist) % w->nimages;

    g_return_val_if_fail(w->images[slot] != NULL, NULL);
    g_return_val_if_fail(w->images[slot]->hdr.id == id - dist, NULL);
    g_return_val_if_fail(w->images[slot]->hdr.gross_pixels >= offset, NULL);

    return w->images[slot]->data + offset * 4;
}

static void glz_decoder_window_release(SpiceGlzDecoderWindow *w,
                                       uint64_t oldest)
{
    int slot;

    while (w->oldest < oldest) {
        slot = w->oldest % w->nimages;
        glz_image_destroy(w->images[slot]);
        w->images[slot] = NULL;
        w->oldest++;
    }
}

/* ------------------------------------------------------------------ */

typedef struct GlibGlzDecoder {
    SpiceGlzDecoder         base;
    uint8_t                 *in_start;
    uint8_t                 *in_now;
    SpiceGlzDecoderWindow   *window;
    struct glz_image_hdr    image;
} GlibGlzDecoder;

/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define LZ_EXPECT_CONDITIONAL(c) (__builtin_expect((c), 1))
#define LZ_UNEXPECT_CONDITIONAL(c) (__builtin_expect((c), 0))
#else
#define LZ_EXPECT_CONDITIONAL(c) (c)
#define LZ_UNEXPECT_CONDITIONAL(c) (c)
#endif


#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#pragma pack(push)
#pragma pack(1)
#endif

/*
 * the palette images will be treated as one byte pixels. Their width
 * should be transformed accordingly.
 */
typedef struct ATTR_PACKED one_byte_pixel_t {
    uint8_t a;
} one_byte_pixel_t;

typedef struct ATTR_PACKED rgb32_pixel_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t pad;
} rgb32_pixel_t;

typedef struct ATTR_PACKED rgb24_pixel_t {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} rgb24_pixel_t;

typedef uint16_t rgb16_pixel_t;

#ifndef __GNUC__
#pragma pack(pop)
#endif

#undef ATTR_PACKED

#define LZ_PLT
#include "decode-glz-tmpl.c"

#define LZ_PLT
#define PLT8
#define TO_RGB32
#include "decode-glz-tmpl.c"

#define LZ_PLT
#define PLT4_BE
#define TO_RGB32
#include "decode-glz-tmpl.c"

#define LZ_PLT
#define PLT4_LE
#define TO_RGB32
#include "decode-glz-tmpl.c"

#define LZ_PLT
#define PLT1_BE
#define TO_RGB32
#include "decode-glz-tmpl.c"

#define LZ_PLT
#define PLT1_LE
#define TO_RGB32
#include "decode-glz-tmpl.c"


#define LZ_RGB16
#include "decode-glz-tmpl.c"
#define LZ_RGB16
#define TO_RGB32
#include "decode-glz-tmpl.c"

#define LZ_RGB24
#include "decode-glz-tmpl.c"

#define LZ_RGB32
#include "decode-glz-tmpl.c"

#define LZ_RGB_ALPHA
#include "decode-glz-tmpl.c"

#undef LZ_UNEXPECT_CONDITIONAL
#undef LZ_EXPECT_CONDITIONAL

typedef size_t (*decode_function)(SpiceGlzDecoderWindow *window,
                                  uint8_t* in_buf, uint8_t *out_buf, int size,
                                  uint64_t id, SpicePalette *plt);

// ordered according to LZ_IMAGE_TYPE
const decode_function DECODE_TO_RGB32[] = {
    NULL,
    glz_plt1_le_to_rgb32_decode,
    glz_plt1_be_to_rgb32_decode,
    glz_plt4_le_to_rgb32_decode,
    glz_plt4_be_to_rgb32_decode,
    glz_plt8_to_rgb32_decode,
    glz_rgb16_to_rgb32_decode,
    glz_rgb32_decode,
    glz_rgb32_decode,
    glz_rgb32_decode
};

const decode_function DECODE_TO_SAME[] = {
    NULL,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_plt_decode,
    glz_rgb16_decode,
    glz_rgb24_decode,
    glz_rgb32_decode,
    glz_rgb32_decode
};

static uint32_t decode_32(GlibGlzDecoder *d)
{
    uint32_t word = 0;
    word |= *(d->in_now++);
    word <<= 8;
    word |= *(d->in_now++);
    word <<= 8;
    word |= *(d->in_now++);
    word <<= 8;
    word |= *(d->in_now++);
    return word;
}

static uint64_t decode_64(GlibGlzDecoder *d)
{
    uint64_t long_word = decode_32(d);
    long_word <<= 32;
    long_word |= decode_32(d);
    return long_word;
}

static void decode_header(GlibGlzDecoder *d)
{
    uint32_t magic;
    uint32_t version;
    uint32_t stride;
    uint8_t tmp;

    magic = decode_32(d);
    g_return_if_fail(magic == LZ_MAGIC);

    version = decode_32(d);
    g_return_if_fail(version == LZ_VERSION);

    tmp = *(d->in_now++);

    d->image.type = (LzImageType)(tmp & LZ_IMAGE_TYPE_MASK);
    d->image.top_down = (tmp >> LZ_IMAGE_TYPE_LOG) ? true : false;
    d->image.width = decode_32(d);
    d->image.height = decode_32(d);
    stride = decode_32(d);

    if (IS_IMAGE_TYPE_PLT[d->image.type]) {
        d->image.gross_pixels = stride * PLT_PIXELS_PER_BYTE[d->image.type]
            * d->image.height;
    } else {
        d->image.gross_pixels = d->image.width * d->image.height;
    }

    d->image.id = decode_64(d);
    d->image.win_head_dist = decode_32(d);

    SPICE_DEBUG("%s: %dx%d, id %" PRId64 ", ref %" PRId64,
            __FUNCTION__,
            d->image.width, d->image.height, d->image.id,
            d->image.id - d->image.win_head_dist);
}

static void decode(SpiceGlzDecoder *decoder,
                   uint8_t *data, SpicePalette *palette,
                   void *usr_data)
{
    GlibGlzDecoder *d = SPICE_CONTAINEROF(decoder, GlibGlzDecoder, base);
    LzImageType decoded_type;
    struct glz_image *decoded_image;
    size_t n_in_bytes_decoded;

    d->in_start = data;
    d->in_now = data;

    decode_header(d);

    if (d->image.type == LZ_IMAGE_TYPE_RGBA) {
        decoded_type = LZ_IMAGE_TYPE_RGBA;
    } else {
        decoded_type = LZ_IMAGE_TYPE_RGB32;
    }

    decoded_image = glz_image_new(&d->image, decoded_type, usr_data);

    n_in_bytes_decoded = DECODE_TO_RGB32[d->image.type]
        (d->window, d->in_now, decoded_image->data,
         d->image.gross_pixels, d->image.id, palette);

    d->in_now += n_in_bytes_decoded;

    if (d->image.type == LZ_IMAGE_TYPE_RGBA) {
        glz_rgb_alpha_decode(d->window, d->in_now, decoded_image->data,
                             d->image.gross_pixels, d->image.id, palette);
    }

    glz_decoder_window_add(d->window, decoded_image);

    { /* release old images from last tail_gap, only if the gap is closed  */
        uint64_t oldest;
        struct glz_image *image = d->window->images[(d->window->tail_gap - 1) % d->window->nimages];

        g_return_if_fail(image != NULL);

        oldest = image->hdr.id - image->hdr.win_head_dist;
        glz_decoder_window_release(d->window, oldest);
    }
}

/* ------------------------------------------------------------------ */

static SpiceGlzDecoderOps glz_decoder_ops = {
    .decode = decode,
};

void glz_decoder_window_clear(SpiceGlzDecoderWindow *w)
{
    int i;

    g_return_if_fail(w->nimages == 0 || w->images != NULL);

    for (i = 0; i < w->nimages; i++) {
        if (w->images[i]) {
            glz_image_destroy(w->images[i]);
        }
    }

    w->nimages = 16;
    g_free(w->images);
    w->images = spice_new0(struct glz_image*, w->nimages);
    w->tail_gap = 0;
}

SpiceGlzDecoderWindow *glz_decoder_window_new(void)
{
    SpiceGlzDecoderWindow *w = spice_new0(SpiceGlzDecoderWindow, 1);
    glz_decoder_window_clear(w);
    return w;
}

void glz_decoder_window_destroy(SpiceGlzDecoderWindow *w)
{
    if (w == NULL)
        return;

    glz_decoder_window_clear(w);
    free(w->images);
    free(w);
}

SpiceGlzDecoder *glz_decoder_new(SpiceGlzDecoderWindow *w)
{
    GlibGlzDecoder *d = spice_new0(GlibGlzDecoder, 1);
    d->base.ops = &glz_decoder_ops;
    d->window = w;
    return &d->base;
}

void glz_decoder_destroy(SpiceGlzDecoder *d)
{
    free(d);
}
