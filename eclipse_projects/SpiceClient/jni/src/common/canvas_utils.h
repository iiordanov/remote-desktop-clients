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

#ifndef _H_CANVAS_UTILS
#define _H_CANVAS_UTILS

#ifdef WIN32
#include <windows.h>
#endif

#include <spice/types.h>
#include <spice/macros.h>

#include "pixman_utils.h"
#include "lz.h"

SPICE_BEGIN_DECLS

typedef struct PixmanData {
#ifdef WIN32
    HBITMAP bitmap;
    HANDLE mutex;
#endif
    uint8_t *data;
    pixman_format_code_t format;
} PixmanData;

void spice_pixman_image_set_format(pixman_image_t *image,
                                   pixman_format_code_t format);
int spice_pixman_image_get_format(pixman_image_t *image, pixman_format_code_t *format);


#ifdef WIN32
pixman_image_t *surface_create(HDC dc, pixman_format_code_t format,
                               int width, int height, int top_down);
#else
pixman_image_t *surface_create(pixman_format_code_t format, int width, int height, int top_down);
#endif

#ifdef WIN32
pixman_image_t *surface_create_stride(HDC dc, pixman_format_code_t format, int width, int height,
                                      int stride);
#else
pixman_image_t *surface_create_stride(pixman_format_code_t format, int width, int height,
                                      int stride);
#endif


typedef struct LzDecodeUsrData {
#ifdef WIN32
    HDC dc;
#endif
    pixman_image_t       *out_surface;
} LzDecodeUsrData;


pixman_image_t *alloc_lz_image_surface(LzDecodeUsrData *canvas_data,
                                       pixman_format_code_t pixman_format, int width,
                                       int height, int gross_pixels, int top_down);

SPICE_END_DECLS

#endif
