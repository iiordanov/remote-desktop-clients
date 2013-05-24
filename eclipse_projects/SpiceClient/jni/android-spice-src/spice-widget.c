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

#define GtkObject GtkWidget
#define GtkObjectClass GtkWidgetClass
#define GTK_OBJECT_CLASS(c) GTK_WIDGET_CLASS(c)

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
    PROP_DISABLE_INPUTS,
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
static HWND focus_window = NULL;
#endif

static void update_keyboard_grab(SpiceDisplay *display);
static void try_keyboard_grab(SpiceDisplay *display);
static void try_keyboard_ungrab(SpiceDisplay *display);
static void update_mouse_grab(SpiceDisplay *display);
static void try_mouse_grab(SpiceDisplay *display);
static void try_mouse_ungrab(SpiceDisplay *display);
static void recalc_geometry(GtkWidget *widget, gboolean set_display);
static void disconnect_main(SpiceDisplay *display);
static void disconnect_cursor(SpiceDisplay *display);
static void disconnect_display(SpiceDisplay *display);
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void sync_keyboard_lock_modifiers(SpiceDisplay *display);
static void cursor_invalidate(SpiceDisplay *display);

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
    case PROP_DISABLE_INPUTS:
        g_value_set_boolean(value, d->disable_inputs);
	break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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
        d->session = g_object_ref(g_value_get_object(value));
        d->gtk_session = spice_gtk_session_get(d->session);
        break;
    case PROP_CHANNEL_ID:
        d->channel_id = g_value_get_int(value);
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
        if (d->resize_guest_enable) {
            gtk_widget_set_size_request(GTK_WIDGET(display), 640, 480);
            recalc_geometry(GTK_WIDGET(display), TRUE);
        } else {
            gtk_widget_set_size_request(GTK_WIDGET(display),
                                        d->width, d->height);
        }
        break;
    case PROP_SCALING:
        d->allow_scaling = g_value_get_boolean(value);
        recalc_geometry(GTK_WIDGET(display), FALSE);
        if (d->ximage &&
            gtk_widget_get_window(GTK_WIDGET(display))) { /* if not yet shown */
            int ww, wh;
            gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);
            gtk_widget_queue_draw_area(GTK_WIDGET(display), 0, 0, ww, wh);
        }
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
}

static void spice_display_dispose(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("spice display dispose");

    disconnect_main(display);
    disconnect_display(display);
    disconnect_cursor(display);

    if (d->session) {
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_new),
                                             display);
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_destroy),
                                             display);
        g_object_unref(d->session);
        d->session = NULL;
        d->gtk_session = NULL;
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

static void spice_display_init(SpiceDisplay *display)
{
    GtkWidget *widget = GTK_WIDGET(display);
    SpiceDisplayPrivate *d;

    d = display->priv = SPICE_DISPLAY_GET_PRIVATE(display);

    gtk_widget_add_events(widget,
                          GDK_STRUCTURE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_BUTTON_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK |
                          GDK_KEY_PRESS_MASK);
    gtk_widget_set_double_buffered(widget, false);
    gtk_widget_set_can_focus(widget, true);

    d->keycode_map = vnc_display_keymap_gdk2xtkbd_table(&d->keycode_maplen);
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

    g_signal_connect(d->session, "channel-new",
                     G_CALLBACK(channel_new), display);
    g_signal_connect(d->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), display);
    list = spice_session_get_channels(d->session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(d->session, it->data, (gpointer*)display);
    }
    g_list_free(list);

    spice_g_signal_connect_object(d->gtk_session, "notify::auto-clipboard",
                                  G_CALLBACK(gtk_session_property_changed), display, 0);

    g_signal_connect(d->session, "notify::inhibit-keyboard-grab",
                     G_CALLBACK(session_inhibit_keyboard_grab_changed),
                     display);

    return obj;
}

/**
 * spice_display_set_grab_keys:
 * @display:
 * @seq: key sequence
 *
 * Set the key combination to grab/ungrab the keyboard. The default is
 * "Control L + Alt L".
 **/
void spice_display_set_grab_keys(SpiceDisplay *display, SpiceGrabSequence *seq)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
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
    if  (focus_window && code == HC_ACTION) {
        KBDLLHOOKSTRUCT *hooked = (KBDLLHOOKSTRUCT*)lparam;
        DWORD dwmsg = (hooked->flags << 24) | (hooked->scanCode << 16) | 1;

        if (hooked->vkCode == VK_NUMLOCK || hooked->vkCode == VK_RSHIFT) {
            dwmsg &= ~(1 << 24);
            SendMessage(focus_window, wparam, hooked->vkCode, dwmsg);
        }
        switch (hooked->vkCode) {
        case VK_CAPITAL:
        case VK_SCROLL:
        case VK_NUMLOCK:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
            break;
        default:
            SendMessage(focus_window, wparam, hooked->vkCode, dwmsg);
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}
#endif

/**
 * spice_display_get_grab_keys:
 * @display:
 *
 * Returns: the current grab key combination.
 **/
SpiceGrabSequence *spice_display_get_grab_keys(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
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

static GdkGrabStatus do_pointer_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));
    GdkGrabStatus status;
    GdkCursor *blank = get_blank_cursor();

    if (!gtk_widget_get_realized(GTK_WIDGET(display)))
        return GDK_GRAB_BROKEN;
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
#ifdef WIN32
    {
        RECT client_rect;
        POINT origin;

        origin.x = origin.y = 0;
        ClientToScreen(focus_window, &origin);
        GetClientRect(focus_window, &client_rect);
        OffsetRect(&client_rect, origin.x, origin.y);
        ClipCursor(&client_rect);
    }
#endif
    gtk_grab_add(GTK_WIDGET(display));

#ifdef GDK_WINDOWING_X11
    if (status == GDK_GRAB_SUCCESS) {
        int accel_numerator;
        int accel_denominator;
        int threshold;
        GdkWindow *w = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));
        Display *x_display = GDK_WINDOW_XDISPLAY(w);
    
        XGetPointerControl(x_display, &accel_numerator, &accel_denominator,
                           &threshold);
        XChangePointerControl(x_display, False, False, accel_numerator,
                              accel_denominator, threshold);
    }
#endif

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

static void mouse_check_edges(GtkWidget *widget, GdkEventMotion *motion)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkScreen *screen = gtk_widget_get_screen(widget);
    gint ww, wh;

    gdk_drawable_get_size(gtk_widget_get_window(widget), &ww, &wh);
    int x = (int)motion->x;
    int y = (int)motion->y;
    int xr = (int)motion->x_root;
    int yr = (int)motion->y_root;

    /* from gtk-vnc: In relative mode check to see if client pointer
     * hit the window edges, and if so move it back by 100px. This is
     * important because the pointer in the server doesn't correspond
     * 1-for-1, and so may still be only half way across the
     * screen. Without this warp, the server pointer would thus appear
     * to hit an invisible wall */
    if (x <= 0) xr += 100;
    if (y <= 0) yr += 100;
    if (x >= (ww - 1)) xr -= 100;
    if (y >= (wh - 1)) yr -= 100;

    if (xr != (int)motion->x_root || yr != (int)motion->y_root) {
        /* FIXME: we try our best to ignore that next pointer move event.. */
        gdk_display_sync(gdk_screen_get_display(screen));

        gdk_display_warp_pointer(gtk_widget_get_display(widget),
                                 screen, xr, yr);
        d->mouse_last_x = -1;
        d->mouse_last_y = -1;
    }
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
#ifdef GDK_WINDOWING_X11
    {
        int accel_numerator;
        int accel_denominator;
        int threshold;
        GdkWindow *w = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));
        Display *x_display = GDK_WINDOW_XDISPLAY(w);
    
        XGetPointerControl(x_display, &accel_numerator, &accel_denominator,
                           &threshold);
        XChangePointerControl(x_display, True, True, accel_numerator,
                              accel_denominator, threshold);
    }
#endif

    d->mouse_grab_active = false;

    g_signal_emit(display, signals[SPICE_DISPLAY_MOUSE_GRAB], 0, false);
}

static void update_mouse_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->mouse_grab_enable && !d->disable_inputs)
        try_mouse_grab(display);
    else
        try_mouse_ungrab(display);
}

static void recalc_geometry(GtkWidget *widget, gboolean set_display)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    d->mx = 0;
    d->my = 0;
    if (!spicex_is_scaled(display)) {
        if (d->ww > d->width)
            d->mx = (d->ww - d->width) / 2;
        if (d->wh > d->height)
            d->my = (d->wh - d->height) / 2;
    }

    SPICE_DEBUG("monitors: id %d, guest %dx%d, window %dx%d, offset +%d+%d",
                d->channel_id, d->width, d->height, d->ww, d->wh, d->mx, d->my);

    if (d->resize_guest_enable && set_display) {
        spice_main_set_display(d->main, d->channel_id,
                               0, 0, d->ww, d->wh);
    }
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

static gboolean do_color_convert(SpiceDisplay *display,
                                 gint x, gint y, gint w, gint h)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i, j, maxy, maxx, miny, minx;
    guint32 *dest = d->data;
    guint16 *src = d->data_origin;

    if (!d->convert)
        return true;

    g_return_val_if_fail(d->format == SPICE_SURFACE_FMT_16_555 ||
                         d->format == SPICE_SURFACE_FMT_16_565, false);

    miny = MAX(y, 0);
    minx = MAX(x, 0);
    maxy = MIN(y + h, d->height);
    maxx = MIN(x + w, d->width);

    dest +=  (d->stride / 4) * miny;
    src += (d->stride / 2) * miny;

    if (d->format == SPICE_SURFACE_FMT_16_555) {
        for (j = miny; j < maxy; j++) {
            for (i = minx; i < maxx; i++) {
                dest[i] = CONVERT_0555_TO_0888(src[i]);
            }

            dest += d->stride / 4;
            src += d->stride / 2;
        }
    } else if (d->format == SPICE_SURFACE_FMT_16_565) {
        for (j = miny; j < maxy; j++) {
            for (i = minx; i < maxx; i++) {
                dest[i] = CONVERT_0565_TO_0888(src[i]);
            }

            dest += d->stride / 4;
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

    if (d->mark == 0 || d->data == NULL)
        return false;

    if (!d->ximage) {
        spicex_image_create(display);
    }

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

    if (d->mark == 0 || d->data == NULL)
        return false;

    if (!d->ximage) {
        spicex_image_create(display);
    }

    spicex_expose_event(display, expose);
    update_mouse_pointer(display);

    return true;
}
#endif

/* ---------------------------------------------------------------- */

static void send_key(SpiceDisplay *display, int scancode, int down)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    uint32_t i, b, m;

    if (!d->inputs)
        return;

    if (d->disable_inputs)
        return;

    i = scancode / 32;
    b = scancode % 32;
    m = (1 << b);
    g_return_if_fail(i < SPICE_N_ELEMENTS(d->key_state));

    if (down) {
        spice_inputs_key_press(d->inputs, scancode);
        d->key_state[i] |= m;
    } else {
        if (!(d->key_state[i] & m)) {
            return;
        }
        spice_inputs_key_release(d->inputs, scancode);
        d->key_state[i] &= ~m;
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
            send_key(display, i * 32 + b, 0);
        }
    }
}

static gboolean check_for_grab_key(SpiceDisplay *display, int type, int keyval)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i;

    if (!d->grabseq->nkeysyms)
        return FALSE;

    if (type == GDK_KEY_RELEASE) {
        /* Any key release resets the whole grab sequence */
        memset(d->activeseq, 0, sizeof(gboolean) * d->grabseq->nkeysyms);
        return FALSE;
    } else {
        /* Record the new key press */
        for (i = 0 ; i < d->grabseq->nkeysyms ; i++)
            if (d->grabseq->keysyms[i] == keyval)
                d->activeseq[i] = TRUE;

        /* Return if any key is not pressed */
        for (i = 0 ; i < d->grabseq->nkeysyms ; i++)
            if (d->activeseq[i] == FALSE)
                return FALSE;

        return TRUE;
    }
}

static gboolean key_event(GtkWidget *widget, GdkEventKey *key)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int scancode;

    SPICE_DEBUG("%s %s: keycode: %d  state: %d  group %d",
            __FUNCTION__, key->type == GDK_KEY_PRESS ? "press" : "release",
            key->hardware_keycode, key->state, key->group);

    if (check_for_grab_key(display, key->type, key->keyval)) {
        g_signal_emit(widget, signals[SPICE_DISPLAY_GRAB_KEY_PRESSED], 0);

        if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER) {
            if (d->mouse_grab_active)
                try_mouse_ungrab(display);
            else
                try_mouse_grab(display);
        }

        return true;
    }

    if (!d->inputs)
        return true;

    scancode = vnc_display_keymap_gdk2xtkbd(d->keycode_map, d->keycode_maplen,
                                            key->hardware_keycode);
    switch (key->type) {
    case GDK_KEY_PRESS:
        send_key(display, scancode, 1);
        break;
    case GDK_KEY_RELEASE:
        send_key(display, scancode, 0);
        break;
    default:
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
    }

    return vnc_display_keymap_gdk2xtkbd(d->keycode_map, d->keycode_maplen, keycode);
}

void spice_display_send_keys(SpiceDisplay *display, const guint *keyvals,
                             int nkeyvals, SpiceDisplayKeyEvent kind)
{
    int i;

    g_return_if_fail(SPICE_DISPLAY(display) != NULL);
    g_return_if_fail(keyvals != NULL);

    SPICE_DEBUG("%s", __FUNCTION__);

    if (kind & SPICE_DISPLAY_KEY_EVENT_PRESS) {
        for (i = 0 ; i < nkeyvals ; i++)
            send_key(display, get_scancode_from_keyval(display, keyvals[i]), 1);
    }

    if (kind & SPICE_DISPLAY_KEY_EVENT_RELEASE) {
        for (i = (nkeyvals-1) ; i >= 0 ; i--)
            send_key(display, get_scancode_from_keyval(display, keyvals[i]), 0);
    }
}

static gboolean enter_event(GtkWidget *widget, GdkEventCrossing *crossing G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);
    d->mouse_have_pointer = true;
    try_keyboard_grab(display);
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
    d->keyboard_have_focus = true;
    try_keyboard_grab(display);
    spice_gtk_session_update_keyboard_focus(d->gtk_session,
                                            d->keyboard_have_focus);
#ifdef WIN32
    focus_window = GDK_WINDOW_HWND(gtk_widget_get_window(widget));
    g_return_val_if_fail(focus_window != NULL, true);
#endif
    return true;
}

static gboolean focus_out_event(GtkWidget *widget, GdkEventFocus *focus G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);

    /*
     * Ignore focus out after a keyboard grab
     * (this happens when doing the grab from the enter_event callback).
     */
    if (d->keyboard_grab_active)
        return true;

    release_keys(display);
    d->keyboard_have_focus = false;
    spice_gtk_session_update_keyboard_focus(d->gtk_session,
                                            d->keyboard_have_focus);
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

static gboolean motion_event(GtkWidget *widget, GdkEventMotion *motion)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int ww, wh;

    if (!d->inputs)
        return true;
    if (d->disable_inputs)
        return true;

    gdk_drawable_get_size(gtk_widget_get_window(widget), &ww, &wh);
    if (spicex_is_scaled(display)) {
        double sx, sy;
        sx = (double)d->width / (double)ww;
        sy = (double)d->height / (double)wh;

        /* Scaling the desktop, so scale the mouse coords by same
         * ratio - ceil() seems to be more accurate - not sure
         * though - scaling is more likely reversible.. */
        motion->x = ceil(motion->x * sx);
        motion->y = ceil(motion->y * sy);
    } else {
        motion->x -= d->mx;
        motion->y -= d->my;
    }

    switch (d->mouse_mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        if (motion->x >= 0 && motion->x < d->width &&
            motion->y >= 0 && motion->y < d->height) {
            spice_inputs_position(d->inputs,
                                  motion->x, motion->y,
                                  d->channel_id,
                                  button_mask_gdk_to_spice(motion->state));
        }
        break;
    case SPICE_MOUSE_MODE_SERVER:
        if (d->mouse_grab_active) {
            if (d->mouse_last_x != -1 &&
                d->mouse_last_y != -1) {
                gint dx = motion->x - d->mouse_last_x;
                gint dy = motion->y - d->mouse_last_y;

                spice_inputs_motion(d->inputs, dx, dy,
                                    button_mask_gdk_to_spice(motion->state));
            }

            d->mouse_last_x = motion->x;
            d->mouse_last_y = motion->y;
            mouse_check_edges(widget, motion);
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

    SPICE_DEBUG("%s %s: button %d, state 0x%x", __FUNCTION__,
            button->type == GDK_BUTTON_PRESS ? "press" : "release",
            button->button, button->state);

    if (d->disable_inputs)
        return true;

    if (!spicex_is_scaled(display)
        && d->mouse_mode == SPICE_MOUSE_MODE_CLIENT) {
        gint x, y;

        /* rule out clicks in outside region */
        gtk_widget_get_pointer (widget, &x, &y);
        x -= d->mx;
        y -= d->my;
        if (!(x >= 0 && x < d->width && y >= 0 && y < d->height))
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

    if (conf->width != d->ww  || conf->height != d->wh) {
        d->ww = conf->width;
        d->wh = conf->height;
        recalc_geometry(widget, TRUE);
    }
    return true;
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
                              FALSE,
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

static void update_mouse_mode(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    g_object_get(channel, "mouse-mode", &d->mouse_mode, NULL);
    SPICE_DEBUG("mouse mode %d", d->mouse_mode);

    switch (d->mouse_mode) {
    case SPICE_MOUSE_MODE_CLIENT:
        try_mouse_ungrab(display);
        break;
    case SPICE_MOUSE_MODE_SERVER:
        try_mouse_grab(display);
        break;
    default:
        g_warn_if_reached();
    }

    update_mouse_pointer(display);
    cursor_invalidate(display);
}

static void primary_create(SpiceChannel *channel, gint format,
                           gint width, gint height, gint stride,
                           gint shmid, gpointer imgdata, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gboolean set_display = FALSE;

    d->format = format;
    d->stride = stride;
    d->shmid  = shmid;
    d->data_origin = d->data = imgdata;

    if (d->width != width || d->height != height) {
        if (d->width != 0 && d->height != 0)
            set_display = TRUE;
        d->width  = width;
        d->height = height;
        recalc_geometry(GTK_WIDGET(display), set_display);
        if (!d->resize_guest_enable) {
            gtk_widget_set_size_request(GTK_WIDGET(display), width, height);
        }
    }
}

static void primary_destroy(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = SPICE_DISPLAY(data);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    spicex_image_destroy(display);
    d->format = 0;
    d->width  = 0;
    d->height = 0;
    d->stride = 0;
    d->shmid  = 0;
    d->data   = 0;
    d->data_origin = 0;
}

static void invalidate(SpiceChannel *channel,
                       gint x, gint y, gint w, gint h, gpointer data)
{
    SpiceDisplay *display = data;

    if (!gtk_widget_get_window(GTK_WIDGET(display)))
        return;

    if (!do_color_convert(display, x, y, w, h))
        return;

    spicex_image_invalidate(display, &x, &y, &w, &h);
    gtk_widget_queue_draw_area(GTK_WIDGET(display),
                               x, y, w, h);
}

static void mark(SpiceChannel *channel, gint mark, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    g_return_if_fail(d != NULL);

    SPICE_DEBUG("widget mark: %d, channel %d", mark, d->channel_id);
    d->mark = mark;
    spice_main_set_display_enabled(d->main, d->channel_id, d->mark != 0);
    if (mark != 0 && gtk_widget_get_window(GTK_WIDGET(display)))
        gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(display)),
                                   NULL, FALSE);
}

static void cursor_set(SpiceCursorChannel *channel,
                       gint width, gint height, gint hot_x, gint hot_y,
                       gpointer rgba, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    cursor_invalidate(display);

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

        /* gdk_cursor_new_from_pixbuf() will copy pixbuf data on
           x11/win32/macos. No worries if rgba pointer is freed
           after. */
        d->mouse_cursor = gdk_cursor_new_from_pixbuf(gtk_widget_get_display(GTK_WIDGET(display)),
                                                     d->mouse_pixbuf, hot_x, hot_y);
    }

    update_mouse_pointer(display);
}

static void cursor_hide(SpiceCursorChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->show_cursor != NULL) /* then we are already hidden */
        return;

    d->show_cursor = d->mouse_cursor;
    d->mouse_cursor = get_blank_cursor();
    update_mouse_pointer(display);
}

G_GNUC_INTERNAL
void spice_display_get_scaling(SpiceDisplay *display, double *sx, double *sy)
{
    SpiceDisplayPrivate *d = display->priv;
    int fbw = d->width, fbh = d->height;
    int ww, wh;

    if (!spicex_is_scaled(display)) {
        *sx = 1.0;
        *sy = 1.0;
        return;
    }

    gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);

    *sx = (double)ww / (double)fbw;
    *sy = (double)wh / (double)fbh;
}

static void cursor_invalidate(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = display->priv;
    double sx, sy;

    if (d->mouse_pixbuf == NULL)
        return;

    spice_display_get_scaling(display, &sx, &sy);

    gtk_widget_queue_draw_area(GTK_WIDGET(display),
                               (d->mouse_guest_x + d->mx - d->mouse_hotspot.x) * sx,
                               (d->mouse_guest_y + d->my - d->mouse_hotspot.y) * sy,
                               gdk_pixbuf_get_width(d->mouse_pixbuf) * sx,
                               gdk_pixbuf_get_height(d->mouse_pixbuf) * sy);
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

static void disconnect_main(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->main == NULL)
        return;
    g_signal_handlers_disconnect_by_func(d->main, G_CALLBACK(update_mouse_mode),
                                         display);
    d->main = NULL;
}

static void disconnect_display(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->display == NULL)
        return;
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(primary_create),
                                         display);
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(primary_destroy),
                                         display);
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(invalidate),
                                         display);
    d->display = NULL;
}

static void disconnect_cursor(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->cursor == NULL)
        return;
    g_signal_handlers_disconnect_by_func(d->cursor, G_CALLBACK(cursor_set),
                                         display);
    g_signal_handlers_disconnect_by_func(d->cursor, G_CALLBACK(cursor_move),
                                         display);
    g_signal_handlers_disconnect_by_func(d->cursor, G_CALLBACK(cursor_hide),
                                         display);
    g_signal_handlers_disconnect_by_func(d->cursor, G_CALLBACK(cursor_reset),
                                         display);
    d->cursor = NULL;
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        d->main = SPICE_MAIN_CHANNEL(channel);
        g_signal_connect(channel, "main-mouse-update",
                         G_CALLBACK(update_mouse_mode), display);
        update_mouse_mode(channel, display);
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        d->display = channel;
        g_signal_connect(channel, "display-primary-create",
                         G_CALLBACK(primary_create), display);
        g_signal_connect(channel, "display-primary-destroy",
                         G_CALLBACK(primary_destroy), display);
        g_signal_connect(channel, "display-invalidate",
                         G_CALLBACK(invalidate), display);
        g_signal_connect(channel, "display-mark",
                         G_CALLBACK(mark), display);
        spice_channel_connect(channel);
        return;
    }

    if (SPICE_IS_CURSOR_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        d->cursor = SPICE_CURSOR_CHANNEL(channel);
        g_signal_connect(channel, "cursor-set",
                         G_CALLBACK(cursor_set), display);
        g_signal_connect(channel, "cursor-move",
                         G_CALLBACK(cursor_move), display);
        g_signal_connect(channel, "cursor-hide",
                         G_CALLBACK(cursor_hide), display);
        g_signal_connect(channel, "cursor-reset",
                         G_CALLBACK(cursor_reset), display);
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
        disconnect_main(display);
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        disconnect_display(display);
        return;
    }

    if (SPICE_IS_CURSOR_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        disconnect_cursor(display);
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
 * @id: the display channel ID to associate with #SpiceDisplay
 *
 * Returns: a new #SpiceDisplay widget.
 **/
SpiceDisplay *spice_display_new(SpiceSession *session, int id)
{
    return g_object_new(SPICE_TYPE_DISPLAY, "session", session,
                        "channel-id", id, NULL);
}

/**
 * spice_display_mouse_ungrab:
 * @display:
 *
 * Ungrab the mouse.
 **/
void spice_display_mouse_ungrab(SpiceDisplay *display)
{
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
G_GNUC_DEPRECATED_FOR(spice_gtk_session_copy_to_guest)
void spice_display_copy_to_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

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
G_GNUC_DEPRECATED_FOR(spice_gtk_session_paste_from_guest)
void spice_display_paste_from_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

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
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkPixbuf *pixbuf;
    int x, y;
    guchar *src, *data, *dest;

    g_return_val_if_fail(d != NULL, NULL);
    /* TODO: ensure d->data has been exposed? */

    data = g_malloc(d->width * d->height * 3);
    src = d->data;
    dest = data;

    if (src == NULL || dest == NULL)
        return NULL;

    for (y = 0; y < d->height; ++y) {
        for (x = 0; x < d->width; ++x) {
          dest[0] = src[x * 4 + 2];
          dest[1] = src[x * 4 + 1];
          dest[2] = src[x * 4 + 0];
          dest += 3;
        }
        src += d->stride;
    }

    pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, false,
                                      8, d->width, d->height, d->width * 3,
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

    x_display = GDK_WINDOW_XDISPLAY(w);
    modifiers = get_keyboard_lock_modifiers(x_display);
    if (d->inputs)
        spice_inputs_set_key_locks(d->inputs, modifiers);
}

typedef enum SpiceLed {
    CAPS_LOCK_LED = 1,
    NUM_LOCK_LED,
    SCROLL_LOCK_LED,
} SpiceLed;

static guint get_modifier_mask(Display *x_display, KeySym modifier)
{
    int mask = 0;
    int i;

    XModifierKeymap* map = XGetModifierMapping(x_display);
    KeyCode keycode = XKeysymToKeycode(x_display, modifier);
    if (keycode == NoSymbol) {
        return 0;
    }

    for (i = 0; i < 8; i++) {
        if (map->modifiermap[map->max_keypermod * i] == keycode) {
            mask = 1 << i;
        }
    }
    XFreeModifiermap(map);
    return mask;
}

static void set_keyboard_led(Display *x_display, SpiceLed led, int set)
{
    guint mask;
    XKeyboardControl keyboard_control;

    switch (led) {
    case CAPS_LOCK_LED:
        if ((mask = get_modifier_mask(x_display, XK_Caps_Lock)) != 0) {
            XkbLockModifiers(x_display, XkbUseCoreKbd, mask, set ? mask : 0);
        }
        return;
    case NUM_LOCK_LED:
        if ((mask = get_modifier_mask(x_display, XK_Num_Lock)) != 0) {
            XkbLockModifiers(x_display, XkbUseCoreKbd, mask, set ? mask : 0);
        }
        return;
    case SCROLL_LOCK_LED:
        keyboard_control.led_mode = set ? LedModeOn : LedModeOff;
        keyboard_control.led = led;
        XChangeKeyboardControl(x_display, KBLed | KBLedMode, &keyboard_control);
        return;
    }
}

G_GNUC_UNUSED
static void spice_set_keyboard_lock_modifiers(SpiceDisplay *display, uint32_t modifiers)
{
    Display *x_display;

    x_display = GDK_WINDOW_XDISPLAY(gtk_widget_get_parent_window(GTK_WIDGET(display)));

    set_keyboard_led(x_display, CAPS_LOCK_LED, !!(modifiers & SPICE_INPUTS_CAPS_LOCK));
    set_keyboard_led(x_display, NUM_LOCK_LED, !!(modifiers & SPICE_INPUTS_NUM_LOCK));
    set_keyboard_led(x_display, SCROLL_LOCK_LED, !!(modifiers & SPICE_INPUTS_SCROLL_LOCK));
}
#else
static void sync_keyboard_lock_modifiers(SpiceDisplay *display)
{
    g_warning("sync_keyboard_lock_modifiers not implemented");
}
#endif // HAVE_X11_XKBLIB_H
