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
#include <celt051/celt.h>

#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"
#include "spice-session-priv.h"

#include "spice-marshal.h"

/**
 * SECTION:channel-playback
 * @short_description: audio stream for playback
 * @title: Playback Channel
 * @section_id:
 * @see_also: #SpiceChannel, and #SpiceAudio
 * @stability: Stable
 * @include: channel-playback.h
 *
 * #SpicePlaybackChannel class handles an audio playback stream. The
 * audio data is received via #SpicePlaybackChannel::playback-data
 * signal, and is controlled by the guest with
 * #SpicePlaybackChannel::playback-stop and
 * #SpicePlaybackChannel::playback-start signal events.
 *
 * Note: You may be interested to let the #SpiceAudio class play and
 * record audio channels for your application.
 */

#define SPICE_PLAYBACK_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_PLAYBACK_CHANNEL, SpicePlaybackChannelPrivate))

struct _SpicePlaybackChannelPrivate {
    int                         mode;
    CELTMode                    *celt_mode;
    CELTDecoder                 *celt_decoder;
    guint32                     frame_count;
    guint32                     last_time;
    guint8                      nchannels;
    guint16                     *volume;
    guint8                      mute;
};

G_DEFINE_TYPE(SpicePlaybackChannel, spice_playback_channel, SPICE_TYPE_CHANNEL)

/* Properties */
enum {
    PROP_0,
    PROP_NCHANNELS,
    PROP_VOLUME,
    PROP_MUTE,
};

/* Signals */
enum {
    SPICE_PLAYBACK_START,
    SPICE_PLAYBACK_DATA,
    SPICE_PLAYBACK_STOP,
    SPICE_PLAYBACK_GET_DELAY,

    SPICE_PLAYBACK_LAST_SIGNAL,
};

static guint signals[SPICE_PLAYBACK_LAST_SIGNAL];

static void spice_playback_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg);

/* ------------------------------------------------------------------ */

static void spice_playback_channel_init(SpicePlaybackChannel *channel)
{
    channel->priv = SPICE_PLAYBACK_CHANNEL_GET_PRIVATE(channel);

    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_PLAYBACK_CAP_CELT_0_5_1);
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_PLAYBACK_CAP_VOLUME);
}

static void spice_playback_channel_finalize(GObject *obj)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(obj)->priv;

    if (c->celt_decoder) {
        celt_decoder_destroy(c->celt_decoder);
        c->celt_decoder = NULL;
    }

    if (c->celt_mode) {
        celt_mode_destroy(c->celt_mode);
        c->celt_mode = NULL;
    }

    g_free(c->volume);
    c->volume = NULL;

    if (G_OBJECT_CLASS(spice_playback_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_playback_channel_parent_class)->finalize(obj);
}

static void spice_playback_channel_get_property(GObject    *gobject,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
    SpicePlaybackChannel *channel = SPICE_PLAYBACK_CHANNEL(gobject);
    SpicePlaybackChannelPrivate *c = channel->priv;

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

static void spice_playback_channel_set_property(GObject      *gobject,
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

/* main or coroutine context */
static void spice_playback_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;

    if (c->celt_decoder) {
        celt_decoder_destroy(c->celt_decoder);
        c->celt_decoder = NULL;
    }

    if (c->celt_mode) {
        celt_mode_destroy(c->celt_mode);
        c->celt_mode = NULL;
    }

    SPICE_CHANNEL_CLASS(spice_playback_channel_parent_class)->channel_reset(channel, migrating);
}

static void spice_playback_channel_class_init(SpicePlaybackChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_playback_channel_finalize;
    gobject_class->get_property = spice_playback_channel_get_property;
    gobject_class->set_property = spice_playback_channel_set_property;

    channel_class->handle_msg   = spice_playback_handle_msg;
    channel_class->channel_reset = spice_playback_channel_reset;

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
                              "Playback volume",
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
     * SpicePlaybackChannel::playback-start:
     * @channel: the #SpicePlaybackChannel that emitted the signal
     * @format: a #SPICE_AUDIO_FMT
     * @channels: number of channels
     * @rate: audio rate
     *
     * Notify when the playback should start, and provide audio format
     * characteristics.
     **/
    signals[SPICE_PLAYBACK_START] =
        g_signal_new("playback-start",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, playback_start),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT_INT,
                     G_TYPE_NONE,
                     3,
                     G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    /**
     * SpicePlaybackChannel::playback-data:
     * @channel: the #SpicePlaybackChannel that emitted the signal
     * @data: pointer to audio data
     * @data_size: size in byte of @data
     *
     * Provide audio data to be played.
     **/
    signals[SPICE_PLAYBACK_DATA] =
        g_signal_new("playback-data",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, playback_data),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__POINTER_INT,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_POINTER, G_TYPE_INT);

    /**
     * SpicePlaybackChannel::playback-stop:
     * @channel: the #SpicePlaybackChannel that emitted the signal
     *
     * Notify when the playback should stop.
     **/
    signals[SPICE_PLAYBACK_STOP] =
        g_signal_new("playback-stop",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, playback_stop),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /**
     * SpicePlaybackChannel::playback-get-delay:
     * @channel: the #SpicePlaybackChannel that emitted the signal
     *
     * Notify when the current playback delay is requested
     **/
    signals[SPICE_PLAYBACK_GET_DELAY] =
        g_signal_new("playback-get-delay",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(SpicePlaybackChannelPrivate));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_PLAYBACK_START {
    gint format;
    gint channels;
    gint frequency;
};

struct SPICE_PLAYBACK_DATA {
    uint8_t *data;
    gsize data_size;
};

struct SPICE_PLAYBACK_STOP {
};

struct SPICE_PLAYBACK_GET_DELAY {
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_PLAYBACK_GET_DELAY:
    case SPICE_PLAYBACK_STOP: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    case SPICE_PLAYBACK_START: {
        struct SPICE_PLAYBACK_START *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->format, p->channels, p->frequency);
        break;
    }
    case SPICE_PLAYBACK_DATA: {
        struct SPICE_PLAYBACK_DATA *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->data, p->data_size);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* ------------------------------------------------------------------ */

/* coroutine context */
static void playback_handle_data(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackPacket *packet = spice_msg_in_parsed(in);

#ifdef DEBUG
    SPICE_DEBUG("%s: time %d data %p size %d", __FUNCTION__,
            packet->time, packet->data, packet->data_size);
#endif

    if (c->last_time > packet->time)
        g_warn_if_reached();

    c->last_time = packet->time;

    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
        emit_main_context(channel, SPICE_PLAYBACK_DATA,
                          packet->data, packet->data_size);
        break;
    case SPICE_AUDIO_DATA_MODE_CELT_0_5_1: {
        celt_int16_t pcm[256 * 2];

        g_return_if_fail(c->celt_decoder != NULL);

        if (celt_decode(c->celt_decoder, packet->data,
                           packet->data_size, pcm) != CELT_OK) {
            g_warning("celt_decode() error");
            return;
        }

        emit_main_context(channel, SPICE_PLAYBACK_DATA,
                          (uint8_t *)pcm, sizeof(pcm));
        break;
    }
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }

    if ((c->frame_count++ % 100) == 0) {
        emit_main_context(channel, SPICE_PLAYBACK_GET_DELAY);
    }
}

/* coroutine context */
static void playback_handle_mode(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackMode *mode = spice_msg_in_parsed(in);

    SPICE_DEBUG("%s: time %d mode %d data %p size %d", __FUNCTION__,
            mode->time, mode->mode, mode->data, mode->data_size);

    c->mode = mode->mode;
    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
    case SPICE_AUDIO_DATA_MODE_CELT_0_5_1:
        break;
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }
}

/* coroutine context */
static void playback_handle_start(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackStart *start = spice_msg_in_parsed(in);
    int celt_mode_err;

    SPICE_DEBUG("%s: fmt %d channels %d freq %d time %d", __FUNCTION__,
            start->format, start->channels, start->frequency, start->time);

    c->frame_count = 0;
    c->last_time = start->time;

    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
        emit_main_context(channel, SPICE_PLAYBACK_START,
                          start->format, start->channels, start->frequency);
        break;
    case SPICE_AUDIO_DATA_MODE_CELT_0_5_1: {
        /* TODO: only support one setting now */
        int frame_size = 256;
        if (!c->celt_mode)
            c->celt_mode = celt_mode_create(start->frequency, start->channels,
                                               frame_size, &celt_mode_err);
        if (!c->celt_mode)
            g_warning("create celt mode failed %d", celt_mode_err);

        if (!c->celt_decoder)
            c->celt_decoder = celt_decoder_create(c->celt_mode);

        if (!c->celt_decoder)
            g_warning("create celt decoder failed");

        emit_main_context(channel, SPICE_PLAYBACK_START,
                          start->format, start->channels, start->frequency);
        break;
    }
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }
}

/* coroutine context */
static void playback_handle_stop(SpiceChannel *channel, SpiceMsgIn *in)
{
    emit_main_context(channel, SPICE_PLAYBACK_STOP);
}

/* coroutine context */
static void playback_handle_set_volume(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgAudioVolume *vol = spice_msg_in_parsed(in);

    g_free(c->volume);
    c->nchannels = vol->nchannels;
    c->volume = g_new(guint16, c->nchannels);
    memcpy(c->volume, vol->volume, sizeof(guint16) * c->nchannels);
    g_object_notify_main_context(G_OBJECT(channel), "volume");
}

/* coroutine context */
static void playback_handle_set_mute(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpicePlaybackChannelPrivate *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgAudioMute *m = spice_msg_in_parsed(in);

    c->mute = m->mute;
    g_object_notify_main_context(G_OBJECT(channel), "mute");
}

static const spice_msg_handler playback_handlers[] = {
    [ SPICE_MSG_PLAYBACK_DATA ]            = playback_handle_data,
    [ SPICE_MSG_PLAYBACK_MODE ]            = playback_handle_mode,
    [ SPICE_MSG_PLAYBACK_START ]           = playback_handle_start,
    [ SPICE_MSG_PLAYBACK_STOP ]            = playback_handle_stop,
    [ SPICE_MSG_PLAYBACK_VOLUME ]          = playback_handle_set_volume,
    [ SPICE_MSG_PLAYBACK_MUTE ]            = playback_handle_set_mute,
};

/* coroutine context */
static void spice_playback_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg)
{
    int type = spice_msg_in_type(msg);
    SpiceChannelClass *parent_class;

    g_return_if_fail(type < SPICE_N_ELEMENTS(playback_handlers));

    parent_class = SPICE_CHANNEL_CLASS(spice_playback_channel_parent_class);

    if (playback_handlers[type] != NULL)
        playback_handlers[type](channel, msg);
    else if (parent_class->handle_msg)
        parent_class->handle_msg(channel, msg);
    else
        g_return_if_reached();
}

void spice_playback_channel_set_delay(SpicePlaybackChannel *channel, guint32 delay_ms)
{
    SpicePlaybackChannelPrivate *c;

    g_return_if_fail(SPICE_IS_PLAYBACK_CHANNEL(channel));

    SPICE_DEBUG("playback set_delay %d ms", delay_ms);

    c = channel->priv;
    spice_session_set_mm_time(spice_channel_get_session(SPICE_CHANNEL(channel)),
                              c->last_time - delay_ms);
}
