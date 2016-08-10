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
#include "config.h"

#include "decode.h"

#ifdef G_OS_WIN32
/* We need some hacks to avoid warnings from the jpeg headers, ex: */
/* #define HAVE_BOOLEAN */
#define XMD_H
/* #undef FAR */
/* but they are not compatible: uchar vs int........!@@(#$$??!@! */
/* fix this with UGLY HACK! */
/* #define boolean spice_jpeg_boolean */
/* #define INT32 spice_jpeg_int32 */
#endif

#include <stdio.h>
#include <jpeglib.h>

typedef struct GlibJpegDecoder
{
    SpiceJpegDecoder              base;
    struct jpeg_decompress_struct _cinfo;
    struct jpeg_error_mgr         _jerr;
    struct jpeg_source_mgr        _jsrc;

    uint8_t* _data;
    int      _data_size;
    int      _width;
    int      _height;
} GlibJpegDecoder;

static void begin_decode(SpiceJpegDecoder *decoder,
                         uint8_t* data, int data_size,
                         int* out_width, int* out_height)
{
    GlibJpegDecoder *d = SPICE_CONTAINEROF(decoder, GlibJpegDecoder, base);

    g_return_if_fail(data != NULL);
    g_return_if_fail(data_size != 0);

    if (d->_data)
        jpeg_abort_decompress(&d->_cinfo);

    d->_data = data;
    d->_data_size = data_size;

    d->_cinfo.src->next_input_byte = d->_data;
    d->_cinfo.src->bytes_in_buffer = d->_data_size;

    jpeg_read_header(&d->_cinfo, TRUE);

    d->_cinfo.out_color_space = JCS_RGB;
    d->_width = d->_cinfo.image_width;
    d->_height = d->_cinfo.image_height;

    *out_width = d->_width;
    *out_height = d->_height;
}

/* TODO: move it elsewhere and reuse it in get_pixbuf(), optimize? */
typedef void (*converter_rgb_t)(uint8_t* src, uint8_t* dest, int width);

static void convert_rgb_to_bgr(uint8_t* src, uint8_t* dest, int width)
{
    int x;

    for (x = 0; x < width; x++) {
        *dest++ = src[2];
        *dest++ = src[1];
        *dest++ = src[0];
        src += 3;
    }
}

static void convert_rgb_to_bgrx(uint8_t* src, uint8_t* dest, int width)
{
    int x;

    for (x = 0; x < width; x++) {
        *dest++ = src[2];
        *dest++ = src[1];
        *dest++ = src[0];
        *dest++ = 0;
        src += 3;
    }
}

static void decode(SpiceJpegDecoder *decoder,
                   uint8_t* dest, int stride, int format)
{
    GlibJpegDecoder *d = SPICE_CONTAINEROF(decoder, GlibJpegDecoder, base);
    uint8_t* scan_line = g_alloca(d->_width * 3);
    converter_rgb_t converter = NULL;
    int row;

    switch (format) {
    case SPICE_BITMAP_FMT_24BIT:
        converter = convert_rgb_to_bgr;
        break;
    case SPICE_BITMAP_FMT_32BIT:
        converter = convert_rgb_to_bgrx;
        break;
    default:
        g_warning("bad bitmap format, %d", format);
        return;
    }

    g_return_if_fail(converter != NULL);

    jpeg_start_decompress(&d->_cinfo);

    for (row = 0; row < d->_height; row++) {
        jpeg_read_scanlines(&d->_cinfo, &scan_line, 1);
        converter(scan_line, dest, d->_width);
        dest += stride;
    }

    jpeg_finish_decompress(&d->_cinfo);
}

static SpiceJpegDecoderOps jpeg_decoder_ops = {
    .begin_decode = begin_decode,
    .decode = decode,
};

static void jpeg_decoder_init_source(j_decompress_ptr cinfo)
{
}

static boolean jpeg_decoder_fill_input_buffer(j_decompress_ptr cinfo)
{
    g_warning("no more data for jpeg");
    return FALSE;
}

static void jpeg_decoder_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    g_return_if_fail(num_bytes < (long)cinfo->src->bytes_in_buffer);

    cinfo->src->next_input_byte += num_bytes;
    cinfo->src->bytes_in_buffer -= num_bytes;
}

static void jpeg_decoder_term_source (j_decompress_ptr cinfo)
{
    return;
}

SpiceJpegDecoder *jpeg_decoder_new(void)
{
    GlibJpegDecoder *d = g_new0(GlibJpegDecoder, 1);

    d->_cinfo.err = jpeg_std_error(&d->_jerr);
    jpeg_create_decompress(&d->_cinfo);

    d->_cinfo.src = &d->_jsrc;
    d->_cinfo.src->init_source = jpeg_decoder_init_source;
    d->_cinfo.src->fill_input_buffer = jpeg_decoder_fill_input_buffer;
    d->_cinfo.src->skip_input_data = jpeg_decoder_skip_input_data;
    d->_cinfo.src->resync_to_restart = jpeg_resync_to_restart;
    d->_cinfo.src->term_source = jpeg_decoder_term_source;

    d->base.ops = &jpeg_decoder_ops;

    return &d->base;
}

void jpeg_decoder_destroy(SpiceJpegDecoder *decoder)
{
    GlibJpegDecoder *d = SPICE_CONTAINEROF(decoder, GlibJpegDecoder, base);

    jpeg_destroy_decompress(&d->_cinfo);
    free(d);
}
