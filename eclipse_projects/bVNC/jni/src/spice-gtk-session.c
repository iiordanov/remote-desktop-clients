/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010-2011 Red Hat, Inc.

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

#include <gtk/gtk.h>
#include <spice/vd_agent.h>
#include "desktop-integration.h"
#include "spice-common.h"
#include "spice-gtk-session.h"
#include "spice-gtk-session-priv.h"
#include "spice-session-priv.h"
#include "spice-util-priv.h"

#define CLIPBOARD_LAST (VD_AGENT_CLIPBOARD_SELECTION_SECONDARY + 1)

struct _SpiceGtkSessionPrivate {
    SpiceSession            *session;
    /* Clipboard related */
    gboolean                auto_clipboard_enable;
    SpiceMainChannel        *main;
    GtkClipboard            *clipboard;
    GtkClipboard            *clipboard_primary;
    GtkTargetEntry          *clip_targets[CLIPBOARD_LAST];
    guint                   nclip_targets[CLIPBOARD_LAST];
    gboolean                clip_hasdata[CLIPBOARD_LAST];
    gboolean                clip_grabbed[CLIPBOARD_LAST];
    gboolean                clipboard_by_guest[CLIPBOARD_LAST];
    /* auto-usbredir related */
    gboolean                auto_usbredir_enable;
    int                     auto_usbredir_reqs;
};

/**
 * SECTION:spice-gtk-session
 * @short_description: handles GTK connection details
 * @title: Spice GTK Session
 * @section_id:
 * @see_also: #SpiceSession, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: spice-gtk-session.h
 *
 * The #SpiceGtkSession class is the spice-client-gtk counter part of
 * #SpiceSession. It contains functionality which should be handled per
 * session rather then per #SpiceDisplay (one session can have multiple
 * displays), but which cannot live in #SpiceSession as it depends on
 * GTK. For example the clipboard functionality.
 *
 * There should always be a 1:1 relation between #SpiceGtkSession objects
 * and #SpiceSession objects. Therefor there is no spice_gtk_session_new,
 * instead there is spice_gtk_session_get() which ensures this 1:1 relation.
 *
 * Client and guest clipboards will be shared automatically if
 * #SpiceGtkSession:auto-clipboard is set to #TRUE. Alternatively, you
 * can send / receive clipboard data from client to guest with
 * spice_gtk_session_copy_to_guest() / spice_gtk_session_paste_from_guest().
 */

/* ------------------------------------------------------------------ */
/* Prototypes for private functions */
static void clipboard_owner_change(GtkClipboard *clipboard,
                                   GdkEventOwnerChange *event,
                                   gpointer user_data);
static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data);
static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data);
static gboolean read_only(SpiceGtkSession *self);

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_GTK_SESSION_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_GTK_SESSION, SpiceGtkSessionPrivate))

G_DEFINE_TYPE (SpiceGtkSession, spice_gtk_session, G_TYPE_OBJECT);

/* Properties */
enum {
    PROP_0,
    PROP_SESSION,
    PROP_AUTO_CLIPBOARD,
    PROP_AUTO_USBREDIR,
};

static void spice_gtk_session_init(SpiceGtkSession *self)
{
    SpiceGtkSessionPrivate *s;

    s = self->priv = SPICE_GTK_SESSION_GET_PRIVATE(self);

    s->clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(G_OBJECT(s->clipboard), "owner-change",
                     G_CALLBACK(clipboard_owner_change), self);
    s->clipboard_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    g_signal_connect(G_OBJECT(s->clipboard_primary), "owner-change",
                     G_CALLBACK(clipboard_owner_change), self);
}

static GObject *
spice_gtk_session_constructor(GType                  gtype,
                              guint                  n_properties,
                              GObjectConstructParam *properties)
{
    GObject *obj;
    SpiceGtkSession *self;
    SpiceGtkSessionPrivate *s;
    GList *list;
    GList *it;

    {
        /* Always chain up to the parent constructor */
        GObjectClass *parent_class;
        parent_class = G_OBJECT_CLASS(spice_gtk_session_parent_class);
        obj = parent_class->constructor(gtype, n_properties, properties);
    }

    self = SPICE_GTK_SESSION(obj);
    s = self->priv;
    if (!s->session)
        g_error("SpiceGtKSession constructed without a session");

    g_signal_connect(s->session, "channel-new",
                     G_CALLBACK(channel_new), self);
    g_signal_connect(s->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), self);
    list = spice_session_get_channels(s->session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(s->session, it->data, (gpointer*)self);
    }
    g_list_free(list);

    return obj;
}

static void spice_gtk_session_dispose(GObject *gobject)
{
    SpiceGtkSession *self = SPICE_GTK_SESSION(gobject);
    SpiceGtkSessionPrivate *s = self->priv;

    /* release stuff */
    if (s->clipboard) {
        g_signal_handlers_disconnect_by_func(s->clipboard,
                G_CALLBACK(clipboard_owner_change), self);
        s->clipboard = NULL;
    }

    if (s->clipboard_primary) {
        g_signal_handlers_disconnect_by_func(s->clipboard_primary,
                G_CALLBACK(clipboard_owner_change), self);
        s->clipboard_primary = NULL;
    }

    if (s->session) {
        g_signal_handlers_disconnect_by_func(s->session,
                                             G_CALLBACK(channel_new),
                                             self);
        g_signal_handlers_disconnect_by_func(s->session,
                                             G_CALLBACK(channel_destroy),
                                             self);
        s->session = NULL;
    }

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_gtk_session_parent_class)->dispose)
        G_OBJECT_CLASS(spice_gtk_session_parent_class)->dispose(gobject);
}

static void spice_gtk_session_finalize(GObject *gobject)
{
    SpiceGtkSession *self = SPICE_GTK_SESSION(gobject);
    SpiceGtkSessionPrivate *s = self->priv;
    int i;

    /* release stuff */
    for (i = 0; i < CLIPBOARD_LAST; ++i) {
        g_free(s->clip_targets[i]);
        s->clip_targets[i] = NULL;
    }

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_gtk_session_parent_class)->finalize)
        G_OBJECT_CLASS(spice_gtk_session_parent_class)->finalize(gobject);
}

static void spice_gtk_session_get_property(GObject    *gobject,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
    SpiceGtkSession *self = SPICE_GTK_SESSION(gobject);
    SpiceGtkSessionPrivate *s = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, s->session);
	break;
    case PROP_AUTO_CLIPBOARD:
        g_value_set_boolean(value, s->auto_clipboard_enable);
        break;
    case PROP_AUTO_USBREDIR:
        g_value_set_boolean(value, s->auto_usbredir_enable);
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
	break;
    }
}

static void spice_gtk_session_set_property(GObject      *gobject,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
    SpiceGtkSession *self = SPICE_GTK_SESSION(gobject);
    SpiceGtkSessionPrivate *s = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        s->session = g_value_get_object(value);
        break;
    case PROP_AUTO_CLIPBOARD:
        s->auto_clipboard_enable = g_value_get_boolean(value);
        break;
    case PROP_AUTO_USBREDIR: {
        SpiceDesktopIntegration *desktop_int;
        gboolean orig_value = s->auto_usbredir_enable;

        s->auto_usbredir_enable = g_value_get_boolean(value);
        if (s->auto_usbredir_enable == orig_value)
            break;

        if (s->auto_usbredir_reqs) {
            SpiceUsbDeviceManager *manager =
                spice_usb_device_manager_get(s->session, NULL);

            if (!manager)
                break;

            g_object_set(manager, "auto-connect", s->auto_usbredir_enable,
                         NULL);

            desktop_int = spice_desktop_integration_get(s->session);
            if (s->auto_usbredir_enable)
                spice_desktop_integration_inhibit_automount(desktop_int);
            else
                spice_desktop_integration_uninhibit_automount(desktop_int);
        }
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_gtk_session_class_init(SpiceGtkSessionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->constructor  = spice_gtk_session_constructor;
    gobject_class->dispose      = spice_gtk_session_dispose;
    gobject_class->finalize     = spice_gtk_session_finalize;
    gobject_class->get_property = spice_gtk_session_get_property;
    gobject_class->set_property = spice_gtk_session_set_property;

    /**
     * SpiceGtkSession:session:
     *
     * #SpiceSession this #SpiceGtkSession is associated with
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_SESSION,
         g_param_spec_object("session",
                             "Session",
                             "SpiceSession",
                             SPICE_TYPE_SESSION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceGtkSession:auto-clipboard:
     *
     * When this is true the clipboard gets automatically shared between host
     * and guest.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_AUTO_CLIPBOARD,
         g_param_spec_boolean("auto-clipboard",
                              "Auto clipboard",
                              "Automatically relay clipboard changes between "
                              "host and guest.",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceGtkSession:auto-usbredir:
     *
     * Automatically redirect newly plugged in USB devices. Note the auto
     * redirection only happens when a #SpiceDisplay associated with the
     * session had keyboard focus.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_AUTO_USBREDIR,
         g_param_spec_boolean("auto-usbredir",
                              "Auto USB Redirection",
                              "Automatically redirect newly plugged in USB"
                              "Devices to the guest.",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_type_class_add_private(klass, sizeof(SpiceGtkSessionPrivate));
}

/* ---------------------------------------------------------------- */
/* private functions (clipboard related)                            */

static GtkClipboard* get_clipboard_from_selection(SpiceGtkSessionPrivate *s,
                                                  guint selection)
{
    if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        return s->clipboard;
    } else if (selection == VD_AGENT_CLIPBOARD_SELECTION_PRIMARY) {
        return s->clipboard_primary;
    } else {
        g_warning("Unhandled clipboard selection: %d", selection);
        return NULL;
    }
}

static gint get_selection_from_clipboard(SpiceGtkSessionPrivate *s,
                                         GtkClipboard* cb)
{
    if (cb == s->clipboard) {
        return VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;
    } else if (cb == s->clipboard_primary) {
        return VD_AGENT_CLIPBOARD_SELECTION_PRIMARY;
    } else {
        g_warning("Unhandled clipboard");
        return -1;
    }
}

static const struct {
    const char  *xatom;
    uint32_t    vdagent;
} atom2agent[] = {
    {
        .vdagent = VD_AGENT_CLIPBOARD_UTF8_TEXT,
        .xatom   = "UTF8_STRING",
    },{
        .vdagent = VD_AGENT_CLIPBOARD_UTF8_TEXT,
        .xatom   = "text/plain;charset=utf-8"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_UTF8_TEXT,
        .xatom   = "STRING"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_UTF8_TEXT,
        .xatom   = "TEXT"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_UTF8_TEXT,
        .xatom   = "text/plain"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_PNG,
        .xatom   = "image/png"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_BMP,
        .xatom   = "image/bmp"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_BMP,
        .xatom   = "image/x-bmp"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_BMP,
        .xatom   = "image/x-MS-bmp"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_BMP,
        .xatom   = "image/x-win-bitmap"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_TIFF,
        .xatom   = "image/tiff"
    },{
        .vdagent = VD_AGENT_CLIPBOARD_IMAGE_JPG,
        .xatom   = "image/jpeg"
    }
};

typedef struct _WeakRef {
    GObject *object;
} WeakRef;

static void weak_notify_cb(WeakRef *weakref, GObject *object)
{
    weakref->object = NULL;
}

static WeakRef* weak_ref(GObject *object)
{
    WeakRef *weakref = g_new(WeakRef, 1);

    g_object_weak_ref(object, (GWeakNotify)weak_notify_cb, weakref);
    weakref->object = object;

    return weakref;
}

static void weak_unref(WeakRef* weakref)
{
    if (weakref->object)
        g_object_weak_unref(weakref->object, (GWeakNotify)weak_notify_cb, weakref);

    g_free(weakref);
}

static void clipboard_get_targets(GtkClipboard *clipboard,
                                  GdkAtom *atoms,
                                  gint n_atoms,
                                  gpointer user_data)
{
    WeakRef *weakref = user_data;
    SpiceGtkSession *self = (SpiceGtkSession*)weakref->object;
    weak_unref(weakref);

    if (self == NULL)
        return;

    g_return_if_fail(SPICE_IS_GTK_SESSION(self));

    SpiceGtkSessionPrivate *s = self->priv;
    guint32 types[SPICE_N_ELEMENTS(atom2agent)];
    char *name;
    int a, m, t;
    int selection;

    if (s->main == NULL)
        return;

    selection = get_selection_from_clipboard(s, clipboard);
    g_return_if_fail(selection != -1);

    SPICE_DEBUG("%s:", __FUNCTION__);
    if (spice_util_get_debug()) {
        for (a = 0; a < n_atoms; a++) {
            name = gdk_atom_name(atoms[a]);
            SPICE_DEBUG(" \"%s\"", name);
            g_free(name);
        }
    }

    memset(types, 0, sizeof(types));
    for (a = 0; a < n_atoms; a++) {
        name = gdk_atom_name(atoms[a]);
        for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
            if (strcasecmp(name, atom2agent[m].xatom) != 0) {
                continue;
            }
            /* found match */
            for (t = 0; t < SPICE_N_ELEMENTS(atom2agent); t++) {
                if (types[t] == atom2agent[m].vdagent) {
                    /* type already in list */
                    break;
                }
                if (types[t] == 0) {
                    /* add type to empty slot */
                    types[t] = atom2agent[m].vdagent;
                    break;
                }
            }
            break;
        }
        g_free(name);
    }
    for (t = 0; t < SPICE_N_ELEMENTS(atom2agent); t++) {
        if (types[t] == 0) {
            break;
        }
    }
    if (!s->clip_grabbed[selection] && t > 0) {
        s->clip_grabbed[selection] = TRUE;

        if (spice_main_agent_test_capability(s->main, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND))
            spice_main_clipboard_selection_grab(s->main, selection, types, t);
        /* Sending a grab causes the agent to do an impicit release */
        s->nclip_targets[selection] = 0;
    }
}

static void clipboard_owner_change(GtkClipboard        *clipboard,
                                   GdkEventOwnerChange *event,
                                   gpointer            user_data)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(user_data));

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    int selection;

    selection = get_selection_from_clipboard(s, clipboard);
    g_return_if_fail(selection != -1);

    if (s->main == NULL)
        return;

    if (s->clip_grabbed[selection]) {
        s->clip_grabbed[selection] = FALSE;
        if (spice_main_agent_test_capability(s->main, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND))
            spice_main_clipboard_selection_release(s->main, selection);
    }

    switch (event->reason) {
    case GDK_OWNER_CHANGE_NEW_OWNER:
        if (gtk_clipboard_get_owner(clipboard) == G_OBJECT(self))
            break;

        s->clipboard_by_guest[selection] = FALSE;
        s->clip_hasdata[selection] = TRUE;
        if (s->auto_clipboard_enable && !read_only(self))
            gtk_clipboard_request_targets(clipboard, clipboard_get_targets,
                                          weak_ref(G_OBJECT(self)));
        break;
    default:
        s->clip_hasdata[selection] = FALSE;
        break;
    }
}

typedef struct
{
    SpiceGtkSession *self;
    GMainLoop *loop;
    GtkSelectionData *selection_data;
    guint info;
    guint selection;
} RunInfo;

static void clipboard_got_from_guest(SpiceMainChannel *main, guint selection,
                                     guint type, const guchar *data, guint size,
                                     gpointer user_data)
{
    RunInfo *ri = user_data;
    SpiceGtkSessionPrivate *s = ri->self->priv;
    gchar *conv = NULL;

    g_return_if_fail(selection == ri->selection);

    SPICE_DEBUG("clipboard got data");

    if (atom2agent[ri->info].vdagent == VD_AGENT_CLIPBOARD_UTF8_TEXT) {
        /* on windows, gtk+ would already convert to LF endings, but
           not on unix */
        if (spice_main_agent_test_capability(s->main, VD_AGENT_CAP_GUEST_LINEEND_CRLF)) {
            GError *err = NULL;

            conv = spice_dos2unix((gchar*)data, size, &err);
            if (err) {
                g_warning("Failed to convert text line ending: %s", err->message);
                g_clear_error(&err);
                goto end;
            }

            size = strlen(conv);
        }

        gtk_selection_data_set_text(ri->selection_data, conv ?: (gchar*)data, size);
    } else {
        gtk_selection_data_set(ri->selection_data,
            gdk_atom_intern_static_string(atom2agent[ri->info].xatom),
            8, data, size);
    }

end:
    if (g_main_loop_is_running (ri->loop))
        g_main_loop_quit (ri->loop);

    g_free(conv);
}

static void clipboard_agent_connected(RunInfo *ri)
{
    g_warning("agent status changed, cancel clipboard request");

    if (g_main_loop_is_running(ri->loop))
        g_main_loop_quit(ri->loop);
}

static void clipboard_get(GtkClipboard *clipboard,
                          GtkSelectionData *selection_data,
                          guint info, gpointer user_data)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(user_data));

    RunInfo ri = { NULL, };
    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    gboolean agent_connected = FALSE;
    gulong clipboard_handler;
    gulong agent_handler;
    int selection;

    SPICE_DEBUG("clipboard get");

    selection = get_selection_from_clipboard(s, clipboard);
    g_return_if_fail(selection != -1);
    g_return_if_fail(info < SPICE_N_ELEMENTS(atom2agent));
    g_return_if_fail(s->main != NULL);

    ri.selection_data = selection_data;
    ri.info = info;
    ri.loop = g_main_loop_new(NULL, FALSE);
    ri.selection = selection;
    ri.self = self;

    clipboard_handler = g_signal_connect(s->main, "main-clipboard-selection",
                                         G_CALLBACK(clipboard_got_from_guest),
                                         &ri);
    agent_handler = g_signal_connect_swapped(s->main, "notify::agent-connected",
                                     G_CALLBACK(clipboard_agent_connected),
                                     &ri);

    spice_main_clipboard_selection_request(s->main, selection,
                                           atom2agent[info].vdagent);


    g_object_get(s->main, "agent-connected", &agent_connected, NULL);
    if (!agent_connected) {
        SPICE_DEBUG("canceled clipboard_get, before running loop");
        goto cleanup;
    }

    /* apparently, this is needed to avoid dead-lock, from
       gtk_dialog_run */
    gdk_threads_leave();
    g_main_loop_run(ri.loop);
    gdk_threads_enter();

cleanup:
    g_main_loop_unref(ri.loop);
    ri.loop = NULL;
    g_signal_handler_disconnect(s->main, clipboard_handler);
    g_signal_handler_disconnect(s->main, agent_handler);
}

static void clipboard_clear(GtkClipboard *clipboard, gpointer user_data)
{
    SPICE_DEBUG("clipboard_clear");
    /* We watch for clipboard ownership changes and act on those, so we
       don't need to do anything here */
}

static gboolean clipboard_grab(SpiceMainChannel *main, guint selection,
                               guint32* types, guint32 ntypes,
                               gpointer user_data)
{
    g_return_val_if_fail(SPICE_IS_GTK_SESSION(user_data), FALSE);

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    GtkTargetEntry targets[SPICE_N_ELEMENTS(atom2agent)];
    gboolean target_selected[SPICE_N_ELEMENTS(atom2agent)] = { FALSE, };
    gboolean found;
    GtkClipboard* cb;
    int m, n, i;

    cb = get_clipboard_from_selection(s, selection);
    g_return_val_if_fail(cb != NULL, FALSE);

    i = 0;
    for (n = 0; n < ntypes; ++n) {
        found = FALSE;
        for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
            if (atom2agent[m].vdagent == types[n] && !target_selected[m]) {
                found = TRUE;
                g_return_val_if_fail(i < SPICE_N_ELEMENTS(atom2agent), FALSE);
                targets[i].target = (gchar*)atom2agent[m].xatom;
                targets[i].info = m;
                target_selected[m] = TRUE;
                i += 1;
            }
        }
        if (!found) {
            g_warning("clipboard: couldn't find a matching type for: %d",
                      types[n]);
        }
    }

    g_free(s->clip_targets[selection]);
    s->nclip_targets[selection] = i;
    s->clip_targets[selection] = g_memdup(targets, sizeof(GtkTargetEntry) * i);
    /* Receiving a grab implies we've released our own grab */
    s->clip_grabbed[selection] = FALSE;

    if (read_only(self) ||
        !s->auto_clipboard_enable ||
        s->nclip_targets[selection] == 0)
        goto skip_grab_clipboard;

    if (!gtk_clipboard_set_with_owner(cb, targets, i,
                                      clipboard_get, clipboard_clear, G_OBJECT(self))) {
        g_warning("clipboard grab failed");
        return FALSE;
    }
    s->clipboard_by_guest[selection] = TRUE;
    s->clip_hasdata[selection] = FALSE;

skip_grab_clipboard:
    return TRUE;
}

static void clipboard_received_cb(GtkClipboard *clipboard,
                                  GtkSelectionData *selection_data,
                                  gpointer user_data)
{
    WeakRef *weakref = user_data;
    SpiceGtkSession *self = (SpiceGtkSession*)weakref->object;
    weak_unref(weakref);

    if (self == NULL)
        return;

    g_return_if_fail(SPICE_IS_GTK_SESSION(self));

    SpiceGtkSessionPrivate *s = self->priv;
    gint len = 0, m;
    guint32 type = VD_AGENT_CLIPBOARD_NONE;
    gchar* name;
    GdkAtom atom;
    int selection;
    int max_clipboard;

    selection = get_selection_from_clipboard(s, clipboard);
    g_return_if_fail(selection != -1);

    g_object_get(s->main, "max-clipboard", &max_clipboard, NULL);
    len = gtk_selection_data_get_length(selection_data);
    if (len == 0 || (max_clipboard != -1 && len > max_clipboard)) {
        g_warning("discarded clipboard of size %d (max: %d)", len, max_clipboard);
        return;
    } else if (len == -1) {
        SPICE_DEBUG("empty clipboard");
        len = 0;
    } else {
        atom = gtk_selection_data_get_data_type(selection_data);
        name = gdk_atom_name(atom);
        for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
            if (strcasecmp(name, atom2agent[m].xatom) == 0) {
                break;
            }
        }

        if (m >= SPICE_N_ELEMENTS(atom2agent)) {
            g_warning("clipboard_received for unsupported type: %s", name);
        } else {
            type = atom2agent[m].vdagent;
        }

        g_free(name);
    }

    const guchar *data = gtk_selection_data_get_data(selection_data);
    gpointer conv = NULL;

    /* gtk+ internal utf8 newline is always LF, even on windows */
    if (type == VD_AGENT_CLIPBOARD_UTF8_TEXT &&
        spice_main_agent_test_capability(s->main, VD_AGENT_CAP_GUEST_LINEEND_CRLF)) {
        GError *err = NULL;

        conv = spice_unix2dos((gchar*)data, len, &err);
        if (err) {
            g_warning("Failed to convert text line ending: %s", err->message);
            g_clear_error(&err);
            return;
        }

        len = strlen(conv);
    }

    spice_main_clipboard_selection_notify(s->main, selection, type,
                                          conv ?: data, len);
    g_free(conv);
}

static gboolean clipboard_request(SpiceMainChannel *main, guint selection,
                                  guint type, gpointer user_data)
{
    g_return_val_if_fail(SPICE_IS_GTK_SESSION(user_data), FALSE);

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    GdkAtom atom;
    GtkClipboard* cb;
    int m;

    if (read_only(self))
        return FALSE;

    cb = get_clipboard_from_selection(s, selection);
    g_return_val_if_fail(cb != NULL, FALSE);

    for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
        if (atom2agent[m].vdagent == type)
            break;
    }

    g_return_val_if_fail(m < SPICE_N_ELEMENTS(atom2agent), FALSE);

    atom = gdk_atom_intern_static_string(atom2agent[m].xatom);
    gtk_clipboard_request_contents(cb, atom, clipboard_received_cb,
                                   weak_ref(G_OBJECT(self)));

    return TRUE;
}

static void clipboard_release(SpiceMainChannel *main, guint selection,
                              gpointer user_data)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(user_data));

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    GtkClipboard* clipboard = get_clipboard_from_selection(s, selection);

    if (!clipboard)
        return;

    s->nclip_targets[selection] = 0;

    if (!s->clipboard_by_guest[selection])
        return;
    gtk_clipboard_clear(clipboard);
    s->clipboard_by_guest[selection] = FALSE;
}

static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(user_data));

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("Changing main channel from %p to %p", s->main, channel);
        s->main = SPICE_MAIN_CHANNEL(channel);
        g_signal_connect(channel, "main-clipboard-selection-grab",
                         G_CALLBACK(clipboard_grab), self);
        g_signal_connect(channel, "main-clipboard-selection-request",
                         G_CALLBACK(clipboard_request), self);
        g_signal_connect(channel, "main-clipboard-selection-release",
                         G_CALLBACK(clipboard_release), self);
    }
}

static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(user_data));

    SpiceGtkSession *self = user_data;
    SpiceGtkSessionPrivate *s = self->priv;
    guint i;

    if (SPICE_IS_MAIN_CHANNEL(channel) && SPICE_MAIN_CHANNEL(channel) == s->main) {
        s->main = NULL;
        for (i = 0; i < CLIPBOARD_LAST; ++i) {
            if (s->clipboard_by_guest[i]) {
                GtkClipboard *cb = get_clipboard_from_selection(s, i);
                if (cb)
                    gtk_clipboard_clear(cb);
                s->clipboard_by_guest[i] = FALSE;
            }
            s->clip_grabbed[i] = FALSE;
            s->nclip_targets[i] = 0;
        }
    }
}

static gboolean read_only(SpiceGtkSession *self)
{
    return spice_session_get_read_only(self->priv->session);
}

/* ---------------------------------------------------------------- */
/* private functions (usbredir related)                             */
G_GNUC_INTERNAL
void spice_gtk_session_request_auto_usbredir(SpiceGtkSession *self,
                                             gboolean state)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(self));

    SpiceGtkSessionPrivate *s = self->priv;
    SpiceDesktopIntegration *desktop_int;
    SpiceUsbDeviceManager *manager;

    if (state) {
        s->auto_usbredir_reqs++;
        if (s->auto_usbredir_reqs != 1)
            return;
    } else {
        g_return_if_fail(s->auto_usbredir_reqs > 0);
        s->auto_usbredir_reqs--;
        if (s->auto_usbredir_reqs != 0)
            return;
    }

    if (!s->auto_usbredir_enable)
        return;

    manager = spice_usb_device_manager_get(s->session, NULL);
    if (!manager)
        return;

    g_object_set(manager, "auto-connect", state, NULL);

    desktop_int = spice_desktop_integration_get(s->session);
    if (state)
        spice_desktop_integration_inhibit_automount(desktop_int);
    else
        spice_desktop_integration_uninhibit_automount(desktop_int);
}

/* ------------------------------------------------------------------ */
/* public functions                                                   */

/**
 * spice_gtk_session_get:
 * @session: #SpiceSession for which to get the #SpiceGtkSession
 *
 * Gets the #SpiceGtkSession associated with the passed in #SpiceSession.
 * A new #SpiceGtkSession instance will be created the first time this
 * function is called for a certain #SpiceSession.
 *
 * Note that this function returns a weak reference, which should not be used
 * after the #SpiceSession itself has been unref-ed by the caller.
 *
 * Returns: (transfer none): a weak reference to the #SpiceGtkSession associated with the passed in #SpiceSession
 *
 * Since 0.8
 **/
SpiceGtkSession *spice_gtk_session_get(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceGtkSession *self;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_static_mutex_lock(&mutex);
    self = session->priv->gtk_session;
    if (self == NULL) {
        self = g_object_new(SPICE_TYPE_GTK_SESSION, "session", session, NULL);
        session->priv->gtk_session = self;
    }
    g_static_mutex_unlock(&mutex);

    return SPICE_GTK_SESSION(self);
}

/**
 * spice_gtk_session_copy_to_guest:
 * @self:
 *
 * Copy client-side clipboard to guest clipboard.
 *
 * Since 0.8
 **/
void spice_gtk_session_copy_to_guest(SpiceGtkSession *self)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(self));
    g_return_if_fail(read_only(self) == FALSE);

    SpiceGtkSessionPrivate *s = self->priv;
    int selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    if (s->clip_hasdata[selection] && !s->clip_grabbed[selection]) {
        gtk_clipboard_request_targets(s->clipboard, clipboard_get_targets,
                                      weak_ref(G_OBJECT(self)));
    }
}

/**
 * spice_gtk_session_paste_from_guest:
 * @self:
 *
 * Copy guest clipboard to client-side clipboard.
 *
 * Since 0.8
 **/
void spice_gtk_session_paste_from_guest(SpiceGtkSession *self)
{
    g_return_if_fail(SPICE_IS_GTK_SESSION(self));
    g_return_if_fail(read_only(self) == FALSE);

    SpiceGtkSessionPrivate *s = self->priv;
    int selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    if (s->nclip_targets[selection] == 0) {
        g_warning("Guest clipboard is not available.");
        return;
    }

    if (!gtk_clipboard_set_with_owner(s->clipboard, s->clip_targets[selection], s->nclip_targets[selection],
                                      clipboard_get, clipboard_clear, G_OBJECT(self))) {
        g_warning("Clipboard grab failed");
        return;
    }
    s->clipboard_by_guest[selection] = TRUE;
    s->clip_hasdata[selection] = FALSE;
}
