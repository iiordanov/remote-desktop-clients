/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Red Hat, Inc.

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
#ifndef __SPICE_VMC_STREAM_H__
#define __SPICE_VMC_STREAM_H__

#include <gio/gio.h>

#include "spice-types.h"

G_BEGIN_DECLS

#define SPICE_TYPE_VMC_INPUT_STREAM         (spice_vmc_input_stream_get_type ())
#define SPICE_VMC_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SPICE_TYPE_VMC_INPUT_STREAM, SpiceVmcInputStream))
#define SPICE_VMC_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SPICE_TYPE_VMC_INPUT_STREAM, SpiceVmcInputStreamClass))
#define SPICE_IS_VMC_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPICE_TYPE_VMC_INPUT_STREAM))
#define SPICE_IS_VMC_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SPICE_TYPE_VMC_INPUT_STREAM))
#define SPICE_VMC_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SPICE_TYPE_VMC_INPUT_STREAM, SpiceVmcInputStreamClass))

typedef struct _SpiceVmcInputStreamClass     SpiceVmcInputStreamClass;
typedef struct _SpiceVmcInputStream          SpiceVmcInputStream;

GType          spice_vmc_input_stream_get_type   (void) G_GNUC_CONST;
void           spice_vmc_input_stream_co_data    (SpiceVmcInputStream *input,
                                                  const gpointer data,
                                                  gsize size);

void           spice_vmc_input_stream_read_all_async(GInputStream        *stream,
                                                     void                *buffer,
                                                     gsize                count,
                                                     int                  io_priority,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
gssize         spice_vmc_input_stream_read_all_finish(GInputStream       *stream,
                                                      GAsyncResult       *result,
                                                      GError            **error);


#define SPICE_TYPE_VMC_OUTPUT_STREAM         (spice_vmc_output_stream_get_type ())
#define SPICE_VMC_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SPICE_TYPE_VMC_OUTPUT_STREAM, SpiceVmcOutputStream))
#define SPICE_VMC_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SPICE_TYPE_VMC_OUTPUT_STREAM, SpiceVmcOutputStreamClass))
#define SPICE_IS_VMC_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPICE_TYPE_VMC_OUTPUT_STREAM))
#define SPICE_IS_VMC_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SPICE_TYPE_VMC_OUTPUT_STREAM))
#define SPICE_VMC_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SPICE_TYPE_VMC_OUTPUT_STREAM, SpiceVmcOutputStreamClass))

typedef struct _SpiceVmcOutputStreamClass     SpiceVmcOutputStreamClass;
typedef struct _SpiceVmcOutputStream          SpiceVmcOutputStream;

GType           spice_vmc_output_stream_get_type (void) G_GNUC_CONST;

#define SPICE_TYPE_VMC_STREAM                (spice_vmc_stream_get_type ())
#define SPICE_VMC_STREAM(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), SPICE_TYPE_VMC_STREAM, SpiceVmcStream))
#define SPICE_VMC_STREAM_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), SPICE_TYPE_VMC_STREAM, SpiceVmcStreamClass))
#define SPICE_IS_VMC_STREAM(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), SPICE_TYPE_VMC_STREAM))
#define SPICE_IS_VMC_STREAM_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), SPICE_TYPE_VMC_STREAM))
#define SPICE_VMC_STREAM_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), SPICE_TYPE_VMC_STREAM, SpiceVmcStreamClass))

typedef struct _SpiceVmcStreamClass           SpiceVmcStreamClass;
typedef struct _SpiceVmcStream                SpiceVmcStream;

GType           spice_vmc_stream_get_type        (void) G_GNUC_CONST;
SpiceVmcStream* spice_vmc_stream_new             (SpiceChannel *channel);

G_END_DECLS

#endif /* __SPICE_VMC_STREAM_H__ */
