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

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/streamvolume.h>

#include "spice-gstaudio.h"
#include "spice-common.h"
#include "spice-session.h"
#include "spice-util.h"

#define SPICE_GSTAUDIO_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_GSTAUDIO, SpiceGstaudioPrivate))

G_DEFINE_TYPE(SpiceGstaudio, spice_gstaudio, SPICE_TYPE_AUDIO)

struct stream {
    GstElement              *pipe;
    GstElement              *src;
    GstElement              *sink;
    guint                   rate;
    guint                   channels;
};

struct _SpiceGstaudioPrivate {
    SpiceChannel            *pchannel;
    SpiceChannel            *rchannel;
    struct stream           playback;
    struct stream           record;
    guint                   mmtime_id;
};

static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel);
static void channel_weak_notified(gpointer data, GObject *where_the_object_was);
static void spice_gstaudio_get_playback_volume_info_async(SpiceAudio *audio,
        GCancellable *cancellable, SpiceMainChannel *main_channel,
        GAsyncReadyCallback callback, gpointer user_data);
static gboolean spice_gstaudio_get_playback_volume_info_finish(SpiceAudio *audio,
        GAsyncResult *res, gboolean *mute, guint8 *nchannels, guint16 **volume, GError **error);
static void spice_gstaudio_get_record_volume_info_async(SpiceAudio *audio,
        GCancellable *cancellable, SpiceMainChannel *main_channel,
        GAsyncReadyCallback callback, gpointer user_data);
static gboolean spice_gstaudio_get_record_volume_info_finish(SpiceAudio *audio,
        GAsyncResult *res, gboolean *mute, guint8 *nchannels, guint16 **volume, GError **error);

static void spice_gstaudio_finalize(GObject *obj)
{
    G_OBJECT_CLASS(spice_gstaudio_parent_class)->finalize(obj);
}

void stream_dispose(struct stream *s)
{
    if (s->pipe) {
        gst_element_set_state(s->pipe, GST_STATE_NULL);
        gst_object_unref(s->pipe);
        s->pipe = NULL;
    }

    if (s->src) {
        gst_object_unref(s->src);
        s->src = NULL;
    }

    if (s->sink) {
        gst_object_unref(s->sink);
        s->sink = NULL;
    }
}

static void spice_gstaudio_dispose(GObject *obj)
{
    SpiceGstaudio *gstaudio = SPICE_GSTAUDIO(obj);
    SpiceGstaudioPrivate *p;
    SPICE_DEBUG("%s", __FUNCTION__);
    p = gstaudio->priv;

    stream_dispose(&p->playback);
    stream_dispose(&p->record);

    if (p->pchannel)
        g_object_weak_unref(G_OBJECT(p->pchannel), channel_weak_notified, gstaudio);
    p->pchannel = NULL;

    if (p->rchannel)
        g_object_weak_unref(G_OBJECT(p->rchannel), channel_weak_notified, gstaudio);
    p->rchannel = NULL;

    if (G_OBJECT_CLASS(spice_gstaudio_parent_class)->dispose)
        G_OBJECT_CLASS(spice_gstaudio_parent_class)->dispose(obj);
}

static void spice_gstaudio_init(SpiceGstaudio *gstaudio)
{
    gstaudio->priv = SPICE_GSTAUDIO_GET_PRIVATE(gstaudio);
}

static void spice_gstaudio_class_init(SpiceGstaudioClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceAudioClass *audio_class = SPICE_AUDIO_CLASS(klass);

    audio_class->connect_channel = connect_channel;
    audio_class->get_playback_volume_info_async = spice_gstaudio_get_playback_volume_info_async;
    audio_class->get_playback_volume_info_finish = spice_gstaudio_get_playback_volume_info_finish;
    audio_class->get_record_volume_info_async = spice_gstaudio_get_record_volume_info_async;
    audio_class->get_record_volume_info_finish = spice_gstaudio_get_record_volume_info_finish;

    gobject_class->finalize = spice_gstaudio_finalize;
    gobject_class->dispose = spice_gstaudio_dispose;

    g_type_class_add_private(klass, sizeof(SpiceGstaudioPrivate));
}

static GstFlowReturn record_new_buffer(GstAppSink *appsink, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstMessage *msg;

    g_return_val_if_fail(p != NULL, GST_FLOW_ERROR);

    msg = gst_message_new_application(GST_OBJECT(p->record.pipe),
                                      gst_structure_new_empty ("new-sample"));
    gst_element_post_message(p->record.pipe, msg);
    return GST_FLOW_OK;
}

static void record_stop(SpiceGstaudio *gstaudio)
{
    SpiceGstaudioPrivate *p = gstaudio->priv;

    SPICE_DEBUG("%s", __FUNCTION__);
    if (p->record.pipe)
        gst_element_set_state(p->record.pipe, GST_STATE_READY);
}

static gboolean record_bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;

    g_return_val_if_fail(p != NULL, FALSE);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_APPLICATION: {
        GstSample *s;
        GstBuffer *buffer;
        GstMapInfo mapping;

        s = gst_app_sink_pull_sample(GST_APP_SINK(p->record.sink));
        if (!s) {
            if (!gst_app_sink_is_eos(GST_APP_SINK(p->record.sink)))
                g_warning("eos not reached, but can't pull new sample");
            return TRUE;
        }

        buffer = gst_sample_get_buffer(s);
        if (!buffer) {
            if (!gst_app_sink_is_eos(GST_APP_SINK(p->record.sink)))
                g_warning("eos not reached, but can't pull new buffer");
            return TRUE;
        }
        if (!gst_buffer_map(buffer, &mapping, GST_MAP_READ)) {
            return TRUE;
        }

        spice_record_send_data(SPICE_RECORD_CHANNEL(p->rchannel),
                               /* FIXME: server side doesn't care about ts?
                                  what is the unit? ms apparently */
                               mapping.data, mapping.size, 0);
        gst_buffer_unmap(buffer, &mapping);
        gst_sample_unref(s);
        break;
    }
    default:
        break;
    }

    return TRUE;
}

static void record_start(SpiceRecordChannel *channel, gint format, gint channels,
                         gint frequency, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;

    g_return_if_fail(p != NULL);
    g_return_if_fail(format == SPICE_AUDIO_FMT_S16);

    if (p->record.pipe &&
        (p->record.rate != frequency ||
         p->record.channels != channels)) {
        record_stop(gstaudio);
        gst_object_unref(p->record.pipe);
        p->record.pipe = NULL;
    }

    if (!p->record.pipe) {
        GError *error = NULL;
        GstBus *bus;
        gchar *audio_caps =
            g_strdup_printf("audio/x-raw,format=\"S16LE\",channels=%d,rate=%d,"
                            "layout=interleaved", channels, frequency);
        gchar *pipeline =
            g_strdup_printf("autoaudiosrc name=audiosrc ! queue ! audioconvert ! audioresample ! "
                            "appsink caps=\"%s\" name=appsink", audio_caps);

        p->record.pipe = gst_parse_launch(pipeline, &error);
        if (error != NULL) {
            g_warning("Failed to create pipeline: %s", error->message);
            goto cleanup;
        }

        bus = gst_pipeline_get_bus(GST_PIPELINE(p->record.pipe));
        gst_bus_add_watch(bus, record_bus_cb, data);
        gst_object_unref(GST_OBJECT(bus));

        p->record.src = gst_bin_get_by_name(GST_BIN(p->record.pipe), "audiosrc");
        p->record.sink = gst_bin_get_by_name(GST_BIN(p->record.pipe), "appsink");
        p->record.rate = frequency;
        p->record.channels = channels;

        gst_app_sink_set_emit_signals(GST_APP_SINK(p->record.sink), TRUE);
        spice_g_signal_connect_object(p->record.sink, "new-sample",
                                      G_CALLBACK(record_new_buffer), gstaudio, 0);

cleanup:
        if (error != NULL && p->record.pipe != NULL) {
            gst_object_unref(p->record.pipe);
            p->record.pipe = NULL;
        }
        g_clear_error(&error);
        g_free(audio_caps);
        g_free(pipeline);
    }

    if (p->record.pipe)
        gst_element_set_state(p->record.pipe, GST_STATE_PLAYING);
}

static void playback_stop(SpiceGstaudio *gstaudio)
{
    SpiceGstaudioPrivate *p = gstaudio->priv;

    if (p->playback.pipe)
        gst_element_set_state(p->playback.pipe, GST_STATE_READY);
    if (p->mmtime_id != 0) {
        g_source_remove(p->mmtime_id);
        p->mmtime_id = 0;
    }
}

static gboolean update_mmtime_timeout_cb(gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstQuery *q;

    q = gst_query_new_latency();
    if (gst_element_query(p->playback.pipe, q)) {
        gboolean live;
        GstClockTime minlat, maxlat;
        gst_query_parse_latency(q, &live, &minlat, &maxlat);
        SPICE_DEBUG("got min latency %" GST_TIME_FORMAT ", max latency %"
                    GST_TIME_FORMAT ", live %d", GST_TIME_ARGS (minlat),
                    GST_TIME_ARGS (maxlat), live);
        spice_playback_channel_set_delay(SPICE_PLAYBACK_CHANNEL(p->pchannel), GST_TIME_AS_MSECONDS(minlat));
    }
    gst_query_unref (q);

    return TRUE;
}

static void playback_start(SpicePlaybackChannel *channel, gint format, gint channels,
                           gint frequency, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;

    g_return_if_fail(p != NULL);
    g_return_if_fail(format == SPICE_AUDIO_FMT_S16);

    if (p->playback.pipe &&
        (p->playback.rate != frequency ||
         p->playback.channels != channels)) {
        playback_stop(gstaudio);
        gst_object_unref(p->playback.pipe);
        p->playback.pipe = NULL;
    }

    if (!p->playback.pipe) {
        GError *error = NULL;
        gchar *audio_caps =
            g_strdup_printf("audio/x-raw,format=\"S16LE\",channels=%d,rate=%d,"
                            "layout=interleaved", channels, frequency);
        gchar *pipeline = g_strdup (g_getenv("SPICE_GST_AUDIOSINK"));
        if (pipeline == NULL)
            pipeline = g_strdup_printf("appsrc is-live=1 do-timestamp=0 caps=\"%s\" name=\"appsrc\" ! queue ! "
                                       "audioconvert ! audioresample ! autoaudiosink name=\"audiosink\"", audio_caps);
        SPICE_DEBUG("audio pipeline: %s", pipeline);
        p->playback.pipe = gst_parse_launch(pipeline, &error);
        if (error != NULL) {
            g_warning("Failed to create pipeline: %s", error->message);
            goto cleanup;
        }
        p->playback.src = gst_bin_get_by_name(GST_BIN(p->playback.pipe), "appsrc");
        p->playback.sink = gst_bin_get_by_name(GST_BIN(p->playback.pipe), "audiosink");
        p->playback.rate = frequency;
        p->playback.channels = channels;

cleanup:
        if (error != NULL && p->playback.pipe != NULL) {
            gst_object_unref(p->playback.pipe);
            p->playback.pipe = NULL;
        }
        g_clear_error(&error);
        g_free(audio_caps);
        g_free(pipeline);
    }

    if (p->playback.pipe)
        gst_element_set_state(p->playback.pipe, GST_STATE_PLAYING);

    if (p->mmtime_id == 0) {
        update_mmtime_timeout_cb(gstaudio);
        p->mmtime_id = g_timeout_add_seconds(1, update_mmtime_timeout_cb, gstaudio);
    }
}

static void playback_data(SpicePlaybackChannel *channel,
                          gpointer *audio, gint size,
                          gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstBuffer *buf;

    g_return_if_fail(p != NULL);

    audio = g_memdup(audio, size); /* TODO: try to avoid memory copy */
    buf = gst_buffer_new_wrapped(audio, size);
    gst_app_src_push_buffer(GST_APP_SRC(p->playback.src), buf);
}

#define VOLUME_NORMAL 65535

static void playback_volume_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    GstElement *e;
    guint16 *volume;
    guint nchannels;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    gdouble vol;

    if (!p->playback.sink)
        return;

    g_object_get(object,
                 "volume", &volume,
                 "nchannels", &nchannels,
                 NULL);

    g_return_if_fail(nchannels > 0);

    vol = 1.0 * volume[0] / VOLUME_NORMAL;
    SPICE_DEBUG("playback volume changed to %u (%0.2f)", volume[0], 100*vol);

    if (GST_IS_BIN(p->playback.sink))
        e = gst_bin_get_by_interface(GST_BIN(p->playback.sink), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->playback.sink);

    if (GST_IS_STREAM_VOLUME(e))
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(e), GST_STREAM_VOLUME_FORMAT_CUBIC, vol);
    else
        g_object_set(e, "volume", vol, NULL);

    g_object_unref(e);
}

static void playback_mute_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstElement *e;
    gboolean mute;

    if (!p->playback.sink)
        return;

    g_object_get(object, "mute", &mute, NULL);
    SPICE_DEBUG("playback mute changed to %u", mute);

    if (GST_IS_BIN(p->playback.sink))
        e = gst_bin_get_by_interface(GST_BIN(p->playback.sink), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->playback.sink);

    if (GST_IS_STREAM_VOLUME(e))
        gst_stream_volume_set_mute(GST_STREAM_VOLUME(e), mute);

    g_object_unref(e);
}

static void record_volume_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstElement *e;
    guint16 *volume;
    guint nchannels;
    gdouble vol;

    if (!p->record.src)
        return;

    g_object_get(object,
                 "volume", &volume,
                 "nchannels", &nchannels,
                 NULL);

    g_return_if_fail(nchannels > 0);

    vol = 1.0 * volume[0] / VOLUME_NORMAL;
    SPICE_DEBUG("record volume changed to %u (%0.2f)", volume[0], 100*vol);

    /* TODO directsoundsrc doesn't support IDirectSoundBuffer_SetVolume */
    /* TODO pulsesrc doesn't support volume property, it's all coming! */

    if (GST_IS_BIN(p->record.src))
        e = gst_bin_get_by_interface(GST_BIN(p->record.src), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->record.src);

    if (GST_IS_STREAM_VOLUME(e))
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(e), GST_STREAM_VOLUME_FORMAT_CUBIC, vol);
    else
        g_warning("gst lacks volume capabilities on src (TODO)");

    g_object_unref(e);
}

static void record_mute_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    SpiceGstaudio *gstaudio = data;
    SpiceGstaudioPrivate *p = gstaudio->priv;
    GstElement *e;
    gboolean mute;

    if (!p->record.src)
        return;

    g_object_get(object, "mute", &mute, NULL);
    SPICE_DEBUG("record mute changed to %u", mute);

    if (GST_IS_BIN(p->record.src))
        e = gst_bin_get_by_interface(GST_BIN(p->record.src), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->record.src);

    if (GST_IS_STREAM_VOLUME (e))
        gst_stream_volume_set_mute(GST_STREAM_VOLUME(e), mute);
    else
        g_warning("gst lacks mute capabilities on src: %d (TODO)", mute);

    g_object_unref(e);
}

static void
channel_weak_notified(gpointer data,
                      GObject *where_the_object_was)
{
    SpiceGstaudio *gstaudio = SPICE_GSTAUDIO(data);
    SpiceGstaudioPrivate *p = gstaudio->priv;

    if (where_the_object_was == (GObject *)p->pchannel) {
        SPICE_DEBUG("playback closed");
        playback_stop(gstaudio);
        p->pchannel = NULL;
    } else if (where_the_object_was == (GObject *)p->rchannel) {
        SPICE_DEBUG("record closed");
        record_stop(gstaudio);
        p->rchannel = NULL;
    }
}

static gboolean connect_channel(SpiceAudio *audio, SpiceChannel *channel)
{
    SpiceGstaudio *gstaudio = SPICE_GSTAUDIO(audio);
    SpiceGstaudioPrivate *p = gstaudio->priv;

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        g_return_val_if_fail(p->pchannel == NULL, FALSE);

        p->pchannel = channel;
        g_object_weak_ref(G_OBJECT(p->pchannel), channel_weak_notified, audio);
        spice_g_signal_connect_object(channel, "playback-start",
                                      G_CALLBACK(playback_start), gstaudio, 0);
        spice_g_signal_connect_object(channel, "playback-data",
                                      G_CALLBACK(playback_data), gstaudio, 0);
        spice_g_signal_connect_object(channel, "playback-stop",
                                      G_CALLBACK(playback_stop), gstaudio, G_CONNECT_SWAPPED);
        spice_g_signal_connect_object(channel, "notify::volume",
                                      G_CALLBACK(playback_volume_changed), gstaudio, 0);
        spice_g_signal_connect_object(channel, "notify::mute",
                                      G_CALLBACK(playback_mute_changed), gstaudio, 0);

        return TRUE;
    }

    if (SPICE_IS_RECORD_CHANNEL(channel)) {
        g_return_val_if_fail(p->rchannel == NULL, FALSE);

        p->rchannel = channel;
        g_object_weak_ref(G_OBJECT(p->rchannel), channel_weak_notified, audio);
        spice_g_signal_connect_object(channel, "record-start",
                                      G_CALLBACK(record_start), gstaudio, 0);
        spice_g_signal_connect_object(channel, "record-stop",
                                      G_CALLBACK(record_stop), gstaudio, G_CONNECT_SWAPPED);
        spice_g_signal_connect_object(channel, "notify::volume",
                                      G_CALLBACK(record_volume_changed), gstaudio, 0);
        spice_g_signal_connect_object(channel, "notify::mute",
                                      G_CALLBACK(record_mute_changed), gstaudio, 0);

        return TRUE;
    }

    return FALSE;
}

SpiceGstaudio *spice_gstaudio_new(SpiceSession *session, GMainContext *context,
                                  const char *name)
{
    SpiceGstaudio *gstaudio;

    gst_init(NULL, NULL);
    gstaudio = g_object_new(SPICE_TYPE_GSTAUDIO,
                            "session", session,
                            "main-context", context,
                            NULL);

    return gstaudio;
}

static void spice_gstaudio_get_playback_volume_info_async(SpiceAudio *audio,
                                                          GCancellable *cancellable,
                                                          SpiceMainChannel *main_channel,
                                                          GAsyncReadyCallback callback,
                                                          gpointer user_data)
{
    GSimpleAsyncResult *simple;

    simple = g_simple_async_result_new(G_OBJECT(audio),
                                       callback,
                                       user_data,
                                       spice_gstaudio_get_playback_volume_info_async);
    g_simple_async_result_set_check_cancellable (simple, cancellable);

    g_simple_async_result_set_op_res_gboolean(simple, TRUE);
    g_simple_async_result_complete_in_idle(simple);
}

static gboolean spice_gstaudio_get_playback_volume_info_finish(SpiceAudio *audio,
                                                               GAsyncResult *res,
                                                               gboolean *mute,
                                                               guint8 *nchannels,
                                                               guint16 **volume,
                                                               GError **error)
{
    SpiceGstaudioPrivate *p = SPICE_GSTAUDIO(audio)->priv;
    GstElement *e;
    gboolean lmute;
    gdouble vol;
    gboolean fake_channel = FALSE;
    GSimpleAsyncResult *simple = (GSimpleAsyncResult *) res;

    g_return_val_if_fail(g_simple_async_result_is_valid(res,
        G_OBJECT(audio), spice_gstaudio_get_playback_volume_info_async), FALSE);

    if (g_simple_async_result_propagate_error(simple, error)) {
        return FALSE;
    }

    if (p->playback.sink == NULL || p->playback.channels == 0) {
        SPICE_DEBUG("PlaybackChannel not created yet, force start");
        /* In order to get system volume, we start the pipeline */
        playback_start(NULL, SPICE_AUDIO_FMT_S16, 2, 48000, audio);
        fake_channel = TRUE;
    }

    if (GST_IS_BIN(p->playback.sink))
        e = gst_bin_get_by_interface(GST_BIN(p->playback.sink), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->playback.sink);

    if (GST_IS_STREAM_VOLUME(e)) {
        vol = gst_stream_volume_get_volume(GST_STREAM_VOLUME(e), GST_STREAM_VOLUME_FORMAT_CUBIC);
        lmute = gst_stream_volume_get_mute(GST_STREAM_VOLUME(e));
    } else {
        g_object_get(e,
                     "volume", &vol,
                     "mute", &lmute, NULL);
    }
    g_object_unref(e);

    if (fake_channel) {
        SPICE_DEBUG("Stop faked PlaybackChannel");
        playback_stop(SPICE_GSTAUDIO(audio));
    }

    if (mute != NULL) {
        *mute = lmute;
    }

    if (nchannels != NULL) {
        *nchannels = p->playback.channels;
    }

    if (volume != NULL) {
        gint i;
        *volume = g_new(guint16, p->playback.channels);
        for (i = 0; i < p->playback.channels; i++) {
            (*volume)[i] = (guint16) (vol * VOLUME_NORMAL);
            SPICE_DEBUG("(playback) volume at %d is %u (%0.2f%%)", i, (*volume)[i], 100*vol);
        }
    }

    return g_simple_async_result_get_op_res_gboolean(simple);
}

static void spice_gstaudio_get_record_volume_info_async(SpiceAudio *audio,
                                                        GCancellable *cancellable,
                                                        SpiceMainChannel *main_channel,
                                                        GAsyncReadyCallback callback,
                                                        gpointer user_data)
{
    GSimpleAsyncResult *simple;

    simple = g_simple_async_result_new(G_OBJECT(audio),
                                       callback,
                                       user_data,
                                       spice_gstaudio_get_record_volume_info_async);
    g_simple_async_result_set_check_cancellable (simple, cancellable);

    g_simple_async_result_set_op_res_gboolean(simple, TRUE);
    g_simple_async_result_complete_in_idle(simple);
}

static gboolean spice_gstaudio_get_record_volume_info_finish(SpiceAudio *audio,
                                                             GAsyncResult *res,
                                                             gboolean *mute,
                                                             guint8 *nchannels,
                                                             guint16 **volume,
                                                             GError **error)
{
    SpiceGstaudioPrivate *p = SPICE_GSTAUDIO(audio)->priv;
    GstElement *e;
    gboolean lmute;
    gdouble vol;
    gboolean fake_channel = FALSE;
    GSimpleAsyncResult *simple = (GSimpleAsyncResult *) res;

    g_return_val_if_fail(g_simple_async_result_is_valid(res,
        G_OBJECT(audio), spice_gstaudio_get_record_volume_info_async), FALSE);

    if (g_simple_async_result_propagate_error(simple, error)) {
        /* set out args that should have new alloc'ed memory to NULL */
        if (volume != NULL) {
            *volume = NULL;
        }
        return FALSE;
    }

    if (p->record.src == NULL || p->record.channels == 0) {
        SPICE_DEBUG("RecordChannel not created yet, force start");
        /* In order to get system volume, we start the pipeline */
        record_start(NULL, SPICE_AUDIO_FMT_S16, 2, 48000, audio);
        fake_channel = TRUE;
    }

    if (GST_IS_BIN(p->record.src))
        e = gst_bin_get_by_interface(GST_BIN(p->record.src), GST_TYPE_STREAM_VOLUME);
    else
        e = g_object_ref(p->record.src);

    if (GST_IS_STREAM_VOLUME(e)) {
        vol = gst_stream_volume_get_volume(GST_STREAM_VOLUME(e), GST_STREAM_VOLUME_FORMAT_CUBIC);
        lmute = gst_stream_volume_get_mute(GST_STREAM_VOLUME(e));
    } else {
        g_object_get(e,
                     "volume", &vol,
                     "mute", &lmute, NULL);
    }
    g_object_unref(e);

    if (fake_channel) {
        SPICE_DEBUG("Stop faked RecordChannel");
        record_stop(SPICE_GSTAUDIO(audio));
    }

    if (mute != NULL) {
        *mute = lmute;
    }

    if (nchannels != NULL) {
        *nchannels = p->record.channels;
    }

    if (volume != NULL) {
        gint i;
        *volume = g_new(guint16, p->record.channels);
        for (i = 0; i < p->record.channels; i++) {
            (*volume)[i] = (guint16) (vol * VOLUME_NORMAL);
            SPICE_DEBUG("(record) volume at %d is %u (%0.2f%%)", i, (*volume)[i], 100*vol);
        }
    }

    return g_simple_async_result_get_op_res_gboolean(simple);
}
