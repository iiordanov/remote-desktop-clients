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

#include "spice-marshal.h"
#include "spice-session-priv.h"

#include "common/snd_codec.h"

/**
 * SECTION:channel-record
 * @short_description: audio stream for recording
 * @title: Record Channel
 * @section_id:
 * @see_also: #SpiceChannel, and #SpiceAudio
 * @stability: Stable
 * @include: channel-record.h
 *
 * #SpiceRecordChannel class handles an audio recording stream. The
 * audio stream should start when #SpiceRecordChannel::record-start is
 * emitted and should be stopped when #SpiceRecordChannel::record-stop
 * is received.
 *
 * The audio is sent to the guest by calling spice_record_send_data()
 * with the recorded PCM data.
 *
 * Note: You may be interested to let the #SpiceAudio class play and
 * record audio channels for your application.
 */

#define SPICE_RECORD_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_RECORD_CHANNEL, SpiceRecordChannelPrivate))

struct _SpiceRecordChannelPrivate {
    int                         mode;
    gboolean                    started;
    SndCodec                    codec;
    gsize                       frame_bytes;
    guint8                      *last_frame;
    gsize                       last_frame_current;
    guint8                      nchannels;
    guint16                     *volume;
    guint8                      mute;
};

G_DEFINE_TYPE(SpiceRecordChannel, spice_record_channel, SPICE_TYPE_CHANNEL)

/* Properties */
enum {
    PROP_0,
    PROP_NCHANNELS,
    PROP_VOLUME,
    PROP_MUTE,
};

/* Signals */
enum {
    SPICE_RECORD_START,
    SPICE_RECORD_STOP,

    SPICE_RECORD_LAST_SIGNAL,
};

static guint signals[SPICE_RECORD_LAST_SIGNAL];

static void channel_set_handlers(SpiceChannelClass *klass);

/* ------------------------------------------------------------------ */

static void spice_record_channel_reset_capabilities(SpiceChannel *channel)
{
    if (!g_getenv("SPICE_DISABLE_CELT"))
        if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, SND_CODEC_ANY_FREQUENCY))
            spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_RECORD_CAP_CELT_0_5_1);
    if (!g_getenv("SPICE_DISABLE_OPUS"))
        if (snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, SND_CODEC_ANY_FREQUENCY))
            spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_RECORD_CAP_OPUS);
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_RECORD_CAP_VOLUME);
}

static void spice_record_channel_init(SpiceRecordChannel *channel)
{
    channel->priv = SPICE_RECORD_CHANNEL_GET_PRIVATE(channel);

    spice_record_channel_reset_capabilities(SPICE_CHANNEL(channel));
}

static void spice_record_channel_finalize(GObject *obj)
{
    SpiceRecordChannelPrivate *c = SPICE_RECORD_CHANNEL(obj)->priv;

    g_free(c->last_frame);
    c->last_frame = NULL;

    snd_codec_destroy(&c->codec);

    g_free(c->volume);
    c->volume = NULL;

    if (G_OBJECT_CLASS(spice_record_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_record_channel_parent_class)->finalize(obj);
}

static void spice_record_channel_get_property(GObject    *gobject,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
    SpiceRecordChannel *channel = SPICE_RECORD_CHANNEL(gobject);
    SpiceRecordChannelPrivate *c = channel->priv;

    switch (prop_id) {
    case PROP_VOLUME:
        g_value_set_pointer(value, c->volume);
        break;
    case PROP_NCHANNELS:
        g_value_set_uint(value, c->nchannels);
        break;
    case PROP_MUTE:
        g_value_set_boolean(value, c->mute);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_record_channel_set_property(GObject      *gobject,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_VOLUME:
        /* TODO: request guest volume change */
        break;
    case PROP_MUTE:
        /* TODO: request guest mute change */
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceRecordChannelPrivate *c = SPICE_RECORD_CHANNEL(channel)->priv;

    g_free(c->last_frame);
    c->last_frame = NULL;

    snd_codec_destroy(&c->codec);

    SPICE_CHANNEL_CLASS(spice_record_channel_parent_class)->channel_reset(channel, migrating);
}

static void spice_record_channel_class_init(SpiceRecordChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_record_channel_finalize;
    gobject_class->get_property = spice_record_channel_get_property;
    gobject_class->set_property = spice_record_channel_set_property;
    channel_class->channel_reset = channel_reset;
    channel_class->channel_reset_capabilities = spice_record_channel_reset_capabilities;

    g_object_class_install_property
        (gobject_class, PROP_NCHANNELS,
         g_param_spec_uint("nchannels",
                           "Number of Channels",
                           "Number of Channels",
                           0, G_MAXUINT8, 2,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_VOLUME,
         g_param_spec_pointer("volume",
                              "Playback volume",
                              "",
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_MUTE,
         g_param_spec_boolean("mute",
                              "Mute",
                              "Mute",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    /**
     * SpiceRecordChannel::record-start:
     * @channel: the #SpiceRecordChannel that emitted the signal
     * @format: a #SPICE_AUDIO_FMT
     * @channels: number of channels
     * @rate: audio rate
     *
     * Notify when the recording should start, and provide audio format
     * characteristics.
     **/
    signals[SPICE_RECORD_START] =
        g_signal_new("record-start",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceRecordChannelClass, record_start),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT_INT,
                     G_TYPE_NONE,
                     3,
                     G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    /**
     * SpiceRecordChannel::record-stop:
     * @channel: the #SpiceRecordChannel that emitted the signal
     *
     * Notify when the recording should stop.
     **/
    signals[SPICE_RECORD_STOP] =
        g_signal_new("record-stop",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceRecordChannelClass, record_stop),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(SpiceRecordChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_RECORD_START {
    gint format;
    gint channels;
    gint frequency;
};

struct SPICE_RECORD_STOP {
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_RECORD_START: {
        struct SPICE_RECORD_START *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->format, p->channels, p->frequency);
        break;
    }
    case SPICE_RECORD_STOP: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* main context */
static void spice_record_mode(SpiceRecordChannel *channel, uint32_t time,
                              uint32_t mode, uint8_t *data, uint32_t data_size)
{
    SpiceMsgcRecordMode m = {0, };
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    m.mode = mode;
    m.time = time;
    m.data = data;
    m.data_size = data_size;

    msg = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_RECORD_MODE);
    msg->marshallers->msgc_record_mode(msg->marshaller, &m);
    spice_msg_out_send(msg);
}

static int spice_record_desired_mode(SpiceChannel *channel, int frequency)
{
    if (!g_getenv("SPICE_DISABLE_OPUS") &&
        snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_OPUS, frequency) &&
        spice_channel_test_capability(channel, SPICE_RECORD_CAP_OPUS)) {
        return SPICE_AUDIO_DATA_MODE_OPUS;
    } else if (!g_getenv("SPICE_DISABLE_CELT") &&
        snd_codec_is_capable(SPICE_AUDIO_DATA_MODE_CELT_0_5_1, frequency) &&
        spice_channel_test_capability(channel, SPICE_RECORD_CAP_CELT_0_5_1)) {
        return SPICE_AUDIO_DATA_MODE_CELT_0_5_1;
    } else {
        return SPICE_AUDIO_DATA_MODE_RAW;
    }
}

/* main context */
static void spice_record_start_mark(SpiceRecordChannel *channel, uint32_t time)
{
    SpiceMsgcRecordStartMark m = {0, };
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    m.time = time;

    msg = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_RECORD_START_MARK);
    msg->marshallers->msgc_record_start_mark(msg->marshaller, &m);
    spice_msg_out_send(msg);
}

/**
 * spice_record_send_data:
 * @channel:
 * @data: PCM data
 * @bytes: size of @data
 * @time: stream timestamp
 *
 * Send recorded PCM data to the guest.
 **/
void spice_record_send_data(SpiceRecordChannel *channel, gpointer data,
                            gsize bytes, uint32_t time)
{
    SpiceRecordChannelPrivate *rc;
    SpiceMsgcRecordPacket p = {0, };

    g_return_if_fail(channel != NULL);
    g_return_if_fail(spice_channel_get_read_only(SPICE_CHANNEL(channel)) == FALSE);

    uint8_t *encode_buf = NULL;

    rc = channel->priv;

    if (!rc->started) {
        spice_record_mode(channel, time, rc->mode, NULL, 0);
        spice_record_start_mark(channel, time);
        rc->started = TRUE;
    }

    if (rc->mode != SPICE_AUDIO_DATA_MODE_RAW)
        encode_buf = g_alloca(SND_CODEC_MAX_COMPRESSED_BYTES);

    p.time = time;

    while (bytes > 0) {
        gsize n;
        int frame_size;
        SpiceMsgOut *msg;
        uint8_t *frame;

        if (rc->last_frame_current > 0) {
            /* complete previous frame */
            n = MIN(bytes, rc->frame_bytes - rc->last_frame_current);
            memcpy(rc->last_frame + rc->last_frame_current, data, n);
            rc->last_frame_current += n;
            if (rc->last_frame_current < rc->frame_bytes)
                /* if the frame is still incomplete, return */
                break;
            frame = rc->last_frame;
            frame_size = rc->frame_bytes;
        } else {
            n = MIN(bytes, rc->frame_bytes);
            frame_size = n;
            frame = data;
        }

        if (rc->last_frame_current == 0 &&
            n < rc->frame_bytes) {
            /* start a new frame */
            memcpy(rc->last_frame, data, n);
            rc->last_frame_current = n;
            break;
        }

        if (rc->mode != SPICE_AUDIO_DATA_MODE_RAW) {
            int len = SND_CODEC_MAX_COMPRESSED_BYTES;
            if (snd_codec_encode(rc->codec, frame, frame_size, encode_buf, &len) != SND_CODEC_OK) {
                g_warning("encode failed");
                return;
            }
            frame = encode_buf;
            frame_size = len;
        }

        msg = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_RECORD_DATA);
        msg->marshallers->msgc_record_data(msg->marshaller, &p);
        spice_marshaller_add(msg->marshaller, frame, frame_size);
        spice_msg_out_send(msg);

        if (rc->last_frame_current == rc->frame_bytes)
            rc->last_frame_current = 0;

        bytes -= n;
        data = (guint8*)data + n;
    }
}

/* ------------------------------------------------------------------ */

/* coroutine context */
static void record_handle_start(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceRecordChannelPrivate *c = SPICE_RECORD_CHANNEL(channel)->priv;
    SpiceMsgRecordStart *start = spice_msg_in_parsed(in);
    int frame_size = SND_CODEC_MAX_FRAME_SIZE;

    c->mode = spice_record_desired_mode(channel, start->frequency);

    CHANNEL_DEBUG(channel, "%s: fmt %d channels %d freq %d", __FUNCTION__,
                  start->format, start->channels, start->frequency);

    g_return_if_fail(start->format == SPICE_AUDIO_FMT_S16);

    c->codec = NULL;

    if (c->mode != SPICE_AUDIO_DATA_MODE_RAW) {
        if (snd_codec_create(&c->codec, c->mode, start->frequency, SND_CODEC_ENCODE) != SND_CODEC_OK) {
            g_warning("Failed to create encoder");
            return;
        }
        frame_size = snd_codec_frame_size(c->codec);
    }

    g_free(c->last_frame);
    c->frame_bytes = frame_size * 16 * start->channels / 8;
    c->last_frame = g_malloc(c->frame_bytes);
    c->last_frame_current = 0;

    emit_main_context(channel, SPICE_RECORD_START,
                      start->format, start->channels, start->frequency);
}

/* coroutine context */
static void record_handle_stop(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceRecordChannelPrivate *rc = SPICE_RECORD_CHANNEL(channel)->priv;

    emit_main_context(channel, SPICE_RECORD_STOP);
    rc->started = FALSE;
}

/* coroutine context */
static void record_handle_set_volume(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceRecordChannelPrivate *c = SPICE_RECORD_CHANNEL(channel)->priv;
    SpiceMsgAudioVolume *vol = spice_msg_in_parsed(in);

    if (vol->nchannels == 0) {
        g_warning("spice-server send audio-volume-msg with 0 channels");
        return;
    }

    g_free(c->volume);
    c->nchannels = vol->nchannels;
    c->volume = g_new(guint16, c->nchannels);
    memcpy(c->volume, vol->volume, sizeof(guint16) * c->nchannels);
    g_object_notify_main_context(G_OBJECT(channel), "volume");
}

/* coroutine context */
static void record_handle_set_mute(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceRecordChannelPrivate *c = SPICE_RECORD_CHANNEL(channel)->priv;
    SpiceMsgAudioMute *m = spice_msg_in_parsed(in);

    c->mute = m->mute;
    g_object_notify_main_context(G_OBJECT(channel), "mute");
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_RECORD_START ]             = record_handle_start,
        [ SPICE_MSG_RECORD_STOP ]              = record_handle_stop,
        [ SPICE_MSG_RECORD_VOLUME ]            = record_handle_set_volume,
        [ SPICE_MSG_RECORD_MUTE ]              = record_handle_set_mute,
    };

    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}
