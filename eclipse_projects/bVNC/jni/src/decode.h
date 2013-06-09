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
#ifndef SPICEGTK_DECODE_H_
# define SPICEGTK_DECODE_H_

#include <glib.h>

#include "common/canvas_base.h"

G_BEGIN_DECLS

typedef struct SpiceGlzDecoderWindow SpiceGlzDecoderWindow;

SpiceGlzDecoderWindow *glz_decoder_window_new(void);
void glz_decoder_window_clear(SpiceGlzDecoderWindow *w);
void glz_decoder_window_destroy(SpiceGlzDecoderWindow *w);

SpiceGlzDecoder *glz_decoder_new(SpiceGlzDecoderWindow *w);
void glz_decoder_destroy(SpiceGlzDecoder *d);

SpiceZlibDecoder *zlib_decoder_new(void);
void zlib_decoder_destroy(SpiceZlibDecoder *d);

SpiceJpegDecoder *jpeg_decoder_new(void);
void jpeg_decoder_destroy(SpiceJpegDecoder *d);

G_END_DECLS

#endif // SPICEGTK_DECODE_H_
