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
#include <math.h>
#include <spice/vd_agent.h>
#include <common/rect.h>
#include <glib/gstdio.h>

#include "glib-compat.h"
#include "spice-client.h"
#include "spice-common.h"
#include "spice-marshal.h"

#include "spice-util-priv.h"
#include "spice-channel-priv.h"
#include "spice-session-priv.h"

/**
 * SECTION:channel-main
 * @short_description: the main Spice channel
 * @title: Main Channel
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: channel-main.h
 *
 * The main channel is the Spice session control channel. It handles
 * communication initialization (channels list), migrations, mouse
 * modes, multimedia time, and agent communication.
 *
 *
 */

#define SPICE_MAIN_CHANNEL_GET_PRIVATE(obj)                             \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_MAIN_CHANNEL, SpiceMainChannelPrivate))

#define MAX_DISPLAY 16 /* Note must fit in a guint32, see monitors_align */

typedef struct spice_migrate spice_migrate;

#define FILE_XFER_CHUNK_SIZE (VD_AGENT_MAX_DATA_SIZE * 32)
typedef struct SpiceFileXferTask {
    uint32_t                       id;
    gboolean                       pending;
    GFile                          *file;
    SpiceMainChannel               *channel;
    GFileInputStream               *file_stream;
    GFileCopyFlags                 flags;
    GCancellable                   *cancellable;
    GFileProgressCallback          progress_callback;
    gpointer                       progress_callback_data;
    GAsyncReadyCallback            callback;
    gpointer                       user_data;
    char                           buffer[FILE_XFER_CHUNK_SIZE];
    uint64_t                       read_bytes;
    uint64_t                       file_size;
    GError                         *error;
} SpiceFileXferTask;

struct _SpiceMainChannelPrivate  {
    enum SpiceMouseMode         mouse_mode;
    bool                        agent_connected;
    bool                        agent_caps_received;

    gboolean                    agent_display_config_sent;
    guint8                      display_color_depth;
    gboolean                    display_disable_wallpaper:1;
    gboolean                    display_disable_font_smooth:1;
    gboolean                    display_disable_animation:1;
    gboolean                    disable_display_position:1;
    gboolean                    disable_display_align:1;

    int                         agent_tokens;
    VDAgentMessage              agent_msg; /* partial msg reconstruction */
    guint8                      *agent_msg_data;
    guint                       agent_msg_pos;
    uint8_t                     agent_msg_size;
    uint32_t                    agent_caps[VD_AGENT_CAPS_SIZE];
    struct {
        int                     x;
        int                     y;
        int                     width;
        int                     height;
        gboolean                enabled;
        gboolean                enabled_set;
    } display[MAX_DISPLAY];
    gint                        timer_id;
    GQueue                      *agent_msg_queue;
    GHashTable                  *file_xfer_tasks;
    GSList                      *flushing;

    guint                       switch_host_delayed_id;
    guint                       migrate_delayed_id;
    spice_migrate               *migrate_data;
    int                         max_clipboard;
};

struct spice_migrate {
    struct coroutine *from;
    SpiceMigrationDstInfo *info;
    SpiceSession *session;
    guint nchannels;
    SpiceChannel *src_channel;
    SpiceChannel *dst_channel;
    bool do_seamless; /* used as input and output for the seamless migration handshake.
                         input: whether to send to the dest SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS
                         output: whether the dest approved seamless migration
                         (SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_ACK/NACK)
                       */
    uint32_t src_mig_version;
};

G_DEFINE_TYPE(SpiceMainChannel, spice_main_channel, SPICE_TYPE_CHANNEL)

/* Properties */
enum {
    PROP_0,
    PROP_MOUSE_MODE,
    PROP_AGENT_CONNECTED,
    PROP_AGENT_CAPS_0,
    PROP_DISPLAY_DISABLE_WALLPAPER,
    PROP_DISPLAY_DISABLE_FONT_SMOOTH,
    PROP_DISPLAY_DISABLE_ANIMATION,
    PROP_DISPLAY_COLOR_DEPTH,
    PROP_DISABLE_DISPLAY_POSITION,
    PROP_DISABLE_DISPLAY_ALIGN,
    PROP_MAX_CLIPBOARD,
};

/* Signals */
enum {
    SPICE_MAIN_MOUSE_UPDATE,
    SPICE_MAIN_AGENT_UPDATE,
    SPICE_MAIN_CLIPBOARD,
    SPICE_MAIN_CLIPBOARD_GRAB,
    SPICE_MAIN_CLIPBOARD_REQUEST,
    SPICE_MAIN_CLIPBOARD_RELEASE,
    SPICE_MAIN_CLIPBOARD_SELECTION,
    SPICE_MAIN_CLIPBOARD_SELECTION_GRAB,
    SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST,
    SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE,
    SPICE_MIGRATION_STARTED,
    SPICE_MAIN_LAST_SIGNAL,
};

static guint signals[SPICE_MAIN_LAST_SIGNAL];

static void spice_main_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg);
static void channel_set_handlers(SpiceChannelClass *klass);
static void agent_send_msg_queue(SpiceMainChannel *channel);
static void agent_free_msg_queue(SpiceMainChannel *channel);
static void migrate_channel_event_cb(SpiceChannel *channel, SpiceChannelEvent event,
                                     gpointer data);
static gboolean main_migrate_handshake_done(gpointer data);
static void spice_main_channel_send_migration_handshake(SpiceChannel *channel);
static void file_xfer_continue_read(SpiceFileXferTask *task);
static void file_xfer_completed(SpiceFileXferTask *task, GError *error);
static void file_xfer_flushed(SpiceMainChannel *channel, gboolean success);
static void spice_main_set_max_clipboard(SpiceMainChannel *self, gint max);
static void set_agent_connected(SpiceMainChannel *channel, gboolean connected);

/* ------------------------------------------------------------------ */

static const char *agent_msg_types[] = {
    [ VD_AGENT_MOUSE_STATE             ] = "mouse state",
    [ VD_AGENT_MONITORS_CONFIG         ] = "monitors config",
    [ VD_AGENT_REPLY                   ] = "reply",
    [ VD_AGENT_CLIPBOARD               ] = "clipboard",
    [ VD_AGENT_DISPLAY_CONFIG          ] = "display config",
    [ VD_AGENT_ANNOUNCE_CAPABILITIES   ] = "announce caps",
    [ VD_AGENT_CLIPBOARD_GRAB          ] = "clipboard grab",
    [ VD_AGENT_CLIPBOARD_REQUEST       ] = "clipboard request",
    [ VD_AGENT_CLIPBOARD_RELEASE       ] = "clipboard release",
};

static const char *agent_caps[] = {
    [ VD_AGENT_CAP_MOUSE_STATE         ] = "mouse state",
    [ VD_AGENT_CAP_MONITORS_CONFIG     ] = "monitors config",
    [ VD_AGENT_CAP_REPLY               ] = "reply",
    [ VD_AGENT_CAP_CLIPBOARD           ] = "clipboard (old)",
    [ VD_AGENT_CAP_DISPLAY_CONFIG      ] = "display config",
    [ VD_AGENT_CAP_CLIPBOARD_BY_DEMAND ] = "clipboard",
    [ VD_AGENT_CAP_CLIPBOARD_SELECTION ] = "clipboard selection",
    [ VD_AGENT_CAP_SPARSE_MONITORS_CONFIG ] = "sparse monitors",
    [ VD_AGENT_CAP_GUEST_LINEEND_LF    ] = "line-end lf",
    [ VD_AGENT_CAP_GUEST_LINEEND_CRLF  ] = "line-end crlf",
    [ VD_AGENT_CAP_MAX_CLIPBOARD       ] = "max-clipboard",
};
#define NAME(_a, _i) ((_i) < SPICE_N_ELEMENTS(_a) ? (_a[(_i)] ?: "?") : "?")

/* ------------------------------------------------------------------ */

static gboolean test_agent_cap(SpiceMainChannel *channel, guint32 cap)
{
    SpiceMainChannelPrivate *c = channel->priv;

    if (!c->agent_caps_received)
        return FALSE;

    return VD_AGENT_HAS_CAPABILITY(c->agent_caps, G_N_ELEMENTS(c->agent_caps), cap);
}

static void spice_main_channel_reset_capabilties(SpiceChannel *channel)
{
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE);
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_MAIN_CAP_NAME_AND_UUID);
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS);
    spice_channel_set_capability(SPICE_CHANNEL(channel), SPICE_MAIN_CAP_SEAMLESS_MIGRATE);
}

static void spice_main_channel_init(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c;

    c = channel->priv = SPICE_MAIN_CHANNEL_GET_PRIVATE(channel);
    c->agent_msg_queue = g_queue_new();
    c->file_xfer_tasks = g_hash_table_new(g_direct_hash, g_direct_equal);

    spice_main_channel_reset_capabilties(SPICE_CHANNEL(channel));
}

static gint spice_main_get_max_clipboard(SpiceMainChannel *self)
{
    g_return_val_if_fail(SPICE_IS_MAIN_CHANNEL(self), 0);

    if (g_getenv("SPICE_MAX_CLIPBOARD"))
        return atoi(g_getenv("SPICE_MAX_CLIPBOARD"));

    return self->priv->max_clipboard;
}

static void spice_main_get_property(GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    SpiceMainChannel *self = SPICE_MAIN_CHANNEL(object);
    SpiceMainChannelPrivate *c = self->priv;

    switch (prop_id) {
    case PROP_MOUSE_MODE:
        g_value_set_int(value, c->mouse_mode);
	break;
    case PROP_AGENT_CONNECTED:
        g_value_set_boolean(value, c->agent_connected);
	break;
    case PROP_AGENT_CAPS_0:
        g_value_set_int(value, c->agent_caps[0]);
	break;
    case PROP_DISPLAY_DISABLE_WALLPAPER:
        g_value_set_boolean(value, c->display_disable_wallpaper);
        break;
    case PROP_DISPLAY_DISABLE_FONT_SMOOTH:
        g_value_set_boolean(value, c->display_disable_font_smooth);
        break;
    case PROP_DISPLAY_DISABLE_ANIMATION:
        g_value_set_boolean(value, c->display_disable_animation);
        break;
    case PROP_DISPLAY_COLOR_DEPTH:
        g_value_set_uint(value, c->display_color_depth);
        break;
    case PROP_DISABLE_DISPLAY_POSITION:
        g_value_set_boolean(value, c->disable_display_position);
        break;
    case PROP_DISABLE_DISPLAY_ALIGN:
        g_value_set_boolean(value, c->disable_display_align);
        break;
    case PROP_MAX_CLIPBOARD:
        g_value_set_int(value, spice_main_get_max_clipboard(self));
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	break;
    }
}

static void spice_main_set_property(GObject *gobject, guint prop_id,
                                    const GValue *value, GParamSpec *pspec)
{
    SpiceMainChannel *self = SPICE_MAIN_CHANNEL(gobject);
    SpiceMainChannelPrivate *c = self->priv;

    switch (prop_id) {
    case PROP_DISPLAY_DISABLE_WALLPAPER:
        c->display_disable_wallpaper = g_value_get_boolean(value);
        break;
    case PROP_DISPLAY_DISABLE_FONT_SMOOTH:
        c->display_disable_font_smooth = g_value_get_boolean(value);
        break;
    case PROP_DISPLAY_DISABLE_ANIMATION:
        c->display_disable_animation = g_value_get_boolean(value);
        break;
    case PROP_DISPLAY_COLOR_DEPTH: {
        guint color_depth = g_value_get_uint(value);
        g_return_if_fail(color_depth % 8 == 0);
        c->display_color_depth = color_depth;
        break;
    }
    case PROP_DISABLE_DISPLAY_POSITION:
        c->disable_display_position = g_value_get_boolean(value);
        break;
    case PROP_DISABLE_DISPLAY_ALIGN:
        c->disable_display_align = g_value_get_boolean(value);
        break;
    case PROP_MAX_CLIPBOARD:
        spice_main_set_max_clipboard(self, g_value_get_int(value));
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
	break;
    }
}

static void spice_main_channel_dispose(GObject *obj)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(obj)->priv;

    if (c->timer_id) {
        g_source_remove(c->timer_id);
        c->timer_id = 0;
    }

    if (c->switch_host_delayed_id) {
        g_source_remove(c->switch_host_delayed_id);
        c->switch_host_delayed_id = 0;
    }

    if (c->migrate_delayed_id) {
        g_source_remove(c->migrate_delayed_id);
        c->migrate_delayed_id = 0;
    }

    if (G_OBJECT_CLASS(spice_main_channel_parent_class)->dispose)
        G_OBJECT_CLASS(spice_main_channel_parent_class)->dispose(obj);
}

static void spice_main_channel_finalize(GObject *obj)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(obj)->priv;

    g_free(c->agent_msg_data);
    agent_free_msg_queue(SPICE_MAIN_CHANNEL(obj));
    if (c->file_xfer_tasks)
        g_hash_table_unref(c->file_xfer_tasks);

    if (G_OBJECT_CLASS(spice_main_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_main_channel_parent_class)->finalize(obj);
}

/* coroutine context */
static void spice_channel_iterate_write(SpiceChannel *channel)
{
    agent_send_msg_queue(SPICE_MAIN_CHANNEL(channel));

    if (SPICE_CHANNEL_CLASS(spice_main_channel_parent_class)->iterate_write)
        SPICE_CHANNEL_CLASS(spice_main_channel_parent_class)->iterate_write(channel);
}

/* main or coroutine context */
static void spice_main_channel_reset_agent(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    GError *error;
    GList *tasks;
    GList *l;

    c->agent_connected = FALSE;
    c->agent_caps_received = FALSE;
    c->agent_display_config_sent = FALSE;
    c->agent_msg_pos = 0;
    g_free(c->agent_msg_data);
    c->agent_msg_data = NULL;
    c->agent_msg_size = 0;

    tasks = g_hash_table_get_values(c->file_xfer_tasks);
    for (l = tasks; l != NULL; l = l->next) {
        SpiceFileXferTask *task = (SpiceFileXferTask *)l->data;

        error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "Agent connection closed");
        file_xfer_completed(task, error);
    }
    g_list_free(tasks);
    file_xfer_flushed(channel, FALSE);
}

/* main or coroutine context */
static void spice_main_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    /* This is not part of reset_agent, since the spice-server expects any
       pending multi-chunk messages to be completed by the client, even after
       it has send an agent-disconnected msg as that is what the original
       spicec did. Also see the TODO in server/reds.c reds_reset_vdp() */
    c->agent_tokens = 0;
    agent_free_msg_queue(SPICE_MAIN_CHANNEL(channel));
    c->agent_msg_queue = g_queue_new();

    set_agent_connected(SPICE_MAIN_CHANNEL(channel), FALSE);

    SPICE_CHANNEL_CLASS(spice_main_channel_parent_class)->channel_reset(channel, migrating);
}

static void spice_main_constructed(GObject *object)
{
    SpiceMainChannel *self = SPICE_MAIN_CHANNEL(object);
    SpiceMainChannelPrivate *c = self->priv;

    /* update default value */
    c->max_clipboard = spice_main_get_max_clipboard(self);

    if (G_OBJECT_CLASS(spice_main_channel_parent_class)->constructed)
        G_OBJECT_CLASS(spice_main_channel_parent_class)->constructed(object);
}

static void spice_main_channel_class_init(SpiceMainChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->dispose      = spice_main_channel_dispose;
    gobject_class->finalize     = spice_main_channel_finalize;
    gobject_class->get_property = spice_main_get_property;
    gobject_class->set_property = spice_main_set_property;
    gobject_class->constructed  = spice_main_constructed;

    channel_class->handle_msg    = spice_main_handle_msg;
    channel_class->iterate_write = spice_channel_iterate_write;
    channel_class->channel_reset = spice_main_channel_reset;
    channel_class->channel_reset_capabilities = spice_main_channel_reset_capabilties;
    channel_class->channel_send_migration_handshake = spice_main_channel_send_migration_handshake;

    /**
     * SpiceMainChannel:mouse-mode:
     *
     * Spice protocol specifies two mouse modes, client mode and
     * server mode. In client mode (%SPICE_MOUSE_MODE_CLIENT), the
     * affective mouse is the client side mouse: the client sends
     * mouse position within the display and the server sends mouse
     * shape messages. In server mode (%SPICE_MOUSE_MODE_SERVER), the
     * client sends relative mouse movements and the server sends
     * position and shape commands.
     **/
    g_object_class_install_property
        (gobject_class, PROP_MOUSE_MODE,
         g_param_spec_int("mouse-mode",
                          "Mouse mode",
                          "Mouse mode",
                          0, INT_MAX, 0,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

    g_object_class_install_property
        (gobject_class, PROP_AGENT_CONNECTED,
         g_param_spec_boolean("agent-connected",
                              "Agent connected",
                              "Whether the agent is connected",
                              FALSE,
                              G_PARAM_READABLE |
                              G_PARAM_STATIC_NAME |
                              G_PARAM_STATIC_NICK |
                              G_PARAM_STATIC_BLURB));

    g_object_class_install_property
        (gobject_class, PROP_AGENT_CAPS_0,
         g_param_spec_int("agent-caps-0",
                          "Agent caps 0",
                          "Agent capability bits 0 -> 31",
                          0, INT_MAX, 0,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

    g_object_class_install_property
        (gobject_class, PROP_DISPLAY_DISABLE_WALLPAPER,
         g_param_spec_boolean("disable-wallpaper",
                              "Disable guest wallpaper",
                              "Disable guest wallpaper",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_DISPLAY_DISABLE_FONT_SMOOTH,
         g_param_spec_boolean("disable-font-smooth",
                              "Disable guest font smooth",
                              "Disable guest font smoothing",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_DISPLAY_DISABLE_ANIMATION,
         g_param_spec_boolean("disable-animation",
                              "Disable guest animations",
                              "Disable guest animations",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_DISABLE_DISPLAY_POSITION,
         g_param_spec_boolean("disable-display-position",
                              "Disable display position",
                              "Disable using display position when setting monitor config",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_DISPLAY_COLOR_DEPTH,
         g_param_spec_uint("color-depth",
                           "Color depth",
                           "Color depth", 0, 32, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_STATIC_STRINGS));

    /**
     * SpiceMainChannel:disable-display-align:
     *
     * Disable automatic horizontal display position alignment.
     *
     * Since: 0.13
     */
    g_object_class_install_property
        (gobject_class, PROP_DISABLE_DISPLAY_ALIGN,
         g_param_spec_boolean("disable-display-align",
                              "Disable display align",
                              "Disable display position alignment",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceMainChannel:max-clipboard:
     *
     * Maximum size of clipboard operations in bytes (default 100MB,
     * -1 for unlimited size);
     *
     * Since: 0.22
     **/
    g_object_class_install_property
        (gobject_class, PROP_MAX_CLIPBOARD,
         g_param_spec_int("max-clipboard",
                          "max clipboard",
                          "Maximum clipboard data size",
                          -1, G_MAXINT, 100 * 1024 * 1024,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /* TODO use notify instead */
    /**
     * SpiceMainChannel::main-mouse-update:
     * @main: the #SpiceMainChannel that emitted the signal
     *
     * Notify when the mouse mode has changed.
     **/
    signals[SPICE_MAIN_MOUSE_UPDATE] =
        g_signal_new("main-mouse-update",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceMainChannelClass, mouse_update),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /* TODO use notify instead */
    /**
     * SpiceMainChannel::main-agent-update:
     * @main: the #SpiceMainChannel that emitted the signal
     *
     * Notify when the %SpiceMainChannel:agent-connected or
     * %SpiceMainChannel:agent-caps-0 property change.
     **/
    signals[SPICE_MAIN_AGENT_UPDATE] =
        g_signal_new("main-agent-update",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceMainChannelClass, agent_update),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
    /**
     * SpiceMainChannel::main-clipboard:
     * @main: the #SpiceMainChannel that emitted the signal
     * @type: the VD_AGENT_CLIPBOARD data type
     * @data: clipboard data
     * @size: size of @data in bytes
     *
     * Provides guest clipboard data requested by spice_main_clipboard_request().
     *
     * Deprecated: 0.6: use SpiceMainChannel::main-clipboard-selection instead.
     **/
    signals[SPICE_MAIN_CLIPBOARD] =
        g_signal_new("main-clipboard",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__UINT_POINTER_UINT,
                     G_TYPE_NONE,
                     3,
                     G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-selection:
     * @main: the #SpiceMainChannel that emitted the signal
     * @selection: a VD_AGENT_CLIPBOARD_SELECTION clipboard
     * @type: the VD_AGENT_CLIPBOARD data type
     * @data: clipboard data
     * @size: size of @data in bytes
     *
     * Since: 0.6
     **/
    signals[SPICE_MAIN_CLIPBOARD_SELECTION] =
        g_signal_new("main-clipboard-selection",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__UINT_UINT_POINTER_UINT,
                     G_TYPE_NONE,
                     4,
                     G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-grab:
     * @main: the #SpiceMainChannel that emitted the signal
     * @types: the VD_AGENT_CLIPBOARD data types
     * @ntypes: the number of @types
     *
     * Inform when clipboard data is available from the guest, and for
     * which @types.
     *
     * Deprecated: 0.6: use SpiceMainChannel::main-clipboard-selection-grab instead.
     **/
    signals[SPICE_MAIN_CLIPBOARD_GRAB] =
        g_signal_new("main-clipboard-grab",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_BOOLEAN__POINTER_UINT,
                     G_TYPE_BOOLEAN,
                     2,
                     G_TYPE_POINTER, G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-selection-grab:
     * @main: the #SpiceMainChannel that emitted the signal
     * @selection: a VD_AGENT_CLIPBOARD_SELECTION clipboard
     * @types: the VD_AGENT_CLIPBOARD data types
     * @ntypes: the number of @types
     *
     * Inform when clipboard data is available from the guest, and for
     * which @types.
     *
     * Since: 0.6
     **/
    signals[SPICE_MAIN_CLIPBOARD_SELECTION_GRAB] =
        g_signal_new("main-clipboard-selection-grab",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_BOOLEAN__UINT_POINTER_UINT,
                     G_TYPE_BOOLEAN,
                     3,
                     G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-request:
     * @main: the #SpiceMainChannel that emitted the signal
     * @types: the VD_AGENT_CLIPBOARD request type
     *
     * Return value: %TRUE if the request is successful
     *
     * Request clipbard data from the client.
     *
     * Deprecated: 0.6: use SpiceMainChannel::main-clipboard-selection-request instead.
     **/
    signals[SPICE_MAIN_CLIPBOARD_REQUEST] =
        g_signal_new("main-clipboard-request",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_BOOLEAN__UINT,
                     G_TYPE_BOOLEAN,
                     1,
                     G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-selection-request:
     * @main: the #SpiceMainChannel that emitted the signal
     * @selection: a VD_AGENT_CLIPBOARD_SELECTION clipboard
     * @types: the VD_AGENT_CLIPBOARD request type
     *
     * Return value: %TRUE if the request is successful
     *
     * Request clipbard data from the client.
     *
     * Since: 0.6
     **/
    signals[SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST] =
        g_signal_new("main-clipboard-selection-request",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_user_marshal_BOOLEAN__UINT_UINT,
                     G_TYPE_BOOLEAN,
                     2,
                     G_TYPE_UINT, G_TYPE_UINT);

    /**
     * SpiceMainChannel::main-clipboard-release:
     * @main: the #SpiceMainChannel that emitted the signal
     *
     * Inform when the clipboard is released from the guest, when no
     * clipboard data is available from the guest.
     *
     * Deprecated: 0.6: use SpiceMainChannel::main-clipboard-selection-release instead.
     **/
    signals[SPICE_MAIN_CLIPBOARD_RELEASE] =
        g_signal_new("main-clipboard-release",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /**
     * SpiceMainChannel::main-clipboard-selection-release:
     * @main: the #SpiceMainChannel that emitted the signal
     * @selection: a VD_AGENT_CLIPBOARD_SELECTION clipboard
     *
     * Inform when the clipboard is released from the guest, when no
     * clipboard data is available from the guest.
     *
     * Since: 0.6
     **/
    signals[SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE] =
        g_signal_new("main-clipboard-selection-release",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_UINT);

    /**
     * SpiceMainChannel::migration-started:
     * @main: the #SpiceMainChannel that emitted the signal
     * @session: a migration #SpiceSession
     *
     * Inform when migration is starting. Application wishing to make
     * connections themself can set the #SpiceSession:client-sockets
     * to @TRUE, then follow #SpiceSession::channel-new creation, and
     * use spice_channel_open_fd() once the socket is created.
     *
     **/
    signals[SPICE_MIGRATION_STARTED] =
        g_signal_new("migration-started",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_OBJECT);

    g_type_class_add_private(klass, sizeof(SpiceMainChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_MAIN_CLIPBOARD_RELEASE {
};

struct SPICE_MAIN_AGENT_UPDATE {
};

struct SPICE_MAIN_MOUSE_UPDATE {
};

struct SPICE_MAIN_CLIPBOARD {
    guint type;
    gpointer data;
    gsize size;
};

struct SPICE_MAIN_CLIPBOARD_GRAB {
    gpointer types;
    gsize ntypes;
    gboolean *ret;
};

struct SPICE_MAIN_CLIPBOARD_REQUEST {
    guint type;
    gboolean *ret;
};

struct SPICE_MAIN_CLIPBOARD_SELECTION {
    guint8 selection;
    guint type;
    gpointer data;
    gsize size;
};

struct SPICE_MAIN_CLIPBOARD_SELECTION_GRAB {
    guint8 selection;
    gpointer types;
    gsize ntypes;
    gboolean *ret;
};

struct SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST {
    guint8 selection;
    guint type;
    gboolean *ret;
};

struct SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE {
    guint8 selection;
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_MAIN_CLIPBOARD_RELEASE:
    case SPICE_MAIN_AGENT_UPDATE:
    case SPICE_MAIN_MOUSE_UPDATE: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    case SPICE_MAIN_CLIPBOARD: {
        struct SPICE_MAIN_CLIPBOARD *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->type, p->data, p->size);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_GRAB: {
        struct SPICE_MAIN_CLIPBOARD_GRAB *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->types, p->ntypes, p->ret);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_REQUEST: {
        struct SPICE_MAIN_CLIPBOARD_REQUEST *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->type, p->ret);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_SELECTION: {
        struct SPICE_MAIN_CLIPBOARD_SELECTION *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->selection, p->type, p->data, p->size);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_SELECTION_GRAB: {
        struct SPICE_MAIN_CLIPBOARD_SELECTION_GRAB *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->selection, p->types, p->ntypes, p->ret);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST: {
        struct SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->selection, p->type, p->ret);
        break;
    }
    case SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE: {
        struct SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE *p = params;
        g_signal_emit(object, signals[signum], 0,
                      p->selection);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* ------------------------------------------------------------------ */


static void agent_free_msg_queue(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceMsgOut *out;

    if (!c->agent_msg_queue)
        return;

    while (!g_queue_is_empty(c->agent_msg_queue)) {
        out = g_queue_pop_head(c->agent_msg_queue);
        spice_msg_out_unref(out);
    }

    g_queue_free(c->agent_msg_queue);
    c->agent_msg_queue = NULL;
}

/* Here, flushing algorithm is stolen from spice-channel.c */
static void file_xfer_flushed(SpiceMainChannel *channel, gboolean success)
{
    SpiceMainChannelPrivate *c = channel->priv;
    GSList *l;

    for (l = c->flushing; l != NULL; l = l->next) {
        GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT(l->data);
        g_simple_async_result_set_op_res_gboolean(result, success);
        g_simple_async_result_complete_in_idle(result);
    }

    g_slist_free_full(c->flushing, g_object_unref);
    c->flushing = NULL;
}

static void file_xfer_flush_async(SpiceMainChannel *channel, GCancellable *cancellable,
                                  GAsyncReadyCallback callback, gpointer user_data)
{
    GSimpleAsyncResult *simple;
    SpiceMainChannelPrivate *c = channel->priv;
    gboolean was_empty;

    simple = g_simple_async_result_new(G_OBJECT(channel), callback, user_data,
                                       file_xfer_flush_async);

    was_empty = g_queue_is_empty(c->agent_msg_queue);
    if (was_empty) {
        g_simple_async_result_set_op_res_gboolean(simple, TRUE);
        g_simple_async_result_complete_in_idle(simple);
        g_object_unref(simple);
        return;
    }

    c->flushing = g_slist_append(c->flushing, simple);
}

static gboolean file_xfer_flush_finish(SpiceMainChannel *channel, GAsyncResult *result,
                                       GError **error)
{
    GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;

    g_return_val_if_fail(g_simple_async_result_is_valid(result,
        G_OBJECT(channel), file_xfer_flush_async), FALSE);

    if (g_simple_async_result_propagate_error(simple, error)) {
        return FALSE;
    }

    CHANNEL_DEBUG(channel, "flushed finished!");
    return g_simple_async_result_get_op_res_gboolean(simple);
}

/* coroutine context */
static void agent_send_msg_queue(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceMsgOut *out;

    while (c->agent_tokens > 0 &&
           !g_queue_is_empty(c->agent_msg_queue)) {
        c->agent_tokens--;
        out = g_queue_pop_head(c->agent_msg_queue);
        spice_msg_out_send_internal(out);
    }
    if (g_queue_is_empty(c->agent_msg_queue) && c->flushing != NULL) {
        file_xfer_flushed(channel, TRUE);
    }
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue()

   expected arguments, pair of data/data_size to send terminated with NULL:
   agent_msg_queue_many(main, VD_AGENT_...,
                        &foo, sizeof(Foo),
                        data, data_size, NULL);
*/
G_GNUC_NULL_TERMINATED
static void agent_msg_queue_many(SpiceMainChannel *channel, int type, const void *data, ...)
{
    va_list args;
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceMsgOut *out;
    VDAgentMessage msg;
    guint8 *payload;
    gsize paysize, s, mins, size = 0;
    const guint8 *d;

    G_STATIC_ASSERT(VD_AGENT_MAX_DATA_SIZE > sizeof(VDAgentMessage));

    va_start(args, data);
    for (d = data; d != NULL; d = va_arg(args, void*)) {
        size += va_arg(args, gsize);
    }
    va_end(args);

    msg.protocol = VD_AGENT_PROTOCOL;
    msg.type = type;
    msg.opaque = 0;
    msg.size = size;

    paysize = MIN(VD_AGENT_MAX_DATA_SIZE, size + sizeof(VDAgentMessage));
    out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MAIN_AGENT_DATA);
    payload = spice_marshaller_reserve_space(out->marshaller, paysize);
    memcpy(payload, &msg, sizeof(VDAgentMessage));
    payload += sizeof(VDAgentMessage);
    paysize -= sizeof(VDAgentMessage);
    if (paysize == 0) {
        g_queue_push_tail(c->agent_msg_queue, out);
        out = NULL;
    }

    va_start(args, data);
    for (d = data; size > 0; d = va_arg(args, void*)) {
        s = va_arg(args, gsize);
        while (s > 0) {
            if (out == NULL) {
                paysize = MIN(VD_AGENT_MAX_DATA_SIZE, size);
                out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MAIN_AGENT_DATA);
                payload = spice_marshaller_reserve_space(out->marshaller, paysize);
            }
            mins = MIN(paysize, s);
            memcpy(payload, d, mins);
            d += mins;
            payload += mins;
            s -= mins;
            size -= mins;
            paysize -= mins;
            if (paysize == 0) {
                g_queue_push_tail(c->agent_msg_queue, out);
                out = NULL;
            }
        }
    }
    va_end(args);
    g_warn_if_fail(out == NULL);
}

static int monitors_cmp(const void *p1, const void *p2, gpointer user_data)
{
    const VDAgentMonConfig *m1 = p1;
    const VDAgentMonConfig *m2 = p2;
    double d1 = sqrt(m1->x * m1->x + m1->y * m1->y);
    double d2 = sqrt(m2->x * m2->x + m2->y * m2->y);
    int diff = d1 - d2;

    return diff == 0 ? (char*)p1 - (char*)p2 : diff;
}

static void monitors_align(VDAgentMonConfig *monitors, int nmonitors)
{
    gint i, j, x = 0;
    guint32 used = 0;
    VDAgentMonConfig *sorted_monitors;

    if (nmonitors == 0)
        return;

    /* sort by distance from origin */
    sorted_monitors = g_memdup(monitors, nmonitors * sizeof(VDAgentMonConfig));
    g_qsort_with_data(sorted_monitors, nmonitors, sizeof(VDAgentMonConfig), monitors_cmp, NULL);

    /* super-KISS ltr alignment, feel free to improve */
    for (i = 0; i < nmonitors; i++) {
        /* Find where this monitor is in the sorted order */
        for (j = 0; j < nmonitors; j++) {
            /* Avoid using the same entry twice, this happens with older
               virt-viewer versions which always set x and y to 0 */
            if (used & (1 << j))
                continue;
            if (memcmp(&monitors[j], &sorted_monitors[i],
                       sizeof(VDAgentMonConfig)) == 0)
                break;
        }
        used |= 1 << j;
        monitors[j].x = x;
        monitors[j].y = 0;
        x += monitors[j].width;
        if (monitors[j].width || monitors[j].height)
            SPICE_DEBUG("#%d +%d+%d-%dx%d", j, monitors[j].x, monitors[j].y,
                        monitors[j].width, monitors[j].height);
    }
    g_free(sorted_monitors);
}


#define agent_msg_queue(Channel, Type, Size, Data) \
    agent_msg_queue_many((Channel), (Type), (Data), (Size), NULL)

/**
 * spice_main_send_monitor_config:
 * @channel:
 *
 * Send monitors configuration previously set with
 * spice_main_set_display() and spice_main_set_display_enabled()
 *
 * Returns: %TRUE on success.
 **/
gboolean spice_main_send_monitor_config(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c;
    VDAgentMonitorsConfig *mon;
    int i, j, monitors;
    size_t size;

    g_return_val_if_fail(SPICE_IS_MAIN_CHANNEL(channel), FALSE);
    c = channel->priv;
    g_return_val_if_fail(c->agent_connected, FALSE);

    if (spice_main_agent_test_capability(channel,
                                     VD_AGENT_CAP_SPARSE_MONITORS_CONFIG)) {
        monitors = SPICE_N_ELEMENTS(c->display);
    } else {
        monitors = 0;
        for (i = 0; i < SPICE_N_ELEMENTS(c->display); i++) {
            if (c->display[i].enabled)
                monitors += 1;
        }
    }

    size = sizeof(VDAgentMonitorsConfig) + sizeof(VDAgentMonConfig) * monitors;
    mon = spice_malloc0(size);

    mon->num_of_monitors = monitors;
    if (c->disable_display_position == FALSE ||
        c->disable_display_align == FALSE)
        mon->flags |= VD_AGENT_CONFIG_MONITORS_FLAG_USE_POS;

    j = 0;
    for (i = 0; i < SPICE_N_ELEMENTS(c->display); i++) {
        if (!c->display[i].enabled) {
            if (spice_main_agent_test_capability(channel,
                                     VD_AGENT_CAP_SPARSE_MONITORS_CONFIG))
                j++;
            continue;
        }
        mon->monitors[j].depth  = c->display_color_depth ? c->display_color_depth : 32;
        mon->monitors[j].width  = c->display[i].width;
        mon->monitors[j].height = c->display[i].height;
        mon->monitors[j].x = c->display[i].x;
        mon->monitors[j].y = c->display[i].y;
        CHANNEL_DEBUG(channel, "monitor config: #%d %dx%d+%d+%d @ %d bpp", j,
                      mon->monitors[j].width, mon->monitors[j].height,
                      mon->monitors[j].x, mon->monitors[j].y,
                      mon->monitors[j].depth);
        j++;
    }

    if (c->disable_display_align == FALSE)
        monitors_align(mon->monitors, mon->num_of_monitors);

    agent_msg_queue(channel, VD_AGENT_MONITORS_CONFIG, size, mon);
    free(mon);

    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
    if (c->timer_id != 0) {
        g_source_remove(c->timer_id);
        c->timer_id = 0;
    }

    return TRUE;
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_display_config(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    VDAgentDisplayConfig config = { 0, };

    if (c->display_disable_wallpaper) {
        config.flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_WALLPAPER;
    }

    if (c->display_disable_font_smooth) {
        config.flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_FONT_SMOOTH;
    }

    if (c->display_disable_animation) {
        config.flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_ANIMATION;
    }

    if (c->display_color_depth != 0) {
        config.flags |= VD_AGENT_DISPLAY_CONFIG_FLAG_SET_COLOR_DEPTH;
        config.depth = c->display_color_depth;
    }

    CHANNEL_DEBUG(channel, "display_config: flags: %u, depth: %u", config.flags, config.depth);

    agent_msg_queue(channel, VD_AGENT_DISPLAY_CONFIG, sizeof(VDAgentDisplayConfig), &config);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_announce_caps(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    VDAgentAnnounceCapabilities *caps;
    size_t size;

    if (!c->agent_connected)
        return;

    size = sizeof(VDAgentAnnounceCapabilities) + VD_AGENT_CAPS_BYTES;
    caps = spice_malloc0(size);
    if (!c->agent_caps_received)
        caps->request = 1;
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_MOUSE_STATE);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_MONITORS_CONFIG);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_REPLY);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_DISPLAY_CONFIG);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND);
    VD_AGENT_SET_CAPABILITY(caps->caps, VD_AGENT_CAP_CLIPBOARD_SELECTION);

    agent_msg_queue(channel, VD_AGENT_ANNOUNCE_CAPABILITIES, size, caps);
    free(caps);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_clipboard_grab(SpiceMainChannel *channel, guint selection,
                                 guint32 *types, int ntypes)
{
    SpiceMainChannelPrivate *c = channel->priv;
    guint8 *msg;
    VDAgentClipboardGrab *grab;
    size_t size;
    int i;

    if (!c->agent_connected)
        return;

    g_return_if_fail(test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND));

    size = sizeof(VDAgentClipboardGrab) + sizeof(uint32_t) * ntypes;
    if (test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        size += 4;
    } else if (selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        CHANNEL_DEBUG(channel, "Ignoring clipboard grab");
        return;
    }

    msg = g_alloca(size);
    memset(msg, 0, size);

    grab = (VDAgentClipboardGrab *)msg;

    if (test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msg[0] = selection;
        grab = (VDAgentClipboardGrab *)(msg + 4);
    }

    for (i = 0; i < ntypes; i++) {
        grab->types[i] = types[i];
    }

    agent_msg_queue(channel, VD_AGENT_CLIPBOARD_GRAB, size, msg);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_clipboard_notify(SpiceMainChannel *self, guint selection,
                                   guint32 type, const guchar *data, size_t size)
{
    SpiceMainChannelPrivate *c = self->priv;
    VDAgentClipboard *cb;
    guint8 *msg;
    size_t msgsize;
    gint max_clipboard = spice_main_get_max_clipboard(self);

    g_return_if_fail(c->agent_connected);
    g_return_if_fail(test_agent_cap(self, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND));
    g_return_if_fail(max_clipboard == -1 || size < max_clipboard);

    msgsize = sizeof(VDAgentClipboard);
    if (test_agent_cap(self, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msgsize += 4;
    } else if (selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        CHANNEL_DEBUG(self, "Ignoring clipboard notify");
        return;
    }

    msg = g_alloca(msgsize);
    memset(msg, 0, msgsize);

    cb = (VDAgentClipboard *)msg;

    if (test_agent_cap(self, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msg[0] = selection;
        cb = (VDAgentClipboard *)(msg + 4);
    }

    cb->type = type;
    agent_msg_queue_many(self, VD_AGENT_CLIPBOARD, msg, msgsize, data, size, NULL);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_clipboard_request(SpiceMainChannel *channel, guint selection, guint32 type)
{
    SpiceMainChannelPrivate *c = channel->priv;
    VDAgentClipboardRequest *request;
    guint8 *msg;
    size_t msgsize;

    g_return_if_fail(c->agent_connected);
    g_return_if_fail(test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND));

    msgsize = sizeof(VDAgentClipboardRequest);
    if (test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msgsize += 4;
    } else if (selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        SPICE_DEBUG("Ignoring clipboard request");
        return;
    }

    msg = g_alloca(msgsize);
    memset(msg, 0, msgsize);

    request = (VDAgentClipboardRequest *)msg;

    if (test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msg[0] = selection;
        request = (VDAgentClipboardRequest *)(msg + 4);
    }

    request->type = type;

    agent_msg_queue(channel, VD_AGENT_CLIPBOARD_REQUEST, msgsize, msg);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_clipboard_release(SpiceMainChannel *channel, guint selection)
{
    SpiceMainChannelPrivate *c = channel->priv;
    guint8 msg[4] = { 0, };
    guint8 msgsize = 0;

    g_return_if_fail(c->agent_connected);
    g_return_if_fail(test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND));

    if (test_agent_cap(channel, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
        msg[0] = selection;
        msgsize += 4;
    } else if (selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        SPICE_DEBUG("Ignoring clipboard release");
        return;
    }

    agent_msg_queue(channel, VD_AGENT_CLIPBOARD_RELEASE, msgsize, msg);
}

/* main context*/
static gboolean timer_set_display(gpointer data)
{
    SpiceMainChannel *channel = data;
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceSession *session;
    gint i;

    c->timer_id = 0;
    if (!c->agent_connected)
        return FALSE;

    session = spice_channel_get_session(SPICE_CHANNEL(channel));

    /* ensure we have an explicit monitor configuration at least for
       number of display channels */
    for (i = 0; i < session->priv->display_channels_count; i++)
        if (!c->display[i].enabled_set) {
            SPICE_DEBUG("Not sending monitors config, missing monitors");
            return FALSE;
        }

    spice_main_send_monitor_config(channel);

    return FALSE;
}

/* any context  */
static void update_display_timer(SpiceMainChannel *channel, guint seconds)
{
    SpiceMainChannelPrivate *c = channel->priv;

    if (c->timer_id)
        g_source_remove(c->timer_id);

    c->timer_id = g_timeout_add_seconds(seconds, timer_set_display, channel);
}

/* coroutine context  */
static void set_agent_connected(SpiceMainChannel *channel, gboolean connected)
{
    SpiceMainChannelPrivate *c = channel->priv;

    SPICE_DEBUG("agent connected: %s", spice_yes_no(connected));
    if (connected != c->agent_connected) {
        c->agent_connected = connected;
        g_object_notify_main_context(G_OBJECT(channel), "agent-connected");
    }
    if (!connected)
        spice_main_channel_reset_agent(SPICE_MAIN_CHANNEL(channel));

    emit_main_context(channel, SPICE_MAIN_AGENT_UPDATE);
}

/* coroutine context  */
static void agent_start(SpiceMainChannel *channel)
{
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceMsgcMainAgentStart agent_start = {
        .num_tokens = ~0,
    };
    SpiceMsgOut *out;

    c->agent_caps_received = false;
    set_agent_connected(channel, TRUE);

    out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MAIN_AGENT_START);
    out->marshallers->msgc_main_agent_start(out->marshaller, &agent_start);
    spice_msg_out_send_internal(out);

    if (c->agent_connected) {
        agent_announce_caps(channel);
        agent_send_msg_queue(channel);
    }
}

/* coroutine context  */
static void agent_stopped(SpiceMainChannel *channel)
{
    set_agent_connected(channel, FALSE);
}

/* coroutine context */
static void set_mouse_mode(SpiceMainChannel *channel, uint32_t supported, uint32_t current)
{
    SpiceMainChannelPrivate *c = channel->priv;

    if (c->mouse_mode != current) {
        c->mouse_mode = current;
        emit_main_context(channel, SPICE_MAIN_MOUSE_UPDATE);
        g_object_notify_main_context(G_OBJECT(channel), "mouse-mode");
    }

    /* switch to client mode if possible */
    if (!spice_channel_get_read_only(SPICE_CHANNEL(channel)) &&
        supported & SPICE_MOUSE_MODE_CLIENT &&
        current != SPICE_MOUSE_MODE_CLIENT) {
        SpiceMsgcMainMouseModeRequest req = {
            .mode = SPICE_MOUSE_MODE_CLIENT,
        };
        SpiceMsgOut *out;

        out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MAIN_MOUSE_MODE_REQUEST);
        out->marshallers->msgc_main_mouse_mode_request(out->marshaller, &req);
        spice_msg_out_send_internal(out);
    }
}

/* coroutine context */
static void main_handle_init(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;
    SpiceMsgMainInit *init = spice_msg_in_parsed(in);
    SpiceSession *session;
    SpiceMsgOut *out;

    session = spice_channel_get_session(channel);
    spice_session_set_connection_id(session, init->session_id);

    set_mouse_mode(SPICE_MAIN_CHANNEL(channel), init->supported_mouse_modes,
                   init->current_mouse_mode);

    spice_session_set_mm_time(session, init->multi_media_time);
    spice_session_set_caches_hints(session, init->ram_hint, init->display_channels_hint);

    c->agent_tokens = init->agent_tokens;
    if (init->agent_connected)
        agent_start(SPICE_MAIN_CHANNEL(channel));

    if (spice_session_migrate_after_main_init(session))
        return;

    out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MAIN_ATTACH_CHANNELS);
    spice_msg_out_send_internal(out);
}

/* coroutine context */
static void main_handle_name(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainName *name = spice_msg_in_parsed(in);
    SpiceSession *session = spice_channel_get_session(channel);

    SPICE_DEBUG("server name: %s", name->name);
    spice_session_set_name(session, (const gchar *)name->name);
}

/* coroutine context */
static void main_handle_uuid(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainUuid *uuid = spice_msg_in_parsed(in);
    SpiceSession *session = spice_channel_get_session(channel);
    gchar *uuid_str = spice_uuid_to_string(uuid->uuid);

    SPICE_DEBUG("server uuid: %s", uuid_str);
    spice_session_set_uuid(session, uuid->uuid);

    g_free(uuid_str);
}

/* coroutine context */
static void main_handle_mm_time(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceSession *session;
    SpiceMsgMainMultiMediaTime *msg = spice_msg_in_parsed(in);

    session = spice_channel_get_session(channel);
    spice_session_set_mm_time(session, msg->time);
}

typedef struct channel_new {
    SpiceSession *session;
    int type;
    int id;
} channel_new_t;

/* main context */
static gboolean _channel_new(channel_new_t *c)
{
    g_return_val_if_fail(c != NULL, FALSE);

    spice_channel_new(c->session, c->type, c->id);

    g_object_unref(c->session);
    g_free(c);

    return FALSE;
}

/* coroutine context */
static void main_handle_channels_list(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgChannels *msg = spice_msg_in_parsed(in);
    SpiceSession *session;
    int i;

    session = spice_channel_get_session(channel);

    /* guarantee that uuid is notified before setting up the channels, even if
     * the server is older and doesn't actually send the uuid */
    g_object_notify_main_context(G_OBJECT(session), "uuid");

    for (i = 0; i < msg->num_of_channels; i++) {
        channel_new_t *c;

        c = g_new(channel_new_t, 1);
        c->session = g_object_ref(session);
        c->type = msg->channels[i].type;
        c->id = msg->channels[i].id;
        /* no need to explicitely switch to main context, since
           synchronous call is not needed. */
        /* no need to track idle, session is refed */
        g_idle_add((GSourceFunc)_channel_new, c);
    }
}

/* coroutine context */
static void main_handle_mouse_mode(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainMouseMode *msg = spice_msg_in_parsed(in);
    set_mouse_mode(SPICE_MAIN_CHANNEL(channel), msg->supported_modes, msg->current_mode);
}

/* coroutine context */
static void main_handle_agent_connected(SpiceChannel *channel, SpiceMsgIn *in)
{
    agent_start(SPICE_MAIN_CHANNEL(channel));
    update_display_timer(SPICE_MAIN_CHANNEL(channel), 0);
}

/* coroutine context */
static void main_handle_agent_connected_tokens(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;
    SpiceMsgMainAgentConnectedTokens *msg = spice_msg_in_parsed(in);

    c->agent_tokens = msg->num_tokens;
    agent_start(SPICE_MAIN_CHANNEL(channel));
    update_display_timer(SPICE_MAIN_CHANNEL(channel), 0);
}

/* coroutine context */
static void main_handle_agent_disconnected(SpiceChannel *channel, SpiceMsgIn *in)
{
    agent_stopped(SPICE_MAIN_CHANNEL(channel));
}

static void file_xfer_task_free(SpiceFileXferTask *task)
{
    SpiceMainChannelPrivate *c;

    g_return_if_fail(task != NULL);

    c = task->channel->priv;
    g_hash_table_remove(c->file_xfer_tasks, GUINT_TO_POINTER(task->id));

    g_clear_object(&task->channel);
    g_clear_object(&task->file);
    g_clear_object(&task->file_stream);
    g_free(task);
}

/* main context */
static void file_xfer_close_cb(GObject      *object,
                               GAsyncResult *close_res,
                               gpointer      user_data)
{
    GSimpleAsyncResult *res;
    SpiceFileXferTask *task;
    GError *error = NULL;

    task = user_data;

    if (object) {
        GInputStream *stream = G_INPUT_STREAM(object);
        g_input_stream_close_finish(stream, close_res, &error);
        if (error) {
            /* This error dont need to report to user, just print a log */
            SPICE_DEBUG("close file error: %s", error->message);
            g_clear_error(&error);
        }
    }

    /* Notify to user that files have been transferred or something error
       happened. */
    res = g_simple_async_result_new(G_OBJECT(task->channel),
                                    task->callback,
                                    task->user_data,
                                    spice_main_file_copy_async);
    if (task->error) {
        g_simple_async_result_take_error(res, task->error);
        g_simple_async_result_set_op_res_gboolean(res, FALSE);
    } else {
        g_simple_async_result_set_op_res_gboolean(res, TRUE);
    }
    g_simple_async_result_complete_in_idle(res);
    g_object_unref(res);

    file_xfer_task_free(task);
}

static void file_xfer_data_flushed_cb(GObject *source_object,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
    SpiceFileXferTask *task = user_data;
    SpiceMainChannel *channel = (SpiceMainChannel *)source_object;
    GError *error = NULL;

    task->pending = FALSE;
    file_xfer_flush_finish(channel, res, &error);
    if (error || task->error) {
        file_xfer_completed(task, error);
        return;
    }

    if (task->progress_callback)
        task->progress_callback(task->read_bytes, task->file_size,
                                task->progress_callback_data);

    /* Read more data */
    file_xfer_continue_read(task);
}

static void file_xfer_queue(SpiceFileXferTask *task, int data_size)
{
    VDAgentFileXferDataMessage msg;
    SpiceMainChannel *channel = SPICE_MAIN_CHANNEL(task->channel);

    msg.id = task->id;
    msg.size = data_size;
    agent_msg_queue_many(channel, VD_AGENT_FILE_XFER_DATA,
                         &msg, sizeof(msg),
                         task->buffer, data_size, NULL);
    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
}

/* main context */
static void file_xfer_read_cb(GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data)
{
    SpiceFileXferTask *task = user_data;
    SpiceMainChannel *channel = task->channel;
    gssize count;
    GError *error = NULL;

    task->pending = FALSE;
    count = g_input_stream_read_finish(G_INPUT_STREAM(task->file_stream),
                                       res, &error);
    /* Check for pending earlier errors */
    if (task->error) {
        file_xfer_completed(task, error);
        return;
    }

    if (count > 0) {
        task->read_bytes += count;
        file_xfer_queue(task, count);
        file_xfer_flush_async(channel, task->cancellable,
                              file_xfer_data_flushed_cb, task);
        task->pending = TRUE;
    } else if (error) {
        VDAgentFileXferStatusMessage msg = {
            .id = task->id,
            .result = VD_AGENT_FILE_XFER_STATUS_ERROR,
        };
        agent_msg_queue_many(task->channel, VD_AGENT_FILE_XFER_STATUS,
                             &msg, sizeof(msg), NULL);
        spice_channel_wakeup(SPICE_CHANNEL(task->channel), FALSE);
        file_xfer_completed(task, error);
    }
    /* else EOF, do nothing (wait for VD_AGENT_FILE_XFER_STATUS from agent) */
}

/* coroutine context */
static void file_xfer_continue_read(SpiceFileXferTask *task)
{
    g_input_stream_read_async(G_INPUT_STREAM(task->file_stream),
                              task->buffer,
                              FILE_XFER_CHUNK_SIZE,
                              G_PRIORITY_DEFAULT,
                              task->cancellable,
                              file_xfer_read_cb,
                              task);
    task->pending = TRUE;
}

/* coroutine context */
static void file_xfer_handle_status(SpiceMainChannel *channel,
                                    VDAgentFileXferStatusMessage *msg)
{
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceFileXferTask *task;
    GError *error = NULL;


    task = g_hash_table_lookup(c->file_xfer_tasks, GUINT_TO_POINTER(msg->id));
    if (task == NULL) {
        SPICE_DEBUG("cannot find task %d", msg->id);
        return;
    }

    SPICE_DEBUG("task %d received response %d", msg->id, msg->result);

    switch (msg->result) {
    case VD_AGENT_FILE_XFER_STATUS_CAN_SEND_DATA:
        if (task->pending) {
            error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                           "transfer received CAN_SEND_DATA in pending state");
            break;
        }
        file_xfer_continue_read(task);
        return;
    case VD_AGENT_FILE_XFER_STATUS_CANCELLED:
        error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "transfer is cancelled by spice agent");
        break;
    case VD_AGENT_FILE_XFER_STATUS_ERROR:
        error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "some errors occurred in the spice agent");
        break;
    case VD_AGENT_FILE_XFER_STATUS_SUCCESS:
        if (task->pending)
            error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                                "transfer received success in pending state");
        break;
    default:
        g_warn_if_reached();
        error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "unhandled status type: %u", msg->result);
        break;
    }

    file_xfer_completed(task, error);
}

/* any context: the message is not flushed immediately,
   you can wakeup() the channel coroutine or send_msg_queue() */
static void agent_max_clipboard(SpiceMainChannel *self)
{
    VDAgentMaxClipboard msg = { .max = spice_main_get_max_clipboard(self) };

    if (!test_agent_cap(self, VD_AGENT_CAP_MAX_CLIPBOARD))
        return;

    agent_msg_queue(self, VD_AGENT_MAX_CLIPBOARD, sizeof(VDAgentMaxClipboard), &msg);
}

static void spice_main_set_max_clipboard(SpiceMainChannel *self, gint max)
{
    SpiceMainChannelPrivate *c;

    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(self));
    g_return_if_fail(max >= -1);

    c = self->priv;
    if (max == spice_main_get_max_clipboard(self))
        return;

    c->max_clipboard = max;
    agent_max_clipboard(self);
    spice_channel_wakeup(SPICE_CHANNEL(self), FALSE);
}

/* coroutine context */
static void main_agent_handle_msg(SpiceChannel *channel,
                                  VDAgentMessage *msg, gpointer payload)
{
    SpiceMainChannel *self = SPICE_MAIN_CHANNEL(channel);
    SpiceMainChannelPrivate *c = self->priv;
    guint8 selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    g_return_if_fail(msg->protocol == VD_AGENT_PROTOCOL);

    switch (msg->type) {
    case VD_AGENT_CLIPBOARD_RELEASE:
    case VD_AGENT_CLIPBOARD_REQUEST:
    case VD_AGENT_CLIPBOARD_GRAB:
    case VD_AGENT_CLIPBOARD:
        if (test_agent_cap(self, VD_AGENT_CAP_CLIPBOARD_SELECTION)) {
            selection = *((guint8*)payload);
            payload = ((guint8*)payload) + 4;
            msg->size -= 4;
        }
        break;
    default:
        break;
    }

    switch (msg->type) {
    case VD_AGENT_ANNOUNCE_CAPABILITIES:
    {
        VDAgentAnnounceCapabilities *caps = payload;
        int i, size;

        size = VD_AGENT_CAPS_SIZE_FROM_MSG_SIZE(msg->size);
        if (size > VD_AGENT_CAPS_SIZE)
            size = VD_AGENT_CAPS_SIZE;
        memset(c->agent_caps, 0, sizeof(c->agent_caps));
        for (i = 0; i < size * 32; i++) {
            if (!VD_AGENT_HAS_CAPABILITY(caps->caps, size, i))
                continue;
            SPICE_DEBUG("%s: cap: %d (%s)", __FUNCTION__,
                        i, NAME(agent_caps, i));
            VD_AGENT_SET_CAPABILITY(c->agent_caps, i);
        }
        c->agent_caps_received = true;
        emit_main_context(self, SPICE_MAIN_AGENT_UPDATE);

        if (caps->request)
            agent_announce_caps(self);

        if (test_agent_cap(self, VD_AGENT_CAP_DISPLAY_CONFIG) &&
            !c->agent_display_config_sent) {
            agent_display_config(self);
            c->agent_display_config_sent = true;
        }

        agent_max_clipboard(self);

        agent_send_msg_queue(self);

        break;
    }
    case VD_AGENT_CLIPBOARD:
    {
        VDAgentClipboard *cb = payload;
        emit_main_context(self, SPICE_MAIN_CLIPBOARD_SELECTION, selection,
                          cb->type, cb->data, msg->size - sizeof(VDAgentClipboard));

       if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
            emit_main_context(self, SPICE_MAIN_CLIPBOARD,
                              cb->type, cb->data, msg->size - sizeof(VDAgentClipboard));
        break;
    }
    case VD_AGENT_CLIPBOARD_GRAB:
    {
        gboolean ret;
        emit_main_context(self, SPICE_MAIN_CLIPBOARD_SELECTION_GRAB, selection,
                          (guint8*)payload, msg->size / sizeof(uint32_t), &ret);
        if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
            emit_main_context(self, SPICE_MAIN_CLIPBOARD_GRAB,
                              payload, msg->size / sizeof(uint32_t), &ret);
        break;
    }
    case VD_AGENT_CLIPBOARD_REQUEST:
    {
        gboolean ret;
        VDAgentClipboardRequest *req = payload;
        emit_main_context(self, SPICE_MAIN_CLIPBOARD_SELECTION_REQUEST, selection,
                          req->type, &ret);

        if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
            emit_main_context(self, SPICE_MAIN_CLIPBOARD_REQUEST,
                              req->type, &ret);
        break;
    }
    case VD_AGENT_CLIPBOARD_RELEASE:
    {
        emit_main_context(self, SPICE_MAIN_CLIPBOARD_SELECTION_RELEASE, selection);

        if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
            emit_main_context(self, SPICE_MAIN_CLIPBOARD_RELEASE);
        break;
    }
    case VD_AGENT_REPLY:
    {
        VDAgentReply *reply = payload;
        SPICE_DEBUG("%s: reply: type %d, %s", __FUNCTION__, reply->type,
                    reply->error == VD_AGENT_SUCCESS ? "success" : "error");
        break;
    }
    case VD_AGENT_FILE_XFER_STATUS:
        file_xfer_handle_status(self, payload);
        break;
    default:
        g_warning("unhandled agent message type: %u (%s), size %u",
                  msg->type, NAME(agent_msg_types, msg->type), msg->size);
    }
}

/* coroutine context */
static void main_handle_agent_data_msg(SpiceChannel* channel, int* msg_size, guchar** msg_pos)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;
    int n;

    if (c->agent_msg_pos < sizeof(VDAgentMessage)) {
        n = MIN(sizeof(VDAgentMessage) - c->agent_msg_pos, *msg_size);
        memcpy((uint8_t*)&c->agent_msg + c->agent_msg_pos, *msg_pos, n);
        c->agent_msg_pos += n;
        *msg_size -= n;
        *msg_pos += n;
        if (c->agent_msg_pos == sizeof(VDAgentMessage)) {
            SPICE_DEBUG("agent msg start: msg_size=%d, protocol=%d, type=%d",
                        c->agent_msg.size, c->agent_msg.protocol, c->agent_msg.type);
            g_return_if_fail(c->agent_msg_data == NULL);
            c->agent_msg_data = g_malloc(c->agent_msg.size);
        }
    }

    if (c->agent_msg_pos >= sizeof(VDAgentMessage)) {
        n = MIN(sizeof(VDAgentMessage) + c->agent_msg.size - c->agent_msg_pos, *msg_size);
        memcpy(c->agent_msg_data + c->agent_msg_pos - sizeof(VDAgentMessage), *msg_pos, n);
        c->agent_msg_pos += n;
        *msg_size -= n;
        *msg_pos += n;
    }

    if (c->agent_msg_pos == sizeof(VDAgentMessage) + c->agent_msg.size) {
        main_agent_handle_msg(channel, &c->agent_msg, c->agent_msg_data);
        g_free(c->agent_msg_data);
        c->agent_msg_data = NULL;
        c->agent_msg_pos = 0;
    }
}

/* coroutine context */
static void main_handle_agent_data(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;
    guint8 *data;
    int len;

    g_warn_if_fail(c->agent_connected);

    /* shortcut to avoid extra message allocation & copy if possible */
    if (c->agent_msg_pos == 0) {
        VDAgentMessage *msg;
        guint msg_size;

        msg = spice_msg_in_raw(in, &len);
        msg_size = msg->size;

        if (msg_size + sizeof(VDAgentMessage) == len) {
            main_agent_handle_msg(channel, msg, msg->data);
            return;
        }
    }

    data = spice_msg_in_raw(in, &len);
    while (len > 0) {
        main_handle_agent_data_msg(channel, &len, &data);
    }
}

/* coroutine context */
static void main_handle_agent_token(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainAgentTokens *tokens = spice_msg_in_parsed(in);
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    c->agent_tokens += tokens->num_tokens;

    agent_send_msg_queue(SPICE_MAIN_CHANNEL(channel));
}

/* main context */
static void migrate_channel_new_cb(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    g_signal_connect(channel, "channel-event",
                     G_CALLBACK(migrate_channel_event_cb), data);
}

static SpiceChannel* migrate_channel_connect(spice_migrate *mig, int type, int id)
{
    SPICE_DEBUG("migrate_channel_connect %d:%d", type, id);

    SpiceChannel *newc = spice_channel_new(mig->session, type, id);
    spice_channel_connect(newc);
    mig->nchannels++;

    return newc;
}

/* coroutine context */
static void spice_main_channel_send_migration_handshake(SpiceChannel *channel)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    if (!spice_channel_test_capability(channel, SPICE_MAIN_CAP_SEAMLESS_MIGRATE)) {
        c->migrate_data->do_seamless = false;
        g_idle_add(main_migrate_handshake_done, c->migrate_data);
    } else {
        SpiceMsgcMainMigrateDstDoSeamless msg_data;
        SpiceMsgOut *msg_out;

        msg_data.src_version = c->migrate_data->src_mig_version;

        msg_out = spice_msg_out_new(channel, SPICE_MSGC_MAIN_MIGRATE_DST_DO_SEAMLESS);
        msg_out->marshallers->msgc_main_migrate_dst_do_seamless(msg_out->marshaller, &msg_data);
        spice_msg_out_send_internal(msg_out);
    }
}

/* main context */
static void migrate_channel_event_cb(SpiceChannel *channel, SpiceChannelEvent event,
                                     gpointer data)
{
    spice_migrate *mig = data;
    SpiceChannelPrivate  *c = SPICE_CHANNEL(channel)->priv;
    SpiceSession *session;

    g_return_if_fail(mig->nchannels > 0);
    g_signal_handlers_disconnect_by_func(channel, migrate_channel_event_cb, data);

    session = spice_channel_get_session(mig->src_channel);

    switch (event) {
    case SPICE_CHANNEL_OPENED:

        if (c->channel_type == SPICE_CHANNEL_MAIN) {
            if (mig->do_seamless) {
                SpiceMainChannelPrivate *main_priv = SPICE_MAIN_CHANNEL(channel)->priv;

                c->state = SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE;
                mig->dst_channel = channel;
                main_priv->migrate_data = mig;
            } else {
                c->state = SPICE_CHANNEL_STATE_MIGRATING;
                mig->nchannels--;
            }
            /* now connect the rest of the channels */
            GList *channels, *l;
            l = channels = spice_session_get_channels(session);
            while (l != NULL) {
                SpiceChannelPrivate  *curc = SPICE_CHANNEL(l->data)->priv;
                l = l->next;
                if (curc->channel_type == SPICE_CHANNEL_MAIN)
                    continue;
                migrate_channel_connect(mig, curc->channel_type, curc->channel_id);
            }
            g_list_free(channels);
        } else {
            c->state = SPICE_CHANNEL_STATE_MIGRATING;
            mig->nchannels--;
        }

        SPICE_DEBUG("migration: channel opened chan:%p, left %d", channel, mig->nchannels);
        if (mig->nchannels == 0)
            coroutine_yieldto(mig->from, NULL);
        break;
    default:
        SPICE_DEBUG("error or unhandled channel event during migration: %d", event);
        /* go back to main channel to report error */
        coroutine_yieldto(mig->from, NULL);
    }
}

/* main context */
static gboolean main_migrate_handshake_done(gpointer data)
{
    spice_migrate *mig = data;
    SpiceChannelPrivate  *c = SPICE_CHANNEL(mig->dst_channel)->priv;

    g_return_val_if_fail(c->channel_type == SPICE_CHANNEL_MAIN, FALSE);
    g_return_val_if_fail(c->state == SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE, FALSE);

    c->state = SPICE_CHANNEL_STATE_MIGRATING;
    mig->nchannels--;
    if (mig->nchannels == 0)
        coroutine_yieldto(mig->from, NULL);
    return FALSE;
}

#ifdef __GNUC__
typedef struct __attribute__ ((__packed__)) OldRedMigrationBegin {
#else
typedef struct __declspec(align(1)) OldRedMigrationBegin {
#endif
    uint16_t port;
    uint16_t sport;
    char host[0];
} OldRedMigrationBegin;

/* main context */
static gboolean migrate_connect(gpointer data)
{
    spice_migrate *mig = data;
    SpiceChannelPrivate  *c;
    int port, sport;
    const char *host;
    SpiceSession *session;

    g_return_val_if_fail(mig != NULL, FALSE);
    g_return_val_if_fail(mig->info != NULL, FALSE);
    g_return_val_if_fail(mig->nchannels == 0, FALSE);
    c = SPICE_CHANNEL(mig->src_channel)->priv;
    g_return_val_if_fail(c != NULL, FALSE);

    session = spice_channel_get_session(mig->src_channel);
    mig->session = spice_session_new_from_session(session);
    mig->session->priv->migration_copy = true;

    if ((c->peer_hdr.major_version == 1) &&
        (c->peer_hdr.minor_version < 1)) {
        OldRedMigrationBegin *info = (OldRedMigrationBegin *)mig->info;
        SPICE_DEBUG("migrate_begin old %s %d %d",
                    info->host, info->port, info->sport);
        port = info->port;
        sport = info->sport;
        host = info->host;
    } else {
        SpiceMigrationDstInfo *info = mig->info;
        SPICE_DEBUG("migrate_begin %d %s %d %d",
                    info->host_size, info->host_data, info->port, info->sport);
        port = info->port;
        sport = info->sport;
        host = (char*)info->host_data;

        if ((c->peer_hdr.major_version == 1) ||
            (c->peer_hdr.major_version == 2 && c->peer_hdr.minor_version < 1)) {
            GByteArray *pubkey = g_byte_array_new();

            g_byte_array_append(pubkey, info->pub_key_data, info->pub_key_size);
            g_object_set(mig->session,
                         "pubkey", pubkey,
                         "verify", SPICE_SESSION_VERIFY_PUBKEY,
                         NULL);
            g_byte_array_unref(pubkey);
        } else if (info->cert_subject_size == 0 ||
                   strlen((const char*)info->cert_subject_data) == 0) {
            /* only verify hostname if no cert subject */
            g_object_set(mig->session, "verify", SPICE_SESSION_VERIFY_HOSTNAME, NULL);
        } else {
            gchar *subject = g_alloca(info->cert_subject_size + 1);
            strncpy(subject, (const char*)info->cert_subject_data, info->cert_subject_size);
            subject[info->cert_subject_size] = '\0';

            // session data are already copied
            g_object_set(mig->session,
                         "cert-subject", subject,
                         "verify", SPICE_SESSION_VERIFY_SUBJECT,
                         NULL);
        }
    }

    if (g_getenv("SPICE_MIG_HOST"))
        host = g_getenv("SPICE_MIG_HOST");

    g_object_set(mig->session, "host", host, NULL);
    spice_session_set_port(mig->session, port, FALSE);
    spice_session_set_port(mig->session, sport, TRUE);
    g_signal_connect(mig->session, "channel-new",
                     G_CALLBACK(migrate_channel_new_cb), mig);

    g_signal_emit(mig->src_channel, signals[SPICE_MIGRATION_STARTED], 0,
                  mig->session);

    /* the migration process is in 2 steps, first the main channel and
       then the rest of the channels */
    mig->session->priv->cmain = migrate_channel_connect(mig, SPICE_CHANNEL_MAIN, 0);

    return FALSE;
}

/* coroutine context */
static void main_migrate_connect(SpiceChannel *channel,
                                 SpiceMigrationDstInfo *dst_info, bool do_seamless,
                                 uint32_t src_mig_version)
{
    SpiceMainChannelPrivate *main_priv = SPICE_MAIN_CHANNEL(channel)->priv;
    spice_migrate mig = { 0, };
    SpiceMsgOut *out;
    int reply_type;

    mig.src_channel = channel;
    mig.info = dst_info;
    mig.from = coroutine_self();
    mig.do_seamless = do_seamless;
    mig.src_mig_version = src_mig_version;

    main_priv->migrate_data = &mig;

    /* no need to track idle, call is sync for this coroutine */
    g_idle_add(migrate_connect, &mig);

    /* switch to main loop and wait for connections */
    coroutine_yield(NULL);
    g_return_if_fail(mig.session != NULL);

    if (mig.nchannels != 0) {
        reply_type = SPICE_MSGC_MAIN_MIGRATE_CONNECT_ERROR;
        spice_session_disconnect(mig.session);
    } else {
        if (mig.do_seamless) {
            SPICE_DEBUG("migration (seamless): connections all ok");
            reply_type = SPICE_MSGC_MAIN_MIGRATE_CONNECTED_SEAMLESS;
        } else {
            SPICE_DEBUG("migration (semi-seamless): connections all ok");
            reply_type = SPICE_MSGC_MAIN_MIGRATE_CONNECTED;
        }
        spice_session_set_migration(spice_channel_get_session(channel),
                                    mig.session,
                                    mig.do_seamless);
    }
    g_object_unref(mig.session);

    out = spice_msg_out_new(SPICE_CHANNEL(channel), reply_type);
    spice_msg_out_send(out);
}

/* coroutine context */
static void main_handle_migrate_begin(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainMigrationBegin *msg = spice_msg_in_parsed(in);

    main_migrate_connect(channel, &msg->dst_info, false, 0);
}

/* coroutine context */
static void main_handle_migrate_begin_seamless(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainMigrateBeginSeamless *msg = spice_msg_in_parsed(in);

    main_migrate_connect(channel, &msg->dst_info, true, msg->src_mig_version);
}

static void main_handle_migrate_dst_seamless_ack(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceChannelPrivate  *c = SPICE_CHANNEL(channel)->priv;
    SpiceMainChannelPrivate *main_priv = SPICE_MAIN_CHANNEL(channel)->priv;

    g_return_if_fail(c->state == SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE);
    main_priv->migrate_data->do_seamless = true;
    g_idle_add(main_migrate_handshake_done, main_priv->migrate_data);
}

static void main_handle_migrate_dst_seamless_nack(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceChannelPrivate  *c = SPICE_CHANNEL(channel)->priv;
    SpiceMainChannelPrivate *main_priv = SPICE_MAIN_CHANNEL(channel)->priv;

    g_return_if_fail(c->state == SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE);
    main_priv->migrate_data->do_seamless = false;
    g_idle_add(main_migrate_handshake_done, main_priv->migrate_data);
}

/* main context */
static gboolean migrate_delayed(gpointer data)
{
    SpiceChannel *channel = data;
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    g_warn_if_fail(c->migrate_delayed_id != 0);
    c->migrate_delayed_id = 0;

    spice_session_migrate_end(channel->priv->session);

    return FALSE;
}

/* coroutine context */
static void main_handle_migrate_end(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    SPICE_DEBUG("migrate end");

    g_return_if_fail(c->migrate_delayed_id == 0);
    g_return_if_fail(spice_channel_test_capability(channel, SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE));

    c->migrate_delayed_id = g_idle_add(migrate_delayed, channel);
}

/* main context */
static gboolean switch_host_delayed(gpointer data)
{
    SpiceChannel *channel = data;
    SpiceSession *session;
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    g_warn_if_fail(c->switch_host_delayed_id != 0);
    c->switch_host_delayed_id = 0;

    session = spice_channel_get_session(channel);

    spice_channel_disconnect(channel, SPICE_CHANNEL_SWITCHING);
    spice_session_switching_disconnect(session);

    spice_channel_connect(channel);
    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_NONE);

    return FALSE;
}

/* coroutine context */
static void main_handle_migrate_switch_host(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgMainMigrationSwitchHost *mig = spice_msg_in_parsed(in);
    SpiceSession *session;
    char *host = (char *)mig->host_data;
    char *subject = NULL;
    SpiceMainChannelPrivate *c = SPICE_MAIN_CHANNEL(channel)->priv;

    g_return_if_fail(host[mig->host_size - 1] == '\0');

    if (mig->cert_subject_size) {
        subject = (char *)mig->cert_subject_data;
        g_return_if_fail(subject[mig->cert_subject_size - 1] == '\0');
    }

    SPICE_DEBUG("migrate_switch %s %d %d %s",
                host, mig->port, mig->sport, subject);

    if (c->switch_host_delayed_id != 0) {
        g_warning("Switching host already in progress, aborting it");
        g_warn_if_fail(g_source_remove(c->switch_host_delayed_id));
        c->switch_host_delayed_id = 0;
    }

    session = spice_channel_get_session(channel);
    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_SWITCHING);
    g_object_set(session,
                 "host", host,
                 "cert-subject", subject,
                 NULL);
    spice_session_set_port(session, mig->port, FALSE);
    spice_session_set_port(session, mig->sport, TRUE);

    c->switch_host_delayed_id = g_idle_add(switch_host_delayed, channel);
}

/* coroutine context */
static void main_handle_migrate_cancel(SpiceChannel *channel,
                                       SpiceMsgIn *in G_GNUC_UNUSED)
{
    SpiceSession *session;

    SPICE_DEBUG("migrate_cancel");
    session = spice_channel_get_session(channel);
    spice_session_abort_migration(session);
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_MAIN_INIT ]                = main_handle_init,
        [ SPICE_MSG_MAIN_NAME ]                = main_handle_name,
        [ SPICE_MSG_MAIN_UUID ]                = main_handle_uuid,
        [ SPICE_MSG_MAIN_CHANNELS_LIST ]       = main_handle_channels_list,
        [ SPICE_MSG_MAIN_MOUSE_MODE ]          = main_handle_mouse_mode,
        [ SPICE_MSG_MAIN_MULTI_MEDIA_TIME ]    = main_handle_mm_time,

        [ SPICE_MSG_MAIN_AGENT_CONNECTED ]     = main_handle_agent_connected,
        [ SPICE_MSG_MAIN_AGENT_DISCONNECTED ]  = main_handle_agent_disconnected,
        [ SPICE_MSG_MAIN_AGENT_DATA ]          = main_handle_agent_data,
        [ SPICE_MSG_MAIN_AGENT_TOKEN ]         = main_handle_agent_token,

        [ SPICE_MSG_MAIN_MIGRATE_BEGIN ]       = main_handle_migrate_begin,
        [ SPICE_MSG_MAIN_MIGRATE_END ]         = main_handle_migrate_end,
        [ SPICE_MSG_MAIN_MIGRATE_CANCEL ]      = main_handle_migrate_cancel,
        [ SPICE_MSG_MAIN_MIGRATE_SWITCH_HOST ] = main_handle_migrate_switch_host,
        [ SPICE_MSG_MAIN_AGENT_CONNECTED_TOKENS ]   = main_handle_agent_connected_tokens,
        [ SPICE_MSG_MAIN_MIGRATE_BEGIN_SEAMLESS ]   = main_handle_migrate_begin_seamless,
        [ SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_ACK]  = main_handle_migrate_dst_seamless_ack,
        [ SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_NACK] = main_handle_migrate_dst_seamless_nack,
    };

    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}

/* coroutine context */
static void spice_main_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg)
{
    int type = spice_msg_in_type(msg);
    SpiceChannelClass *parent_class;
    SpiceChannelPrivate *c = SPICE_CHANNEL(channel)->priv;

    parent_class = SPICE_CHANNEL_CLASS(spice_main_channel_parent_class);

    if (c->state == SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE) {
        if (type != SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_ACK &&
            type != SPICE_MSG_MAIN_MIGRATE_DST_SEAMLESS_NACK) {
            g_critical("unexpected msg (%d)."
                       "Only MIGRATE_DST_SEAMLESS_ACK/NACK are allowed", type);
            return;
        }
    }

    parent_class->handle_msg(channel, msg);
}

/**
 * spice_main_agent_test_capability:
 * @channel:
 * @cap: an agent capability identifier
 *
 * Test capability of a remote agent.
 *
 * Returns: %TRUE if @cap (channel kind capability) is available.
 **/
gboolean spice_main_agent_test_capability(SpiceMainChannel *channel, guint32 cap)
{
    g_return_val_if_fail(SPICE_IS_MAIN_CHANNEL(channel), FALSE);

    return test_agent_cap(channel, cap);
}

/**
 * spice_main_update_display:
 * @channel:
 * @id: display ID
 * @x: x position
 * @y: y position
 * @width: display width
 * @height: display height
 * @update: if %TRUE, update guest resolution after 1sec.
 *
 * Update the display @id resolution.
 *
 * If @update is %TRUE, the remote configuration will be updated too
 * after 1 second without further changes. You can send when you want
 * without delay the new configuration to the remote with
 * spice_main_send_monitor_config()
 **/
void spice_main_update_display(SpiceMainChannel *channel, int id,
                               int x, int y, int width, int height,
                               gboolean update)
{
    SpiceMainChannelPrivate *c;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));
    g_return_if_fail(x >= 0);
    g_return_if_fail(y >= 0);
    g_return_if_fail(width >= 0);
    g_return_if_fail(height >= 0);

    c = SPICE_MAIN_CHANNEL(channel)->priv;

    g_return_if_fail(id < SPICE_N_ELEMENTS(c->display));

    c->display[id].x      = x;
    c->display[id].y      = y;
    c->display[id].width  = width;
    c->display[id].height = height;

    if (update)
        update_display_timer(channel, 1);
}

/**
 * spice_main_set_display:
 * @channel:
 * @id: display ID
 * @x: x position
 * @y: y position
 * @width: display width
 * @height: display height
 *
 * Notify the guest of screen resolution change. The notification is
 * sent 1 second later, if no further changes happen.
 **/
void spice_main_set_display(SpiceMainChannel *channel, int id,
                            int x, int y, int width, int height)
{
    spice_main_update_display(channel, id, x, y, width, height, TRUE);
}

/**
 * spice_main_clipboard_grab:
 * @channel:
 * @types: an array of #VD_AGENT_CLIPBOARD types available in the clipboard
 * @ntypes: the number of @types
 *
 * Grab the guest clipboard, with #VD_AGENT_CLIPBOARD @types.
 *
 * Deprecated: 0.6: use spice_main_clipboard_selection_grab() instead.
 **/
void spice_main_clipboard_grab(SpiceMainChannel *channel, guint32 *types, int ntypes)
{
    spice_main_clipboard_selection_grab(channel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, types, ntypes);
}

/**
 * spice_main_clipboard_selection_grab:
 * @channel:
 * @selection: one of the clipboard #VD_AGENT_CLIPBOARD_SELECTION_*
 * @types: an array of #VD_AGENT_CLIPBOARD types available in the clipboard
 * @ntypes: the number of @types
 *
 * Grab the guest clipboard, with #VD_AGENT_CLIPBOARD @types.
 *
 * Since: 0.6
 **/
void spice_main_clipboard_selection_grab(SpiceMainChannel *channel, guint selection,
                                         guint32 *types, int ntypes)
{
    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));

    agent_clipboard_grab(channel, selection, types, ntypes);
    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
}

/**
 * spice_main_clipboard_release:
 * @channel:
 *
 * Release the clipboard (for example, when the client looses the
 * clipboard grab): Inform the guest no clipboard data is available.
 *
 * Deprecated: 0.6: use spice_main_clipboard_selection_release() instead.
 **/
void spice_main_clipboard_release(SpiceMainChannel *channel)
{
    spice_main_clipboard_selection_release(channel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD);
}

/**
 * spice_main_clipboard_selection_release:
 * @channel:
 * @selection: one of the clipboard #VD_AGENT_CLIPBOARD_SELECTION_*
 *
 * Release the clipboard (for example, when the client looses the
 * clipboard grab): Inform the guest no clipboard data is available.
 *
 * Since: 0.6
 **/
void spice_main_clipboard_selection_release(SpiceMainChannel *channel, guint selection)
{
    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));

    SpiceMainChannelPrivate *c = channel->priv;

    if (!c->agent_connected)
        return;

    agent_clipboard_release(channel, selection);
    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
}

/**
 * spice_main_clipboard_notify:
 * @channel:
 * @type: a #VD_AGENT_CLIPBOARD type
 * @data: clipboard data
 * @size: data length in bytes
 *
 * Send the clipboard data to the guest.
 *
 * Deprecated: 0.6: use spice_main_clipboard_selection_notify() instead.
 **/
void spice_main_clipboard_notify(SpiceMainChannel *channel,
                                 guint32 type, const guchar *data, size_t size)
{
    spice_main_clipboard_selection_notify(channel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD,
                                          type, data, size);
}

/**
 * spice_main_clipboard_selection_notify:
 * @channel:
 * @selection: one of the clipboard #VD_AGENT_CLIPBOARD_SELECTION_*
 * @type: a #VD_AGENT_CLIPBOARD type
 * @data: clipboard data
 * @size: data length in bytes
 *
 * Send the clipboard data to the guest.
 *
 * Since: 0.6
 **/
void spice_main_clipboard_selection_notify(SpiceMainChannel *channel, guint selection,
                                           guint32 type, const guchar *data, size_t size)
{
    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));

    agent_clipboard_notify(channel, selection, type, data, size);
    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
}

/**
 * spice_main_clipboard_request:
 * @channel:
 * @type: a #VD_AGENT_CLIPBOARD type
 *
 * Request clipboard data of @type from the guest. The reply is sent
 * through the #SpiceMainChannel::main-clipboard signal.
 *
 * Deprecated: 0.6: use spice_main_clipboard_selection_request() instead.
 **/
void spice_main_clipboard_request(SpiceMainChannel *channel, guint32 type)
{
    spice_main_clipboard_selection_request(channel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, type);
}

/**
 * spice_main_clipboard_selection_request:
 * @channel:
 * @selection: one of the clipboard #VD_AGENT_CLIPBOARD_SELECTION_*
 * @type: a #VD_AGENT_CLIPBOARD type
 *
 * Request clipboard data of @type from the guest. The reply is sent
 * through the #SpiceMainChannel::main-clipboard-selection signal.
 *
 * Since: 0.6
 **/
void spice_main_clipboard_selection_request(SpiceMainChannel *channel, guint selection, guint32 type)
{
    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));

    agent_clipboard_request(channel, selection, type);
    spice_channel_wakeup(SPICE_CHANNEL(channel), FALSE);
}

/**
 * spice_main_set_display_enabled:
 * @channel: a #SpiceMainChannel
 * @id: display ID (if -1: set all displays)
 * @enabled: wether display @id is enabled
 *
 * When sending monitor configuration to agent guest, don't set
 * display @id, which the agent translates to disabling the display
 * id. Note: this will take effect next time the monitor
 * configuration is sent.
 *
 * Since: 0.6
 **/
void spice_main_set_display_enabled(SpiceMainChannel *channel, int id, gboolean enabled)
{
    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));
    g_return_if_fail(id >= -1);

    SpiceMainChannelPrivate *c = channel->priv;

    if (id == -1) {
        gint i;
        for (i = 0; i < G_N_ELEMENTS(c->display); i++) {
            c->display[i].enabled = enabled;
            c->display[i].enabled_set = TRUE;
        }
    } else {
        g_return_if_fail(id < G_N_ELEMENTS(c->display));
        if (c->display[id].enabled == enabled)
            return;
        c->display[id].enabled = enabled;
        c->display[id].enabled_set = TRUE;
    }

    update_display_timer(channel, 1);
}

static void file_xfer_completed(SpiceFileXferTask *task, GError *error)
{
    /* In case of multiple errors we only report the first error */
    if (task->error)
        g_clear_error(&error);
    if (error) {
        SPICE_DEBUG("File %s xfer failed: %s",
                    g_file_get_path(task->file), error->message);
        task->error = error;
    }

    if (task->pending)
        return;

    if (!task->file_stream) {
        file_xfer_close_cb(NULL, NULL, task);
        return;
    }

    g_input_stream_close_async(G_INPUT_STREAM(task->file_stream),
                               G_PRIORITY_DEFAULT,
                               task->cancellable,
                               file_xfer_close_cb,
                               task);
    task->pending = TRUE;
}

static void file_xfer_info_async_cb(GObject *obj, GAsyncResult *res, gpointer data)
{
    GFileInfo *info;
    GFile *file = G_FILE(obj);
    GError *error = NULL;
    GKeyFile *keyfile = NULL;
    gchar *basename = NULL;
    VDAgentFileXferStartMessage msg;
    gsize /*msg_size*/ data_len;
    gchar *string;
    SpiceFileXferTask *task = (SpiceFileXferTask *)data;

    task->pending = FALSE;
    info = g_file_query_info_finish(file, res, &error);
    if (error || task->error)
        goto failed;

    task->file_size =
        g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
    keyfile = g_key_file_new();

    /* File name */
    basename = g_file_get_basename(file);
    g_key_file_set_string(keyfile, "vdagent-file-xfer", "name", basename);
    g_free(basename);
    /* File size */
    g_key_file_set_uint64(keyfile, "vdagent-file-xfer", "size", task->file_size);

    /* Save keyfile content to memory. TODO: more file attributions
       need to be sent to guest */
    string = g_key_file_to_data(keyfile, &data_len, &error);
    g_key_file_free(keyfile);
    if (error)
        goto failed;

    /* Create file-xfer start message */
    msg.id = task->id;
    agent_msg_queue_many(task->channel, VD_AGENT_FILE_XFER_START,
                         &msg, sizeof(msg),
                         string, data_len + 1, NULL);
    g_free(string);
    spice_channel_wakeup(SPICE_CHANNEL(task->channel), FALSE);
    return;

failed:
    file_xfer_completed(task, error);
}

static void file_xfer_read_async_cb(GObject *obj, GAsyncResult *res, gpointer data)
{
    GFile *file = G_FILE(obj);
    SpiceFileXferTask *task = (SpiceFileXferTask *)data;
    GError *error = NULL;

    task->pending = FALSE;
    task->file_stream = g_file_read_finish(file, res, &error);
    if (error || task->error) {
        file_xfer_completed(task, error);
        return;
    }

    g_file_query_info_async(task->file,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT,
                            task->cancellable,
                            file_xfer_info_async_cb,
                            task);
    task->pending = TRUE;
}

static void file_xfer_send_start_msg_async(SpiceMainChannel *channel,
                                           GFile *file,
                                           GFileCopyFlags flags,
                                           GCancellable *cancellable,
                                           GFileProgressCallback progress_callback,
                                           gpointer progress_callback_data,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
    SpiceMainChannelPrivate *c = channel->priv;
    SpiceFileXferTask *task;
    static uint32_t xfer_id;    /* Used to identify task id */

    task = spice_malloc0(sizeof(SpiceFileXferTask));
    task->id = ++xfer_id;
    task->channel = g_object_ref(channel);
    task->file = g_object_ref(file);
    task->flags = flags;
    task->cancellable = cancellable;
    task->progress_callback = progress_callback;
    task->progress_callback_data = progress_callback_data;
    task->callback = callback;
    task->user_data = user_data;

    CHANNEL_DEBUG(task->channel, "Insert a xfer task:%d to task list", task->id);
    g_hash_table_insert(c->file_xfer_tasks, GUINT_TO_POINTER(task->id), task);

    g_file_read_async(file,
                      G_PRIORITY_DEFAULT,
                      cancellable,
                      file_xfer_read_async_cb,
                      task);
    task->pending = TRUE;
}

/**
 * spice_main_file_copy_async:
 * @sources: #GFile to be transfer
 * @flags: set of #GFileCopyFlags
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @progress_callback: (allow-none) (scope call): function to callback with
 *     progress information, or %NULL if progress information is not needed
 * @progress_callback_data: (closure): user data to pass to @progress_callback
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Copies the file @sources to guest
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If @progress_callback is not %NULL, then the operation can be monitored by
 * setting this to a #GFileProgressCallback function. @progress_callback_data
 * will be passed to this function. It is guaranteed that this callback will
 * be called after all data has been transferred with the total number of bytes
 * copied during the operation.
 *
 * When the operation is finished, callback will be called. You can then call
 * spice_main_file_copy_finish() to get the result of the operation.
 *
 **/
void spice_main_file_copy_async(SpiceMainChannel *channel,
                                GFile **sources,
                                GFileCopyFlags flags,
                                GCancellable *cancellable,
                                GFileProgressCallback progress_callback,
                                gpointer progress_callback_data,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    SpiceMainChannelPrivate *c = channel->priv;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_MAIN_CHANNEL(channel));
    g_return_if_fail(sources != NULL && sources[0] != NULL);

    /* At the moment, the copy() method is limited to a single file,
       support for copying multi-files will be implemented later. */
    g_return_if_fail(sources[1] == NULL);

    if (!c->agent_connected) {
        g_simple_async_report_error_in_idle(G_OBJECT(channel),
                                            callback,
                                            user_data,
                                            SPICE_CLIENT_ERROR,
                                            SPICE_CLIENT_ERROR_FAILED,
                                            "The agent is not connected");
        return;
    }

    file_xfer_send_start_msg_async(channel,
                                   sources[0],
                                   flags,
                                   cancellable,
                                   progress_callback,
                                   progress_callback_data,
                                   callback,
                                   user_data);
}

/**
 * spice_main_file_copy_finish:
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes copying the file started with
 * spice_main_file_copy_async().
 *
 * Returns: a %TRUE on success, %FALSE on error.
 **/
gboolean spice_main_file_copy_finish(SpiceMainChannel *channel,
                                     GAsyncResult *result,
                                     GError **error)
{
    GSimpleAsyncResult *simple;

    g_return_val_if_fail(SPICE_IS_MAIN_CHANNEL(channel), FALSE);
    g_return_val_if_fail(g_simple_async_result_is_valid(result,
        G_OBJECT(channel), spice_main_file_copy_async), FALSE);

    simple = (GSimpleAsyncResult *)result;

    if (g_simple_async_result_propagate_error(simple, error)) {
        return FALSE;
    }

    return g_simple_async_result_get_op_res_gboolean(simple);
}
