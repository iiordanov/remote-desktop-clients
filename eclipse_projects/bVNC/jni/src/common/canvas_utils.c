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

#include "spice_common.h"
#include "canvas_utils.h"

#ifdef __GNUC__
#include <stdlib.h>
#include <stdio.h>
#endif
#include "mem.h"

#ifdef WIN32
static int gdi_handlers = 0;
#endif

static void release_data(pixman_image_t *image, void *release_data)
{
    PixmanData *data = (PixmanData *)release_data;

#ifdef WIN32
    if (data->bitmap) {
        DeleteObject((HBITMAP)data->bitmap);
        CloseHandle(data->mutex);
        gdi_handlers--;
    }
#endif
    free(data->data);

    free(data);
}

static PixmanData *
pixman_image_add_data(pixman_image_t *image)
{
    PixmanData *data;

    data = (PixmanData *)pixman_image_get_destroy_data(image);
    if (data == NULL) {
        data = (PixmanData *)calloc(1, sizeof(PixmanData));
        if (data == NULL) {
            spice_error("out of memory");
        }
        pixman_image_set_destroy_function(image,
                                          release_data,
                                          data);
    }

    return data;
}

void
spice_pixman_image_set_format(pixman_image_t *image,
                              pixman_format_code_t format)
{
    PixmanData *data;

    data = pixman_image_add_data(image);
    data->format = format;
}


int spice_pixman_image_get_format(pixman_image_t *image, pixman_format_code_t *format)
{
    PixmanData *data;

    spice_return_val_if_fail(format != NULL, 0);

    data = (PixmanData *)pixman_image_get_destroy_data(image);
    if (data != NULL && data->format != 0) {
        *format = data->format;
        return 1;
    }

    spice_warn_if_reached();
    return 0;
}

static INLINE pixman_image_t *__surface_create_stride(pixman_format_code_t format, int width, int height,
                                                      int stride)
{
    uint8_t *data;
    uint8_t *stride_data;
    pixman_image_t *surface;
    PixmanData *pixman_data;

    data = (uint8_t *)spice_malloc_n(abs(stride), height);
    if (stride < 0) {
        stride_data = data + (-stride) * (height - 1);
    } else {
        stride_data = data;
    }

    surface = pixman_image_create_bits(format, width, height, (uint32_t *)stride_data, stride);

    if (surface == NULL) {
        free(data);
        spice_error("create surface failed, out of memory");
    }

    pixman_data = pixman_image_add_data(surface);
    pixman_data->data = data;
    pixman_data->format = format;

    return surface;
}

#ifdef WIN32
pixman_image_t *surface_create(HDC dc, pixman_format_code_t format,
                                int width, int height, int top_down)
#else
pixman_image_t * surface_create(pixman_format_code_t format, int width, int height, int top_down)
#endif
{
#ifdef WIN32
    /*
     * Windows xp allow only 10,000 of gdi handlers, considering the fact that
     * we limit here the number to 5000, we dont use atomic operations to sync
     * this calculation against the other canvases (in case of multiple
     * monitors), in worst case there will be little more than 5000 gdi
     * handlers.
     */
    if (dc && gdi_handlers < 5000) {
        uint8_t *data;
        uint8_t *src;
        struct {
            BITMAPINFO inf;
            RGBQUAD palette[255];
        } bitmap_info;
        int nstride;
        pixman_image_t *surface;
        PixmanData *pixman_data;
        HBITMAP bitmap;
        HANDLE mutex;

        memset(&bitmap_info, 0, sizeof(bitmap_info));
        bitmap_info.inf.bmiHeader.biSize = sizeof(bitmap_info.inf.bmiHeader);
        bitmap_info.inf.bmiHeader.biWidth = width;

        bitmap_info.inf.bmiHeader.biHeight = (!top_down) ? height : -height;

        bitmap_info.inf.bmiHeader.biPlanes = 1;
        switch (format) {
        case PIXMAN_a8r8g8b8:
        case PIXMAN_x8r8g8b8:
            bitmap_info.inf.bmiHeader.biBitCount = 32;
            nstride = width * 4;
            break;
        case PIXMAN_x1r5g5b5:
        case PIXMAN_r5g6b5:
            bitmap_info.inf.bmiHeader.biBitCount = 16;
            nstride = SPICE_ALIGN(width * 2, 4);
            break;
        case PIXMAN_a8:
            bitmap_info.inf.bmiHeader.biBitCount = 8;
            nstride = SPICE_ALIGN(width, 4);
            break;
        case PIXMAN_a1:
            bitmap_info.inf.bmiHeader.biBitCount = 1;
            nstride = SPICE_ALIGN(width, 32) / 8;
            break;
        default:
            spice_error("invalid format");
            return NULL;
        }

        bitmap_info.inf.bmiHeader.biCompression = BI_RGB;

        mutex = CreateMutex(NULL, 0, NULL);
        if (!mutex) {
            spice_error("Unable to CreateMutex");
        }

        bitmap = CreateDIBSection(dc, &bitmap_info.inf, 0, (VOID **)&data, NULL, 0);
        if (!bitmap) {
            CloseHandle(mutex);
            spice_error("Unable to CreateDIBSection");
        }

        if (top_down) {
            src = data;
        } else {
            src = data + nstride * (height - 1);
            nstride = -nstride;
        }

        surface = pixman_image_create_bits(format, width, height, (uint32_t *)src, nstride);
        if (surface == NULL) {
            CloseHandle(mutex);
            DeleteObject(bitmap);
            spice_error("create surface failed, out of memory");
        }
        pixman_data = pixman_image_add_data(surface);
        pixman_data->format = format;
        pixman_data->bitmap = bitmap;
        pixman_data->mutex = mutex;
        gdi_handlers++;
        return surface;
    } else {
#endif
    if (top_down) {
        pixman_image_t *surface;
        PixmanData *data;

        surface = pixman_image_create_bits(format, width, height, NULL, 0);
        data = pixman_image_add_data(surface);
        data->format = format;
        return surface;
    } else {
        // NOTE: we assume here that the lz decoders always decode to RGB32.
        int stride = 0;
        switch (format) {
        case PIXMAN_a8r8g8b8:
        case PIXMAN_x8r8g8b8:
            stride = width * 4;
            break;
        case PIXMAN_x1r5g5b5:
        case PIXMAN_r5g6b5:
            stride = SPICE_ALIGN(width * 2, 4);
            break;
        case PIXMAN_a8:
            stride = SPICE_ALIGN(width, 4);
            break;
        case PIXMAN_a1:
            stride = SPICE_ALIGN(width, 32) / 8;
            break;
        default:
            spice_error("invalid format");
        }
        stride = -stride;
        return __surface_create_stride(format, width, height, stride);
    }
#ifdef WIN32
}

#endif
}

#ifdef WIN32
pixman_image_t *surface_create_stride(HDC dc, pixman_format_code_t format, int width, int height,
                                      int stride)
#else
pixman_image_t *surface_create_stride(pixman_format_code_t format, int width, int height,
                                      int stride)
#endif
{
#ifdef WIN32
    if (dc) {
        if (abs(stride) == (width * 4)) {
            return surface_create(dc, format, width, height, (stride > 0));
        }
    }
#endif

    return __surface_create_stride(format, width, height, stride);
}

pixman_image_t *alloc_lz_image_surface(LzDecodeUsrData *canvas_data,
                                       pixman_format_code_t pixman_format, int width,
                                       int height, int gross_pixels, int top_down)
{
    int stride;
    pixman_image_t *surface = NULL;

    stride = (gross_pixels / height) * (PIXMAN_FORMAT_BPP (pixman_format) / 8);

    /* pixman requires strides to be 4-byte aligned */
    stride = SPICE_ALIGN(stride, 4);
    
    if (!top_down) {
        stride = -stride;
    }

   surface = surface_create_stride(
#ifdef WIN32
            canvas_data->dc,
#endif
            pixman_format, width, height, stride);
    canvas_data->out_surface = surface;
    return surface;
}
