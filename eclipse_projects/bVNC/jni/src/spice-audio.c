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
/*
 * simple audio init dispatcher
 */

/**
 * SECTION:spice-audio
 * @short_description: a helper to play and to record audio channels
 * @title: Spice Audio
 * @section_id:
 * @see_also: #SpiceRecordChannel, and #SpicePlaybackChannel
 * @stability: Stable
 * @include: spice-audio.h
 *
 * A class that handles the playback and record channels for your
 * application, and connect them to the default sound system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "spice-client.h"
#include "spice-common.h"

#include "spice-audio.h"
#include "spice-session-priv.h"
#include "spice-channel-priv.h"
#include "spice-audio-priv.h"

#ifdef WITH_PULSE
#include "spice-pulse.h"
#endif
#ifdef WITH_GSTAUDIO
#include "spice-gstaudio.h"
#endif

#include "glib-compat.h"

#define SPICE_AUDIO_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_AUDIO, SpiceAudioPrivate))

G_DEFINE_ABSTRACT_TYPE(SpiceAudio, spice_audio, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_SESSION,
    PROP_MAIN_CONTEXT,
};

static void spice_audio_finalize(GObject *gobject)
{
    SpiceAudio *self = SPICE_AUDIO(gobject);
    SpiceAudioPrivate *priv = self->priv;

    if (priv->main_context) {
        g_main_context_unref(priv->main_context);
        priv->main_context = NULL;
    }

    if (G_OBJECT_CLASS(spice_audio_parent_class)->finalize)
        G_OBJECT_CLASS(spice_audio_parent_class)->finalize(gobject);
}

static void spice_audio_get_property(GObject *gobject,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    SpiceAudio *self = SPICE_AUDIO(gobject);
    SpiceAudioPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, priv->session);
        break;
    case PROP_MAIN_CONTEXT:
        g_value_set_boxed(value, priv->main_context);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_audio_set_property(GObject *gobject,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    SpiceAudio *self = SPICE_AUDIO(gobject);
    SpiceAudioPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        priv->session = g_value_get_object(value);
        break;
    case PROP_MAIN_CONTEXT:
        priv->main_context = g_value_dup_boxed(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_audio_class_init(SpiceAudioClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->finalize     = spice_audio_finalize;
    gobject_class->get_property = spice_audio_get_property;
    gobject_class->set_property = spice_audio_set_property;

    /**
     * SpiceAudio:session:
     *
     * #SpiceSession this #SpiceAudio is associated with
     *
     **/
    pspec = g_param_spec_object("session", "Session", "SpiceSession",
                                SPICE_TYPE_SESSION,
                                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_SESSION, pspec);

    /**
     * SpiceAudio:main-context:
     */
    pspec = g_param_spec_boxed("main-context", "Main Context",
                               "GMainContext to use for the event source",
                               G_TYPE_MAIN_CONTEXT,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_MAIN_CONTEXT, pspec);

    g_type_class_add_private(klass, sizeof(SpiceAudioPrivate));
}

static void spice_audio_init(SpiceAudio *self)
{
    self->priv = SPICE_AUDIO_GET_PRIVATE(self);
}

static void connect_channel(SpiceAudio *self, SpiceChannel *channel)
{
    if (channel->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED)
        return;

    if (SPICE_AUDIO_GET_CLASS(self)->connect_channel(self, channel))
        spice_channel_connect(channel);
}

static void update_audio_channels(SpiceAudio *self, SpiceSession *session)
{
    if (session->priv->audio) {
        GList *list, *tmp;

        list = spice_session_get_channels(session);
        for (tmp = g_list_first(list); tmp != NULL; tmp = g_list_next(tmp)) {
            connect_channel(self, tmp->data);
        }
        g_list_free(list);
    } else {
        g_debug("FIXME: disconnect audio channels");
    }
}

static void channel_new(SpiceSession *session, SpiceChannel *channel, SpiceAudio *self)
{
    connect_channel(self, channel);
}

static void session_enable_audio(GObject *gobject, GParamSpec *pspec,
                                 gpointer user_data)
{
    update_audio_channels(SPICE_AUDIO(user_data), SPICE_SESSION(gobject));
}

/**
 * spice_audio_new:
 * @session: the #SpiceSession to connect to
 * @context: (allow-none): a #GMainContext to attach to (or %NULL for
 * default).
 * @name: (allow-none): a name for the audio channels (or %NULL for
 * application name).
 *
 * Once instantiated, #SpiceAudio will handle the playback and record
 * channels to stream to your local audio system.
 *
 * Returns: a new #SpiceAudio instance or %NULL if no backend or failed.
 * Deprecated: 0.8: Use spice_audio_get() instead
 **/
SpiceAudio *spice_audio_new(SpiceSession *session, GMainContext *context,
                            const char *name)
{
    SpiceAudio *self = NULL;

    if (context == NULL)
        context = g_main_context_default();
    if (name == NULL)
        name = g_get_application_name();

#ifdef WITH_PULSE
    self = SPICE_AUDIO(spice_pulse_new(session, context, name));
#endif
#ifdef WITH_GSTAUDIO
    self = SPICE_AUDIO(spice_gstaudio_new(session, context, name));
#endif
    if (!self)
        return NULL;

    spice_g_signal_connect_object(session, "notify::enable-audio", G_CALLBACK(session_enable_audio), self, 0);
    spice_g_signal_connect_object(session, "channel-new", G_CALLBACK(channel_new), self, 0);
    update_audio_channels(self, session);

    return self;
}

/**
 * spice_audio_get:
 * @session: the #SpiceSession to connect to
 * @context: (allow-none): a #GMainContext to attach to (or %NULL for default).
 *
 * Gets the #SpiceAudio associated with the passed in #SpiceSession.
 * A new #SpiceAudio instance will be created the first time this
 * function is called for a certain #SpiceSession.
 *
 * Note that this function returns a weak reference, which should not be used
 * after the #SpiceSession itself has been unref-ed by the caller.
 *
 * Returns: (transfer none): a weak reference to a #SpiceAudio
 * instance or %NULL if failed.
 **/
SpiceAudio *spice_audio_get(SpiceSession *session, GMainContext *context)
{
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    SpiceAudio *self;

    g_static_mutex_lock(&mutex);
    self = session->priv->audio_manager;
    if (self == NULL) {
        self = spice_audio_new(session, context, NULL);
        session->priv->audio_manager = self;
    }
    g_static_mutex_unlock(&mutex);

    return self;
}
