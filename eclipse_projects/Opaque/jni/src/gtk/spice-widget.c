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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_X11_XKBLIB_H
#include <X11/XKBlib.h>
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <gdk/gdkwin32.h>
#ifndef MAPVK_VK_TO_VSC /* may be undefined in older mingw-headers */
#define MAPVK_VK_TO_VSC 0
#endif
#endif

#include "spice-widget.h"
#include "spice-widget-priv.h"
#include "spice-gtk-session-priv.h"
#include "vncdisplaykeymap.h"

#include "glib-compat.h"

/* Some compatibility defines to let us build on both Gtk2 and Gtk3 */
#if GTK_CHECK_VERSION (2, 91, 0)
static inline void gdk_drawable_get_size(GdkWindow *w, gint *ww, gint *wh)
{
    *ww = gdk_window_get_width(w);
    *wh = gdk_window_get_height(w);
}
#endif

#if !GTK_CHECK_VERSION (2, 91, 0)
#define GDK_IS_X11_DISPLAY(D) TRUE
#define gdk_window_get_display(W) gdk_drawable_get_display(GDK_DRAWABLE(W))
#endif

#if !GTK_CHECK_VERSION(2, 20, 0)
static gboolean gtk_widget_get_realized(GtkWidget *widget)
{
    g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
    return GTK_WIDGET_REALIZED(widget);
}
#endif

/**
 * SECTION:spice-widget
 * @short_description: a GTK display widget
 * @title: Spice Display
 * @section_id:
 * @stability: Stable
 * @include: spice-widget.h
 *
 * A GTK widget that displays a SPICE server. It sends keyboard/mouse
 * events and can also share clipboard...
 *
 * Arbitrary key events can be sent thanks to spice_display_send_keys().
 *
 * The widget will optionally grab the keyboard and the mouse when
 * focused if the properties #SpiceDisplay:grab-keyboard and
 * #SpiceDisplay:grab-mouse are #TRUE respectively.  It can be
 * ungrabbed with spice_display_mouse_ungrab(), and by setting a key
 * combination with spice_display_set_grab_keys().
 *
 * Finally, spice_display_get_pixbuf() will take a screenshot of the
 * current display and return an #GdkPixbuf (that you can then easily
 * save to disk).
 */

G_DEFINE_TYPE(SpiceDisplay, spice_display, GTK_TYPE_DRAWING_AREA)

/* Properties */
enum {
    PROP_0,
    PROP_SESSION,
    PROP_CHANNEL_ID,
    PROP_KEYBOARD_GRAB,
    PROP_MOUSE_GRAB,
    PROP_RESIZE_GUEST,
    PROP_AUTO_CLIPBOARD,
    PROP_SCALING,
    PROP_ONLY_DOWNSCALE,
    PROP_DISABLE_INPUTS,
    PROP_ZOOM_LEVEL,
    PROP_MONITOR_ID,
    PROP_KEYPRESS_DELAY,
    PROP_READY
};

/* Signals */
enum {
    SPICE_DISPLAY_MOUSE_GRAB,
    SPICE_DISPLAY_KEYBOARD_GRAB,
    SPICE_DISPLAY_GRAB_KEY_PRESSED,
    SPICE_DISPLAY_LAST_SIGNAL,
};

static guint signals[SPICE_DISPLAY_LAST_SIGNAL];

#ifdef WIN32
static HWND win32_window = NULL;
#endif

static void update_keyboard_grab(SpiceDisplay *display);
static void try_keyboard_grab(SpiceDisplay *display);
static void try_keyboard_ungrab(SpiceDisplay *display);
static void update_mouse_grab(SpiceDisplay *display);
static void try_mouse_grab(SpiceDisplay *display);
static void try_mouse_ungrab(SpiceDisplay *display);
static void recalc_geometry(GtkWidget *widget);
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void sync_keyboard_lock_modifiers(SpiceDisplay *display);
static void cursor_invalidate(SpiceDisplay *display);
static void update_area(SpiceDisplay *display, gint x, gint y, gint width, gint height);
static void release_keys(SpiceDisplay *display);

/* ---------------------------------------------------------------- */

static void spice_display_get_property(GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    SpiceDisplay *display = SPICE_DISPLAY(object);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gboolean boolean;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, d->session);
        break;
    case PROP_CHANNEL_ID:
        g_value_set_int(value, d->channel_id);
        break;
    case PROP_MONITOR_ID:
        g_value_set_int(value, d->monitor_id);
        break;
    case PROP_KEYBOARD_GRAB:
        g_value_set_boolean(value, d->keyboard_grab_enable);
        break;
    case PROP_MOUSE_GRAB:
        g_value_set_boolean(value, d->mouse_grab_enable);
        break;
    case PROP_RESIZE_GUEST:
        g_value_set_boolean(value, d->resize_guest_enable);
        break;
    case PROP_AUTO_CLIPBOARD:
        g_object_get(d->gtk_session, "auto-clipboard", &boolean, NULL);
        g_value_set_boolean(value, boolean);
        break;
    case PROP_SCALING:
        g_value_set_boolean(value, d->allow_scaling);
        break;
    case PROP_ONLY_DOWNSCALE:
        g_value_set_boolean(value, d->only_downscale);
        break;
    case PROP_DISABLE_INPUTS:
        g_value_set_boolean(value, d->disable_inputs);
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_int(value, d->zoom_level);
        break;
    case PROP_READY:
        g_value_set_boolean(value, d->ready);
        break;
    case PROP_KEYPRESS_DELAY:
        g_value_set_uint(value, d->keypress_delay);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void scaling_updated(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(display));

    recalc_geometry(GTK_WIDGET(display));
    if (d->ximage && window) { /* if not yet shown */
        gtk_widget_queue_draw(GTK_WIDGET(display));
    }
}

static void update_size_request(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gint reqwidth, reqheight;

    if (d->resize_guest_enable) {
        reqwidth = 640;
        reqheight = 480;
    } else {
        reqwidth = d->area.width;
        reqheight = d->area.height;
    }

    gtk_widget_set_size_request(GTK_WIDGET(display), reqwidth, reqheight);
    recalc_geometry(GTK_WIDGET(display));
}

static void update_keyboard_focus(SpiceDisplay *display, gboolean state)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    d->keyboard_have_focus = state;

    /* keyboard grab gets inhibited by usb-device-manager when it is
       in the process of redirecting a usb-device (as this may show a
       policykit dialog). Making autoredir/automount setting changes while
       this is happening is not a good idea! */
    if (d->keyboard_grab_inhibit)
        return;

    spice_gtk_session_request_auto_usbredir(d->gtk_session, state);
}

static void update_ready(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gboolean ready;

    ready = d->mark != 0 && d->monitor_ready;

    if (d->ready == ready)
        return;

    if (ready && gtk_widget_get_window(GTK_WIDGET(display)))
        gtk_widget_queue_draw(GTK_WIDGET(display));

    d->ready = ready;
    g_object_notify(G_OBJECT(display), "ready");
}

static void set_monitor_ready(SpiceDisplay *self, gboolean ready)
{
    SpiceDisplayPrivate *d = self->priv;

    d->monitor_ready = ready;
    update_ready(self);
}

static gint get_display_id(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    /* supported monitor_id only with display channel #0 */
    if (d->channel_id == 0 && d->monitor_id >= 0)
        return d->monitor_id;

    g_return_val_if_fail(d->monitor_id <= 0, -1);

    return d->channel_id;
}

static void update_monitor_area(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    SpiceDisplayMonitorConfig *cfg, *c = NULL;
    GArray *monitors = NULL;
    int i;

    SPICE_DEBUG("update monitor area %d:%d", d->channel_id, d->monitor_id);
    if (d->monitor_id < 0)
        goto whole;

    g_object_get(d->display, "monitors", &monitors, NULL);
    for (i = 0; monitors != NULL && i < monitors->len; i++) {
        cfg = &g_array_index(monitors, SpiceDisplayMonitorConfig, i);
        if (cfg->id == d->monitor_id) {
           c = cfg;
           break;
        }
    }
    if (c == NULL) {
        SPICE_DEBUG("update monitor: no monitor %d", d->monitor_id);
        set_monitor_ready(display, false);
        if (spice_channel_test_capability(d->display, SPICE_DISPLAY_CAP_MONITORS_CONFIG)) {
            SPICE_DEBUG("waiting until MonitorsConfig is received");
            g_clear_pointer(&monitors, g_array_unref);
            return;
        }
        goto whole;
    }

    if (c->surface_id != 0) {
        g_warning("FIXME: only support monitor config with primary surface 0, "
                  "but given config surface %d", c->surface_id);
        goto whole;
    }

    if (!d->resize_guest_enable)
        spice_main_update_display(d->main, get_display_id(display),
                                  c->x, c->y, c->width, c->height, FALSE);

    update_area(display, c->x, c->y, c->width, c->height);
    g_clear_pointer(&monitors, g_array_unref);
    return;

whole:
    g_clear_pointer(&monitors, g_array_unref);
    /* by display whole surface */
    update_area(display, 0, 0, d->width, d->height);
    set_monitor_ready(display, true);
}

static void spice_display_set_property(GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    SpiceDisplay *display = SPICE_DISPLAY(object);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    switch (prop_id) {
    case PROP_SESSION:
        g_warn_if_fail(d->session == NULL);
        d->session = g_value_dup_object(value);
        d->gtk_session = spice_gtk_session_get(d->session);
        break;
    case PROP_CHANNEL_ID:
        d->channel_id = g_value_get_int(value);
        break;
    case PROP_MONITOR_ID:
        d->monitor_id = g_value_get_int(value);
        if (d->display) /* if constructed */
            update_monitor_area(display);
        break;
    case PROP_KEYBOARD_GRAB:
        d->keyboard_grab_enable = g_value_get_boolean(value);
        update_keyboard_grab(display);
        break;
    case PROP_MOUSE_GRAB:
        d->mouse_grab_enable = g_value_get_boolean(value);
        update_mouse_grab(display);
        break;
    case PROP_RESIZE_GUEST:
        d->resize_guest_enable = g_value_get_boolean(value);
        update_size_request(display);
        break;
    case PROP_SCALING:
        d->allow_scaling = g_value_get_boolean(value);
        scaling_updated(display);
        break;
    case PROP_ONLY_DOWNSCALE:
        d->only_downscale = g_value_get_boolean(value);
        scaling_updated(display);
        break;
    case PROP_AUTO_CLIPBOARD:
        g_object_set(d->gtk_session, "auto-clipboard",
                     g_value_get_boolean(value), NULL);
        break;
    case PROP_DISABLE_INPUTS:
        d->disable_inputs = g_value_get_boolean(value);
        gtk_widget_set_can_focus(GTK_WIDGET(display), !d->disable_inputs);
        update_keyboard_grab(display);
        update_mouse_grab(display);
        break;
    case PROP_ZOOM_LEVEL:
        d->zoom_level = g_value_get_int(value);
        scaling_updated(display);
        break;
    case PROP_KEYPRESS_DELAY:
        d->keypress_delay = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gtk_session_property_changed(GObject    *gobject,
                                         GParamSpec *pspec,
                                         gpointer    user_data)
{
    SpiceDisplay *display = user_data;

    g_object_notify(G_OBJECT(display), g_param_spec_get_name(pspec));
}

static void session_inhibit_keyboard_grab_changed(GObject    *gobject,
                                                  GParamSpec *pspec,
                                                  gpointer    user_data)
{
    SpiceDisplay *display = user_data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    g_object_get(d->session, "inhibit-keyboard-grab",
                 &d->keyboard_grab_inhibit, NULL);
    update_keyboard_grab(display);
    update_mouse_grab(display);
}

static void spice_display_dispose(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("spice display dispose");

    spicex_image_destroy(display);
    g_clear_object(&d->session);
    d->gtk_session = NULL;

    if (d->key_delayed_id) {
        g_source_remove(d->key_delayed_id);
        d->key_delayed_id = 0;
    }

    G_OBJECT_CLASS(spice_display_parent_class)->dispose(obj);
}

static void spice_display_finalize(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("Finalize spice display");

    if (d->grabseq) {
        spice_grab_sequence_free(d->grabseq);
        d->grabseq = NULL;
    }
    g_free(d->activeseq);
    d->activeseq = NULL;

    if (d->show_cursor) {
        gdk_cursor_unref(d->show_cursor);
        d->show_cursor = NULL;
    }

    if (d->mouse_cursor) {
        gdk_cursor_unref(d->mouse_cursor);
        d->mouse_cursor = NULL;
    }

    if (d->mouse_pixbuf) {
        g_object_unref(d->mouse_pixbuf);
        d->mouse_pixbuf = NULL;
    }

    G_OBJECT_CLASS(spice_display_parent_class)->finalize(obj);
}

static GdkCursor* get_blank_cursor(void)
{
    if (g_getenv("SPICE_DEBUG_CURSOR"))
        return gdk_cursor_new(GDK_DOT);

    return gdk_cursor_new(GDK_BLANK_CURSOR);
}

static gboolean grab_broken(SpiceDisplay *self, GdkEventGrabBroken *event,
                            gpointer user_data G_GNUC_UNUSED)
{
    SPICE_DEBUG("%s (implicit: %d, keyboard: %d)", __FUNCTION__,
                event->implicit, event->keyboard);

    if (event->keyboard) {
        try_keyboard_ungrab(self);
        release_keys(self);
    }

    /* always release mouse when grab broken, this could be more
       generally placed in keyboard_ungrab(), but one might worry of
       breaking someone else code. */
    try_mouse_ungrab(self);

    return false;
}

static void drag_data_received_callback(SpiceDisplay *self,
                                        GdkDragContext *drag_context,
                                        gint x,
                                        gint y,
                                        GtkSelectionData *data,
                                        guint info,
                                        guint time,
                                        gpointer *user_data)
{
    const guchar *buf;
    gchar **file_urls;
    int n_files;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(self);
    int i = 0;
    GFile **files;

    /* We get a buf like:
     * file:///root/a.txt\r\nfile:///root/b.txt\r\n
     */
    SPICE_DEBUG("%s: drag a file", __FUNCTION__);
    buf = gtk_selection_data_get_data(data);
    g_return_if_fail(buf != NULL);

    file_urls = g_uri_list_extract_uris((const gchar*)buf);
    n_files = g_strv_length(file_urls);
    files = g_new0(GFile*, n_files + 1);
    for (i = 0; i < n_files; i++) {
        files[i] = g_file_new_for_uri(file_urls[i]);
    }
    g_strfreev(file_urls);

    spice_main_file_copy_async(d->main, files, 0, NULL, NULL,
                               NULL, NULL, NULL);
    for (i = 0; i < n_files; i++) {
        g_object_unref(files[i]);
    }
    g_free(files);

    gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

static void grab_notify(SpiceDisplay *display, gboolean was_grabbed)
{
    SPICE_DEBUG("grab notify %d", was_grabbed);

    if (was_grabbed == FALSE)
        release_keys(display);
}

static void spice_display_init(SpiceDisplay *display)
{
    GtkWidget *widget = GTK_WIDGET(display);
    SpiceDisplayPrivate *d;
    GtkTargetEntry targets = { "text/uri-list", 0, 0 };

    d = display->priv = SPICE_DISPLAY_GET_PRIVATE(display);

    g_signal_connect(display, "grab-broken-event", G_CALLBACK(grab_broken), NULL);
    g_signal_connect(display, "grab-notify", G_CALLBACK(grab_notify), NULL);

    gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, &targets, 1, GDK_ACTION_COPY);
    g_signal_connect(display, "drag-data-received",
                     G_CALLBACK(drag_data_received_callback), NULL);

    gtk_widget_add_events(widget,
                          GDK_STRUCTURE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_BUTTON_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK |
                          GDK_KEY_PRESS_MASK |
                          GDK_SCROLL_MASK);
#ifdef WITH_X11
    gtk_widget_set_double_buffered(widget, false);
#else
    gtk_widget_set_double_buffered(widget, true);
#endif
    gtk_widget_set_can_focus(widget, true);
    gtk_widget_set_has_window(widget, true);
    d->grabseq = spice_grab_sequence_new_from_string("Control_L+Alt_L");
    d->activeseq = g_new0(gboolean, d->grabseq->nkeysyms);

    d->mouse_cursor = get_blank_cursor();
    d->have_mitshm = true;
}

static GObject *
spice_display_constructor(GType                  gtype,
                          guint                  n_properties,
                          GObjectConstructParam *properties)
{
    GObject *obj;
    SpiceDisplay *display;
    SpiceDisplayPrivate *d;
    GList *list;
    GList *it;

    {
        /* Always chain up to the parent constructor */
        GObjectClass *parent_class;
        parent_class = G_OBJECT_CLASS(spice_display_parent_class);
        obj = parent_class->constructor(gtype, n_properties, properties);
    }

    display = SPICE_DISPLAY(obj);
    d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (!d->session)
        g_error("SpiceDisplay constructed without a session");

    spice_g_signal_connect_object(d->session, "channel-new",
                                  G_CALLBACK(channel_new), display, 0);
    spice_g_signal_connect_object(d->session, "channel-destroy",
                                  G_CALLBACK(channel_destroy), display, 0);
    list = spice_session_get_channels(d->session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        if (SPICE_IS_MAIN_CHANNEL(it->data)) {
            channel_new(d->session, it->data, (gpointer*)display);
            break;
        }
    }
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        if (!SPICE_IS_MAIN_CHANNEL(it->data))
            channel_new(d->session, it->data, (gpointer*)display);
    }
    g_list_free(list);

    spice_g_signal_connect_object(d->gtk_session, "notify::auto-clipboard",
                                  G_CALLBACK(gtk_session_property_changed), display, 0);

    spice_g_signal_connect_object(d->session, "notify::inhibit-keyboard-grab",
                                  G_CALLBACK(session_inhibit_keyboard_grab_changed),
                                  display, 0);

    return obj;
}

/**
 * spice_display_set_grab_keys:
 * @display: the display widget
 * @seq: (transfer none): key sequence
 *
 * Set the key combination to grab/ungrab the keyboard. The default is
 * "Control L + Alt L".
 **/
void spice_display_set_grab_keys(SpiceDisplay *display, SpiceGrabSequence *seq)
{
    SpiceDisplayPrivate *d;

    g_return_if_fail(SPICE_IS_DISPLAY(display));

    d = display->priv;
    g_return_if_fail(d != NULL);

    if (d->grabseq) {
        spice_grab_sequence_free(d->grabseq);
    }
    if (seq)
        d->grabseq = spice_grab_sequence_copy(seq);
    else
        d->grabseq = spice_grab_sequence_new_from_string("Control_L+Alt_L");
    g_free(d->activeseq);
    d->activeseq = g_new0(gboolean, d->grabseq->nkeysyms);
}

#ifdef WIN32
static LRESULT CALLBACK keyboard_hook_cb(int code, WPARAM wparam, LPARAM lparam)
{
    if  (win32_window && code == HC_ACTION && wparam != WM_KEYUP) {
        KBDLLHOOKSTRUCT *hooked = (KBDLLHOOKSTRUCT*)lparam;
        DWORD dwmsg = (hooked->flags << 24) | (hooked->scanCode << 16) | 1;

        if (hooked->vkCode == VK_NUMLOCK || hooked->vkCode == VK_RSHIFT) {
            dwmsg &= ~(1 << 24);
            SendMessage(win32_window, wparam, hooked->vkCode, dwmsg);
        }
        switch (hooked->vkCode) {
        case VK_CAPITAL:
        case VK_SCROLL:
        case VK_NUMLOCK:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
            break;
        case VK_LCONTROL:
            /* When pressing AltGr, an extra VK_LCONTROL with a special
             * scancode with bit 9 set is sent. Let's ignore the extra
             * VK_LCONTROL, as that will make AltGr misbehave. */
            if (hooked->scanCode & 0x200)
                return 1;
            break;
        default:
            SendMessage(win32_window, wparam, hooked->vkCode, dwmsg);
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}
#endif

/**
 * spice_display_get_grab_keys:
 * @display: the display widget
 *
 * Returns: (transfer none): the current grab key combination.
 **/
SpiceGrabSequence *spice_display_get_grab_keys(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d;

    g_return_val_if_fail(SPICE_IS_DISPLAY(display), NULL);

    d = display->priv;
    g_return_val_if_fail(d != NULL, NULL);

    return d->grabseq;
}

static void try_keyboard_grab(SpiceDisplay *display)
{
    GtkWidget *widget = GTK_WIDGET(display);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkGrabStatus status;

    if (g_getenv("SPICE_NOGRAB"))
        return;
    if (d->disable_inputs)
        return;

    if (d->keyboard_grab_inhibit)
        return;
    if (!d->keyboard_grab_enable)
        return;
    if (d->keyboard_grab_active)
        return;
    if (!d->keyboard_have_focus)
        return;
    if (!d->mouse_have_pointer)
        return;

    g_return_if_fail(gtk_widget_is_focus(widget));

    SPICE_DEBUG("grab keyboard");
    gtk_widget_grab_focus(widget);

#ifdef WIN32
    if (d->keyboard_hook == NULL)
        d->keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_cb,
                                            GetModuleHandle(NULL), 0);
    g_warn_if_fail(d->keyboard_hook != NULL);
#endif
    status = gdk_keyboard_grab(gtk_widget_get_window(widget), FALSE,
                               GDK_CURRENT_TIME);
    if (status != GDK_GRAB_SUCCESS) {
        g_warning("keyboard grab failed %d", status);
        d->keyboard_grab_active = false;
    } else {
        d->keyboard_grab_active = true;
        g_signal_emit(widget, signals[SPICE_DISPLAY_KEYBOARD_GRAB], 0, true);
    }
}

static void try_keyboard_ungrab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GtkWidget *widget = GTK_WIDGET(display);

    if (!d->keyboard_grab_active)
        return;

    SPICE_DEBUG("ungrab keyboard");
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#ifdef WIN32
    if (d->keyboard_hook != NULL) {
        UnhookWindowsHookEx(d->keyboard_hook);
        d->keyboard_hook = NULL;
    }
#endif
    d->keyboard_grab_active = false;
    g_signal_emit(widget, signals[SPICE_DISPLAY_KEYBOARD_GRAB], 0, false);
}

static void update_keyboard_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->keyboard_grab_enable &&
        !d->keyboard_grab_inhibit &&
        !d->disable_inputs)
        try_keyboard_grab(display);
    else
        try_keyboard_ungrab(display);
}

static void set_mouse_accel(SpiceDisplay *display, gboolean enabled)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

#if defined GDK_WINDOWING_X11
    GdkWindow *w = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));

    if (!GDK_IS_X11_DISPLAY(gdk_window_get_display(w))) {
        SPICE_DEBUG("FIXME: gtk backend is not X11");
        return;
    }

    Display *x_display = GDK_WINDOW_XDISPLAY(w);
    if (enabled) {
        /* restore mouse acceleration */
        XChangePointerControl(x_display, True, True,
                              d->x11_accel_numerator, d->x11_accel_denominator, d->x11_threshold);
    } else {
        XGetPointerControl(x_display,
                           &d->x11_accel_numerator, &d->x11_accel_denominator, &d->x11_threshold);
        /* set mouse acceleration to default */
        XChangePointerControl(x_display, True, True, -1, -1, -1);
        SPICE_DEBUG("disabled X11 mouse motion %d %d %d",
                    d->x11_accel_numerator, d->x11_accel_denominator, d->x11_threshold);
    }
#elif defined GDK_WINDOWING_WIN32
    if (enabled) {
        g_return_if_fail(SystemParametersInfo(SPI_SETMOUSE, 0, &d->win_mouse, 0));
        g_return_if_fail(SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(INT_PTR)d->win_mouse_speed, 0));
    } else {
        int accel[3] = { 0, 0, 0 }; // disabled
        g_return_if_fail(SystemParametersInfo(SPI_GETMOUSE, 0, &d->win_mouse, 0));
        g_return_if_fail(SystemParametersInfo(SPI_GETMOUSESPEED, 0, &d->win_mouse_speed, 0));
        g_return_if_fail(SystemParametersInfo(SPI_SETMOUSE, 0, &accel, SPIF_SENDCHANGE));
        g_return_if_fail(SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)10, SPIF_SENDCHANGE)); // default
    }
#else
    g_warning("Mouse acceleration code missing for your platform");
#endif
}

#ifdef WIN32
static gboolean win32_clip_cursor(void)
{
    RECT window, workarea, rect;
    HMONITOR monitor;
    MONITORINFO mi = { 0, };

    g_return_val_if_fail(win32_window != NULL, FALSE);

    if (!GetWindowRect(win32_window, &window))
        goto error;

    monitor = MonitorFromRect(&window, MONITOR_DEFAULTTONEAREST);
    g_return_val_if_fail(monitor != NULL, false);

    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(monitor, &mi))
        goto error;
    workarea = mi.rcWork;

    if (!IntersectRect(&rect, &window, &workarea)) {
        g_critical("error clipping cursor");
        return false;
    }

    SPICE_DEBUG("clip rect %ld %ld %ld %ld\n",
                rect.left, rect.right, rect.top, rect.bottom);

    if (!ClipCursor(&rect))
        goto error;

    return true;

error:
    {
        DWORD errval  = GetLastError();
        gchar *errstr = g_win32_error_message(errval);
        g_warning("failed to clip cursor (%ld) %s", errval, errstr);
    }

    return false;
}
#endif

static GdkGrabStatus do_pointer_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));
    GdkGrabStatus status = GDK_GRAB_BROKEN;
    GdkCursor *blank = get_blank_cursor();

    if (!gtk_widget_get_realized(GTK_WIDGET(display)))
        goto end;

#ifdef WIN32
    if (!win32_clip_cursor())
        goto end;
#endif

    try_keyboard_grab(display);
    /*
     * from gtk-vnc:
     * For relative mouse to work correctly when grabbed we need to
     * allow the pointer to move anywhere on the local desktop, so
     * use NULL for the 'confine_to' argument. Furthermore we need
     * the coords to be reported to our VNC window, regardless of
     * what window the pointer is actally over, so use 'FALSE' for
     * 'owner_events' parameter
     */
    status = gdk_pointer_grab(window, FALSE,
                     GDK_POINTER_MOTION_MASK |
                     GDK_BUTTON_PRESS_MASK |
                     GDK_BUTTON_RELEASE_MASK |
                     GDK_BUTTON_MOTION_MASK |
                     GDK_SCROLL_MASK,
                     NULL,
                     blank,
                     GDK_CURRENT_TIME);
    if (status != GDK_GRAB_SUCCESS) {
        d->mouse_grab_active = false;
        g_warning("pointer grab failed %d", status);
    } else {
        d->mouse_grab_active = true;
        g_signal_emit(display, signals[SPICE_DISPLAY_MOUSE_GRAB], 0, true);
    }

    if (status == GDK_GRAB_SUCCESS)
        set_mouse_accel(display, FALSE);

end:
    gdk_cursor_unref(blank);
    return status;
}

static void update_mouse_pointer(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));

    if (!window)
        return;

    switch (d->mouse_mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        if (gdk_window_get_cursor(window) != d->mouse_cursor)
            gdk_window_set_cursor(window, d->mouse_cursor);
        break;
    case SPICE_MOUSE_MODE_SERVER:
        if (gdk_window_get_cursor(window) != NULL)
            gdk_window_set_cursor(window, NULL);
        break;
    default:
        g_warn_if_reached();
        break;
    }
}

static void try_mouse_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (g_getenv("SPICE_NOGRAB"))
        return;
    if (d->disable_inputs)
        return;

    if (!d->mouse_have_pointer)
        return;
    if (!d->keyboard_have_focus)
        return;

    if (!d->mouse_grab_enable)
        return;
    if (d->mouse_mode != SPICE_MOUSE_MODE_SERVER)
        return;
    if (d->mouse_grab_active)
        return;

    if (do_pointer_grab(display) != GDK_GRAB_SUCCESS)
        return;

    d->mouse_last_x = -1;
    d->mouse_last_y = -1;
}

static void mouse_wrap(SpiceDisplay *display, GdkEventMotion *motion)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gint xr, yr;

#ifdef WIN32
    RECT clip;
    g_return_if_fail(GetClipCursor(&clip));
    xr = clip.left + (clip.right - clip.left) / 2;
    yr = clip.top + (clip.bottom - clip.top) / 2;
    /* the clip rectangle has no offset, so we can't use gdk_wrap_pointer */
    SetCursorPos(xr, yr);
    d->mouse_last_x = -1;
    d->mouse_last_y = -1;
#else
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(display));
    xr = gdk_screen_get_width(screen) / 2;
    yr = gdk_screen_get_height(screen) / 2;

    if (xr != (gint)motion->x_root || yr != (gint)motion->y_root) {
        /* FIXME: we try our best to ignore that next pointer move event.. */
        gdk_display_sync(gdk_screen_get_display(screen));

        gdk_display_warp_pointer(gtk_widget_get_display(GTK_WIDGET(display)),
                                 screen, xr, yr);
        d->mouse_last_x = -1;
        d->mouse_last_y = -1;
    }
#endif

}

static void try_mouse_ungrab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (!d->mouse_grab_active)
        return;

    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gtk_grab_remove(GTK_WIDGET(display));
#ifdef WIN32
    ClipCursor(NULL);
#endif
    set_mouse_accel(display, TRUE);

    d->mouse_grab_active = false;

    g_signal_emit(display, signals[SPICE_DISPLAY_MOUSE_GRAB], 0, false);
}

static void update_mouse_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->mouse_grab_enable &&
        !d->keyboard_grab_inhibit &&
        !d->disable_inputs)
        try_mouse_grab(display);
    else
        try_mouse_ungrab(display);
}

static void recalc_geometry(GtkWidget *widget)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gdouble zoom = 1.0;

    if (spicex_is_scaled(display))
        zoom = (gdouble)d->zoom_level / 100;

    SPICE_DEBUG("recalc geom monitor: %d:%d, guest +%d+%d:%dx%d, window %dx%d, zoom %g",
                d->channel_id, d->monitor_id, d->area.x, d->area.y, d->area.width, d->area.height,
                d->ww, d->wh, zoom);

    if (d->resize_guest_enable)
        spice_main_set_display(d->main, get_display_id(display),
                               d->area.x, d->area.y, d->ww / zoom, d->wh / zoom);
}

/* ---------------------------------------------------------------- */

#define CONVERT_0565_TO_0888(s)                                         \
    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) |                       \
     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) |                   \
     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#define CONVERT_0565_TO_8888(s) (CONVERT_0565_TO_0888(s) | 0xff000000)

#define CONVERT_0555_TO_0888(s)                                         \
    (((((s) & 0x001f) << 3) | (((s) & 0x001c) >> 2)) |                  \
     ((((s) & 0x03e0) << 6) | (((s) & 0x0380) << 1)) |                  \
     ((((s) & 0x7c00) << 9) | ((((s) & 0x7000)) << 4)))

#define CONVERT_0555_TO_8888(s) (CONVERT_0555_TO_0888(s) | 0xff000000)

static gboolean do_color_convert(SpiceDisplay *display, GdkRectangle *r)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    guint32 *dest = d->data;
    guint16 *src = d->data_origin;
    gint x, y;

    g_return_val_if_fail(r != NULL, false);
    g_return_val_if_fail(d->format == SPICE_SURFACE_FMT_16_555 ||
                         d->format == SPICE_SURFACE_FMT_16_565, false);

    src += (d->stride / 2) * r->y + r->x;
    dest += d->area.width * (r->y - d->area.y) + (r->x - d->area.x);

    if (d->format == SPICE_SURFACE_FMT_16_555) {
        for (y = 0; y < r->height; y++) {
            for (x = 0; x < r->width; x++) {
                dest[x] = CONVERT_0555_TO_0888(src[x]);
            }

            dest += d->area.width;
            src += d->stride / 2;
        }
    } else if (d->format == SPICE_SURFACE_FMT_16_565) {
        for (y = 0; y < r->height; y++) {
            for (x = 0; x < r->width; x++) {
                dest[x] = CONVERT_0565_TO_0888(src[x]);
            }

            dest += d->area.width;
            src += d->stride / 2;
        }
    }

    return true;
}


#if GTK_CHECK_VERSION (2, 91, 0)
static gboolean draw_event(GtkWidget *widget, cairo_t *cr)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    g_return_val_if_fail(d != NULL, false);

    if (d->mark == 0 || d->data == NULL ||
        d->area.width == 0 || d->area.height == 0)
        return false;
    g_return_val_if_fail(d->ximage != NULL, false);

    spicex_draw_event(display, cr);
    update_mouse_pointer(display);

    return true;
}
#else
static gboolean expose_event(GtkWidget *widget, GdkEventExpose *expose)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    g_return_val_if_fail(d != NULL, false);

    if (d->mark == 0 || d->data == NULL ||
        d->area.width == 0 || d->area.height == 0)
        return false;
    g_return_val_if_fail(d->ximage != NULL, false);

    spicex_expose_event(display, expose);
    update_mouse_pointer(display);

    return true;
}
#endif

/* ---------------------------------------------------------------- */
typedef enum {
    SEND_KEY_PRESS,
    SEND_KEY_RELEASE,
} SendKeyType;

static void key_press_and_release(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->key_delayed_scancode == 0)
        return;

    spice_inputs_key_press_and_release(d->inputs, d->key_delayed_scancode);
    d->key_delayed_scancode = 0;

    if (d->key_delayed_id) {
        g_source_remove(d->key_delayed_id);
        d->key_delayed_id = 0;
    }
}

static gboolean key_press_delayed(gpointer data)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(data);

    if (d->key_delayed_scancode == 0)
        return FALSE;

    spice_inputs_key_press(d->inputs, d->key_delayed_scancode);
    d->key_delayed_scancode = 0;

    if (d->key_delayed_id) {
        g_source_remove(d->key_delayed_id);
        d->key_delayed_id = 0;
    }

    return FALSE;
}

static void send_key(SpiceDisplay *display, int scancode, SendKeyType type, gboolean press_delayed)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    uint32_t i, b, m;

    g_return_if_fail(scancode != 0);

    if (!d->inputs)
        return;

    if (d->disable_inputs)
        return;

    i = scancode / 32;
    b = scancode % 32;
    m = (1 << b);
    g_return_if_fail(i < SPICE_N_ELEMENTS(d->key_state));

    switch (type) {
    case SEND_KEY_PRESS:
        /* ensure delayed key is pressed before any new input event */
        key_press_delayed(display);

        if (press_delayed &&
            d->keypress_delay != 0 &&
            !(d->key_state[i] & m)) {
            g_warn_if_fail(d->key_delayed_id == 0);
            d->key_delayed_id = g_timeout_add(d->keypress_delay, key_press_delayed, display);
            d->key_delayed_scancode = scancode;
        } else
            spice_inputs_key_press(d->inputs, scancode);

        d->key_state[i] |= m;
        break;

    case SEND_KEY_RELEASE:
        if (!(d->key_state[i] & m))
            break;

        if (d->key_delayed_scancode == scancode)
            key_press_and_release(display);
        else {
            /* ensure delayed key is pressed before other key are released */
            key_press_delayed(display);
            spice_inputs_key_release(d->inputs, scancode);
        }

        d->key_state[i] &= ~m;
        break;

    default:
        g_warn_if_reached();
    }
}

static void release_keys(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    uint32_t i, b;

    SPICE_DEBUG("%s", __FUNCTION__);
    for (i = 0; i < SPICE_N_ELEMENTS(d->key_state); i++) {
        if (!d->key_state[i]) {
            continue;
        }
        for (b = 0; b < 32; b++) {
            unsigned int scancode = i * 32 + b;
            if (scancode != 0) {
                send_key(display, scancode, SEND_KEY_RELEASE, FALSE);
            }
        }
    }
}

static gboolean check_for_grab_key(SpiceDisplay *display, int type, int keyval)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i;

    if (!d->grabseq->nkeysyms)
        return FALSE;

    if (type == GDK_KEY_PRESS) {
        /* Record the new key press */
        for (i = 0 ; i < d->grabseq->nkeysyms ; i++)
            if (d->grabseq->keysyms[i] == keyval)
                d->activeseq[i] = TRUE;

        /* Return if any key is not pressed */
        for (i = 0 ; i < d->grabseq->nkeysyms ; i++)
            if (d->activeseq[i] == FALSE)
                return FALSE;

        /* resets the whole grab sequence on success */
        memset(d->activeseq, 0, sizeof(gboolean) * d->grabseq->nkeysyms);
        return TRUE;
    } else if (type == GDK_KEY_RELEASE) {
        /* Any key release resets the whole grab sequence */
        memset(d->activeseq, 0, sizeof(gboolean) * d->grabseq->nkeysyms);
        return FALSE;
    } else
        g_warn_if_reached();

    return FALSE;
}

static void update_display(SpiceDisplay *display)
{
#ifdef WIN32
    win32_window = display ? GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(display))) : NULL;
#endif
}

static gboolean key_event(GtkWidget *widget, GdkEventKey *key)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int scancode;

#ifdef WIN32
    /* on windows, we ought to ignore the reserved key event? */
    if (key->hardware_keycode == 0xff)
        return false;

    if (!d->keyboard_grab_active) {
        if (key->hardware_keycode == VK_LWIN ||
            key->hardware_keycode == VK_RWIN ||
            key->hardware_keycode == VK_APPS)
            return false;
    }

#endif
    SPICE_DEBUG("%s %s: keycode: %d  state: %d  group %d modifier %d",
            __FUNCTION__, key->type == GDK_KEY_PRESS ? "press" : "release",
            key->hardware_keycode, key->state, key->group, key->is_modifier);

    if (check_for_grab_key(display, key->type, key->keyval)) {
        g_signal_emit(widget, signals[SPICE_DISPLAY_GRAB_KEY_PRESSED], 0);

        if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER) {
            if (d->mouse_grab_active)
                try_mouse_ungrab(display);
            else
                try_mouse_grab(display);
        }
    }

    if (!d->inputs)
        return true;

    scancode = vnc_display_keymap_gdk2xtkbd(d->keycode_map, d->keycode_maplen,
                                            key->hardware_keycode);
#ifdef WIN32
    /* MapVirtualKey doesn't return scancode with needed higher byte */
    scancode = MapVirtualKey(key->hardware_keycode, MAPVK_VK_TO_VSC) |
        (scancode & 0xff00);
#endif

    switch (key->type) {
    case GDK_KEY_PRESS:
        send_key(display, scancode, SEND_KEY_PRESS, !key->is_modifier);
        break;
    case GDK_KEY_RELEASE:
        send_key(display, scancode, SEND_KEY_RELEASE, !key->is_modifier);
        break;
    default:
        g_warn_if_reached();
        break;
    }

    return true;
}

static guint get_scancode_from_keyval(SpiceDisplay *display, guint keyval)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    guint keycode = 0;
    GdkKeymapKey *keys = NULL;
    gint n_keys = 0;

    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(),
                                          keyval, &keys, &n_keys)) {
        /* FIXME what about levels? */
        keycode = keys[0].keycode;
        g_free(keys);
    } else {
        g_warning("could not lookup keyval %u, please report a bug", keyval);
        return 0;
    }

    return vnc_display_keymap_gdk2xtkbd(d->keycode_map, d->keycode_maplen, keycode);
}


/**
 * spice_display_send_keys:
 * @display: The #SpiceDisplay
 * @keyvals: (array length=nkeyvals): Keyval array
 * @nkeyvals: Length of keyvals
 * @kind: #SpiceDisplayKeyEvent action
 *
 * Send keyval press/release events to the display.
 *
 **/
void spice_display_send_keys(SpiceDisplay *display, const guint *keyvals,
                             int nkeyvals, SpiceDisplayKeyEvent kind)
{
    int i;

    g_return_if_fail(SPICE_IS_DISPLAY(display));
    g_return_if_fail(keyvals != NULL);

    SPICE_DEBUG("%s", __FUNCTION__);

    if (kind & SPICE_DISPLAY_KEY_EVENT_PRESS) {
        for (i = 0 ; i < nkeyvals ; i++)
            send_key(display, get_scancode_from_keyval(display, keyvals[i]), SEND_KEY_PRESS, FALSE);
    }

    if (kind & SPICE_DISPLAY_KEY_EVENT_RELEASE) {
        for (i = (nkeyvals-1) ; i >= 0 ; i--)
            send_key(display, get_scancode_from_keyval(display, keyvals[i]), SEND_KEY_RELEASE, FALSE);
    }
}

static gboolean enter_event(GtkWidget *widget, GdkEventCrossing *crossing G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);

    d->mouse_have_pointer = true;
    try_keyboard_grab(display);
    update_display(display);

    return true;
}

static gboolean leave_event(GtkWidget *widget, GdkEventCrossing *crossing G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);

    if (d->mouse_grab_active)
        return true;

    d->mouse_have_pointer = false;
    try_keyboard_ungrab(display);

    return true;
}

static gboolean focus_in_event(GtkWidget *widget, GdkEventFocus *focus G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);

    /*
     * Ignore focus in when we already have the focus
     * (this happens when doing an ungrab from the leave_event callback).
     */
    if (d->keyboard_have_focus)
        return true;

    release_keys(display);
    sync_keyboard_lock_modifiers(display);
    update_keyboard_focus(display, true);
    try_keyboard_grab(display);
    update_display(display);

    return true;
}

static gboolean focus_out_event(GtkWidget *widget, GdkEventFocus *focus G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);
    update_display(NULL);

    /*
     * Ignore focus out after a keyboard grab
     * (this happens when doing the grab from the enter_event callback).
     */
    if (d->keyboard_grab_active)
        return true;

    release_keys(display);
    update_keyboard_focus(display, false);

    return true;
}

static int button_gdk_to_spice(int gdk)
{
    static const int map[] = {
        [ 1 ] = SPICE_MOUSE_BUTTON_LEFT,
        [ 2 ] = SPICE_MOUSE_BUTTON_MIDDLE,
        [ 3 ] = SPICE_MOUSE_BUTTON_RIGHT,
        [ 4 ] = SPICE_MOUSE_BUTTON_UP,
        [ 5 ] = SPICE_MOUSE_BUTTON_DOWN,
    };

    if (gdk < SPICE_N_ELEMENTS(map)) {
        return map [ gdk ];
    }
    return 0;
}

static int button_mask_gdk_to_spice(int gdk)
{
    int spice = 0;

    if (gdk & GDK_BUTTON1_MASK)
        spice |= SPICE_MOUSE_BUTTON_MASK_LEFT;
    if (gdk & GDK_BUTTON2_MASK)
        spice |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
    if (gdk & GDK_BUTTON3_MASK)
        spice |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
    return spice;
}

G_GNUC_INTERNAL
void spicex_transform_input (SpiceDisplay *display,
                             double window_x, double window_y,
                             int *input_x, int *input_y)
{
    SpiceDisplayPrivate *d = display->priv;
    int display_x, display_y, display_w, display_h;
    double is;

    spice_display_get_scaling(display, NULL,
                              &display_x, &display_y,
                              &display_w, &display_h);

    /* For input we need a different scaling factor in order to
       be able to reach the full width of a display. For instance, consider
       a display of 100 pixels showing in a window 10 pixels wide. The normal
       scaling factor here would be 100/10==10, but if you then take the largest
       possible window coordinate, i.e. 9 and multiply by 10 you get 90, not 99,
       which is the max display coord.

       If you want to be able to reach the last pixel in the window you need
       max_window_x * input_scale == max_display_x, which is
       (window_width - 1) * input_scale == (display_width - 1)

       Note, this is the inverse of s (i.e. s ~= 1/is) as we're converting the
       coordinates in the inverse direction (window -> display) as the fb size
       (display -> window).
    */
    is = (double)(d->area.width-1) / (double)(display_w-1);

    window_x -= display_x;
    window_y -= display_y;

    *input_x = floor (window_x * is);
    *input_y = floor (window_y * is);
}

static gboolean motion_event(GtkWidget *widget, GdkEventMotion *motion)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int x, y;

    if (!d->inputs)
        return true;
    if (d->disable_inputs)
        return true;

    spicex_transform_input (display, motion->x, motion->y, &x, &y);

    switch (d->mouse_mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        if (x >= 0 && x < d->area.width &&
            y >= 0 && y < d->area.height) {
            spice_inputs_position(d->inputs, x, y, get_display_id(display),
                                  button_mask_gdk_to_spice(motion->state));
        }
        break;
    case SPICE_MOUSE_MODE_SERVER:
        if (d->mouse_grab_active) {
            gint dx = d->mouse_last_x != -1 ? x - d->mouse_last_x : 0;
            gint dy = d->mouse_last_y != -1 ? y - d->mouse_last_y : 0;

            spice_inputs_motion(d->inputs, dx, dy,
                                button_mask_gdk_to_spice(motion->state));

            d->mouse_last_x = x;
            d->mouse_last_y = y;
            if (dx != 0 || dy != 0)
                mouse_wrap(display, motion);
        }
        break;
    default:
        g_warn_if_reached();
        break;
    }
    return true;
}

static gboolean scroll_event(GtkWidget *widget, GdkEventScroll *scroll)
{
    int button;
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);

    if (!d->inputs)
        return true;
    if (d->disable_inputs)
        return true;

    if (scroll->direction == GDK_SCROLL_UP)
        button = SPICE_MOUSE_BUTTON_UP;
    else if (scroll->direction == GDK_SCROLL_DOWN)
        button = SPICE_MOUSE_BUTTON_DOWN;
    else {
        SPICE_DEBUG("unsupported scroll direction");
        return true;
    }

    spice_inputs_button_press(d->inputs, button,
                              button_mask_gdk_to_spice(scroll->state));
    spice_inputs_button_release(d->inputs, button,
                                button_mask_gdk_to_spice(scroll->state));
    return true;
}

static gboolean button_event(GtkWidget *widget, GdkEventButton *button)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int x, y;

    SPICE_DEBUG("%s %s: button %d, state 0x%x", __FUNCTION__,
            button->type == GDK_BUTTON_PRESS ? "press" : "release",
            button->button, button->state);

    if (d->disable_inputs)
        return true;

    spicex_transform_input (display, button->x, button->y, &x, &y);
    if ((x < 0 || x >= d->area.width ||
         y < 0 || y >= d->area.height) &&
        d->mouse_mode == SPICE_MOUSE_MODE_CLIENT) {
        /* rule out clicks in outside region */
        return true;
    }

    gtk_widget_grab_focus(widget);
    if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER) {
        if (!d->mouse_grab_active) {
            try_mouse_grab(display);
            return true;
        }
    } else
        /* allow to drag and drop between windows/displays:

           By default, X (and other window system) do a pointer grab
           when you press a button, so that the release event is
           received by the same window regardless of where the pointer
           is. Here, we change that behaviour, so that you can press
           and release in two differents displays. This is only
           supported in client mouse mode.

           FIXME: should be multiple widget grab, but how?
           or should know the position of the other widgets?
        */
        gdk_pointer_ungrab(GDK_CURRENT_TIME);

    if (!d->inputs)
        return true;

    switch (button->type) {
    case GDK_BUTTON_PRESS:
        spice_inputs_button_press(d->inputs,
                                  button_gdk_to_spice(button->button),
                                  button_mask_gdk_to_spice(button->state));
        break;
    case GDK_BUTTON_RELEASE:
        spice_inputs_button_release(d->inputs,
                                    button_gdk_to_spice(button->button),
                                    button_mask_gdk_to_spice(button->state));
        break;
    default:
        break;
    }
    return true;
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *conf)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (conf->width == d->ww && conf->height == d->wh &&
            conf->x == d->mx && conf->y == d->my) {
        return true;
    }

    if (conf->width != d->ww  || conf->height != d->wh) {
        d->ww = conf->width;
        d->wh = conf->height;
        recalc_geometry(widget);
    }

    d->mx = conf->x;
    d->my = conf->y;

#ifdef WIN32
    if (d->mouse_grab_active) {
        try_mouse_ungrab(display);
        try_mouse_grab(display);
    }
#endif

    return true;
}

static void update_image(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    spicex_image_create(display);
    if (d->convert)
        do_color_convert(display, &d->area);
}

static void realize(GtkWidget *widget)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = display->priv;

    GTK_WIDGET_CLASS(spice_display_parent_class)->realize(widget);

    d->keycode_map =
        vnc_display_keymap_gdk2xtkbd_table(gtk_widget_get_window(widget),
                                           &d->keycode_maplen);
    update_image(display);
}

static void unrealize(GtkWidget *widget)
{
    spicex_image_destroy(SPICE_DISPLAY(widget));

    GTK_WIDGET_CLASS(spice_display_parent_class)->unrealize(widget);
}


/* ---------------------------------------------------------------- */

static void spice_display_class_init(SpiceDisplayClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);

#if GTK_CHECK_VERSION (2, 91, 0)
    gtkwidget_class->draw = draw_event;
#else
    gtkwidget_class->expose_event = expose_event;
#endif
    gtkwidget_class->key_press_event = key_event;
    gtkwidget_class->key_release_event = key_event;
    gtkwidget_class->enter_notify_event = enter_event;
    gtkwidget_class->leave_notify_event = leave_event;
    gtkwidget_class->focus_in_event = focus_in_event;
    gtkwidget_class->focus_out_event = focus_out_event;
    gtkwidget_class->motion_notify_event = motion_event;
    gtkwidget_class->button_press_event = button_event;
    gtkwidget_class->button_release_event = button_event;
    gtkwidget_class->configure_event = configure_event;
    gtkwidget_class->scroll_event = scroll_event;
    gtkwidget_class->realize = realize;
    gtkwidget_class->unrealize = unrealize;

    gobject_class->constructor = spice_display_constructor;
    gobject_class->dispose = spice_display_dispose;
    gobject_class->finalize = spice_display_finalize;
    gobject_class->get_property = spice_display_get_property;
    gobject_class->set_property = spice_display_set_property;

    /**
     * SpiceDisplay:session:
     *
     * #SpiceSession for this #SpiceDisplay
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_SESSION,
         g_param_spec_object("session",
                             "Session",
                             "SpiceSession",
                             SPICE_TYPE_SESSION,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:channel-id:
     *
     * channel-id for this #SpiceDisplay
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_CHANNEL_ID,
         g_param_spec_int("channel-id",
                          "Channel ID",
                          "Channel ID for this display",
                          0, 255, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_KEYBOARD_GRAB,
         g_param_spec_boolean("grab-keyboard",
                              "Grab Keyboard",
                              "Whether we should grab the keyboard.",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_MOUSE_GRAB,
         g_param_spec_boolean("grab-mouse",
                              "Grab Mouse",
                              "Whether we should grab the mouse.",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_RESIZE_GUEST,
         g_param_spec_boolean("resize-guest",
                              "Resize guest",
                              "Try to adapt guest display on window resize. "
                              "Requires guest cooperation.",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:ready:
     *
     * Indicate whether the display is ready to be shown. It takes
     * into account several conditions, such as the channel display
     * "mark" state, whether the monitor area is visible..
     *
     * Since: 0.13
     **/
    g_object_class_install_property
        (gobject_class, PROP_READY,
         g_param_spec_boolean("ready",
                              "Ready",
                              "Ready to display",
                              FALSE,
                              G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:auto-clipboard:
     *
     * When this is true the clipboard gets automatically shared between host
     * and guest.
     *
     * Deprecated: 0.8: Use SpiceGtkSession:auto-clipboard property instead
     **/
    g_object_class_install_property
        (gobject_class, PROP_AUTO_CLIPBOARD,
         g_param_spec_boolean("auto-clipboard",
                              "Auto clipboard",
                              "Automatically relay clipboard changes between "
                              "host and guest.",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_DEPRECATED));

    g_object_class_install_property
        (gobject_class, PROP_SCALING,
         g_param_spec_boolean("scaling", "Scaling",
                              "Whether we should use scaling",
                              TRUE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:only-downscale:
     *
     * If scaling, only scale down, never up.
     *
     * Since: 0.14
     **/
    g_object_class_install_property
        (gobject_class, PROP_ONLY_DOWNSCALE,
         g_param_spec_boolean("only-downscale", "Only Downscale",
                              "If scaling, only scale down, never up",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:keypress-delay:
     *
     * Delay in ms of non-modifiers key press events. If the key is
     * released before this delay, a single press & release event is
     * sent to the server. If the key is pressed longer than the
     * keypress-delay, the server will receive the delayed press
     * event, and a following release event when the key is released.
     *
     * Since: 0.13
     **/
    g_object_class_install_property
        (gobject_class, PROP_KEYPRESS_DELAY,
         g_param_spec_uint("keypress-delay", "Keypress delay",
                           "Keypress delay",
                           0, G_MAXUINT, 100,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:disable-inputs:
     *
     * Disable all keyboard & mouse inputs.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_DISABLE_INPUTS,
         g_param_spec_boolean("disable-inputs", "Disable inputs",
                              "Whether inputs should be disabled",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));


    /**
     * SpiceDisplay:zoom-level:
     *
     * Zoom level in percentage, from 10 to 400. Default to 100.
     * (this option is only supported with cairo backend when scaling
     * is enabled)
     *
     * Since: 0.10
     **/
    g_object_class_install_property
        (gobject_class, PROP_ZOOM_LEVEL,
         g_param_spec_int("zoom-level", "Zoom Level",
                          "Zoom Level",
                          10, 400, 100,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay:monitor-id:
     *
     * Select monitor from #SpiceDisplay to show.
     * The value -1 means the whole display is shown.
     * By default, the monitor 0 is selected.
     *
     * Since: 0.13
     **/
    g_object_class_install_property
        (gobject_class, PROP_MONITOR_ID,
         g_param_spec_int("monitor-id",
                          "Monitor ID",
                          "Select monitor ID",
                          -1, G_MAXINT, 0,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceDisplay::mouse-grab:
     * @display: the #SpiceDisplay that emitted the signal
     * @status: 1 if grabbed, 0 otherwise.
     *
     * Notify when the mouse grab is active or not.
     **/
    signals[SPICE_DISPLAY_MOUSE_GRAB] =
        g_signal_new("mouse-grab",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceDisplayClass, mouse_grab),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);

    /**
     * SpiceDisplay::keyboard-grab:
     * @display: the #SpiceDisplay that emitted the signal
     * @status: 1 if grabbed, 0 otherwise.
     *
     * Notify when the keyboard grab is active or not.
     **/
    signals[SPICE_DISPLAY_KEYBOARD_GRAB] =
        g_signal_new("keyboard-grab",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceDisplayClass, keyboard_grab),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);

    /**
     * SpiceDisplay::grab-keys-pressed:
     * @display: the #SpiceDisplay that emitted the signal
     *
     * Notify when the grab keys have been pressed
     **/
    signals[SPICE_DISPLAY_GRAB_KEY_PRESSED] =
        g_signal_new("grab-keys-pressed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceDisplayClass, keyboard_grab),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(SpiceDisplayPrivate));
}

/* ---------------------------------------------------------------- */

#define SPICE_GDK_BUTTONS_MASK \
    (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK|GDK_BUTTON4_MASK|GDK_BUTTON5_MASK)

static void update_mouse_mode(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(display));

    g_object_get(channel, "mouse-mode", &d->mouse_mode, NULL);
    SPICE_DEBUG("mouse mode %d", d->mouse_mode);

    switch (d->mouse_mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        try_mouse_ungrab(display);
        break;
    case SPICE_MOUSE_MODE_SERVER:
        d->mouse_guest_x = -1;
        d->mouse_guest_y = -1;

        if (window != NULL) {
            GdkModifierType modifiers;
            gdk_window_get_pointer(window, NULL, NULL, &modifiers);

            if (modifiers & SPICE_GDK_BUTTONS_MASK)
                try_mouse_grab(display);
        }
        break;
    default:
        g_warn_if_reached();
    }

    update_mouse_pointer(display);
}

static void update_area(SpiceDisplay *display,
                        gint x, gint y, gint width, gint height)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkRectangle primary = {
        .x = 0,
        .y = 0,
        .width = d->width,
        .height = d->height
    };
    GdkRectangle area = {
        .x = x,
        .y = y,
        .width = width,
        .height = height
    };

    SPICE_DEBUG("update area, primary: %dx%d, area: +%d+%d %dx%d", d->width, d->height, area.x, area.y, area.width, area.height);

    if (!gdk_rectangle_intersect(&primary, &area, &area)) {
        SPICE_DEBUG("The monitor area is not intersecting primary surface");
        memset(&d->area, '\0', sizeof(d->area));
        set_monitor_ready(display, false);
        return;
    }

    spicex_image_destroy(display);
    d->area = area;
    if (gtk_widget_get_realized(GTK_WIDGET(display)))
        update_image(display);

    update_size_request(display);

    set_monitor_ready(display, true);
}

static void primary_create(SpiceChannel *channel, gint format,
                           gint width, gint height, gint stride,
                           gint shmid, gpointer imgdata, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    d->format = format;
    d->stride = stride;
    d->shmid = shmid;
    d->width = width;
    d->height = height;
    d->data_origin = d->data = imgdata;

    update_monitor_area(display);
}

static void primary_destroy(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = SPICE_DISPLAY(data);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    spicex_image_destroy(display);
    d->width  = 0;
    d->height = 0;
    d->stride = 0;
    d->shmid  = 0;
    d->data = NULL;
    d->data_origin = NULL;
    set_monitor_ready(display, false);
}

static void invalidate(SpiceChannel *channel,
                       gint x, gint y, gint w, gint h, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int display_x, display_y;
    int x1, y1, x2, y2;
    double s;
    GdkRectangle rect = {
        .x = x,
        .y = y,
        .width = w,
        .height = h
    };

    if (!gtk_widget_get_window(GTK_WIDGET(display)))
        return;

    if (!gdk_rectangle_intersect(&rect, &d->area, &rect))
        return;

    if (d->convert)
        do_color_convert(display, &rect);

    spice_display_get_scaling(display, &s,
                              &display_x, &display_y,
                              NULL, NULL);

    x1 = floor ((rect.x - d->area.x) * s);
    y1 = floor ((rect.y - d->area.y) * s);
    x2 = ceil ((rect.x - d->area.x + rect.width) * s);
    y2 = ceil ((rect.y - d->area.y + rect.height) * s);

    gtk_widget_queue_draw_area(GTK_WIDGET(display),
                               display_x + x1, display_y + y1,
                               x2 - x1, y2-y1);
}

static void mark(SpiceDisplay *display, gint mark)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    g_return_if_fail(d != NULL);

    SPICE_DEBUG("widget mark: %d, %d:%d %p", mark, d->channel_id, d->monitor_id, display);
    d->mark = mark;
    update_ready(display);
}

static void cursor_set(SpiceCursorChannel *channel,
                       gint width, gint height, gint hot_x, gint hot_y,
                       gpointer rgba, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkCursor *cursor = NULL;

    cursor_invalidate(display);

    if (d->mouse_pixbuf) {
        g_object_unref(d->mouse_pixbuf);
        d->mouse_pixbuf = NULL;
    }

    if (rgba != NULL) {
        d->mouse_pixbuf = gdk_pixbuf_new_from_data(g_memdup(rgba, width * height * 4),
                                                   GDK_COLORSPACE_RGB,
                                                   TRUE, 8,
                                                   width,
                                                   height,
                                                   width * 4,
                                                   (GdkPixbufDestroyNotify)g_free, NULL);
        d->mouse_hotspot.x = hot_x;
        d->mouse_hotspot.y = hot_y;
        cursor = gdk_cursor_new_from_pixbuf(gtk_widget_get_display(GTK_WIDGET(display)),
                                            d->mouse_pixbuf, hot_x, hot_y);
    } else
        g_warn_if_reached();

    if (d->show_cursor) {
        /* unhide */
        gdk_cursor_unref(d->show_cursor);
        d->show_cursor = NULL;
        if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER) {
            /* keep a hidden cursor, will be shown in cursor_move() */
            d->show_cursor = cursor;
            return;
        }
    }

    gdk_cursor_unref(d->mouse_cursor);
    d->mouse_cursor = cursor;

    update_mouse_pointer(display);
    cursor_invalidate(display);
}

static void cursor_hide(SpiceCursorChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->show_cursor != NULL) /* then we are already hidden */
        return;

    cursor_invalidate(display);
    d->show_cursor = d->mouse_cursor;
    d->mouse_cursor = get_blank_cursor();
    update_mouse_pointer(display);
}

G_GNUC_INTERNAL
void spice_display_get_scaling(SpiceDisplay *display,
                               double *s_out,
                               int *x_out, int *y_out,
                               int *w_out, int *h_out)
{
    SpiceDisplayPrivate *d = display->priv;
    int fbw = d->area.width, fbh = d->area.height;
    int ww, wh;
    int x, y, w, h;
    double s;

    if (gtk_widget_get_realized (GTK_WIDGET(display)))
        gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);
    else {
        ww = fbw;
        wh = fbh;
    }

    if (!spicex_is_scaled(display)) {
        s = 1.0;
        x = 0;
        y = 0;
        if (ww > d->area.width)
            x = (ww - d->area.width) / 2;
        if (wh > d->area.height)
            y = (wh - d->area.height) / 2;
        w = fbw;
        h = fbh;
    } else {
        s = MIN ((double)ww / (double)fbw, (double)wh / (double)fbh);

        if (d->only_downscale && s >= 1.0)
            s = 1.0;

        /* Round to int size */
        w = floor (fbw * s + 0.5);
        h = floor (fbh * s + 0.5);

        /* Center the display */
        x = (ww - w) / 2;
        y = (wh - h) / 2;
    }

    if (s_out)
        *s_out = s;
    if (w_out)
        *w_out = w;
    if (h_out)
        *h_out = h;
    if (x_out)
        *x_out = x;
    if (y_out)
        *y_out = y;
}

static void cursor_invalidate(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = display->priv;
    double s;
    int x, y;

    if (d->mouse_pixbuf == NULL)
        return;

    spice_display_get_scaling(display, &s, &x, &y, NULL, NULL);

    gtk_widget_queue_draw_area(GTK_WIDGET(display),
                               floor ((d->mouse_guest_x - d->mouse_hotspot.x - d->area.x) * s) + x,
                               floor ((d->mouse_guest_y - d->mouse_hotspot.y - d->area.y) * s) + y,
                               ceil (gdk_pixbuf_get_width(d->mouse_pixbuf) * s),
                               ceil (gdk_pixbuf_get_height(d->mouse_pixbuf) * s));
}

static void cursor_move(SpiceCursorChannel *channel, gint x, gint y, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = display->priv;

    cursor_invalidate(display);

    d->mouse_guest_x = x;
    d->mouse_guest_y = y;

    cursor_invalidate(display);

    /* apparently we have to restore cursor when "cursor_move" */
    if (d->show_cursor != NULL) {
        gdk_cursor_unref(d->mouse_cursor);
        d->mouse_cursor = d->show_cursor;
        d->show_cursor = NULL;
        update_mouse_pointer(display);
    }
}

static void cursor_reset(SpiceCursorChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(display));

    if (!window) {
        SPICE_DEBUG("%s: no window, returning",  __FUNCTION__);
        return;
    }

    SPICE_DEBUG("%s",  __FUNCTION__);
    gdk_window_set_cursor(window, NULL);
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        d->main = SPICE_MAIN_CHANNEL(channel);
        spice_g_signal_connect_object(channel, "main-mouse-update",
                                      G_CALLBACK(update_mouse_mode), display, 0);
        update_mouse_mode(channel, display);
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        SpiceDisplayPrimary primary;
        if (id != d->channel_id)
            return;
        d->display = channel;
        spice_g_signal_connect_object(channel, "display-primary-create",
                                      G_CALLBACK(primary_create), display, 0);
        spice_g_signal_connect_object(channel, "display-primary-destroy",
                                      G_CALLBACK(primary_destroy), display, 0);
        spice_g_signal_connect_object(channel, "display-invalidate",
                                      G_CALLBACK(invalidate), display, 0);
        spice_g_signal_connect_object(channel, "display-mark",
                                      G_CALLBACK(mark), display, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
        spice_g_signal_connect_object(channel, "notify::monitors",
                                      G_CALLBACK(update_monitor_area), display, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
        if (spice_display_get_primary(channel, 0, &primary)) {
            primary_create(channel, primary.format, primary.width, primary.height,
                           primary.stride, primary.shmid, primary.data, display);
            mark(display, primary.marked);
        }
        spice_channel_connect(channel);
        spice_main_set_display_enabled(d->main, get_display_id(display), TRUE);
        return;
    }

    if (SPICE_IS_CURSOR_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        d->cursor = SPICE_CURSOR_CHANNEL(channel);
        spice_g_signal_connect_object(channel, "cursor-set",
                                      G_CALLBACK(cursor_set), display, 0);
        spice_g_signal_connect_object(channel, "cursor-move",
                                      G_CALLBACK(cursor_move), display, 0);
        spice_g_signal_connect_object(channel, "cursor-hide",
                                      G_CALLBACK(cursor_hide), display, 0);
        spice_g_signal_connect_object(channel, "cursor-reset",
                                      G_CALLBACK(cursor_reset), display, 0);
        spice_channel_connect(channel);
        return;
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        d->inputs = SPICE_INPUTS_CHANNEL(channel);
        spice_channel_connect(channel);
        sync_keyboard_lock_modifiers(display);
        return;
    }

#ifdef USE_SMARTCARD
    if (SPICE_IS_SMARTCARD_CHANNEL(channel)) {
        d->smartcard = SPICE_SMARTCARD_CHANNEL(channel);
        spice_channel_connect(channel);
        return;
    }
#endif

    return;
}

static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    SPICE_DEBUG("channel_destroy %d", id);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        d->main = NULL;
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        primary_destroy(d->display, display);
        d->display = NULL;
        return;
    }

    if (SPICE_IS_CURSOR_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        d->cursor = NULL;
        return;
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        d->inputs = NULL;
        return;
    }

#ifdef USE_SMARTCARD
    if (SPICE_IS_SMARTCARD_CHANNEL(channel)) {
        d->smartcard = NULL;
        return;
    }
#endif

    return;
}

/**
 * spice_display_new:
 * @session: a #SpiceSession
 * @channel_id: the display channel ID to associate with #SpiceDisplay
 *
 * Returns: a new #SpiceDisplay widget.
 **/
SpiceDisplay *spice_display_new(SpiceSession *session, int id)
{
    return g_object_new(SPICE_TYPE_DISPLAY, "session", session,
                        "channel-id", id, NULL);
}

/**
 * spice_display_new_with_monitor:
 * @session: a #SpiceSession
 * @channel_id: the display channel ID to associate with #SpiceDisplay
 * @monitor_id: the monitor id within the display channel
 *
 * Since: 0.13
 * Returns: a new #SpiceDisplay widget.
 **/
SpiceDisplay* spice_display_new_with_monitor(SpiceSession *session, gint channel_id, gint monitor_id)
{
    return g_object_new(SPICE_TYPE_DISPLAY,
                        "session", session,
                        "channel-id", channel_id,
                        "monitor-id", monitor_id,
                        NULL);
}

/**
 * spice_display_mouse_ungrab:
 * @display:
 *
 * Ungrab the mouse.
 **/
void spice_display_mouse_ungrab(SpiceDisplay *display)
{
    g_return_if_fail(SPICE_IS_DISPLAY(display));

    try_mouse_ungrab(display);
}

/**
 * spice_display_copy_to_guest:
 * @display:
 *
 * Copy client-side clipboard to guest clipboard.
 *
 * Deprecated: 0.8: Use spice_gtk_session_copy_to_guest() instead
 **/
void spice_display_copy_to_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d;

    g_return_if_fail(SPICE_IS_DISPLAY(display));

    d = display->priv;

    g_return_if_fail(d->gtk_session != NULL);

    spice_gtk_session_copy_to_guest(d->gtk_session);
}

/**
 * spice_display_paste_from_guest:
 * @display:
 *
 * Copy guest clipboard to client-side clipboard.
 *
 * Deprecated: 0.8: Use spice_gtk_session_paste_from_guest() instead
 **/
void spice_display_paste_from_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d;

    g_return_if_fail(SPICE_IS_DISPLAY(display));

    d = display->priv;

    g_return_if_fail(d->gtk_session != NULL);

    spice_gtk_session_paste_from_guest(d->gtk_session);
}

/**
 * spice_display_get_pixbuf:
 * @display:
 *
 * Take a screenshot of the display.
 *
 * Returns: (transfer full): a #GdkPixbuf with the screenshot image buffer
 **/
GdkPixbuf *spice_display_get_pixbuf(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d;
    GdkPixbuf *pixbuf;
    int x, y;
    guchar *src, *data, *dest;

    g_return_val_if_fail(SPICE_IS_DISPLAY(display), NULL);

    d = display->priv;

    g_return_val_if_fail(d != NULL, NULL);
    /* TODO: ensure d->data has been exposed? */
    g_return_val_if_fail(d->data != NULL, NULL);

    data = g_malloc(d->area.width * d->area.height * 3);
    src = d->data;
    dest = data;

    for (y = d->area.y; y < d->area.height; ++y) {
        for (x = d->area.x; x < d->area.width; ++x) {
          dest[0] = src[x * 4 + 2];
          dest[1] = src[x * 4 + 1];
          dest[2] = src[x * 4 + 0];
          dest += 3;
        }
        src += d->stride;
    }

    pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, false,
                                      8, d->area.width, d->area.height, d->area.width * 3,
                                      (GdkPixbufDestroyNotify)g_free, NULL);
    return pixbuf;
}

#if HAVE_X11_XKBLIB_H
static guint32 get_keyboard_lock_modifiers(Display *x_display)
{
    XKeyboardState keyboard_state;
    guint32 modifiers = 0;

    XGetKeyboardControl(x_display, &keyboard_state);

    if (keyboard_state.led_mask & 0x01) {
        modifiers |= SPICE_INPUTS_CAPS_LOCK;
    }
    if (keyboard_state.led_mask & 0x02) {
        modifiers |= SPICE_INPUTS_NUM_LOCK;
    }
    if (keyboard_state.led_mask & 0x04) {
        modifiers |= SPICE_INPUTS_SCROLL_LOCK;
    }
    return modifiers;
}

static void sync_keyboard_lock_modifiers(SpiceDisplay *display)
{
    Display *x_display;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    guint32 modifiers;
    GdkWindow *w;

    if (d->disable_inputs)
        return;

    w = gtk_widget_get_parent_window(GTK_WIDGET(display));
    if (w == NULL) /* it can happen if the display is not yet shown */
        return;

    if (!GDK_IS_X11_DISPLAY(gdk_window_get_display(w))) {
        SPICE_DEBUG("FIXME: gtk backend is not X11");
        return;
    }

    x_display = GDK_WINDOW_XDISPLAY(w);
    modifiers = get_keyboard_lock_modifiers(x_display);
    if (d->inputs)
        spice_inputs_set_key_locks(d->inputs, modifiers);
}

#elif defined (WIN32)
static guint32 get_keyboard_lock_modifiers(void)
{
    guint32 modifiers = 0;

    if (GetKeyState(VK_CAPITAL) & 1) {
        modifiers |= SPICE_INPUTS_CAPS_LOCK;
    }
    if (GetKeyState(VK_NUMLOCK) & 1) {
        modifiers |= SPICE_INPUTS_NUM_LOCK;
    }
    if (GetKeyState(VK_SCROLL) & 1) {
        modifiers |= SPICE_INPUTS_SCROLL_LOCK;
    }

    return modifiers;
}

static void sync_keyboard_lock_modifiers(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    guint32 modifiers;
    GdkWindow *w;

    if (d->disable_inputs)
        return;

    w = gtk_widget_get_parent_window(GTK_WIDGET(display));
    if (w == NULL) /* it can happen if the display is not yet shown */
        return;

    modifiers = get_keyboard_lock_modifiers();
    if (d->inputs)
        spice_inputs_set_key_locks(d->inputs, modifiers);
}
#else
static void sync_keyboard_lock_modifiers(SpiceDisplay *display)
{
    g_warning("sync_keyboard_lock_modifiers not implemented");
}
#endif // HAVE_X11_XKBLIB_H
