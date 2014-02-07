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
#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"

#include "channel-display-priv.h"

static void mjpeg_src_init(struct jpeg_decompress_struct *cinfo)
{
    display_stream *st = SPICE_CONTAINEROF(cinfo->src, display_stream, mjpeg_src);
    uint8_t *data;

    cinfo->src->bytes_in_buffer = stream_get_current_frame(st, &data);
    cinfo->src->next_input_byte = data;
}

static boolean mjpeg_src_fill(struct jpeg_decompress_struct *cinfo)
{
    g_critical("need more input data");
    return 0;
}

static void mjpeg_src_skip(struct jpeg_decompress_struct *cinfo,
                           long num_bytes)
{
    cinfo->src->next_input_byte += num_bytes;
}

static void mjpeg_src_term(struct jpeg_decompress_struct *cinfo)
{
    /* nothing */
}

G_GNUC_INTERNAL
void stream_mjpeg_init(display_stream *st)
{
    st->mjpeg_cinfo.err = jpeg_std_error(&st->mjpeg_jerr);
    jpeg_create_decompress(&st->mjpeg_cinfo);

    st->mjpeg_src.init_source         = mjpeg_src_init;
    st->mjpeg_src.fill_input_buffer   = mjpeg_src_fill;
    st->mjpeg_src.skip_input_data     = mjpeg_src_skip;
    st->mjpeg_src.resync_to_restart   = jpeg_resync_to_restart;
    st->mjpeg_src.term_source         = mjpeg_src_term;
    st->mjpeg_cinfo.src               = &st->mjpeg_src;
}

G_GNUC_INTERNAL
void stream_mjpeg_data(display_stream *st)
{
    gboolean back_compat = st->channel->priv->peer_hdr.major_version == 1;
    int width;
    int height;
    uint8_t *dest;
    uint8_t *lines[4];

    stream_get_dimensions(st, &width, &height);
    dest = malloc(width * height * 4);

    if (st->out_frame) {
        free(st->out_frame);
    }
    st->out_frame = dest;

    jpeg_read_header(&st->mjpeg_cinfo, 1);
#ifdef JCS_EXTENSIONS
    // requires jpeg-turbo
    if (back_compat)
        st->mjpeg_cinfo.out_color_space = JCS_EXT_RGBX;
    else
        st->mjpeg_cinfo.out_color_space = JCS_EXT_BGRX;
#else
#warning "You should consider building with libjpeg-turbo"
    st->mjpeg_cinfo.out_color_space = JCS_RGB;
#endif

#ifndef SPICE_QUALITY
    st->mjpeg_cinfo.dct_method = JDCT_IFAST;
    st->mjpeg_cinfo.do_fancy_upsampling = FALSE;
    st->mjpeg_cinfo.do_block_smoothing = FALSE;
    st->mjpeg_cinfo.dither_mode = JDITHER_ORDERED;
#endif
    // TODO: in theory should check cinfo.output_height match with our height
    jpeg_start_decompress(&st->mjpeg_cinfo);
    /* rec_outbuf_height is the recommended size of the output buffer we
     * pass to libjpeg for optimum performance
     */
    if (st->mjpeg_cinfo.rec_outbuf_height > G_N_ELEMENTS(lines)) {
        jpeg_abort_decompress(&st->mjpeg_cinfo);
        g_return_if_reached();
    }

    while (st->mjpeg_cinfo.output_scanline < st->mjpeg_cinfo.output_height) {
        /* only used when JCS_EXTENSIONS is undefined */
        G_GNUC_UNUSED unsigned int lines_read;

        for (unsigned int j = 0; j < st->mjpeg_cinfo.rec_outbuf_height; j++) {
            lines[j] = dest;
#ifdef JCS_EXTENSIONS
            dest += 4 * width;
#else
            dest += 3 * width;
#endif
        }
        lines_read = jpeg_read_scanlines(&st->mjpeg_cinfo, lines,
                                st->mjpeg_cinfo.rec_outbuf_height);
#ifndef JCS_EXTENSIONS
        {
            uint8_t *s = lines[0];
            uint32_t *d = (uint32_t *)s;

            if (back_compat) {
                for (unsigned int j = lines_read * width; j > 0; ) {
                    j -= 1; // reverse order, bad for cache?
                    d[j] = s[j * 3 + 0] |
                        s[j * 3 + 1] << 8 |
                        s[j * 3 + 2] << 16;
                }
            } else {
                for (unsigned int j = lines_read * width; j > 0; ) {
                    j -= 1; // reverse order, bad for cache?
                    d[j] = s[j * 3 + 0] << 16 |
                        s[j * 3 + 1] << 8 |
                        s[j * 3 + 2];
                }
            }
        }
#endif
        dest = &st->out_frame[st->mjpeg_cinfo.output_scanline * width * 4];
    }
    jpeg_finish_decompress(&st->mjpeg_cinfo);
}

G_GNUC_INTERNAL
void stream_mjpeg_cleanup(display_stream *st)
{
    jpeg_destroy_decompress(&st->mjpeg_cinfo);
    free(st->out_frame);
    st->out_frame = NULL;
}
