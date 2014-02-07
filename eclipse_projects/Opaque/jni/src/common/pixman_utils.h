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

#ifndef _H__PIXMAN_UTILS
#define _H__PIXMAN_UTILS

#include <spice/types.h>
#include <spice/macros.h>
#include <stdlib.h>
#define PIXMAN_DONT_DEFINE_STDINT
#include <pixman.h>

#include "draw.h"

SPICE_BEGIN_DECLS

/* This lists all possible 2 argument binary raster ops.
 * This enum has the same values as the X11 GXcopy type
 * and same as the GL constants (GL_AND etc) if you
 * or it with 0x1500. However it is not exactly the
 * same as the win32 ROP2 type (they use another order).
 */
typedef enum {
    SPICE_ROP_CLEAR,         /* 0x0    0 */
    SPICE_ROP_AND,           /* 0x1    src AND dst */
    SPICE_ROP_AND_REVERSE,   /* 0x2    src AND NOT dst */
    SPICE_ROP_COPY,          /* 0x3    src */
    SPICE_ROP_AND_INVERTED,  /* 0x4    (NOT src) AND dst */
    SPICE_ROP_NOOP,          /* 0x5    dst */
    SPICE_ROP_XOR,           /* 0x6    src XOR dst */
    SPICE_ROP_OR,            /* 0x7    src OR dst */
    SPICE_ROP_NOR,           /* 0x8    (NOT src) AND (NOT dst) */
    SPICE_ROP_EQUIV,         /* 0x9    (NOT src) XOR dst */
    SPICE_ROP_INVERT,        /* 0xa    NOT dst */
    SPICE_ROP_OR_REVERSE,    /* 0xb    src OR (NOT dst) */
    SPICE_ROP_COPY_INVERTED, /* 0xc    NOT src */
    SPICE_ROP_OR_INVERTED,   /* 0xd    (NOT src) OR dst */
    SPICE_ROP_NAND,          /* 0xe    (NOT src) OR (NOT dst) */
    SPICE_ROP_SET            /* 0xf    1 */
} SpiceROP;


int spice_pixman_image_get_bpp(pixman_image_t *image);

pixman_format_code_t spice_surface_format_to_pixman(uint32_t surface_format);
pixman_format_code_t spice_bitmap_format_to_pixman(int bitmap_format,
                                                   uint32_t palette_surface_format);
pixman_image_t *spice_bitmap_try_as_pixman(int src_format, int flags,
                                           int width, int height,
                                           uint8_t *data, int stride);
pixman_image_t *spice_bitmap_to_pixman(pixman_image_t *dest_image,
                                       int src_format, int flags,
                                       int width, int height,
                                       uint8_t *src, int src_stride,
                                       uint32_t palette_surface_format,
                                       SpicePalette *palette);
pixman_image_t *spice_bitmap_convert_to_pixman(pixman_format_code_t dest_format,
                                               pixman_image_t *dest_image,
                                               int src_format, int flags,
                                               int width, int height,
                                               uint8_t *src, int src_stride,
                                               uint32_t palette_surface_format,
                                               SpicePalette *palette);

void spice_pixman_region32_init_from_bitmap(pixman_region32_t *region,
                                            uint32_t *data,
                                            int width, int height,
                                            int stride);
pixman_bool_t spice_pixman_region32_init_rects(pixman_region32_t *region,
                                               const SpiceRect   *rects,
                                               int                count);
void spice_pixman_fill_rect(pixman_image_t *dest,
                            int x, int y,
                            int w, int h,
                            uint32_t value);
void spice_pixman_fill_rect_rop(pixman_image_t *dest,
                                int x, int y,
                                int w, int h,
                                uint32_t value,
                                SpiceROP rop);
void spice_pixman_tile_rect(pixman_image_t *dest,
                            int x, int y,
                            int w, int h,
                            pixman_image_t *tile,
                            int offset_x,
                            int offset_y);
void spice_pixman_tile_rect_rop(pixman_image_t *dest,
                                int x, int y,
                                int w, int h,
                                pixman_image_t *tile,
                                int offset_x,
                                int offset_y,
                                SpiceROP rop);
void spice_pixman_blit(pixman_image_t *dest,
                       pixman_image_t *src,
                       int src_x, int src_y,
                       int dest_x, int dest_y,
                       int w, int h);
void spice_pixman_blit_rop(pixman_image_t *dest,
                           pixman_image_t *src,
                           int src_x, int src_y,
                           int dest_x, int dest_y,
                           int w, int h,
                           SpiceROP rop);
void spice_pixman_blit_colorkey(pixman_image_t *dest,
                                pixman_image_t *src,
                                int src_x, int src_y,
                                int dest_x, int dest_y,
                                int width, int height,
                                uint32_t transparent_color);
void spice_pixman_copy_rect(pixman_image_t *image,
                            int src_x, int src_y,
                            int w, int h,
                            int dest_x, int dest_y);

SPICE_END_DECLS

#endif /* _H__PIXMAN_UTILS */
