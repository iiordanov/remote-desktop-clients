/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*

 Copyright (C) 2009 Red Hat, Inc. and/or its affiliates.

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

 This file incorporates work covered by the following copyright and
 permission notice:
   Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)
   Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
   Copyright (C) 2005 Ariya Hidayat (ariya@kde.org)

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "spice_common.h"
#include "lz.h"

#define HASH_LOG 13
#define HASH_SIZE (1 << HASH_LOG)
#define HASH_MASK (HASH_SIZE - 1)


typedef struct LzImageSegment LzImageSegment;
struct LzImageSegment {
    uint8_t            *lines;
    uint8_t            *lines_end;
    unsigned int size_delta;    // total size of the previous segments in units of
                                // pixels for rgb and bytes for plt.
    LzImageSegment    *next;
};

//    TODO: pack?
typedef struct HashEntry {
    LzImageSegment    *image_seg;
    uint8_t            *ref;
} HashEntry;

typedef struct Encoder {
    LzUsrContext    *usr;

    LzImageType type;
    const SpicePalette    *palette;    // for decoding images with palettes to rgb
    int stride;                       // stride is in bytes. For rgb must be equal to
                                      // width*bytes_per_pix.
    // For palettes stride can be bigger than width/pixels_per_byte by 1 only if
    // width%pixels_per_byte != 0.
    int height;
    int width;                       // the original width (in pixels)

    LzImageSegment *head_image_segs;
    LzImageSegment *tail_image_segs;
    LzImageSegment *free_image_segs;

    // the dictionary hash table is composed (1) a pointer to the segment the word was found in
    // (2) a pointer to the first byte in the segment that matches the word
    HashEntry htab[HASH_SIZE];

    uint8_t            *io_start;
    uint8_t            *io_now;
    uint8_t            *io_end;
    size_t io_bytes_count;

    uint8_t            *io_last_copy;  // pointer to the last byte in which copy count was written
} Encoder;

/****************************************************/
/* functions for managing the pool of image segments*/
/****************************************************/
static INLINE LzImageSegment *lz_alloc_image_seg(Encoder *encoder);
static void lz_reset_image_seg(Encoder *encoder);
static int lz_read_image_segments(Encoder *encoder, uint8_t *first_lines,
                                  unsigned int num_first_lines);


// return a free image segment if one exists. Make allocation if needed. adds it to the
// tail of the image segments lists
static INLINE LzImageSegment *lz_alloc_image_seg(Encoder *encoder)
{
    LzImageSegment *ret;

    if (encoder->free_image_segs) {
        ret = encoder->free_image_segs;
        encoder->free_image_segs = ret->next;
    } else {
        if (!(ret = (LzImageSegment *)encoder->usr->malloc(encoder->usr, sizeof(*ret)))) {
            return NULL;
        }
    }

    ret->next = NULL;
    if (encoder->tail_image_segs) {
        encoder->tail_image_segs->next = ret;
    }
    encoder->tail_image_segs = ret;

    if (!encoder->head_image_segs) {
        encoder->head_image_segs = ret;
    }

    return ret;
}

// adding seg to the head of free segments (lz_reset_image_seg removes it from used ones)
static INLINE void __lz_free_image_seg(Encoder *encoder, LzImageSegment *seg)
{
    seg->next = encoder->free_image_segs;
    encoder->free_image_segs = seg;
}

// moves all the used image segments to the free pool
static void lz_reset_image_seg(Encoder *encoder)
{
    while (encoder->head_image_segs) {
        LzImageSegment *seg = encoder->head_image_segs;
        encoder->head_image_segs = seg->next;
        __lz_free_image_seg(encoder, seg);
    }
    encoder->tail_image_segs = NULL;
}

static void lz_dealloc_free_segments(Encoder *encoder)
{
    while (encoder->free_image_segs) {
        LzImageSegment *seg = encoder->free_image_segs;
        encoder->free_image_segs = seg->next;
        encoder->usr->free(encoder->usr, seg);
    }
}

// return FALSE when operation fails (due to failure in allocation)
static int lz_read_image_segments(Encoder *encoder, uint8_t *first_lines,
                                  unsigned int num_first_lines)
{
    LzImageSegment *image_seg;
    uint32_t size_delta = 0;
    unsigned int num_lines = num_first_lines;
    uint8_t* lines = first_lines;
    int row;

    spice_return_val_if_fail(!encoder->head_image_segs, FALSE);

    image_seg = lz_alloc_image_seg(encoder);
    if (!image_seg) {
        goto error_1;
    }

    image_seg->lines = lines;
    image_seg->lines_end = lines + num_lines * encoder->stride;
    image_seg->size_delta = size_delta;

    size_delta += num_lines * encoder->stride / RGB_BYTES_PER_PIXEL[encoder->type];

    for (row = num_first_lines; row < encoder->height; row += num_lines) {
        num_lines = encoder->usr->more_lines(encoder->usr, &lines);
        if (num_lines <= 0) {
            encoder->usr->error(encoder->usr, "more lines failed\n");
        }
        image_seg = lz_alloc_image_seg(encoder);

        if (!image_seg) {
            goto error_1;
        }

        image_seg->lines = lines;
        image_seg->lines_end = lines + num_lines * encoder->stride;
        image_seg->size_delta = size_delta;

        size_delta += num_lines * encoder->stride / RGB_BYTES_PER_PIXEL[encoder->type];
    }

    return TRUE;
error_1:
    lz_reset_image_seg(encoder);
    return FALSE;
}

/**************************************************************************
* Handling encoding and decoding of a byte
***************************************************************************/
static INLINE int more_io_bytes(Encoder *encoder)
{
    uint8_t *io_ptr;
    int num_io_bytes = encoder->usr->more_space(encoder->usr, &io_ptr);
    encoder->io_bytes_count += num_io_bytes;
    encoder->io_now = io_ptr;
    encoder->io_end = encoder->io_now + num_io_bytes;
    return num_io_bytes;
}

static INLINE void encode(Encoder *encoder, uint8_t byte)
{
    if (encoder->io_now == encoder->io_end) {
        if (more_io_bytes(encoder) <= 0) {
            encoder->usr->error(encoder->usr, "%s: no more bytes\n", __FUNCTION__);
        }
        spice_return_if_fail(encoder->io_now);
    }

    spice_return_if_fail(encoder->io_now < encoder->io_end);
    *(encoder->io_now++) = byte;
}

static INLINE void encode_32(Encoder *encoder, unsigned int word)
{
    encode(encoder, (uint8_t)(word >> 24));
    encode(encoder, (uint8_t)(word >> 16) & 0x0000ff);
    encode(encoder, (uint8_t)(word >> 8) & 0x0000ff);
    encode(encoder, (uint8_t)(word & 0x0000ff));
}

static INLINE void encode_copy_count(Encoder *encoder, uint8_t copy_count)
{
    encode(encoder, copy_count);
    encoder->io_last_copy = encoder->io_now - 1; // io_now cannot be the first byte of the buffer
}

static INLINE void update_copy_count(Encoder *encoder, uint8_t copy_count)
{
    spice_return_if_fail(encoder->io_last_copy);
    *(encoder->io_last_copy) = copy_count;
}

static INLINE void encode_level(Encoder *encoder, uint8_t level_code)
{
    *(encoder->io_start) |= level_code;
}

// decrease the io ptr by 1
static INLINE void compress_output_prev(Encoder *encoder)
{
    // io_now cannot be the first byte of the buffer
    encoder->io_now--;
    // the function should be called only when copy count is written unnecessarily by lz_compress
    spice_return_if_fail(encoder->io_now == encoder->io_last_copy);
}

static int encoder_reset(Encoder *encoder, uint8_t *io_ptr, uint8_t *io_ptr_end)
{
    spice_return_val_if_fail(io_ptr <= io_ptr_end, FALSE);

    encoder->io_bytes_count = io_ptr_end - io_ptr;
    encoder->io_start = io_ptr;
    encoder->io_now = io_ptr;
    encoder->io_end = io_ptr_end;
    encoder->io_last_copy = NULL;

    return TRUE;
}

static INLINE uint8_t decode(Encoder *encoder)
{
    if (encoder->io_now == encoder->io_end) {
        int num_io_bytes = more_io_bytes(encoder);
        if (num_io_bytes <= 0) {
            encoder->usr->error(encoder->usr, "%s: no more bytes\n", __FUNCTION__);
        }
        spice_assert(encoder->io_now);
    }
    spice_assert(encoder->io_now < encoder->io_end);
    return *(encoder->io_now++);
}

static INLINE uint32_t decode_32(Encoder *encoder)
{
    uint32_t word = 0;
    word |= decode(encoder);
    word <<= 8;
    word |= decode(encoder);
    word <<= 8;
    word |= decode(encoder);
    word <<= 8;
    word |= decode(encoder);
    return word;
}

static INLINE int is_io_to_decode_end(Encoder *encoder)
{
    if (encoder->io_now != encoder->io_end) {
        return FALSE;
    } else {
        int num_io_bytes = more_io_bytes(encoder); //disable inline optimizations
        return (num_io_bytes <= 0);
    }
}

/*******************************************************************
* intialization and finalization of lz
********************************************************************/
static int init_encoder(Encoder *encoder, LzUsrContext *usr)
{
    encoder->usr = usr;
    encoder->free_image_segs = NULL;
    encoder->head_image_segs = NULL;
    encoder->tail_image_segs = NULL;
    return TRUE;
}

LzContext *lz_create(LzUsrContext *usr)
{
    Encoder *encoder;

    if (!usr || !usr->error || !usr->warn || !usr->info || !usr->malloc ||
        !usr->free || !usr->more_space || !usr->more_lines) {
        return NULL;
    }

    if (!(encoder = (Encoder *)usr->malloc(usr, sizeof(Encoder)))) {
        return NULL;
    }

    if (!init_encoder(encoder, usr)) {
        usr->free(usr, encoder);
        return NULL;
    }
    return (LzContext *)encoder;
}

void lz_destroy(LzContext *lz)
{
    Encoder *encoder = (Encoder *)lz;

    if (!lz) {
        return;
    }

    if (encoder->head_image_segs) {
        encoder->usr->error(encoder->usr, "%s: used_image_segments not empty\n", __FUNCTION__);
        lz_reset_image_seg(encoder);
    }
    lz_dealloc_free_segments(encoder);

    encoder->usr->free(encoder->usr, encoder);
}

/*******************************************************************
*                encoding and decoding the image
********************************************************************/
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


/* the palette images will be treated as one byte pixels. Their width should be transformed
   accordingly.
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


#define MAX_COPY 32
#define MAX_LEN 264          /* 256 + 8 */
#define BOUND_OFFSET 2
#define LIMIT_OFFSET 6
#define MIN_FILE_SIZE 4
#define COMP_LEVEL_SIZE_LIMIT 65536

// TODO: implemented lz2. should lz1 be an option (no RLE + distance limitation of MAX_DISTANCE)
// TODO: I think MAX_FARDISTANCE can be changed easily to 2^29
//       (and maybe even more when pixel > byte).
// i.e. we can support 512M Bytes/Pixels distance instead of only ~68K.
#define MAX_DISTANCE 8191                        // 2^13
#define MAX_FARDISTANCE (65535 + MAX_DISTANCE - 1)    // ~2^16+2^13


#define LZ_PLT
#include "lz_compress_tmpl.c"
#define LZ_PLT
#include "lz_decompress_tmpl.c"

#define LZ_PLT
#define PLT8
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_PLT
#define PLT4_BE
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_PLT
#define PLT4_LE
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_PLT
#define PLT1_BE
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_PLT
#define PLT1_LE
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_A8
#include "lz_compress_tmpl.c"
#define LZ_A8
#include "lz_decompress_tmpl.c"
#define LZ_A8
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_RGB16
#include "lz_compress_tmpl.c"
#define LZ_RGB16
#include "lz_decompress_tmpl.c"
#define LZ_RGB16
#define TO_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_RGB24
#include "lz_compress_tmpl.c"
#define LZ_RGB24
#include "lz_decompress_tmpl.c"


#define LZ_RGB32
#include "lz_compress_tmpl.c"
#define LZ_RGB32
#include "lz_decompress_tmpl.c"

#define LZ_RGB_ALPHA
#include "lz_compress_tmpl.c"
#define LZ_RGB_ALPHA
#include "lz_decompress_tmpl.c"

#undef LZ_UNEXPECT_CONDITIONAL
#undef LZ_EXPECT_CONDITIONAL

int lz_encode(LzContext *lz, LzImageType type, int width, int height, int top_down,
              uint8_t *lines, unsigned int num_lines, int stride,
              uint8_t *io_ptr, unsigned int num_io_bytes)
{
    Encoder *encoder = (Encoder *)lz;
    uint8_t *io_ptr_end = io_ptr + num_io_bytes;

    encoder->type = type;
    encoder->width = width;
    encoder->height = height;
    encoder->stride = stride;

    if (IS_IMAGE_TYPE_PLT[encoder->type]) {
        if (encoder->stride > (width / PLT_PIXELS_PER_BYTE[encoder->type])) {
            if (((width % PLT_PIXELS_PER_BYTE[encoder->type]) == 0) || (
                    (encoder->stride - (width / PLT_PIXELS_PER_BYTE[encoder->type])) > 1)) {
                encoder->usr->error(encoder->usr, "stride overflows (plt)\n");
            }
        }
    } else {
        if (encoder->stride != width * RGB_BYTES_PER_PIXEL[encoder->type]) {
            encoder->usr->error(encoder->usr, "stride != width*bytes_per_pixel (rgb) %d != %d * %d (%d)\n",
                                encoder->stride, width, RGB_BYTES_PER_PIXEL[encoder->type],
                                encoder->type);
        }
    }

    // assign the output buffer
    if (!encoder_reset(encoder, io_ptr, io_ptr_end)) {
        encoder->usr->error(encoder->usr, "lz encoder io reset failed\n");
    }

    // first read the list of the image segments
    if (!lz_read_image_segments(encoder, lines, num_lines)) {
        encoder->usr->error(encoder->usr, "lz encoder reading image segments failed\n");
    }

    encode_32(encoder, LZ_MAGIC);
    encode_32(encoder, LZ_VERSION);
    encode_32(encoder, type);
    encode_32(encoder, width);
    encode_32(encoder, height);
    encode_32(encoder, stride);
    encode_32(encoder, top_down); // TODO: maybe compress type and top_down to one byte

    switch (encoder->type) {
    case LZ_IMAGE_TYPE_PLT1_BE:
    case LZ_IMAGE_TYPE_PLT1_LE:
    case LZ_IMAGE_TYPE_PLT4_BE:
    case LZ_IMAGE_TYPE_PLT4_LE:
    case LZ_IMAGE_TYPE_PLT8:
        lz_plt_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB16:
        lz_rgb16_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB24:
        lz_rgb24_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGB32:
        lz_rgb32_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_RGBA:
        lz_rgb32_compress(encoder);
        lz_rgb_alpha_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_XXXA:
        lz_rgb_alpha_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_A8:
        lz_a8_compress(encoder);
        break;
    case LZ_IMAGE_TYPE_INVALID:
    default:
        encoder->usr->error(encoder->usr, "bad image type\n");
    }

    // move all the used segments to the free ones
    lz_reset_image_seg(encoder);

    encoder->io_bytes_count -= (encoder->io_end - encoder->io_now);

    return encoder->io_bytes_count;
}

/*
    initialize and read lz magic
*/
void lz_decode_begin(LzContext *lz, uint8_t *io_ptr, unsigned int num_io_bytes,
                     LzImageType *out_type, int *out_width, int *out_height,
                     int *out_n_pixels, int *out_top_down, const SpicePalette *palette)
{
    Encoder *encoder = (Encoder *)lz;
    uint8_t *io_ptr_end = io_ptr + num_io_bytes;
    uint32_t magic;
    uint32_t version;

    if (!encoder_reset(encoder, io_ptr, io_ptr_end)) {
        encoder->usr->error(encoder->usr, "io reset failed");
    }

    magic = decode_32(encoder);
    if (magic != LZ_MAGIC) {
        encoder->usr->error(encoder->usr, "bad magic\n");
    }

    version = decode_32(encoder);
    if (version != LZ_VERSION) {
        encoder->usr->error(encoder->usr, "bad version\n");
    }

    encoder->type = (LzImageType)decode_32(encoder);
    encoder->width = decode_32(encoder);
    encoder->height = decode_32(encoder);
    encoder->stride = decode_32(encoder);
    *out_top_down = decode_32(encoder);

    *out_width = encoder->width;
    *out_height = encoder->height;
//    *out_stride = encoder->stride;
    *out_type = encoder->type;

    // TODO: maybe instead of stride we can encode out_n_pixels
    //       (if stride is not necessary in decoding).
    if (IS_IMAGE_TYPE_PLT[encoder->type]) {
        encoder->palette = palette;
        *out_n_pixels = encoder->stride * PLT_PIXELS_PER_BYTE[encoder->type] * encoder->height;
    } else {
        *out_n_pixels = encoder->width * encoder->height;
    }
}

void lz_decode(LzContext *lz, LzImageType to_type, uint8_t *buf)
{
    Encoder *encoder = (Encoder *)lz;
    size_t out_size = 0;
    size_t alpha_size = 0;
    size_t size = 0;
    if (IS_IMAGE_TYPE_PLT[encoder->type]) {
        if (to_type == encoder->type) {
            size = encoder->height * encoder->stride;
            out_size = lz_plt_decompress(encoder, (one_byte_pixel_t *)buf, size);
        } else if (to_type == LZ_IMAGE_TYPE_RGB32) {
            size = encoder->height * encoder->stride * PLT_PIXELS_PER_BYTE[encoder->type];
            if (!encoder->palette) {
                encoder->usr->error(encoder->usr,
                                    "a palette is missing (for bpp to rgb decoding)\n");
            }
            switch (encoder->type) {
            case LZ_IMAGE_TYPE_PLT1_BE:
                out_size = lz_plt1_be_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                break;
            case LZ_IMAGE_TYPE_PLT1_LE:
                out_size = lz_plt1_le_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                break;
            case LZ_IMAGE_TYPE_PLT4_BE:
                out_size = lz_plt4_be_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                break;
            case LZ_IMAGE_TYPE_PLT4_LE:
                out_size = lz_plt4_le_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                break;
            case LZ_IMAGE_TYPE_PLT8:
                out_size = lz_plt8_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                break;
            case LZ_IMAGE_TYPE_RGB16:
            case LZ_IMAGE_TYPE_RGB24:
            case LZ_IMAGE_TYPE_RGB32:
            case LZ_IMAGE_TYPE_RGBA:
            case LZ_IMAGE_TYPE_XXXA:
            case LZ_IMAGE_TYPE_INVALID:
            default:
                encoder->usr->error(encoder->usr, "bad image type\n");
            }
        } else {
            encoder->usr->error(encoder->usr, "unsupported output format\n");
        }
    } else {
        size = encoder->height * encoder->width;
        switch (encoder->type) {
        case LZ_IMAGE_TYPE_RGB16:
            if (encoder->type == to_type) {
                out_size = lz_rgb16_decompress(encoder, (rgb16_pixel_t *)buf, size);
            } else if (to_type == LZ_IMAGE_TYPE_RGB32) {
                out_size = lz_rgb16_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_RGB24:
            if (encoder->type == to_type) {
                out_size = lz_rgb24_decompress(encoder, (rgb24_pixel_t *)buf, size);
            } else if (to_type == LZ_IMAGE_TYPE_RGB32) {
                out_size = lz_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_RGB32:
            if (encoder->type == to_type) {
                out_size = lz_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_RGBA:
            if (encoder->type == to_type) {
                out_size = lz_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                alpha_size = lz_rgb_alpha_decompress(encoder, (rgb32_pixel_t *)buf, size);
                spice_assert(alpha_size == size);
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_XXXA:
            if (encoder->type == to_type) {
                alpha_size = lz_rgb_alpha_decompress(encoder, (rgb32_pixel_t *)buf, size);
                out_size = alpha_size;
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_A8:
            if (encoder->type == to_type) {
                alpha_size = lz_a8_decompress(encoder, (one_byte_pixel_t *)buf, size);
                out_size = alpha_size;
            } else if (to_type == LZ_IMAGE_TYPE_RGB32) {
                alpha_size = lz_a8_to_rgb32_decompress(encoder, (rgb32_pixel_t *)buf, size);
                out_size = alpha_size;
            } else {
                encoder->usr->error(encoder->usr, "unsupported output format\n");
            }
            break;
        case LZ_IMAGE_TYPE_PLT1_LE:
        case LZ_IMAGE_TYPE_PLT1_BE:
        case LZ_IMAGE_TYPE_PLT4_LE:
        case LZ_IMAGE_TYPE_PLT4_BE:
        case LZ_IMAGE_TYPE_PLT8:
        case LZ_IMAGE_TYPE_INVALID:
        default:
            encoder->usr->error(encoder->usr, "bad image type\n");
        }
    }

    spice_assert(is_io_to_decode_end(encoder));
    spice_assert(out_size == size);

    if (out_size != size) {
        encoder->usr->error(encoder->usr, "bad decode size\n");
    }
}
