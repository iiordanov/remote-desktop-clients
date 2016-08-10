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

#ifndef __GNUC__
#define ZLIB_WINAPI
#endif

#include <zlib.h>

typedef struct GlibZlibDecoder
{
    SpiceZlibDecoder         base;
    z_stream                 _z_strm;
} GlibZlibDecoder;

static void decode(SpiceZlibDecoder *decoder,
                   uint8_t *data, int data_size,
                   uint8_t *dest, int dest_size)
{
    GlibZlibDecoder *d = SPICE_CONTAINEROF(decoder, GlibZlibDecoder, base);
    int z_ret;

    inflateReset(&d->_z_strm);
    d->_z_strm.next_in = data;
    d->_z_strm.avail_in = data_size;
    d->_z_strm.next_out = dest;
    d->_z_strm.avail_out = dest_size;

    z_ret = inflate(&d->_z_strm, Z_FINISH);

    if (z_ret != Z_STREAM_END) {
        g_warning("zlib inflate failed, error %d", z_ret);
    }
}

static SpiceZlibDecoderOps zlib_decoder_ops = {
    .decode = decode,
};

SpiceZlibDecoder *zlib_decoder_new(void)
{
    GlibZlibDecoder *d = g_new0(GlibZlibDecoder, 1);
    int z_ret;

    d->_z_strm.zalloc = Z_NULL;
    d->_z_strm.zfree = Z_NULL;
    d->_z_strm.opaque = Z_NULL;
    d->_z_strm.next_in = Z_NULL;
    d->_z_strm.avail_in = 0;
    z_ret = inflateInit(&d->_z_strm);
    if (z_ret != Z_OK) {
        g_warning("zlib decoder init failed, error %d", z_ret);
        goto fail;
    }

    d->base.ops = &zlib_decoder_ops;

    return &d->base;

fail:
    free(d);
    return NULL;
}

void zlib_decoder_destroy(SpiceZlibDecoder *decoder)
{
    GlibZlibDecoder *d = SPICE_CONTAINEROF(decoder, GlibZlibDecoder, base);

    inflateEnd(&d->_z_strm);
    free(d);
}
