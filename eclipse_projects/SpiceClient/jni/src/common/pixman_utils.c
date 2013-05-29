/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
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

#include "pixman_utils.h"
#include "spice_common.h"
#include <spice/macros.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mem.h"

#define SOLID_RASTER_OP(_name, _size, _type, _equation)  \
static void                                        \
solid_rop_ ## _name ## _ ## _size (_type *ptr, int len, _type src)  \
{                                                  \
    while (len--) {                                \
        _type dst = *ptr;                          \
        if (dst) /* avoid unused warning */{};       \
        *ptr = (_type)(_equation);                 \
        ptr++;                                     \
    }                                              \
}                                                  \

#define TILED_RASTER_OP(_name, _size, _type, _equation) \
static void                                        \
tiled_rop_ ## _name ## _ ## _size (_type *ptr, int len, _type *tile, _type *tile_end, int tile_width)   \
{                                                  \
    while (len--) {                                \
        _type src = *tile;                         \
        _type dst = *ptr;                          \
        if (src) /* avoid unused warning */{};       \
        if (dst) /* avoid unused warning */{};       \
        *ptr = (_type)(_equation);                 \
        ptr++;                                     \
        tile++;                                    \
        if (tile == tile_end)                      \
            tile -= tile_width;                    \
    }                                              \
}                                                  \

#define COPY_RASTER_OP(_name, _size, _type, _equation) \
static void                                        \
 copy_rop_ ## _name ## _ ## _size (_type *ptr, _type *src_line, int len)        \
{                                                  \
    while (len--) {                                \
        _type src = *src_line;                     \
        _type dst = *ptr;                          \
        if (src) /* avoid unused warning */ {};       \
        if (dst) /* avoid unused warning */{};       \
        *ptr = (_type)(_equation);                 \
        ptr++;                                     \
        src_line++;                                \
    }                                              \
}                                                  \

#define RASTER_OP(name, equation) \
    SOLID_RASTER_OP(name, 8, uint8_t, equation) \
    SOLID_RASTER_OP(name, 16, uint16_t, equation) \
    SOLID_RASTER_OP(name, 32, uint32_t, equation) \
    TILED_RASTER_OP(name, 8, uint8_t, equation) \
    TILED_RASTER_OP(name, 16, uint16_t, equation) \
    TILED_RASTER_OP(name, 32, uint32_t, equation) \
    COPY_RASTER_OP(name, 8, uint8_t, equation) \
    COPY_RASTER_OP(name, 16, uint16_t, equation) \
    COPY_RASTER_OP(name, 32, uint32_t, equation)

RASTER_OP(clear, 0x0)
RASTER_OP(and, src & dst)
RASTER_OP(and_reverse, src & ~dst)
RASTER_OP(copy, src)
RASTER_OP(and_inverted, ~src & dst)
RASTER_OP(noop, dst)
RASTER_OP(xor, src ^ dst)
RASTER_OP(or, src | dst)
RASTER_OP(nor, ~src & ~dst)
RASTER_OP(equiv, ~src ^ dst)
RASTER_OP(invert, ~dst)
RASTER_OP(or_reverse, src | ~dst)
RASTER_OP(copy_inverted, ~src)
RASTER_OP(or_inverted, ~src | dst)
RASTER_OP(nand, ~src | ~dst)
RASTER_OP(set, 0xffffffff)

typedef void (*solid_rop_8_func_t)(uint8_t *ptr, int len, uint8_t src);
typedef void (*solid_rop_16_func_t)(uint16_t *ptr, int len, uint16_t src);
typedef void (*solid_rop_32_func_t)(uint32_t *ptr, int len, uint32_t src);
typedef void (*tiled_rop_8_func_t)(uint8_t *ptr, int len,
                                   uint8_t *tile, uint8_t *tile_end, int tile_width);
typedef void (*tiled_rop_16_func_t)(uint16_t *ptr, int len,
                                    uint16_t *tile, uint16_t *tile_end, int tile_width);
typedef void (*tiled_rop_32_func_t)(uint32_t *ptr, int len,
                                    uint32_t *tile, uint32_t *tile_end, int tile_width);
typedef void (*copy_rop_8_func_t)(uint8_t *ptr, uint8_t *src, int len);
typedef void (*copy_rop_16_func_t)(uint16_t *ptr, uint16_t *src, int len);
typedef void (*copy_rop_32_func_t)(uint32_t *ptr, uint32_t *src, int len);

#define ROP_TABLE(_type, _size)                                                 \
static void (*solid_rops_ ## _size[16]) (_type *ptr, int len, _type src) = { \
    solid_rop_clear_ ## _size,  \
    solid_rop_and_ ## _size,    \
    solid_rop_and_reverse_ ## _size,    \
    solid_rop_copy_ ## _size,    \
    solid_rop_and_inverted_ ## _size,    \
    solid_rop_noop_ ## _size,    \
    solid_rop_xor_ ## _size,    \
    solid_rop_or_ ## _size,    \
    solid_rop_nor_ ## _size,    \
    solid_rop_equiv_ ## _size,    \
    solid_rop_invert_ ## _size,    \
    solid_rop_or_reverse_ ## _size,    \
    solid_rop_copy_inverted_ ## _size,    \
    solid_rop_or_inverted_ ## _size,    \
    solid_rop_nand_ ## _size,    \
    solid_rop_set_ ## _size    \
};                          \
static void (*tiled_rops_ ## _size[16]) (_type *ptr, int len, _type *tile, _type *tile_end, int tile_width) = { \
    tiled_rop_clear_ ## _size,  \
    tiled_rop_and_ ## _size,    \
    tiled_rop_and_reverse_ ## _size,    \
    tiled_rop_copy_ ## _size,    \
    tiled_rop_and_inverted_ ## _size,    \
    tiled_rop_noop_ ## _size,    \
    tiled_rop_xor_ ## _size,    \
    tiled_rop_or_ ## _size,    \
    tiled_rop_nor_ ## _size,    \
    tiled_rop_equiv_ ## _size,    \
    tiled_rop_invert_ ## _size,    \
    tiled_rop_or_reverse_ ## _size,    \
    tiled_rop_copy_inverted_ ## _size,    \
    tiled_rop_or_inverted_ ## _size,    \
    tiled_rop_nand_ ## _size,    \
    tiled_rop_set_ ## _size    \
}; \
static void (*copy_rops_ ## _size[16]) (_type *ptr, _type *tile, int len) = { \
    copy_rop_clear_ ## _size,  \
    copy_rop_and_ ## _size,    \
    copy_rop_and_reverse_ ## _size,    \
    copy_rop_copy_ ## _size,    \
    copy_rop_and_inverted_ ## _size,    \
    copy_rop_noop_ ## _size,    \
    copy_rop_xor_ ## _size,    \
    copy_rop_or_ ## _size,    \
    copy_rop_nor_ ## _size,    \
    copy_rop_equiv_ ## _size,    \
    copy_rop_invert_ ## _size,    \
    copy_rop_or_reverse_ ## _size,    \
    copy_rop_copy_inverted_ ## _size,    \
    copy_rop_or_inverted_ ## _size,    \
    copy_rop_nand_ ## _size,    \
    copy_rop_set_ ## _size    \
};

ROP_TABLE(uint8_t, 8)
ROP_TABLE(uint16_t, 16)
ROP_TABLE(uint32_t, 32)

/* We can't get the real bits per pixel info from pixman_image_t,
   only the DEPTH which is the sum of all a+r+g+b bits, which
   is e.g. 24 for 32bit xRGB. We really want the bpp, so
   we have this ugly conversion thing */
int spice_pixman_image_get_bpp(pixman_image_t *image)
{
    int depth;

    depth = pixman_image_get_depth(image);
    if (depth == 24) {
        return 32;
    }
    if (depth == 15) {
        return 16;
    }
    return depth;
}

void spice_pixman_fill_rect(pixman_image_t *dest,
                            int x, int y,
                            int width, int height,
                            uint32_t value)
{
    uint32_t *bits;
    int stride, depth;
    uint32_t byte_width;
    uint8_t *byte_line;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    spice_assert(x >= 0);
    spice_assert(y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(x + width <= pixman_image_get_width(dest));
    spice_assert(y + height <= pixman_image_get_height(dest));

    if (pixman_fill(bits,
                    stride / 4,
                    depth,
                    x, y,
                    width, height,
                    value)) {
        return;
    }

    if (depth == 8) {
        byte_line = ((uint8_t *)bits) + stride * y + x;
        byte_width = width;
        value = (value & 0xff) * 0x01010101;
    } else if (depth == 16) {
        byte_line = ((uint8_t *)bits) + stride * y + x * 2;
        byte_width = 2 * width;
        value = (value & 0xffff) * 0x00010001;
    } else {
        spice_assert (depth == 32);
        byte_line = ((uint8_t *)bits) + stride * y + x * 4;
        byte_width = 4 * width;
    }

    while (height--) {
        int w;
        uint8_t *d = byte_line;

        byte_line += stride;
        w = byte_width;

        while (w >= 1 && ((uintptr_t)d & 1)) {
            *(uint8_t *)d = (value & 0xff);
            w--;
            d++;
        }

        while (w >= 2 && ((uintptr_t)d & 3)) {
            *(uint16_t *)d = value;
            w -= 2;
            d += 2;
        }

        while (w >= 4 && ((uintptr_t)d & 7)) {
            *(uint32_t *)d = value;

            w -= 4;
            d += 4;
        }

        while (w >= 4) {
            *(uint32_t *)d = value;

            w -= 4;
            d += 4;
        }

        while (w >= 2) {
            *(uint16_t *)d = value;
            w -= 2;
            d += 2;
        }

        while (w >= 1) {
            *(uint8_t *)d = (value & 0xff);
            w--;
            d++;
        }
    }
}

void spice_pixman_fill_rect_rop(pixman_image_t *dest,
                                int x, int y,
                                int width, int height,
                                uint32_t value,
                                SpiceROP rop)
{
    uint32_t *bits;
    int stride, depth;
    uint8_t *byte_line;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    spice_assert(x >= 0);
    spice_assert(y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(x + width <= pixman_image_get_width(dest));
    spice_assert(y + height <= pixman_image_get_height(dest));
    spice_assert(rop < 16);

    if (depth == 8) {
        solid_rop_8_func_t rop_func = solid_rops_8[rop];

        byte_line = ((uint8_t *)bits) + stride * y + x;
        while (height--) {
            rop_func((uint8_t *)byte_line, width, (uint8_t)value);
            byte_line += stride;
        }

    } else if (depth == 16) {
        solid_rop_16_func_t rop_func = solid_rops_16[rop];

        byte_line = ((uint8_t *)bits) + stride * y + x * 2;
        while (height--) {
            rop_func((uint16_t *)byte_line, width, (uint16_t)value);
            byte_line += stride;
        }
    }  else {
        solid_rop_32_func_t rop_func = solid_rops_32[rop];

        byte_line = ((uint8_t *)bits) + stride * y + x * 4;
        while (height--) {
            rop_func((uint32_t *)byte_line, width, (uint32_t)value);
            byte_line += stride;
        }
    }
}

void spice_pixman_tile_rect(pixman_image_t *dest,
                            int x, int y,
                            int width, int height,
                            pixman_image_t *tile,
                            int offset_x,
                            int offset_y)
{
    uint32_t *bits, *tile_bits;
    int stride, depth;
    int tile_width, tile_height, tile_stride;
    uint8_t *byte_line;
    uint8_t *tile_line;
    int tile_start_x, tile_start_y, tile_end_dx;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    tile_bits = pixman_image_get_data(tile);
    tile_stride = pixman_image_get_stride(tile);
    tile_width = pixman_image_get_width(tile);
    tile_height = pixman_image_get_height(tile);

    spice_assert(x >= 0);
    spice_assert(y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(x + width <= pixman_image_get_width(dest));
    spice_assert(y + height <= pixman_image_get_height(dest));
    spice_assert(depth == spice_pixman_image_get_bpp(tile));

    tile_start_x = (x - offset_x) % tile_width;
    if (tile_start_x < 0) {
        tile_start_x += tile_width;
    }
    tile_start_y = (y - offset_y) % tile_height;
    if (tile_start_y < 0) {
        tile_start_y += tile_height;
    }
    tile_end_dx = tile_width - tile_start_x;

    if (depth == 8) {
        byte_line = ((uint8_t *)bits) + stride * y + x;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x;
        while (height--) {
            tiled_rop_copy_8((uint8_t *)byte_line, width,
                             (uint8_t *)tile_line, (uint8_t *)tile_line + tile_end_dx,
                             tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }

    } else if (depth == 16) {
        byte_line = ((uint8_t *)bits) + stride * y + x * 2;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x * 2;
        while (height--) {
            tiled_rop_copy_16((uint16_t *)byte_line, width,
                              (uint16_t *)tile_line, (uint16_t *)tile_line + tile_end_dx,
                              tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }
    }  else {
        spice_assert (depth == 32);

        byte_line = ((uint8_t *)bits) + stride * y + x * 4;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x * 4;
        while (height--) {
            tiled_rop_copy_32((uint32_t *)byte_line, width,
                              (uint32_t *)tile_line, (uint32_t *)tile_line + tile_end_dx,
                              tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }
    }
}

void spice_pixman_tile_rect_rop(pixman_image_t *dest,
                                int x, int y,
                                int width, int height,
                                pixman_image_t *tile,
                                int offset_x,
                                int offset_y,
                                SpiceROP rop)
{
    uint32_t *bits, *tile_bits;
    int stride, depth;
    int tile_width, tile_height, tile_stride;
    uint8_t *byte_line;
    uint8_t *tile_line;
    int tile_start_x, tile_start_y, tile_end_dx;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    tile_bits = pixman_image_get_data(tile);
    tile_stride = pixman_image_get_stride(tile);
    tile_width = pixman_image_get_width(tile);
    tile_height = pixman_image_get_height(tile);

    spice_assert(x >= 0);
    spice_assert(y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(x + width <= pixman_image_get_width(dest));
    spice_assert(y + height <= pixman_image_get_height(dest));
    spice_assert(rop < 16);
    spice_assert(depth == spice_pixman_image_get_bpp(tile));

    tile_start_x = (x - offset_x) % tile_width;
    if (tile_start_x < 0) {
        tile_start_x += tile_width;
    }
    tile_start_y = (y - offset_y) % tile_height;
    if (tile_start_y < 0) {
        tile_start_y += tile_height;
    }
    tile_end_dx = tile_width - tile_start_x;

    if (depth == 8) {
        tiled_rop_8_func_t rop_func = tiled_rops_8[rop];

        byte_line = ((uint8_t *)bits) + stride * y + x;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x;
        while (height--) {
            rop_func((uint8_t *)byte_line, width,
                     (uint8_t *)tile_line, (uint8_t *)tile_line + tile_end_dx,
                     tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }

    } else if (depth == 16) {
        tiled_rop_16_func_t rop_func = tiled_rops_16[rop];

        byte_line = ((uint8_t *)bits) + stride * y + x * 2;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x * 2;
        while (height--) {
            rop_func((uint16_t *)byte_line, width,
                     (uint16_t *)tile_line, (uint16_t *)tile_line + tile_end_dx,
                     tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }
    }  else {
        tiled_rop_32_func_t rop_func = tiled_rops_32[rop];

        spice_assert (depth == 32);

        byte_line = ((uint8_t *)bits) + stride * y + x * 4;
        tile_line = ((uint8_t *)tile_bits) + tile_stride * tile_start_y + tile_start_x * 4;
        while (height--) {
            rop_func((uint32_t *)byte_line, width,
                     (uint32_t *)tile_line, (uint32_t *)tile_line + tile_end_dx,
                     tile_width);
            byte_line += stride;
            tile_line += tile_stride;
            if (++tile_start_y == tile_height) {
                tile_line -= tile_height * tile_stride;
                tile_start_y = 0;
            }
        }
    }
}


void spice_pixman_blit(pixman_image_t *dest,
                       pixman_image_t *src,
                       int src_x, int src_y,
                       int dest_x, int dest_y,
                       int width, int height)
{
    uint32_t *bits, *src_bits;
    int stride, depth, src_depth;
    int src_width, src_height, src_stride;
    uint8_t *byte_line;
    uint8_t *src_line;
    int byte_width;

    if (!src) {
        fprintf(stderr, "missing src!");
        return;
    }

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    src_bits = pixman_image_get_data(src);
    src_stride = pixman_image_get_stride(src);
    src_width = pixman_image_get_width(src);
    src_height = pixman_image_get_height(src);
    src_depth = spice_pixman_image_get_bpp(src);

    /* Clip source */
    if (src_x < 0) {
        width += src_x;
        dest_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        height += src_y;
        dest_y -= src_y;
        src_y = 0;
    }
    if (src_x + width > src_width) {
        width = src_width - src_x;
    }
    if (src_y + height > src_height) {
        height = src_height - src_y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    spice_assert(src_x >= 0);
    spice_assert(src_y >= 0);
    spice_assert(dest_x >= 0);
    spice_assert(dest_y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(dest_x + width <= pixman_image_get_width(dest));
    spice_assert(dest_y + height <= pixman_image_get_height(dest));
    spice_assert(src_x + width <= pixman_image_get_width(src));
    spice_assert(src_y + height <= pixman_image_get_height(src));
    spice_assert(depth == src_depth);

    if (pixman_blt(src_bits,
                   bits,
                   src_stride / 4,
                   stride / 4,
                   depth, depth,
                   src_x, src_y,
                   dest_x, dest_y,
                   width, height)) {
        return;
    }

    if (depth == 8) {
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x;
        byte_width = width;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x;
    } else if (depth == 16) {
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 2;
        byte_width = width * 2;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 2;
    }  else {
        spice_assert (depth == 32);
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 4;
        byte_width = width * 4;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 4;
    }

    while (height--) {
        memcpy(byte_line, src_line, byte_width);
        byte_line += stride;
        src_line += src_stride;
    }
}

void spice_pixman_blit_rop (pixman_image_t *dest,
                            pixman_image_t *src,
                            int src_x, int src_y,
                            int dest_x, int dest_y,
                            int width, int height,
                            SpiceROP rop)
{
    uint32_t *bits, *src_bits;
    int stride, depth, src_depth;
    int src_width, src_height, src_stride;
    uint8_t *byte_line;
    uint8_t *src_line;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    src_bits = pixman_image_get_data(src);
    src_stride = pixman_image_get_stride(src);
    src_width = pixman_image_get_width(src);
    src_height = pixman_image_get_height(src);
    src_depth = spice_pixman_image_get_bpp(src);

    /* Clip source */
    if (src_x < 0) {
        width += src_x;
        dest_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        height += src_y;
        dest_y -= src_y;
        src_y = 0;
    }
    if (src_x + width > src_width) {
        width = src_width - src_x;
    }
    if (src_y + height > src_height) {
        height = src_height - src_y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    spice_assert(src_x >= 0);
    spice_assert(src_y >= 0);
    spice_assert(dest_x >= 0);
    spice_assert(dest_y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(dest_x + width <= pixman_image_get_width(dest));
    spice_assert(dest_y + height <= pixman_image_get_height(dest));
    spice_assert(src_x + width <= pixman_image_get_width(src));
    spice_assert(src_y + height <= pixman_image_get_height(src));
    spice_assert(depth == src_depth);

    if (depth == 8) {
        copy_rop_8_func_t rop_func = copy_rops_8[rop];

        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x;

        while (height--) {
            rop_func((uint8_t *)byte_line, (uint8_t *)src_line, width);
            byte_line += stride;
            src_line += src_stride;
        }
    } else if (depth == 16) {
        copy_rop_16_func_t rop_func = copy_rops_16[rop];

        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 2;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 2;

        while (height--) {
            rop_func((uint16_t *)byte_line, (uint16_t *)src_line, width);
            byte_line += stride;
            src_line += src_stride;
        }
    }  else {
        copy_rop_32_func_t rop_func = copy_rops_32[rop];

        spice_assert (depth == 32);
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 4;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 4;

        while (height--) {
            rop_func((uint32_t *)byte_line, (uint32_t *)src_line, width);
            byte_line += stride;
            src_line += src_stride;
        }
    }

}

void spice_pixman_blit_colorkey (pixman_image_t *dest,
                                 pixman_image_t *src,
                                 int src_x, int src_y,
                                 int dest_x, int dest_y,
                                 int width, int height,
                                 uint32_t transparent_color)
{
    uint32_t *bits, *src_bits;
    int stride, depth;
    int src_width, src_height, src_stride;
    uint8_t *byte_line;
    uint8_t *src_line;
    int x;

    bits = pixman_image_get_data(dest);
    stride = pixman_image_get_stride(dest);
    depth = spice_pixman_image_get_bpp(dest);
    /* stride is in bytes, depth in bits */

    src_bits = pixman_image_get_data(src);
    src_stride = pixman_image_get_stride(src);
    src_width = pixman_image_get_width(src);
    src_height = pixman_image_get_height(src);

    /* Clip source */
    if (src_x < 0) {
        width += src_x;
        dest_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0) {
        height += src_y;
        dest_y -= src_y;
        src_y = 0;
    }
    if (src_x + width > src_width) {
        width = src_width - src_x;
    }
    if (src_y + height > src_height) {
        height = src_height - src_y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    spice_assert(src_x >= 0);
    spice_assert(src_y >= 0);
    spice_assert(dest_x >= 0);
    spice_assert(dest_y >= 0);
    spice_assert(width > 0);
    spice_assert(height > 0);
    spice_assert(dest_x + width <= pixman_image_get_width(dest));
    spice_assert(dest_y + height <= pixman_image_get_height(dest));
    spice_assert(src_x + width <= pixman_image_get_width(src));
    spice_assert(src_y + height <= pixman_image_get_height(src));
    spice_assert(depth == spice_pixman_image_get_bpp(src));

    if (depth == 8) {
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x;

        while (height--) {
            uint8_t *d = (uint8_t *)byte_line;
            uint8_t *s = (uint8_t *)src_line;
            for (x = 0; x < width; x++) {
                uint8_t val = *s;
                if (val != (uint8_t)transparent_color) {
                    *d = val;
                }
                s++; d++;
            }

            byte_line += stride;
            src_line += src_stride;
        }
    } else if (depth == 16) {
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 2;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 2;

        while (height--) {
            uint16_t *d = (uint16_t *)byte_line;
            uint16_t *s = (uint16_t *)src_line;

            for (x = 0; x < width; x++) {
                uint16_t val = *s;
                if (val != (uint16_t)transparent_color) {
                    *d = val;
                }
                s++; d++;
            }

            byte_line += stride;
            src_line += src_stride;
        }
    }  else {
        spice_assert (depth == 32);
        byte_line = ((uint8_t *)bits) + stride * dest_y + dest_x * 4;
        src_line = ((uint8_t *)src_bits) + src_stride * src_y + src_x * 4;

        while (height--) {
            uint32_t *d = (uint32_t *)byte_line;
            uint32_t *s = (uint32_t *)src_line;

            transparent_color &= 0xffffff;
            for (x = 0; x < width; x++) {
                uint32_t val = *s;
                if ((0xffffff & val) != transparent_color) {
                    *d = val;
                }
                s++; d++;
            }

            byte_line += stride;
            src_line += src_stride;
        }
    }
}

static void copy_bits_up(uint8_t *data, const int stride, int bpp,
                         const int src_x, const int src_y,
                         const int width, const int height,
                         const int dest_x, const int dest_y)
{
    uint8_t *src = data + src_y * stride + src_x * bpp;
    uint8_t *dest = data + dest_y * stride + dest_x * bpp;
    uint8_t *end = dest + height * stride;
    for (; dest != end; dest += stride, src += stride) {
        memcpy(dest, src, width * bpp);
    }
}

static void copy_bits_down(uint8_t *data, const int stride, int bpp,
                           const int src_x, const int src_y,
                           const int width, const int height,
                           const int dest_x, const int dest_y)
{
    uint8_t *src = data + (src_y + height - 1) * stride + src_x * bpp;
    uint8_t *end = data + (dest_y - 1) * stride + dest_x * bpp;
    uint8_t *dest = end + height * stride;

    for (; dest != end; dest -= stride, src -= stride) {
        memcpy(dest, src, width * bpp);
    }
}

static void copy_bits_same_line(uint8_t *data, const int stride, int bpp,
                                const int src_x, const int src_y,
                                const int width, const int height,
                                const int dest_x, const int dest_y)
{
    uint8_t *src = data + src_y * stride + src_x * bpp;
    uint8_t *dest = data + dest_y * stride + dest_x * bpp;
    uint8_t *end = dest + height * stride;
    for (; dest != end; dest += stride, src += stride) {
        memmove(dest, src, width * bpp);
    }
}

void spice_pixman_copy_rect (pixman_image_t *image,
                             int src_x, int src_y,
                             int width, int height,
                             int dest_x, int dest_y)
{
    uint8_t *data;
    int stride;
    int bpp;

    data = (uint8_t *)pixman_image_get_data(image);
    stride = pixman_image_get_stride(image);
    bpp = spice_pixman_image_get_bpp(image) / 8;

    if (dest_y > src_y) {
        copy_bits_down(data, stride, bpp,
                       src_x, src_y,
                       width, height,
                       dest_x, dest_y);
    } else if (dest_y < src_y) {
        copy_bits_up(data, stride, bpp,
                     src_x, src_y,
                     width, height,
                     dest_x, dest_y);
    } else {
        copy_bits_same_line(data, stride, bpp,
                            src_x, src_y,
                            width, height,
                            dest_x, dest_y);
    }
}

pixman_bool_t spice_pixman_region32_init_rects (pixman_region32_t *region,
                                                const SpiceRect   *rects,
                                                int                count)
{
    /* These types are compatible, so just cast */
    return pixman_region32_init_rects(region, (pixman_box32_t *)rects, count);
}

pixman_format_code_t spice_surface_format_to_pixman(uint32_t surface_format)
{
    switch (surface_format) {
    case SPICE_SURFACE_FMT_1_A:
        return PIXMAN_a1;
    case SPICE_SURFACE_FMT_8_A:
        return PIXMAN_a8;
    case SPICE_SURFACE_FMT_16_555:
        return PIXMAN_x1r5g5b5;
    case SPICE_SURFACE_FMT_16_565:
        return PIXMAN_r5g6b5;
    case SPICE_SURFACE_FMT_32_xRGB:
        return PIXMAN_x8r8g8b8;
    case SPICE_SURFACE_FMT_32_ARGB:
        return PIXMAN_a8r8g8b8;
    default:
        printf("Unknown surface format %d\n", surface_format);
        spice_abort();
        break;
    }
        return (pixman_format_code_t)0; /* Not reached */
}

/* Returns the "spice native" pixman version of a specific bitmap format.
 * This isn't bitwise the same as the bitmap format, for instance we
 * typically convert indexed to real color modes and use the standard
 * surface modes rather than weird things like 24bit
 */
pixman_format_code_t spice_bitmap_format_to_pixman(int bitmap_format,
                                                   uint32_t palette_surface_format)
{
    switch (bitmap_format) {
    case SPICE_BITMAP_FMT_1BIT_LE:
    case SPICE_BITMAP_FMT_1BIT_BE:
    case SPICE_BITMAP_FMT_4BIT_LE:
    case SPICE_BITMAP_FMT_4BIT_BE:
    case SPICE_BITMAP_FMT_8BIT:
        /* Indexed mode palettes are the same as their destination canvas format */
        return spice_surface_format_to_pixman(palette_surface_format);

    case SPICE_BITMAP_FMT_16BIT:
        return PIXMAN_x1r5g5b5;

    case SPICE_BITMAP_FMT_24BIT:
    case SPICE_BITMAP_FMT_32BIT:
        return PIXMAN_x8r8g8b8;

    case SPICE_BITMAP_FMT_RGBA:
        return PIXMAN_a8r8g8b8;

    case SPICE_BITMAP_FMT_8BIT_A:
        return PIXMAN_a8;

    case SPICE_BITMAP_FMT_INVALID:
    default:
        printf("Unknown bitmap format %d\n", bitmap_format);
        spice_abort();
        return PIXMAN_a8r8g8b8;
    }
}

/* Tries to view a spice bitmap as a pixman_image_t without copying,
 * will often fail due to unhandled formats or strides.
 */
pixman_image_t *spice_bitmap_try_as_pixman(int src_format,
                                           int flags,
                                           int width,
                                           int height,
                                           uint8_t *data,
                                           int stride)
{
    pixman_format_code_t pixman_format;

    /* Pixman stride must be multiple of 4 */
    if (stride % 4 != 0) {
        return NULL;
    }

    switch (src_format) {
    case SPICE_BITMAP_FMT_32BIT:
#ifdef WORDS_BIGENDIAN
        pixman_format = PIXMAN_b8g8r8x8;
#else
        pixman_format = PIXMAN_x8r8g8b8;
#endif
        break;
    case SPICE_BITMAP_FMT_RGBA:
#ifdef WORDS_BIGENDIAN
        pixman_format = PIXMAN_b8g8r8a8;
#else
        pixman_format = PIXMAN_a8r8g8b8;
#endif
        break;
    case SPICE_BITMAP_FMT_24BIT:
#ifdef WORDS_BIGENDIAN
        pixman_format = PIXMAN_b8g8r8;
#else
        pixman_format = PIXMAN_r8g8b8;
#endif
        break;
    case SPICE_BITMAP_FMT_16BIT:
#ifdef WORDS_BIGENDIAN
        return NULL;
#else
        pixman_format = PIXMAN_x1r5g5b5;
#endif
        break;

    default:
        return NULL;
    }

    if (!(flags & SPICE_BITMAP_FLAGS_TOP_DOWN)) {
        data += stride * (height - 1);
        stride = -stride;
    }

    return pixman_image_create_bits (pixman_format,
                                     width,
                                     height,
                                     (uint32_t *)data,
                                     stride);
}

#ifdef WORDS_BIGENDIAN
#define UINT16_FROM_LE(x) SPICE_BYTESWAP16(x)
#define UINT32_FROM_LE(x) SPICE_BYTESWAP32(x)
#else
#define UINT16_FROM_LE(x) (x)
#define UINT32_FROM_LE(x) (x)
#endif

static INLINE uint32_t rgb_16_555_to_32(uint16_t color)
{
    uint32_t ret;

    ret = ((color & 0x001f) << 3) | ((color & 0x001c) >> 2);
    ret |= ((color & 0x03e0) << 6) | ((color & 0x0380) << 1);
    ret |= ((color & 0x7c00) << 9) | ((color & 0x7000) << 4);

    return ret;
}

static INLINE uint16_t rgb_32_to_16_555(uint32_t color)
{
    return
        (((color) >> 3) & 0x001f) |
        (((color) >> 6) & 0x03e0) |
        (((color) >> 9) & 0x7c00);
}


static void bitmap_32_to_32(uint8_t* dest, int dest_stride,
                            uint8_t* src, int src_stride,
                            int width, uint8_t* end)
{
#ifdef WORDS_BIGENDIAN
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint32_t* src_line = (uint32_t *)src;
        uint32_t* src_line_end = src_line + width;
        uint32_t* dest_line = (uint32_t *)dest;

        for (; src_line < src_line_end; ++dest_line, ++src_line) {
            *dest_line = UINT32_FROM_LE(*src_line);
        }
    }
#else
    for (; src != end; src += src_stride, dest += dest_stride) {
        memcpy(dest, src, width * 4);
    }
#endif
}

static void bitmap_8_to_8(uint8_t* dest, int dest_stride,
                          uint8_t* src, int src_stride,
                          int width, uint8_t* end)
{
    for (; src != end; src += src_stride, dest += dest_stride) {
        memcpy(dest, src, width);
    }
}

static void bitmap_24_to_32(uint8_t* dest, int dest_stride,
                            uint8_t* src, int src_stride,
                            int width, uint8_t* end)
{
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint8_t* src_line = src;
        uint8_t* src_line_end = src_line + width * 3;
        uint32_t* dest_line = (uint32_t *)dest;

        for (; src_line < src_line_end; ++dest_line) {
            uint32_t r, g, b;
            b = *(src_line++);
            g = *(src_line++);
            r = *(src_line++);
            *dest_line = (r << 16) | (g << 8) | (b);
        }
    }
}

static void bitmap_16_to_16_555(uint8_t* dest, int dest_stride,
                                uint8_t* src, int src_stride,
                                int width, uint8_t* end)
{
#ifdef WORDS_BIGENDIAN
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint16_t* src_line = (uint16_t *)src;
        uint16_t* src_line_end = src_line + width;
        uint16_t* dest_line = (uint16_t *)dest;

        for (; src_line < src_line_end; ++dest_line, ++src_line) {
            *dest_line = UINT16_FROM_LE(*src_line);
        }
    }
#else
    for (; src != end; src += src_stride, dest += dest_stride) {
        memcpy(dest, src, width * 2);
    }
#endif
}

static void bitmap_8_32_to_32(uint8_t *dest, int dest_stride,
                              uint8_t *src, int src_stride,
                              int width, uint8_t *end,
                              SpicePalette *palette)
{
    uint32_t local_ents[256];
    uint32_t *ents;
    int n_ents;
#ifdef WORDS_BIGENDIAN
    int i;
#endif

    if (!palette) {
        spice_error("No palette");
        return;
    }

    n_ents = MIN(palette->num_ents, 256);
    ents = palette->ents;

    if (n_ents < 255
#ifdef WORDS_BIGENDIAN
        || TRUE
#endif
        ) {
        memcpy(local_ents, ents, n_ents*4);
        ents = local_ents;

#ifdef WORDS_BIGENDIAN
        for (i = 0; i < n_ents; i++) {
            ents[i] = UINT32_FROM_LE(ents[i]);
        }
#endif
    }

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint32_t *dest_line = (uint32_t*)dest;
        uint8_t *src_line = src;
        uint8_t *src_line_end = src_line + width;

        while (src_line < src_line_end) {
            *(dest_line++) = ents[*(src_line++)];
        }
    }
}

static void bitmap_8_16_to_16_555(uint8_t *dest, int dest_stride,
                                  uint8_t *src, int src_stride,
                                  int width, uint8_t *end,
                                  SpicePalette *palette)
{
    uint32_t local_ents[256];
    uint32_t *ents;
    int n_ents;
#ifdef WORDS_BIGENDIAN
    int i;
#endif

    if (!palette) {
        spice_error("No palette");
        return;
    }

    n_ents = MIN(palette->num_ents, 256);
    ents = palette->ents;

    if (n_ents < 255
#ifdef WORDS_BIGENDIAN
        || TRUE
#endif
        ) {
        memcpy(local_ents, ents, n_ents*4);
        ents = local_ents;

#ifdef WORDS_BIGENDIAN
        for (i = 0; i < n_ents; i++) {
            ents[i] = UINT32_FROM_LE(ents[i]);
        }
#endif
    }

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint16_t *dest_line = (uint16_t*)dest;
        uint8_t *src_line = src;
        uint8_t *src_line_end = src_line + width;

        while (src_line < src_line_end) {
            *(dest_line++) = ents[*(src_line++)];
        }
    }
}

static void bitmap_4be_32_to_32(uint8_t* dest, int dest_stride,
                                uint8_t* src, int src_stride,
                                int width, uint8_t* end,
                                SpicePalette *palette)
{
    uint32_t local_ents[16];
    uint32_t *ents;
    int n_ents;
#ifdef WORDS_BIGENDIAN
    int i;
#endif

    if (!palette) {
        spice_error("No palette");
        return;
    }

    n_ents = MIN(palette->num_ents, 16);
    ents = palette->ents;

    if (n_ents < 16
#ifdef WORDS_BIGENDIAN
        || TRUE
#endif
        ) {
        memcpy(local_ents, ents, n_ents*4);
        ents = local_ents;

#ifdef WORDS_BIGENDIAN
        for (i = 0; i < n_ents; i++) {
            ents[i] = UINT32_FROM_LE(ents[i]);
        }
#endif
    }

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint32_t *dest_line = (uint32_t *)dest;
        uint8_t *row = src;
        int i;

        for (i = 0; i < (width >> 1); i++) {
            *(dest_line++) = ents[(*row >> 4) & 0x0f];
            *(dest_line++) = ents[*(row++) & 0x0f];
        }
        if (width & 1) {
            *(dest_line) = ents[(*row >> 4) & 0x0f];
        }
    }
}

static void bitmap_4be_16_to_16_555(uint8_t* dest, int dest_stride,
                                    uint8_t* src, int src_stride,
                                    int width, uint8_t* end,
                                    SpicePalette *palette)
{
    uint32_t local_ents[16];
    uint32_t *ents;
    int n_ents;
#ifdef WORDS_BIGENDIAN
    int i;
#endif

    if (!palette) {
        spice_error("No palette");
        return;
    }

    n_ents = MIN(palette->num_ents, 16);
    ents = palette->ents;

    if (n_ents < 16
#ifdef WORDS_BIGENDIAN
        || TRUE
#endif
        ) {
        memcpy(local_ents, ents, n_ents*4);
        ents = local_ents;

#ifdef WORDS_BIGENDIAN
        for (i = 0; i < n_ents; i++) {
            ents[i] = UINT32_FROM_LE(ents[i]);
        }
#endif
    }

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint16_t *dest_line = (uint16_t *)dest;
        uint8_t *row = src;
        int i;

        for (i = 0; i < (width >> 1); i++) {
            *(dest_line++) = ents[(*row >> 4) & 0x0f];
            *(dest_line++) = ents[*(row++) & 0x0f];
        }
        if (width & 1) {
            *(dest_line) = ents[(*row >> 4) & 0x0f];
        }
    }
}

static INLINE int test_bit_be(void* addr, int bit)
{
    return !!(((uint8_t*)addr)[bit >> 3] & (0x80 >> (bit & 0x07)));
}

static void bitmap_1be_32_to_32(uint8_t* dest, int dest_stride,
                                uint8_t* src, int src_stride,
                                int width, uint8_t* end,
                                SpicePalette *palette)
{
    uint32_t fore_color;
    uint32_t back_color;

    spice_assert(palette != NULL);

    if (!palette) {
        return;
    }

    fore_color = UINT32_FROM_LE(palette->ents[1]);
    back_color = UINT32_FROM_LE(palette->ents[0]);

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint32_t* dest_line = (uint32_t*)dest;
        int i;

        for (i = 0; i < width; i++) {
            if (test_bit_be(src, i)) {
                *(dest_line++) = fore_color;
            } else {
                *(dest_line++) = back_color;
            }
        }
    }
}


static void bitmap_1be_16_to_16_555(uint8_t* dest, int dest_stride,
                                    uint8_t* src, int src_stride,
                                    int width, uint8_t* end,
                                    SpicePalette *palette)
{
    uint16_t fore_color;
    uint16_t back_color;

    spice_assert(palette != NULL);

    if (!palette) {
        return;
    }

    fore_color = (uint16_t) UINT32_FROM_LE(palette->ents[1]);
    back_color = (uint16_t) UINT32_FROM_LE(palette->ents[0]);

    for (; src != end; src += src_stride, dest += dest_stride) {
        uint16_t* dest_line = (uint16_t*)dest;
        int i;

        for (i = 0; i < width; i++) {
            if (test_bit_be(src, i)) {
                *(dest_line++) = fore_color;
            } else {
                *(dest_line++) = back_color;
            }
        }
    }
}

#ifdef NOT_USED_ATM

static void bitmap_16_to_32(uint8_t* dest, int dest_stride,
                            uint8_t* src, int src_stride,
                            int width, uint8_t* end)
{
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint16_t* src_line = (uint16_t*)src;
        uint16_t* src_line_end = src_line + width;
        uint32_t* dest_line = (uint32_t*)dest;

        for (; src_line < src_line_end; ++dest_line, src_line++) {
            *dest_line = rgb_16_555_to_32(UINT16_FROM_LE(*src_line));
        }
    }
}

static void bitmap_32_to_16_555(uint8_t* dest, int dest_stride,
                                uint8_t* src, int src_stride,
                                int width, uint8_t* end)
{
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint32_t* src_line = (uint32_t *)src;
        uint32_t* src_line_end = src_line + width;
        uint16_t* dest_line = (uint16_t *)dest;

        for (; src_line < src_line_end; ++dest_line, ++src_line) {
            *dest_line = rgb_32_to_16_555(UINT16_FROM_LE(*src_line));
        }
    }
}


static void bitmap_24_to_16_555(uint8_t* dest, int dest_stride,
                                uint8_t* src, int src_stride,
                                int width, uint8_t* end)
{
    for (; src != end; src += src_stride, dest += dest_stride) {
        uint8_t* src_line = src;
        uint8_t* src_line_end = src_line + width * 3;
        uint16_t* dest_line = (uint16_t *)dest;

        for (; src_line < src_line_end; ++dest_line) {
            uint8_t r, g, b;
            b = *(src_line++);
            g = *(src_line++);
            r = *(src_line++);
            *dest_line = rgb_32_to_16_555(r << 24 | g << 16 | b);
        }
    }
}

#endif

/* This assumes that the dest, if set is the same format as
   spice_bitmap_format_to_pixman would have picked */
pixman_image_t *spice_bitmap_to_pixman(pixman_image_t *dest_image,
                                       int src_format,
                                       int flags,
                                       int width,
                                       int height,
                                       uint8_t *src,
                                       int src_stride,
                                       uint32_t palette_surface_format,
                                       SpicePalette *palette)
{
    uint8_t* dest;
    int dest_stride;
    uint8_t* end;

    if (dest_image == NULL) {
        pixman_format_code_t dest_format;

        dest_format = spice_bitmap_format_to_pixman(src_format,
                                                    palette_surface_format);
        dest_image = pixman_image_create_bits (dest_format,
                                               width, height,
                                               NULL, 0);
    }

    dest = (uint8_t *)pixman_image_get_data(dest_image);
    dest_stride = pixman_image_get_stride(dest_image);
    if (!(flags & SPICE_BITMAP_FLAGS_TOP_DOWN)) {
        spice_assert(height > 0);
        dest += dest_stride * (height - 1);
        dest_stride = -dest_stride;
    }
    end = src + (height * src_stride);

    switch (src_format) {
    case SPICE_BITMAP_FMT_32BIT:
    case SPICE_BITMAP_FMT_RGBA:
        bitmap_32_to_32(dest, dest_stride, src, src_stride, width, end);
        break;
    case SPICE_BITMAP_FMT_8BIT_A:
        bitmap_8_to_8(dest, dest_stride, src, src_stride, width, end);
        break;
    case SPICE_BITMAP_FMT_24BIT:
        bitmap_24_to_32(dest, dest_stride, src, src_stride, width, end);
        break;
    case SPICE_BITMAP_FMT_16BIT:
        bitmap_16_to_16_555(dest, dest_stride, src, src_stride, width, end);
        break;
    case SPICE_BITMAP_FMT_8BIT:
        if (palette_surface_format == SPICE_SURFACE_FMT_32_ARGB ||
            palette_surface_format == SPICE_SURFACE_FMT_32_xRGB) {
            bitmap_8_32_to_32(dest, dest_stride, src, src_stride, width, end, palette);
        } else if (palette_surface_format == SPICE_SURFACE_FMT_16_555) {
            bitmap_8_16_to_16_555(dest, dest_stride, src, src_stride, width, end, palette);
        } else {
            spice_error("Unsupported palette format");
        }
        break;
    case SPICE_BITMAP_FMT_4BIT_BE:
        if (palette_surface_format == SPICE_SURFACE_FMT_32_ARGB ||
            palette_surface_format == SPICE_SURFACE_FMT_32_xRGB) {
            bitmap_4be_32_to_32(dest, dest_stride, src, src_stride, width, end, palette);
        } else if (palette_surface_format == SPICE_SURFACE_FMT_16_555) {
            bitmap_4be_16_to_16_555(dest, dest_stride, src, src_stride, width, end, palette);
        } else {
            spice_error("Unsupported palette format");
        }
        break;
    case SPICE_BITMAP_FMT_1BIT_BE:
        if (palette_surface_format == SPICE_SURFACE_FMT_32_ARGB ||
            palette_surface_format == SPICE_SURFACE_FMT_32_xRGB) {
            bitmap_1be_32_to_32(dest, dest_stride, src, src_stride, width, end, palette);
        } else if (palette_surface_format == SPICE_SURFACE_FMT_16_555) {
            bitmap_1be_16_to_16_555(dest, dest_stride, src, src_stride, width, end, palette);
        } else {
            spice_error("Unsupported palette format");
        }
        break;
    default:
        spice_error("Unsupported bitmap format");
        break;
    }

    return dest_image;
}

static int pixman_format_compatible (pixman_format_code_t dest_format,
                              pixman_format_code_t src_format)
{
    if (dest_format == src_format) {
        return TRUE;
    }

    if (src_format == PIXMAN_a8r8g8b8 &&
        dest_format == PIXMAN_x8r8g8b8) {
        /* This is the same, we just ignore the alphas */
        return TRUE;
    }

    return FALSE;
}

pixman_image_t *spice_bitmap_convert_to_pixman(pixman_format_code_t dest_format,
                                               pixman_image_t *dest_image,
                                               int src_format,
                                               int flags,
                                               int width,
                                               int height,
                                               uint8_t *src,
                                               int src_stride,
                                               uint32_t palette_surface_format,
                                               SpicePalette *palette)
{
    pixman_image_t *src_image;
    pixman_format_code_t native_format;

    if (dest_image == NULL) {
        dest_image = pixman_image_create_bits (dest_format,
                                               width, height,
                                               NULL, 0);
    }

    native_format =
        spice_bitmap_format_to_pixman(src_format, palette_surface_format);

    if (pixman_format_compatible (dest_format, native_format)) {
        return spice_bitmap_to_pixman(dest_image,
                                      src_format,
                                      flags, width,height,
                                      src, src_stride,
                                      palette_surface_format, palette);
    }

    src_image = spice_bitmap_try_as_pixman(src_format,
                                           flags, width,height,
                                           src, src_stride);

    /* Can't convert directly, need a temporary copy
     * Hopefully most bitmap reads should not need conversion (i.e.
     * hit the spice_bitmap_to_pixmap case above) or work with the
     * try_as_pixmap case, but in case some specific combination
     * shows up here commonly we might want to add non-temporary
     * conversion special casing here */
    if (src_image == NULL) {
        src_image = spice_bitmap_to_pixman(NULL,
                                           src_format,
                                           flags, width,height,
                                           src, src_stride,
                                           palette_surface_format, palette);
    }

    pixman_image_composite32 (PIXMAN_OP_SRC,
                              src_image, NULL, dest_image,
                              0, 0,
                              0, 0,
                              0, 0,
                              width, height);

    pixman_image_unref (src_image);

    return dest_image;
}
