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
#ifndef CHANNEL_DISPLAY_PRIV_H_
# define CHANNEL_DISPLAY_PRIV_H_

#include <pixman.h>
#ifdef WIN32
/* We need some hacks to avoid warnings from the jpeg headers */
#define HAVE_BOOLEAN
#define XMD_H
#endif
#include <jpeglib.h>

#include "common/canvas_base.h"
#include "common/canvas_utils.h"
#include "common/sw_canvas.h"
#include "common/ring.h"
#include "common/quic.h"
#include "common/rop3.h"

G_BEGIN_DECLS


typedef struct display_surface {
    guint32                     surface_id;
    bool                        primary;
    enum SpiceSurfaceFmt        format;
    int                         width, height, stride, size;
    int                         shmid;
    uint8_t                     *data;
    SpiceCanvas                 *canvas;
    SpiceGlzDecoder             *glz_decoder;
    SpiceZlibDecoder            *zlib_decoder;
    SpiceJpegDecoder            *jpeg_decoder;
} display_surface;

typedef struct drops_sequence_stats {
    uint32_t len;
    uint32_t start_mm_time;
    uint32_t duration;
} drops_sequence_stats;

typedef struct display_stream {
    SpiceMsgIn                  *msg_create;
    SpiceMsgIn                  *msg_clip;
    SpiceMsgIn                  *msg_data;

    /* from messages */
    display_surface             *surface;
    SpiceClip                   *clip;
    QRegion                     region;
    int                         have_region;
    int                         codec;

    /* mjpeg decoder */
    struct jpeg_source_mgr         mjpeg_src;
    struct jpeg_decompress_struct  mjpeg_cinfo;
    struct jpeg_error_mgr          mjpeg_jerr;

    uint8_t                     *out_frame;
    GQueue                      *msgq;
    guint                       timeout;
    SpiceChannel                *channel;

    /* stats */
    uint32_t             first_frame_mm_time;
    uint32_t             num_drops_on_arive;
    uint64_t             arrive_late_time;
    uint32_t             num_drops_on_playback;
    uint32_t             num_input_frames;
    drops_sequence_stats cur_drops_seq_stats;
    GArray               *drops_seqs_stats_arr;
    uint32_t             num_drops_seqs;

    uint32_t             playback_sync_drops_seq_len;

    /* playback quality report to server */
    gboolean report_is_active;
    uint32_t report_id;
    uint32_t report_max_window;
    uint32_t report_timeout;
    uint64_t report_start_time;
    uint32_t report_start_frame_time;
    uint32_t report_num_frames;
    uint32_t report_num_drops;
    uint32_t report_drops_seq_len;
} display_stream;

void stream_get_dimensions(display_stream *st, int *width, int *height);
uint32_t stream_get_current_frame(display_stream *st, uint8_t **data);

/* channel-display-mjpeg.c */
void stream_mjpeg_init(display_stream *st);
void stream_mjpeg_data(display_stream *st);
void stream_mjpeg_cleanup(display_stream *st);

G_END_DECLS

#endif // CHANNEL_DISPLAY_PRIV_H_
