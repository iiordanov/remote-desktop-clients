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
#include "spice-pulse.h"
#include "spice-common.h"
#include "spice-session-priv.h"
#include "spice-channel-priv.h"
#include "spice-util-priv.h"

#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>

#define SPICE_PULSE_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_PULSE, SpicePulsePrivate))

struct stream {
    pa_sample_spec          spec;
    pa_stream               *stream;
    int                     state;
    pa_operation            *uncork_op;
    pa_operation            *cork_op;
    gboolean                started;
    guint                   num_underflow;
};

struct _SpicePulsePrivate {
    SpiceChannel            *pchannel;
    SpiceChannel            *rchannel;

    pa_glib_mainloop        *mainloop;
    pa_context              *context;
    int                     state;
    struct stream           playback;
    struct stream           record;
    guint                   last_delay;
    guint                   target_delay;
};

G_DEFINE_TYPE(SpicePulse, spice_pulse, SPICE_TYPE_AUDIO)

static const char *stream_state_names[] = {
    [ PA_STREAM_UNCONNECTED ] = "unconnected",
    [ PA_STREAM_CREATING    ] = "creating",
    [ PA_STREAM_READY       ] = "ready",
    [ PA_STREAM_FAILED      ] = "failed",
    [ PA_STREAM_TERMINATED  ] = "terminated",
};

static const char *context_state_names[] = {
    [ PA_CONTEXT_UNCONNECTED  ] = "unconnected",
    [ PA_CONTEXT_CONNECTING   ] = "connecting",
    [ PA_CONTEXT_AUTHORIZING  ] = "authorizing",
    [ PA_CONTEXT_SETTING_NAME ] = "setting_name",
    [ PA_CONTEXT_READY        ] = "ready",
    [ PA_CONTEXT_FAILED       ] = "failed",
    [ PA_CONTEXT_TERMINATED   ] = "terminated",
};
#define STATE_NAME(array, state) \
    ((state < G_N_ELEMENTS(array)) ? array[state] : NULL)

static void channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                          gpointer data);
static void stream_stop(SpicePulse *pulse, struct stream *s);
static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel);

static void spice_pulse_finalize(GObject *obj)
{
    SpicePulsePrivate *p;

    p = SPICE_PULSE_GET_PRIVATE(obj);

    if (p->context != NULL)
        pa_context_unref(p->context);

    if (p->mainloop != NULL)
        pa_glib_mainloop_free(p->mainloop);

    G_OBJECT_CLASS(spice_pulse_parent_class)->finalize(obj);
}

static void spice_pulse_dispose(GObject *obj)
{
    SpicePulsePrivate *p;

    SPICE_DEBUG("%s", __FUNCTION__);
    p = SPICE_PULSE_GET_PRIVATE(obj);

    if (p->playback.uncork_op)
        pa_operation_unref(p->playback.uncork_op);
    p->playback.uncork_op = NULL;

    if (p->playback.cork_op)
        pa_operation_unref(p->playback.cork_op);
    p->playback.cork_op = NULL;

    if (p->record.uncork_op)
        pa_operation_unref(p->record.uncork_op);
    p->record.uncork_op = NULL;

    if (p->record.cork_op)
        pa_operation_unref(p->record.cork_op);
    p->record.cork_op = NULL;

    if (p->pchannel)
        g_object_unref(p->pchannel);
    p->pchannel = NULL;

    if (p->rchannel)
        g_object_unref(p->rchannel);
    p->rchannel = NULL;

    G_OBJECT_CLASS(spice_pulse_parent_class)->dispose(obj);
}

static void spice_pulse_init(SpicePulse *pulse)
{
    pulse->priv = SPICE_PULSE_GET_PRIVATE(pulse);
}

static void spice_pulse_class_init(SpicePulseClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceAudioClass *audio_class = SPICE_AUDIO_CLASS(klass);

    audio_class->connect_channel = connect_channel;

    gobject_class->finalize = spice_pulse_finalize;
    gobject_class->dispose = spice_pulse_dispose;

    g_type_class_add_private(klass, sizeof(SpicePulsePrivate));
}

/* ------------------------------------------------------------------ */
static void pulse_uncork_cb(pa_stream *pastream, int success, void *data)
{
    struct stream *s = data;

    if (!success)
        g_warning("pulseaudio uncork operation failed");

    pa_operation_unref(s->uncork_op);
    s->uncork_op = NULL;
}

static void stream_uncork(SpicePulse *pulse, struct stream *s)
{
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_operation *o = NULL;

    g_return_if_fail(s->stream);

    if (s->cork_op) {
        pa_operation_cancel(s->cork_op);
        pa_operation_unref(s->cork_op);
        s->cork_op = NULL;
    }

    if (pa_stream_is_corked(s->stream) && !s->uncork_op) {
        if (!(o = pa_stream_cork(s->stream, 0, pulse_uncork_cb, s))) {
            g_warning("pa_stream_uncork() failed: %s",
                      pa_strerror(pa_context_errno(p->context)));
        }
        s->uncork_op = o;
    }
}

static void pulse_flush_cb(pa_stream *pastream, int success, void *data)
{
    struct stream *s = data;

    if (!success)
        g_warning("pulseaudio flush operation failed");

    pa_operation_unref(s->cork_op);
    s->cork_op = NULL;
}

static void pulse_cork_flush_cb(pa_stream *pastream, int success, void *data)
{
    struct stream *s = data;

    if (!success)
        g_warning("pulseaudio cork operation failed");

    pa_operation_unref(s->cork_op);

    if (!(s->cork_op = pa_stream_flush(s->stream, pulse_flush_cb, s))) {
        g_warning("pa_stream_flush() failed");
    }
}

static void pulse_cork_cb(pa_stream *pastream, int success, void *data)
{
    struct stream *s = data;

    SPICE_DEBUG("%s: cork started", __FUNCTION__);
    if (!success)
        g_warning("pulseaudio cork operation failed");

    pa_operation_unref(s->cork_op);
    s->cork_op = NULL;
}

static void stream_cork(SpicePulse *pulse, struct stream *s, gboolean with_flush)
{
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_operation *o = NULL;

    if (s->uncork_op) {
        pa_operation_cancel(s->uncork_op);
        pa_operation_unref(s->uncork_op);
        s->uncork_op = NULL;
    }

    if (!pa_stream_is_corked(s->stream) && !s->cork_op) {
        if (!(o = pa_stream_cork(s->stream, 1,
                                 with_flush ? pulse_cork_flush_cb :
                                              pulse_cork_cb,
                                 s))) {
            g_warning("pa_stream_cork() failed: %s",
                      pa_strerror(pa_context_errno(p->context)));
        }
        s->cork_op = o;
    }
}

static void stream_stop(SpicePulse *pulse, struct stream *s)
{
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);

    if (pa_stream_disconnect(s->stream) < 0) {
        g_warning("pa_stream_disconnect() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    }
    pa_stream_unref(s->stream);
    s->stream = NULL;
}

static void stream_state_callback(pa_stream *s, void *userdata)
{
    SpicePulsePrivate *p;
    p = SPICE_PULSE_GET_PRIVATE(userdata);

    g_return_if_fail(p != NULL);
    g_return_if_fail(s != NULL);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
        case PA_STREAM_READY:
            break;
        case PA_STREAM_FAILED:
        default:
            g_warning("Stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
    }
}

static void stream_underflow_cb(pa_stream *s, void *userdata)
{
    SpicePulsePrivate *p;

    SPICE_DEBUG("PA stream underflow!!");

    p = SPICE_PULSE_GET_PRIVATE(userdata);
    g_return_if_fail(p != NULL);
    p->playback.num_underflow++;
#ifdef PULSE_ADJUST_LATENCY
    const pa_buffer_attr *buffer_attr;
    pa_buffer_attr new_buffer_attr;
    pa_operation *op;

    buffer_attr = pa_stream_get_buffer_attr(s);
    g_return_if_fail(buffer_attr != NULL);

    new_buffer_attr = *buffer_attr;
    new_buffer_attr.tlength *= 2;
    new_buffer_attr.minreq *= 2;
    op = pa_stream_set_buffer_attr(s, &new_buffer_attr, NULL, NULL);
    pa_operation_unref(op);
#endif
}

static void stream_update_latency_callback(pa_stream *s, void *userdata)
{
    SpicePulse *pulse = userdata;
    pa_usec_t usec;
    int negative = 0;
    SpicePulsePrivate *p;

    p = SPICE_PULSE_GET_PRIVATE(pulse);

    g_return_if_fail(s != NULL);
    g_return_if_fail(p != NULL);

    if (!p->playback.stream || !p->playback.started)
        return;

    if (pa_stream_get_latency(s, &usec, &negative) < 0) {
        g_warning("Failed to get latency: %s", pa_strerror(pa_context_errno(p->context)));
        return;
    }

    g_return_if_fail(negative == FALSE);
    p->last_delay = usec / PA_USEC_PER_MSEC;
    spice_playback_channel_set_delay(SPICE_PLAYBACK_CHANNEL(p->pchannel), usec / 1000);
    if (pa_stream_is_corked(p->playback.stream)) {
        if (p->last_delay >= p->target_delay) {
            SPICE_DEBUG("%s: uncork playback. delay %u target %u",  __FUNCTION__, p->last_delay, p->target_delay);
            stream_uncork(pulse, &p->playback);
        } else {
            SPICE_DEBUG("%s: still corked. delay %u target %u",  __FUNCTION__, p->last_delay, p->target_delay);
        }
    }
}

static void create_playback(SpicePulse *pulse)
{
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_stream_flags_t flags;
    pa_buffer_attr buffer_attr = { 0, };

    g_return_if_fail(p != NULL);
    g_return_if_fail(p->context != NULL);
    g_return_if_fail(p->playback.stream == NULL);
    g_return_if_fail(pa_context_get_state(p->context) == PA_CONTEXT_READY);

    p->playback.state = PA_STREAM_READY;
    p->playback.stream = pa_stream_new(p->context, "playback",
                                       &p->playback.spec, NULL);
    pa_stream_set_state_callback(p->playback.stream, stream_state_callback, pulse);
    pa_stream_set_underflow_callback(p->playback.stream, stream_underflow_cb, pulse);
    pa_stream_set_latency_update_callback(p->playback.stream, stream_update_latency_callback, pulse);

    buffer_attr.maxlength = -1;
    buffer_attr.tlength = pa_usec_to_bytes(p->target_delay * PA_USEC_PER_MSEC, &p->playback.spec);
    buffer_attr.prebuf = -1;
    buffer_attr.minreq = -1;
    flags = PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE;

    if (pa_stream_connect_playback(p->playback.stream,
                                   NULL, &buffer_attr, flags, NULL, NULL) < 0) {
        g_warning("pa_stream_connect_playback() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    }
}

static void playback_start(SpicePlaybackChannel *channel, gint format, gint channels,
                           gint frequency, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_context_state_t state;
    guint latency;

    g_return_if_fail(p != NULL);

    p->playback.started = TRUE;
    p->playback.num_underflow = 0;
    g_object_get(p->pchannel, "min-latency", &latency, NULL);

    if (p->playback.stream &&
        (p->playback.spec.rate != frequency ||
         p->playback.spec.channels != channels ||
         p->target_delay != latency)) {
        stream_stop(pulse, &p->playback);
    }

    g_return_if_fail(format == SPICE_AUDIO_FMT_S16);
    p->playback.spec.format   = PA_SAMPLE_S16LE;
    p->playback.spec.rate     = frequency;
    p->playback.spec.channels = channels;
    p->target_delay = latency;
    p->last_delay = 0;

    state = pa_context_get_state(p->context);
    switch (state) {
    case PA_CONTEXT_READY:
        if (p->state != state) {
            SPICE_DEBUG("%s: pulse context ready", __FUNCTION__);
        }
        if (p->playback.stream == NULL) {
            create_playback(pulse);
        } else
            stream_uncork(pulse, &p->playback);
        break;
    default:
        if (p->state != state) {
            SPICE_DEBUG("%s: pulse context not ready (%s)",
                        __FUNCTION__, STATE_NAME(context_state_names, state));
        }
        break;
    }
    p->state = state;
}

static void playback_data(SpicePlaybackChannel *channel,
                          gpointer *audio, gint size,
                          gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_stream_state_t state;

    if (!p->playback.stream)
        return;

    state = pa_stream_get_state(p->playback.stream);
    switch (state) {
    case PA_STREAM_CREATING:
        SPICE_DEBUG("stream creating, dropping data");
        break;
    case PA_STREAM_READY:
        if (p->playback.state != state) {
            SPICE_DEBUG("%s: pulse playback stream ready", __FUNCTION__);
        }
        if (pa_stream_write(p->playback.stream, audio, size, NULL, 0, PA_SEEK_RELATIVE) < 0) {
            g_warning("pa_stream_write() failed: %s",
                      pa_strerror(pa_context_errno(p->context)));
        }
        break;
    default:
        if (p->playback.state != state) {
            SPICE_DEBUG("%s: pulse playback stream not ready (%s)",
                        __FUNCTION__, STATE_NAME(stream_state_names, state));
        }
        break;
    }
    p->playback.state = state;
}

static void playback_stop(SpicePlaybackChannel *channel, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;

    SPICE_DEBUG("%s: #underflow %u", __FUNCTION__, p->playback.num_underflow);

    p->playback.started = FALSE;
    if (!p->playback.stream)
        return;

    stream_cork(pulse, &p->playback, TRUE);
}

static void stream_read_callback(pa_stream *s, size_t length, void *data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);

    g_return_if_fail(p != NULL);

    while (pa_stream_readable_size(s) > 0) {
        const void *snddata;

        if (pa_stream_peek(s, &snddata, &length) < 0) {
            g_warning("pa_stream_peek() failed: %s",
                      pa_strerror(pa_context_errno(p->context)));
            return;
        }

        g_return_if_fail(snddata);
        g_return_if_fail(length > 0);

        if (p->rchannel != NULL)
            spice_record_send_data(SPICE_RECORD_CHANNEL(p->rchannel),
                                   /* FIXME: server side doesn't care about ts?
                                      what is the unit? ms apparently */
                                   (gpointer)snddata, length, 0);

        if (pa_stream_drop(s) < 0) {
            g_warning("pa_stream_drop() failed: %s",
                      pa_strerror(pa_context_errno(p->context)));
            return;
        }
    }
}

static void create_record(SpicePulse *pulse)
{
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_buffer_attr buffer_attr = { 0, };
    pa_stream_flags_t flags;

    g_return_if_fail(p != NULL);
    g_return_if_fail(p->context != NULL);
    g_return_if_fail(p->record.stream == NULL);
    g_return_if_fail(pa_context_get_state(p->context) == PA_CONTEXT_READY);

    p->record.state = PA_STREAM_READY;
    p->record.stream = pa_stream_new(p->context, "record",
                                     &p->record.spec, NULL);
    pa_stream_set_read_callback(p->record.stream, stream_read_callback, pulse);
    pa_stream_set_state_callback(p->record.stream, stream_state_callback, pulse);

    /* FIXME: we might want customizable latency */
    buffer_attr.maxlength = -1;
    buffer_attr.prebuf = -1;
    buffer_attr.fragsize = buffer_attr.tlength = pa_usec_to_bytes(20 * PA_USEC_PER_MSEC, &p->record.spec);
    buffer_attr.minreq = (uint32_t) -1;
    flags = PA_STREAM_ADJUST_LATENCY;

    if (pa_stream_connect_record(p->record.stream, NULL, &buffer_attr, flags) < 0) {
        g_warning("pa_stream_connect_record() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    }
}

static void record_start(SpiceRecordChannel *channel, gint format, gint channels,
                         gint frequency, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = SPICE_PULSE_GET_PRIVATE(pulse);
    pa_context_state_t state;

    p->record.started = TRUE;

    if (p->record.stream &&
        (p->record.spec.rate != frequency ||
         p->record.spec.channels != channels)) {
        stream_stop(pulse, &p->record);
    }

    g_return_if_fail(format == SPICE_AUDIO_FMT_S16);
    p->record.spec.format = PA_SAMPLE_S16LE;
    p->record.spec.rate = frequency;
    p->record.spec.channels = channels;

    state = pa_context_get_state(p->context);
    switch (state) {
    case PA_CONTEXT_READY:
        if (p->state != state) {
            SPICE_DEBUG("%s: pulse context ready", __FUNCTION__);
        }
        if (p->record.stream == NULL) {
            create_record(pulse);
        } else
            stream_uncork(pulse, &p->record);
        break;
    default:
        if (p->state != state) {
            g_warning("%s: pulse context not ready (%s)",
                      __FUNCTION__, STATE_NAME(context_state_names, state));
        }
        break;
    }
    p->state = state;
}

static void record_stop(SpiceRecordChannel *channel, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;

    SPICE_DEBUG("%s", __FUNCTION__);

    p->record.started = FALSE;
    if (!p->record.stream)
        return;

    stream_stop(pulse, &p->record);
}

static void channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                          gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;

    switch (event) {
    case SPICE_CHANNEL_OPENED:
        break;
    case SPICE_CHANNEL_CLOSED:
        if (channel == p->pchannel) {
            SPICE_DEBUG("playback closed");
            p->pchannel = NULL;
            g_object_unref(channel);
        } else if (channel == p->rchannel) {
            SPICE_DEBUG("record closed");
            record_stop(SPICE_RECORD_CHANNEL(channel), pulse);
            p->rchannel = NULL;
            g_object_unref(channel);
        } else /* if (p->pchannel || p->rchannel) */
            g_warn_if_reached();
        break;
    default:
        break;
    }
}

static void playback_volume_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;
    guint16 *volume;
    guint nchannels;
    pa_operation *op;
    pa_cvolume v;
    guint i;

    g_object_get(object,
                 "volume", &volume,
                 "nchannels", &nchannels,
                 NULL);

    pa_cvolume_init(&v);
    v.channels = p->playback.spec.channels;
    for (i = 0; i < nchannels; ++i) {
        v.values[i] = (PA_VOLUME_NORM - PA_VOLUME_MUTED) * volume[i] / G_MAXUINT16;
        SPICE_DEBUG("playback volume changed %u", v.values[i]);
    }

    if (!p->playback.stream ||
        pa_stream_get_index(p->playback.stream) == PA_INVALID_INDEX)
        return;

    op = pa_context_set_sink_input_volume(p->context,
        pa_stream_get_index(p->playback.stream),
        &v, NULL, NULL);
    if (!op)
        g_warning("set_sink_input_volume() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    else
        pa_operation_unref(op);
}

static void playback_mute_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;
    gboolean mute;
    pa_operation *op;

    g_object_get(object, "mute", &mute, NULL);
    SPICE_DEBUG("playback mute changed %u", mute);

    if (!p->playback.stream ||
        pa_stream_get_index(p->playback.stream) == PA_INVALID_INDEX)
        return;

    op = pa_context_set_sink_input_mute(p->context,
        pa_stream_get_index(p->playback.stream),
        mute, NULL, NULL);
    if (!op)
        g_warning("set_sink_input_mute() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    else
        pa_operation_unref(op);
}

static void playback_min_latency_changed(GObject *object, GParamSpec *pspec, gpointer data)
{

    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;
    guint min_latency;

    g_object_get(object, "min-latency", &min_latency, NULL);
    p->target_delay = min_latency;

    if (p->last_delay < p->target_delay) {
        spice_debug("%s: corking", __FUNCTION__);
        if (p->playback.stream)
            stream_cork(pulse, &p->playback, FALSE);
    } else {
        spice_debug("%s: not corking. The current delay satisfies the requirement", __FUNCTION__);
    }
}

static void record_mute_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;
    gboolean mute;
    pa_operation *op;

    g_object_get(object, "mute", &mute, NULL);
    SPICE_DEBUG("record mute changed %u", mute);

    if (!p->record.stream ||
        pa_stream_get_device_index(p->record.stream) == PA_INVALID_INDEX)
        return;

    op = pa_context_set_source_mute_by_index(p->context,
        pa_stream_get_device_index(p->record.stream),
        mute, NULL, NULL);
    if (!op)
        g_warning("set_source_mute() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    else
        pa_operation_unref(op);
}

static void record_volume_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpicePulse *pulse = data;
    SpicePulsePrivate *p = pulse->priv;
    guint16 *volume;
    guint nchannels;
    pa_operation *op;
    pa_cvolume v;
    guint i;

    g_object_get(object,
                 "volume", &volume,
                 "nchannels", &nchannels,
                 NULL);

    pa_cvolume_init(&v);
    v.channels = p->record.spec.channels;
    for (i = 0; i < nchannels; ++i) {
        v.values[i] = (PA_VOLUME_NORM - PA_VOLUME_MUTED) * volume[i] / G_MAXUINT16;
        SPICE_DEBUG("record volume changed %u", v.values[i]);
    }

    if (!p->record.stream ||
        pa_stream_get_device_index(p->record.stream) == PA_INVALID_INDEX)
        return;

    /* FIXME: use the upcoming "set_source_output_volume" */
    op = pa_context_set_source_volume_by_index(p->context,
        pa_stream_get_device_index(p->record.stream),
        &v, NULL, NULL);
    if (!op)
        g_warning("set_source_volume() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    else
        pa_operation_unref(op);
}

static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel)
{
    SpicePulse *pulse = SPICE_PULSE(audio);
    SpicePulsePrivate *p = pulse->priv;

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        g_return_val_if_fail(p->pchannel == NULL, FALSE);

        p->pchannel = g_object_ref(channel);
        spice_g_signal_connect_object(channel, "playback-start",
                                      G_CALLBACK(playback_start), pulse, 0);
        spice_g_signal_connect_object(channel, "playback-data",
                                      G_CALLBACK(playback_data), pulse, 0);
        spice_g_signal_connect_object(channel, "playback-stop",
                                      G_CALLBACK(playback_stop), pulse, 0);
        spice_g_signal_connect_object(channel, "channel-event",
                                      G_CALLBACK(channel_event), pulse, 0);
        spice_g_signal_connect_object(channel, "notify::volume",
                                      G_CALLBACK(playback_volume_changed), pulse, 0);
        spice_g_signal_connect_object(channel, "notify::mute",
                                      G_CALLBACK(playback_mute_changed), pulse, 0);
        spice_g_signal_connect_object(channel, "notify::min-latency",
                                      G_CALLBACK(playback_min_latency_changed), pulse, 0);

        return TRUE;
    }

    if (SPICE_IS_RECORD_CHANNEL(channel)) {
        g_return_val_if_fail(p->rchannel == NULL, FALSE);

        p->rchannel = g_object_ref(channel);
        spice_g_signal_connect_object(channel, "record-start",
                                      G_CALLBACK(record_start), pulse, 0);
        spice_g_signal_connect_object(channel, "record-stop",
                                      G_CALLBACK(record_stop), pulse, 0);
        spice_g_signal_connect_object(channel, "channel-event",
                                      G_CALLBACK(channel_event), pulse, 0);
        spice_g_signal_connect_object(channel, "notify::volume",
                                      G_CALLBACK(record_volume_changed), pulse, 0);
        spice_g_signal_connect_object(channel, "notify::mute",
                                      G_CALLBACK(record_mute_changed), pulse, 0);

        return TRUE;
    }

    return FALSE;
}

static void context_state_callback(pa_context *c, void *userdata)
{
    SpicePulsePrivate *p;
    p = SPICE_PULSE_GET_PRIVATE(userdata);

    g_return_if_fail(p != NULL);
    g_return_if_fail(c != NULL);
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    case PA_CONTEXT_UNCONNECTED:
        break;

    case PA_CONTEXT_READY: {
        if (!p->record.stream && p->record.started)
            create_record(SPICE_PULSE(userdata));

        if (!p->playback.stream && p->playback.started)
            create_playback(SPICE_PULSE(userdata));
        break;
    }

    case PA_CONTEXT_FAILED:
        g_warning("PulseAudio context failed %s",
                  pa_strerror(pa_context_errno(p->context)));
        break;

    case PA_CONTEXT_TERMINATED:
    default:
        SPICE_DEBUG("PulseAudio context terminated");
        break;
    }
}

SpicePulse *spice_pulse_new(SpiceSession *session, GMainContext *context,
                            const char *name)
{
    SpicePulse *pulse;
    SpicePulsePrivate *p;

    pulse = g_object_new(SPICE_TYPE_PULSE,
                         "session", session,
                         "main-context", context,
                         NULL);
    p = SPICE_PULSE_GET_PRIVATE(pulse);

    p->mainloop = pa_glib_mainloop_new(context);
    p->state = PA_CONTEXT_READY;
    p->context = pa_context_new(pa_glib_mainloop_get_api(p->mainloop), name);
    pa_context_set_state_callback(p->context, context_state_callback, pulse);
    if (pa_context_connect(p->context, NULL, 0, NULL) < 0) {
        g_warning("pa_context_connect() failed: %s",
            pa_strerror(pa_context_errno(p->context)));
        goto error;
    }

    return pulse;

error:
    g_object_unref(pulse);
    return  NULL;
}
