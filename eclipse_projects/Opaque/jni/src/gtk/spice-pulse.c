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

#include "spice-pulse.h"
#include "spice-common.h"
#include "spice-session-priv.h"
#include "spice-channel-priv.h"
#include "spice-util-priv.h"
#include "glib-compat.h"

#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

#define SPICE_PULSE_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_PULSE, SpicePulsePrivate))

struct async_task {
    SpicePulse                 *pulse;
    SpiceMainChannel           *main_channel;
    GSimpleAsyncResult         *res;
    GAsyncReadyCallback        callback;
    gpointer                   user_data;
    gboolean                   is_playback;
    pa_operation               *pa_op;
    gulong                     cancel_id;
    GCancellable               *cancellable;
};

struct stream {
    pa_sample_spec             spec;
    pa_stream                  *stream;
    int                        state;
    pa_operation               *uncork_op;
    pa_operation               *cork_op;
    gboolean                   started;
    guint                      num_underflow;
    gboolean                   info_updated;
    gchar                      *name;
    pa_ext_stream_restore_info info;
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
    struct async_task       *pending_restore_task;
    GList                   *results;
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

static void stream_stop(SpicePulse *pulse, struct stream *s);
static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel);
static void channel_weak_notified(gpointer data, GObject *where_the_object_was);
static void spice_pulse_get_playback_volume_info_async(SpiceAudio *audio, GCancellable *cancellable,
        SpiceMainChannel *main_channel, GAsyncReadyCallback callback, gpointer user_data);
static gboolean spice_pulse_get_playback_volume_info_finish(SpiceAudio *audio, GAsyncResult *res,
        gboolean *mute, guint8 *nchannels, guint16 **volume, GError **error);
static void spice_pulse_get_record_volume_info_async(SpiceAudio *audio, GCancellable *cancellable,
        SpiceMainChannel *main_channel, GAsyncReadyCallback callback, gpointer user_data);
static gboolean spice_pulse_get_record_volume_info_finish(SpiceAudio *audio,GAsyncResult *res,
        gboolean *mute, guint8 *nchannels, guint16 **volume, GError **error);
static void stream_restore_read_cb(pa_context *context,
        const pa_ext_stream_restore_info *info, int eol, void *userdata);
static void spice_pulse_complete_async_task(struct async_task *task, const gchar *err_msg);
static void spice_pulse_complete_all_async_tasks(SpicePulse *pulse, const gchar *err_msg);

static void spice_pulse_finalize(GObject *obj)
{
    SpicePulse *pulse = SPICE_PULSE(obj);
    SpicePulsePrivate *p;

    p = pulse->priv;

    if (p->context != NULL)
        pa_context_unref(p->context);

    if (p->mainloop != NULL)
        pa_glib_mainloop_free(p->mainloop);

    G_OBJECT_CLASS(spice_pulse_parent_class)->finalize(obj);
}

static void spice_pulse_dispose(GObject *obj)
{
    SpicePulse *pulse = SPICE_PULSE(obj);
    SpicePulsePrivate *p;

    SPICE_DEBUG("%s", __FUNCTION__);
    p = pulse->priv;

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

    if (p->results != NULL)
        spice_pulse_complete_all_async_tasks(pulse, "PulseAudio is being dispose");

    g_clear_pointer(&p->playback.name, g_free);
    g_clear_pointer(&p->record.name, g_free);

    if (p->pchannel)
        g_object_weak_unref(G_OBJECT(p->pchannel), channel_weak_notified, pulse);
    p->pchannel = NULL;

    if (p->rchannel)
        g_object_weak_unref(G_OBJECT(p->rchannel), channel_weak_notified, pulse);
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
    audio_class->get_playback_volume_info_async = spice_pulse_get_playback_volume_info_async;
    audio_class->get_playback_volume_info_finish = spice_pulse_get_playback_volume_info_finish;
    audio_class->get_record_volume_info_async = spice_pulse_get_record_volume_info_async;
    audio_class->get_record_volume_info_finish = spice_pulse_get_record_volume_info_finish;

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
    SpicePulsePrivate *p = pulse->priv;
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
    SpicePulsePrivate *p = pulse->priv;
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
    SpicePulsePrivate *p = pulse->priv;

    if (pa_stream_disconnect(s->stream) < 0) {
        g_warning("pa_stream_disconnect() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    }
    pa_stream_unref(s->stream);
    s->stream = NULL;
}

static void stream_state_callback(pa_stream *s, void *userdata)
{
    SpicePulse *pulse = userdata;
    SpicePulsePrivate *p;

    p = pulse->priv;

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
    SpicePulse *pulse = userdata;
    SpicePulsePrivate *p;

    SPICE_DEBUG("PA stream underflow!!");

    p = pulse->priv;
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

    p = pulse->priv;

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
    SpicePulsePrivate *p = pulse->priv;
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
    SpicePulsePrivate *p = pulse->priv;
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
    SpicePulsePrivate *p = pulse->priv;
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

static void playback_stop(SpicePulse *pulse)
{
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
    SpicePulsePrivate *p = pulse->priv;

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
    SpicePulsePrivate *p = pulse->priv;
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
    SpicePulsePrivate *p = pulse->priv;
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

static void record_stop(SpicePulse *pulse)
{
    SpicePulsePrivate *p = pulse->priv;

    SPICE_DEBUG("%s", __FUNCTION__);

    p->record.started = FALSE;
    if (!p->record.stream)
        return;

    stream_stop(pulse, &p->record);
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

#if PA_CHECK_VERSION(1,0,0)
    op = pa_context_set_source_output_mute(p->context,
        pa_stream_get_index(p->record.stream),
#else
    op = pa_context_set_source_mute_by_index(p->context,
        pa_stream_get_device_index(p->record.stream),
#endif
        mute, NULL, NULL);
    if (!op)
        g_warning("set_source_output_mute() failed: %s",
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

#if PA_CHECK_VERSION(1,0,0)
    op = pa_context_set_source_output_volume(p->context,
        pa_stream_get_index(p->record.stream),
#else
    op = pa_context_set_source_volume_by_index(p->context,
        pa_stream_get_device_index(p->record.stream),
#endif
        &v, NULL, NULL);
    if (!op)
        g_warning("set_source_output_volume() failed: %s",
                  pa_strerror(pa_context_errno(p->context)));
    else
        pa_operation_unref(op);
}

static void
channel_weak_notified(gpointer data,
                      GObject *where_the_object_was)
{
    SpicePulse *pulse = SPICE_PULSE(data);
    SpicePulsePrivate *p = pulse->priv;

    if (where_the_object_was == (GObject *)p->pchannel) {
        SPICE_DEBUG("playback closed");
        playback_stop(pulse);
        p->pchannel = NULL;
    } else if (where_the_object_was == (GObject *)p->rchannel) {
        SPICE_DEBUG("record closed");
        record_stop(pulse);
        p->rchannel = NULL;
    }
}

static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel)
{
    SpicePulse *pulse = SPICE_PULSE(audio);
    SpicePulsePrivate *p = pulse->priv;

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        g_return_val_if_fail(p->pchannel == NULL, FALSE);

        p->pchannel = channel;
        g_object_weak_ref(G_OBJECT(p->pchannel), channel_weak_notified, audio);
        spice_g_signal_connect_object(channel, "playback-start",
                                      G_CALLBACK(playback_start), pulse, 0);
        spice_g_signal_connect_object(channel, "playback-data",
                                      G_CALLBACK(playback_data), pulse, 0);
        spice_g_signal_connect_object(channel, "playback-stop",
                                      G_CALLBACK(playback_stop), pulse, G_CONNECT_SWAPPED);
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

        p->rchannel = channel;
        g_object_weak_ref(G_OBJECT(p->rchannel), channel_weak_notified, audio);
        spice_g_signal_connect_object(channel, "record-start",
                                      G_CALLBACK(record_start), pulse, 0);
        spice_g_signal_connect_object(channel, "record-stop",
                                      G_CALLBACK(record_stop), pulse, G_CONNECT_SWAPPED);
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
    SpicePulse *pulse = userdata;
    SpicePulsePrivate *p;

    p = pulse->priv;

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

        if (p->pending_restore_task != NULL &&
                p->pending_restore_task->pa_op == NULL) {
            pa_operation *op = pa_ext_stream_restore_read(p->context,
                                                          stream_restore_read_cb,
                                                          pulse);
            if (!op)
                goto context_fail;
            p->pending_restore_task->pa_op = op;
        }
        break;
    }

    case PA_CONTEXT_FAILED:
        g_warning("PulseAudio context failed %s",
                  pa_strerror(pa_context_errno(p->context)));
        goto context_fail;

    case PA_CONTEXT_TERMINATED:
    default:
        SPICE_DEBUG("PulseAudio context terminated");
        goto context_fail;
    }

    return;

context_fail:
    if (p->pending_restore_task != NULL) {
        const gchar *errmsg = pa_strerror(pa_context_errno(p->context));
        errmsg = (errmsg != NULL) ? errmsg : "PulseAudio context terminated";
        spice_pulse_complete_all_async_tasks(pulse, errmsg);
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
    p = pulse->priv;

    p->mainloop = pa_glib_mainloop_new(context);
    p->state = PA_CONTEXT_READY;
    p->context = pa_context_new(pa_glib_mainloop_get_api(p->mainloop), name);
    pa_context_set_state_callback(p->context, context_state_callback, pulse);
    if (pa_context_connect(p->context, NULL, 0, NULL) < 0) {
        g_warning("pa_context_connect() failed: %s",
            pa_strerror(pa_context_errno(p->context)));
        goto error;
    }

    p->playback.name = g_strconcat("sink-input-by-application-name:",
                                   g_get_application_name(), NULL);
    p->record.name = g_strconcat("source-output-by-application-name:",
                                 g_get_application_name(), NULL);
    return pulse;

error:
    g_object_unref(pulse);
    return  NULL;
}

static gboolean free_async_task(gpointer user_data)
{
    struct async_task *task = user_data;

    if (task == NULL)
        return G_SOURCE_REMOVE;

    if (task->pa_op != NULL) {
        pa_operation_cancel(task->pa_op);
        pa_operation_unref(task->pa_op);
        task->pa_op = NULL;
    }

    if (task->pulse) {
        if (task->pulse->priv->pending_restore_task == task) {
            task->pulse->priv->pending_restore_task = NULL;
        }
        g_object_unref(task->pulse);
    }

    if (task->res)
        g_object_unref(task->res);

    if (task->main_channel)
        g_object_unref(task->main_channel);

    if (task->pa_op != NULL)
        pa_operation_unref(task->pa_op);

    if (task->cancel_id != 0) {
        g_cancellable_disconnect(task->cancellable, task->cancel_id);
        g_clear_object(&task->cancellable);
    }

    g_free(task);
    return G_SOURCE_REMOVE;
}

static void cancel_task(GCancellable *cancellable, gpointer user_data)
{
    struct async_task *task = user_data;
    g_return_if_fail(task != NULL);

#if GLIB_CHECK_VERSION(2,40,0)
    free_async_task(task);
#else
    /* This must be done now otherwise pulseaudio may return to a
     * cancelled task operation before free_async_task is called */
    if (task->pa_op != NULL) {
        pa_operation_cancel(task->pa_op);
        pa_operation_unref(task->pa_op);
        task->pa_op = NULL;
    }

    /* Clear the pending_restore_task reference to avoid triggering a
     * pa_operation when context state is in READY state */
    if (task->pulse->priv->pending_restore_task == task) {
        task->pulse->priv->pending_restore_task = NULL;
    }

#if !GLIB_CHECK_VERSION(2,32,0)
    /* g_simple_async_result_set_check_cancellable is not present. Set an error
     * in the GSimpleAsyncResult in case of _finish functions is called */
    g_simple_async_result_set_error(task->res,
                                    SPICE_CLIENT_ERROR,
                                    SPICE_CLIENT_ERROR_FAILED,
                                    "Operation was cancelled");
#endif
    /* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=705395
     * Free the memory in idle */
    g_idle_add(free_async_task, task);
#endif
}

static void complete_task(SpicePulse *pulse, struct async_task *task, const gchar *err_msg)
{
    SpicePulsePrivate *p = pulse->priv;

    /* If we do have any err_msg, we failed */
    if (err_msg != NULL) {
        g_simple_async_result_set_op_res_gboolean(task->res, FALSE);
        g_simple_async_result_set_error(task->res,
                                        SPICE_CLIENT_ERROR,
                                        SPICE_CLIENT_ERROR_FAILED,
                                        "restore-info failed due %s",
                                        err_msg);
    /* Volume-info does not change if stream is not found */
    } else if ((task->is_playback == TRUE && p->playback.info_updated == FALSE) ||
               (task->is_playback == FALSE && p->record.info_updated == FALSE)) {
        g_simple_async_result_set_op_res_gboolean(task->res, FALSE);
        g_simple_async_result_set_error(task->res,
                                        SPICE_CLIENT_ERROR,
                                        SPICE_CLIENT_ERROR_FAILED,
                                        "Stream not found by pulse");
    } else {
        g_simple_async_result_set_op_res_gboolean(task->res, TRUE);
    }

    /* As all async calls to PulseAudio are done with glib mainloop, it is
     * safe to complete the operation synchronously here. */
    g_simple_async_result_complete(task->res);
}

static void spice_pulse_complete_async_task(struct async_task *task, const gchar *err_msg)
{
    SpicePulsePrivate *p;

    g_return_if_fail(task != NULL);
    p = task->pulse->priv;

    complete_task(task->pulse, task, err_msg);
    if (p->results != NULL) {
        p->results = g_list_remove(p->results, task);
        SPICE_DEBUG("Number of async task is %d", g_list_length(p->results));
    }
    free_async_task(task);
}

static void spice_pulse_complete_all_async_tasks(SpicePulse *pulse, const gchar *err_msg)
{
    SpicePulsePrivate *p;
    GList *it;

    g_return_if_fail(pulse != NULL);
    p = pulse->priv;

    /* Complete all tasks in list */
    for(it = p->results; it != NULL; it = it->next) {
        struct async_task *task = it->data;
        complete_task(pulse, task, err_msg);
        free_async_task(task);
    }
    g_list_free(p->results);
    p->results = NULL;
    SPICE_DEBUG("All async tasks completed");
}

static void stream_restore_read_cb(pa_context *context,
                                   const pa_ext_stream_restore_info *info,
                                   int eol,
                                   void *userdata)
{
    SpicePulsePrivate *p = SPICE_PULSE(userdata)->priv;
    struct stream *pstream = NULL;

    if (eol ||
            (p->playback.info_updated == TRUE &&
             p->record.info_updated == TRUE)) {
        /* We only have one pa_operation running the stream-restore-info
         * which retrieves volume-info from both Playback and Record channels;
         * We can complete all async tasks now that this operation ended.
         * (or we already have the volume-info we want)
         * Note: the following function cancel the current pa_operation */
        spice_pulse_complete_all_async_tasks(SPICE_PULSE(userdata), NULL);
        return;
    }

    if (g_strcmp0(info->name, p->playback.name) == 0) {
        pstream = &p->playback;
    } else if (g_strcmp0(info->name, p->record.name) == 0) {
        pstream = &p->record;
    } else {
        /* This is not the stream you are looking for. */
        return;
    }

    if (info->channel_map.channels == 0) {
        SPICE_DEBUG("Number of channels stored is zero. Ignore. (%s)", info->name);
        return;
    }

    pstream->info_updated = TRUE;
    pstream->info.name = pstream->name;
    pstream->info.mute = info->mute;
    pstream->info.channel_map = info->channel_map;
    pstream->info.volume = info->volume;
}

#if PA_CHECK_VERSION(1,0,0)
static void source_output_info_cb(pa_context *context,
                                  const pa_source_output_info *info,
                                  int eol,
                                  void *userdata)
#else
static void source_info_cb(pa_context *context,
                           const pa_source_info *info,
                           int eol,
                           void *userdata)
#endif
{
    struct async_task *task = userdata;
    SpicePulsePrivate *p = task->pulse->priv;
    struct stream *pstream = &p->record;

    if (eol) {
        spice_pulse_complete_async_task(task, NULL);
        return;
    }

    pstream->info_updated = TRUE;
    pstream->info.name = pstream->name;
    pstream->info.mute = info->mute;
    pstream->info.channel_map = info->channel_map;
    pstream->info.volume = info->volume;
}

static void sink_input_info_cb(pa_context *context,
                               const pa_sink_input_info *info,
                               int eol,
                               void *userdata)
{
    struct async_task *task = userdata;
    SpicePulsePrivate *p = task->pulse->priv;
    struct stream *pstream = &p->playback;

    if (eol) {
        spice_pulse_complete_async_task(task, NULL);
        return;
    }

    pstream->info_updated = TRUE;
    pstream->info.name = pstream->name;
    pstream->info.mute = info->mute;
    pstream->info.channel_map = info->channel_map;
    pstream->info.volume = info->volume;
}

/* to avoid code duplication */
static void pulse_stream_restore_info_async(gboolean is_playback,
                                            SpiceAudio *audio,
                                            GCancellable *cancellable,
                                            SpiceMainChannel *main_channel,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data)
{
    SpicePulsePrivate *p = SPICE_PULSE(audio)->priv;
    GSimpleAsyncResult *simple;
    struct async_task *task = g_malloc0(sizeof(struct async_task));
    pa_operation *op = NULL;

    simple = g_simple_async_result_new(G_OBJECT(audio),
                                       callback,
                                       user_data,
                                       pulse_stream_restore_info_async);
#if GLIB_CHECK_VERSION(2,32,0)
    g_simple_async_result_set_check_cancellable (simple, cancellable);
#endif

    task->res = simple;
    task->pulse = g_object_ref(audio);
    task->callback = callback;
    task->user_data = user_data;
    task->is_playback = is_playback;
    task->main_channel = g_object_ref(main_channel);
    task->pa_op = NULL;

    if (cancellable) {
        task->cancellable = g_object_ref(cancellable);
        task->cancel_id = g_cancellable_connect(cancellable, G_CALLBACK(cancel_task), task, NULL);
    }

    /* If Playback/Record stream is created we use pulse API to get volume-info
     * from those streams directly. If the stream is not created, retrieve last
     * volume/mute values from Pulse database using the application name;
     * If we already have retrieved volume-info from Pulse database then it is
     * safe to return the volume-info we already have in <stream>info */

    if (is_playback == TRUE &&
            p->playback.stream != NULL &&
            pa_stream_get_index(p->playback.stream) != PA_INVALID_INDEX) {
        SPICE_DEBUG("Playback stream is created - get-sink-input-info");
        p->playback.info_updated = FALSE;
        op = pa_context_get_sink_input_info(p->context,
                                            pa_stream_get_index(p->playback.stream),
                                            sink_input_info_cb,
                                            task);
        if (!op)
            goto fail;
        task->pa_op = op;

    } else if (is_playback == FALSE &&
            p->record.stream != NULL &&
            pa_stream_get_index(p->record.stream) != PA_INVALID_INDEX) {
        SPICE_DEBUG("Record stream is created - get-source-output-info");
        p->record.info_updated = FALSE;
#if PA_CHECK_VERSION(1,0,0)
        op = pa_context_get_source_output_info(p->context,
                                               pa_stream_get_index(p->record.stream),
                                               source_output_info_cb,
                                               task);
#else
        op = pa_context_get_source_info_by_index(p->context,
                                                 pa_stream_get_device_index(p->record.stream),
                                                 source_info_cb,
                                                 task);
#endif
        if (!op)
            goto fail;
        task->pa_op = op;

    } else {
        if (p->playback.info.name != NULL ||
                p->record.info.name != NULL) {
            /* If the pstream->info.name is set then we already have updated
             * volume information. We can complete the request now */
            SPICE_DEBUG("Return the volume-information we already have");
            spice_pulse_complete_async_task(task, NULL);
            return;
        }

        if (p->results == NULL) {
            SPICE_DEBUG("Streams are not created - ext-stream-restore");
            p->playback.info_updated = FALSE;
            p->record.info_updated = FALSE;

            if (pa_context_get_state(p->context) == PA_CONTEXT_READY) {
                /* Restore value from pulse db */
                op = pa_ext_stream_restore_read(p->context, stream_restore_read_cb, audio);
                if (!op)
                    goto fail;
                task->pa_op = op;
            } else {
                /* It is possible that we want to get volume-info before the
                 * context is in READY state. In this case, we wait for the
                 * context state change to READY. */
                p->pending_restore_task = task;
            }
        }
    }

    p->results = g_list_append(p->results, task);
    SPICE_DEBUG ("Number of async task is %d", g_list_length(p->results));
    return;

fail:
    if (!op) {
        g_simple_async_report_error_in_idle(G_OBJECT(audio),
                                            callback,
                                            user_data,
                                            SPICE_CLIENT_ERROR,
                                            SPICE_CLIENT_ERROR_FAILED,
                                            "Volume-Info failed: %s",
                                            pa_strerror(pa_context_errno(p->context)));
        free_async_task(task);
    }
}

/* to avoid code duplication */
static gboolean pulse_stream_restore_info_finish(gboolean is_playback,
                                                 SpiceAudio *audio,
                                                 GAsyncResult *res,
                                                 gboolean *mute,
                                                 guint8 *nchannels,
                                                 guint16 **volume,
                                                 GError **error)
{
    SpicePulsePrivate *p = SPICE_PULSE(audio)->priv;
    struct stream *pstream = (is_playback) ? &p->playback : &p->record;
    GSimpleAsyncResult *simple = (GSimpleAsyncResult *) res;

    g_return_val_if_fail(g_simple_async_result_is_valid(res,
        G_OBJECT(audio), pulse_stream_restore_info_async), FALSE);

    if (g_simple_async_result_propagate_error(simple, error)) {
        /* set out args that should have new alloc'ed memory to NULL */
        if (volume != NULL) {
            *volume = NULL;
        }
        return FALSE;
    }

    if (mute != NULL) {
        *mute = (pstream->info.mute) ? TRUE : FALSE;
    }

    if (nchannels != NULL) {
        *nchannels = pstream->info.channel_map.channels;
    }

    if (volume != NULL) {
        gint i;
        *volume = g_new(guint16, pstream->info.channel_map.channels);
        for (i = 0; i < pstream->info.channel_map.channels; i++) {
            (*volume)[i] = MIN(pstream->info.volume.values[i], G_MAXUINT16);
            SPICE_DEBUG("(%s) volume at channel %d is %u",
                        (is_playback) ? "playback" : "record", i, (*volume)[i]);
        }
    }

    return g_simple_async_result_get_op_res_gboolean(simple);
}

static void spice_pulse_get_playback_volume_info_async(SpiceAudio *audio,
                                                       GCancellable *cancellable,
                                                       SpiceMainChannel *main_channel,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data)
{
    pulse_stream_restore_info_async(TRUE, audio, cancellable, main_channel, callback, user_data);
}

static gboolean spice_pulse_get_playback_volume_info_finish(SpiceAudio *audio,
                                                            GAsyncResult *res,
                                                            gboolean *mute,
                                                            guint8 *nchannels,
                                                            guint16 **volume,
                                                            GError **error)
{
    return pulse_stream_restore_info_finish(TRUE, audio, res, mute,
                                            nchannels, volume, error);
}

static void spice_pulse_get_record_volume_info_async(SpiceAudio *audio,
                                                     GCancellable *cancellable,
                                                     SpiceMainChannel *main_channel,
                                                     GAsyncReadyCallback callback,
                                                     gpointer user_data)
{
    pulse_stream_restore_info_async(FALSE, audio, cancellable, main_channel, callback, user_data);
}

static gboolean spice_pulse_get_record_volume_info_finish(SpiceAudio *audio,
                                                          GAsyncResult *res,
                                                          gboolean *mute,
                                                          guint8 *nchannels,
                                                          guint16 **volume,
                                                          GError **error)
{
    return pulse_stream_restore_info_finish(FALSE, audio, res, mute,
                                            nchannels, volume, error);
}
