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
#ifdef WIN32
#include <windows.h>
#include <gdk/gdkwin32.h>
#endif

#include "spice-widget.h"
#include "spice-widget-priv.h"
#include "vncdisplaykeymap.h"

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
 * Client and guest clipboards will be shared automatically if
 * #SpiceDisplay:auto-clipboard is set to #TRUE. Alternatively, you
 * can send clipboard data from client to guest with
 * spice_display_copy_to_guest().

 * Finally, spice_display_get_pixbuf() will take a screenshot of the
 * current display and return an #GdkPixbuf (that you can then easily
 * save to disk).
 */

G_DEFINE_TYPE(SpiceDisplay, spice_display, GTK_TYPE_DRAWING_AREA)

/* Properties */
enum {
    PROP_0,
    PROP_KEYBOARD_GRAB,
    PROP_MOUSE_GRAB,
    PROP_RESIZE_GUEST,
    PROP_AUTO_CLIPBOARD,
    PROP_SCALING,
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

static void try_keyboard_grab(SpiceDisplay *display);
static void try_keyboard_ungrab(SpiceDisplay *display);
static void try_mouse_grab(GtkWidget *widget);
static void try_mouse_ungrab(GtkWidget *widget);
static void recalc_geometry(GtkWidget *widget, gboolean set_display);
static void clipboard_owner_change(GtkClipboard *clipboard,
                                   GdkEventOwnerChange *event, gpointer user_data);
static void disconnect_main(SpiceDisplay *display);
static void disconnect_cursor(SpiceDisplay *display);
static void disconnect_display(SpiceDisplay *display);
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void sync_keyboard_lock_modifiers(SpiceDisplay *display);

/* ---------------------------------------------------------------- */

static void spice_display_get_property(GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    SpiceDisplay *display = SPICE_DISPLAY(object);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    switch (prop_id) {
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
        g_value_set_boolean(value, d->auto_clipboard_enable);
        break;
    case PROP_SCALING:
        g_value_set_boolean(value, d->allow_scaling);
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
    case PROP_KEYBOARD_GRAB:
        d->keyboard_grab_enable = g_value_get_boolean(value);
        if (d->keyboard_grab_enable) {
            try_keyboard_grab(display);
        } else {
            try_keyboard_ungrab(display);
        }
        break;
    case PROP_MOUSE_GRAB:
        d->mouse_grab_enable = g_value_get_boolean(value);
        if (!d->mouse_grab_enable) {
            try_mouse_ungrab(GTK_WIDGET(display));
        }
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
        if (d->ximage &&
            gtk_widget_get_window(GTK_WIDGET(display))) { /* if not yet shown */
            int ww, wh;
            gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);
            gtk_widget_queue_draw_area(GTK_WIDGET(display), 0, 0, ww, wh);
        }
        break;
    case PROP_AUTO_CLIPBOARD:
        d->auto_clipboard_enable = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void spice_display_dispose(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("spice display dispose");

    disconnect_main(display);
    disconnect_display(display);
    disconnect_cursor(display);

    if (d->clipboard) {
        g_signal_handlers_disconnect_by_func(d->clipboard, G_CALLBACK(clipboard_owner_change),
                                             display);
        d->clipboard = NULL;
    }

    if (d->clipboard_primary) {
        g_signal_handlers_disconnect_by_func(d->clipboard_primary, G_CALLBACK(clipboard_owner_change),
                                             display);
        d->clipboard_primary = NULL;
    }
    if (d->session) {
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_new),
                                             display);
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_destroy),
                                             display);
        g_object_unref(d->session);
        d->session = NULL;
    }
}

static void spice_display_finalize(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i;

    SPICE_DEBUG("Finalize spice display");

    if (d->grabseq) {
        spice_grab_sequence_free(d->grabseq);
        d->grabseq = NULL;
    }
    g_free(d->activeseq);
    d->activeseq = NULL;

    for (i = 0; i < CLIPBOARD_LAST; ++i) {
        g_free(d->clip_targets[i]);
        d->clip_targets[i] = NULL;
    }

    G_OBJECT_CLASS(spice_display_parent_class)->finalize(obj);
}

static void spice_display_init(SpiceDisplay *display)
{
    GtkWidget *widget = GTK_WIDGET(display);
    SpiceDisplayPrivate *d;

    d = display->priv = SPICE_DISPLAY_GET_PRIVATE(display);
    memset(d, 0, sizeof(*d));

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

    d->clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(G_OBJECT(d->clipboard), "owner-change",
                     G_CALLBACK(clipboard_owner_change), display);
    d->clipboard_primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    g_signal_connect(G_OBJECT(d->clipboard_primary), "owner-change",
                     G_CALLBACK(clipboard_owner_change), display);

    if (g_getenv("SPICE_DEBUG_CURSOR"))
        d->mouse_cursor = gdk_cursor_new(GDK_DOT);
    else
        d->mouse_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
    d->have_mitshm = true;
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
    time_t now;
    GdkGrabStatus status;

    if (d->keyboard_grab_active)
        return;

    if (!d->keyboard_grab_enable)
        return;
    if (!d->keyboard_have_focus)
        return;
    if (!d->mouse_have_pointer)
        return;

    g_return_if_fail(gtk_widget_is_focus(widget));
    g_return_if_fail(gtk_widget_has_focus(widget));

    /*
     * focus / keyboard grab behavior is funky sometime
     * when going fullscreen (with KDE and GNOME-shell):
     * focus-in-event -> grab -> focus-out-event -> ungrab -> repeat
     * I have no idea why the grab triggers focus-out :-(
     */
    now = time(NULL);
    if (d->keyboard_grab_time != now) {
        d->keyboard_grab_time = now;
        d->keyboard_grab_count = 0;
    }
    if (d->keyboard_grab_count++ > 32) {
        g_critical("32 grabs last second -> emergency exit");
        return;
    }

    SPICE_DEBUG("grab keyboard");

#ifdef WIN32
    d->keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_cb,
                                        GetModuleHandle(NULL), 0);
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
    UnhookWindowsHookEx(d->keyboard_hook);
    d->keyboard_hook = 0;
#endif
    d->keyboard_grab_active = false;
    g_signal_emit(widget, signals[SPICE_DISPLAY_KEYBOARD_GRAB], 0, false);
}

static GdkGrabStatus do_pointer_grab(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkWindow *window = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display)));
    GdkGrabStatus status;

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
                     NULL, d->mouse_cursor,
                     GDK_CURRENT_TIME);
    if (status != GDK_GRAB_SUCCESS) {
        d->mouse_grab_active = false;
        g_warning("pointer grab failed %d", status);
    } else {
        d->mouse_grab_active = true;
        g_signal_emit(display, signals[SPICE_DISPLAY_MOUSE_GRAB], 0, true);
    }

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
        gdk_window_set_cursor(window, d->mouse_cursor);
        break;
    case SPICE_MOUSE_MODE_SERVER:
        if (!d->mouse_grab_active) {
            gdk_window_set_cursor(window, NULL);
        } else {
            gdk_window_set_cursor(window, d->mouse_cursor);
            try_mouse_grab(GTK_WIDGET(display));
        }
        break;
    default:
        g_warn_if_reached();
        break;
    }
}

static void try_mouse_grab(GtkWidget *widget)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

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
    int x = (int)motion->x_root;
    int y = (int)motion->y_root;

    if (d->mouse_guest_x != -1 && d->mouse_guest_y != -1) {
        SPICE_DEBUG("skip mouse_check_edges");
        return;
    }

    /* from gtk-vnc:
     * In relative mode check to see if client pointer hit one of the
     * screen edges, and if so move it back by 200 pixels. This is
     * important because the pointer in the server doesn't correspond
     * 1-for-1, and so may still be only half way across the
     * screen. Without this warp, the server pointer would thus appear
     * to hit an invisible wall */
    if (motion->x <= 0) x += 200;
    if (motion->y <= 0) y += 200;
    if (motion->x >= (gdk_screen_get_width(screen) - 1)) x -= 200;
    if (motion->y >= (gdk_screen_get_height(screen) - 1)) y -= 200;

    if (x != (int)motion->x_root || y != (int)motion->y_root) {
        gdk_display_warp_pointer(gtk_widget_get_display(widget),
                                 screen, x, y);
        d->mouse_last_x = -1;
        d->mouse_last_y = -1;
    }
}

static void try_mouse_ungrab(GtkWidget *widget)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (!d->mouse_grab_active)
        return;

    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    d->mouse_grab_active = false;
    update_mouse_pointer(display);
    g_signal_emit(widget, signals[SPICE_DISPLAY_MOUSE_GRAB], 0, false);
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

    if (d->mark == 0 || d->data == NULL)
        return false;

    if (!d->ximage) {
        spicex_image_create(display);
    }

    spicex_draw_event(display, cr);

    return true;
}
#else
static gboolean expose_event(GtkWidget *widget, GdkEventExpose *expose)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->mark == 0 || d->data == NULL)
        return false;

    if (!d->ximage) {
        spicex_image_create(display);
    }

    spicex_expose_event(display, expose);
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

    if (check_for_grab_key(display, key->type, key->keyval)) {
        g_signal_emit(widget, signals[SPICE_DISPLAY_GRAB_KEY_PRESSED], 0);
        if (d->mouse_grab_active)
            try_mouse_ungrab(widget);
        else
            /* TODO: gtk-vnc has a weird condition here
               if (!d->grab_keyboard || !d->absolute) */
            try_mouse_grab(widget);
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
    d->mouse_have_pointer = false;
    try_keyboard_ungrab(display);
    return true;
}

static gboolean focus_in_event(GtkWidget *widget, GdkEventFocus *focus G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);
    release_keys(display);
    sync_keyboard_lock_modifiers(display);
    d->keyboard_have_focus = true;
    try_keyboard_grab(display);
#ifdef WIN32
    focus_window = (HWND)gdk_win32_drawable_get_handle(GDK_DRAWABLE(widget->window));
    g_return_val_if_fail(focus_window != NULL, true);
#endif
    return true;
}

static gboolean focus_out_event(GtkWidget *widget, GdkEventFocus *focus G_GNUC_UNUSED)
{
    SpiceDisplay *display = SPICE_DISPLAY(widget);
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("%s", __FUNCTION__);
    d->keyboard_have_focus = false;
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

    if (!spicex_is_scaled(display)) {
        gint x, y;

        /* rule out clicks in outside region */
        gtk_widget_get_pointer (widget, &x, &y);
        x -= d->mx;
        y -= d->my;
        if (!(x >= 0 && x < d->width && y >= 0 && y < d->height))
            return true;
    }

    gtk_widget_grab_focus(widget);
    if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER)
        try_mouse_grab(widget);
    else /* allow to drag and drop between windows/displays:
            FIXME: should be multiple widget grab, but how?
            or should now the position of the other widgets?..
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

static GtkClipboard* get_clipboard_from_selection(SpiceDisplayPrivate *d, guint selection)
{
    if (selection == VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD) {
        return d->clipboard;
    } else if (selection == VD_AGENT_CLIPBOARD_SELECTION_PRIMARY) {
        return d->clipboard_primary;
    } else {
        g_warning("Unhandled clipboard selection: %d", selection);
        return NULL;
    }
}

static gint get_selection_from_clipboard(SpiceDisplayPrivate *d, GtkClipboard* cb)
{
    if (cb == d->clipboard) {
        return VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;
    } else if (cb == d->clipboard_primary) {
        return VD_AGENT_CLIPBOARD_SELECTION_PRIMARY;
    } else {
        g_warning("Unhandled clipboard");
        return -1;
    }
}

static const struct {
    const char  *xatom;
    uint32_t    vdagent;
    uint32_t    flags;
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

static void clipboard_get_targets(GtkClipboard *clipboard,
                                  GdkAtom *atoms,
                                  gint n_atoms,
                                  gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    guint32 types[SPICE_N_ELEMENTS(atom2agent)];
    char *name;
    int a, m, t;
    int selection;

    selection = get_selection_from_clipboard(d, clipboard);
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
    if (!d->clip_grabbed[selection] && t > 0) {
        d->clip_grabbed[selection] = TRUE;
        spice_main_clipboard_selection_grab(d->main,
            get_selection_from_clipboard(d, clipboard), types, t);
        /* Sending a grab causes the agent to do an impicit release */
        d->nclip_targets[selection] = 0;
    }
}

static void clipboard_owner_change(GtkClipboard        *clipboard,
                                   GdkEventOwnerChange *event,
                                   gpointer            data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int selection;

    selection = get_selection_from_clipboard(d, clipboard);
    g_return_if_fail(selection != -1);

    if (d->main == NULL)
        return;

    if (d->clip_grabbed[selection]) {
        d->clip_grabbed[selection] = FALSE;
        spice_main_clipboard_selection_release(d->main,
            get_selection_from_clipboard(d, clipboard));
    }

    switch (event->reason) {
    case GDK_OWNER_CHANGE_NEW_OWNER:
        if (d->clipboard_selfgrab_pending[selection]) {
            d->clipboard_selfgrab_pending[selection] = FALSE;
            break;
        }
        d->clipboard_by_guest[selection] = FALSE;
        d->clip_hasdata[selection] = TRUE;
        if (d->auto_clipboard_enable)
            gtk_clipboard_request_targets(clipboard, clipboard_get_targets, data);
        break;
    default:
        d->clip_hasdata[selection] = FALSE;
        break;
    }
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

    gobject_class->dispose = spice_display_dispose;
    gobject_class->finalize = spice_display_finalize;
    gobject_class->get_property = spice_display_get_property;
    gobject_class->set_property = spice_display_set_property;

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

    g_object_class_install_property
        (gobject_class, PROP_SCALING,
         g_param_spec_boolean("scaling", "Scaling",
                              "Whether we should use scaling",
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

static void mouse_update(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    g_object_get(channel, "mouse-mode", &d->mouse_mode, NULL);
    SPICE_DEBUG("mouse mode %d", d->mouse_mode);

    d->mouse_guest_x = -1;
    d->mouse_guest_y = -1;
    if (d->mouse_mode == SPICE_MOUSE_MODE_CLIENT) {
        try_mouse_ungrab(GTK_WIDGET(display));
    }
    update_mouse_pointer(display);
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

    if (d->mouse_cursor) {
        gdk_cursor_unref(d->mouse_cursor);
        d->mouse_cursor = NULL;
    }

    if (rgba != NULL) {
        GdkPixbuf *pixbuf;
        GdkDisplay *gtkdpy;

        gtkdpy = gtk_widget_get_display(GTK_WIDGET(display));

        pixbuf = gdk_pixbuf_new_from_data(rgba,
                                          GDK_COLORSPACE_RGB,
                                          TRUE, 8,
                                          width,
                                          height,
                                          width * 4,
                                          NULL, NULL);

        /* gdk_cursor_new_from_pixbuf() will copy pixbuf data on
           x11/win32/macos. No worries if rgba pointer is freed
           after. */
        d->mouse_cursor = gdk_cursor_new_from_pixbuf(gtkdpy, pixbuf,
                                                     hot_x, hot_y);
        g_object_unref(pixbuf);
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
    d->mouse_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
    update_mouse_pointer(display);
}

static void cursor_move(SpiceCursorChannel *channel, gint x, gint y, gpointer data)
{
    SpiceDisplay *display = data;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(display));
    int wx, wy;

    SPICE_DEBUG("%s: +%d+%d", __FUNCTION__, x, y);

    d->mouse_last_x = x;
    d->mouse_last_y = y;
    d->mouse_guest_x = x;
    d->mouse_guest_y = y;

    if (spicex_is_scaled(display) && gtk_widget_get_realized(GTK_WIDGET(display))) {
        gint ww, wh;

        gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);
        x = x * ((double)ww / (double)(d->width));
        y = y * ((double)wh / (double)(d->height));
    } else {
        /* black borders offset */
        x += d->mx;
        y += d->my;
    }

    if (d->mouse_grab_active && gtk_widget_get_realized(GTK_WIDGET(display))) {
        gdk_window_get_origin(GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(display))), &wx, &wy);
        gdk_display_warp_pointer(gtk_widget_get_display(GTK_WIDGET(display)), screen, x + wx, y + wy);
    }

    /* FIXME: apparently we have to restore cursor when "cursor_move" ?? */
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
    gint i;

    if (d->main == NULL)
        return;
    g_signal_handlers_disconnect_by_func(d->main, G_CALLBACK(mouse_update),
                                         display);
    d->main = NULL;
    for (i = 0; i < CLIPBOARD_LAST; ++i) {
        d->clipboard_by_guest[i] = FALSE;
        d->clip_grabbed[i] = FALSE;
        d->nclip_targets[i] = 0;
    }
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

typedef struct
{
    GMainLoop *loop;
    SpiceDisplay *display;
    GtkSelectionData *selection_data;
    guint info;
    gulong timeout_handler;
    guint selection;
} RunInfo;

static void clipboard_got_from_guest(SpiceMainChannel *main, guint selection,
                                     guint type, guchar *data, guint size,
                                     gpointer userdata)
{
    RunInfo *ri = userdata;

    g_return_if_fail(selection == ri->selection);

    SPICE_DEBUG("clipboard got data");

    gtk_selection_data_set(ri->selection_data,
        gdk_atom_intern_static_string(atom2agent[ri->info].xatom),
        8, data, size);

    if (g_main_loop_is_running (ri->loop))
        g_main_loop_quit (ri->loop);
}

static gboolean clipboard_timeout(gpointer data)
{
    RunInfo *ri = data;

    g_warning("clipboard get timed out");
    if (g_main_loop_is_running (ri->loop))
        g_main_loop_quit (ri->loop);

    ri->timeout_handler = 0;
    return FALSE;
}

static void clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection_data,
                          guint info, gpointer display)
{
    RunInfo ri = { NULL, };
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gulong clipboard_handler;

    SPICE_DEBUG("clipboard get");

    g_return_if_fail(info < SPICE_N_ELEMENTS(atom2agent));
    g_return_if_fail(get_selection_from_clipboard(d, clipboard) != -1);

    ri.display = display;
    ri.selection_data = selection_data;
    ri.info = info;
    ri.loop = g_main_loop_new(NULL, FALSE);
    ri.selection = get_selection_from_clipboard(d, clipboard);

    clipboard_handler = g_signal_connect(d->main, "main-clipboard-selection",
                                         G_CALLBACK(clipboard_got_from_guest), &ri);
    ri.timeout_handler = g_timeout_add_seconds(7, clipboard_timeout, &ri);
    spice_main_clipboard_selection_request(d->main, ri.selection, atom2agent[info].vdagent);

    /* apparently, this is needed to avoid dead-lock, from
       gtk_dialog_run */
    GDK_THREADS_LEAVE();
    g_main_loop_run(ri.loop);
    GDK_THREADS_ENTER();

    g_main_loop_unref(ri.loop);
    ri.loop = NULL;
    g_signal_handler_disconnect(d->main, clipboard_handler);
    if (ri.timeout_handler != 0)
        g_source_remove(ri.timeout_handler);
}

static void clipboard_clear(GtkClipboard *clipboard, gpointer display)
{
    SPICE_DEBUG("clipboard_clear");
    /* We watch for clipboard ownership changes and act on those, so we
       don't need to do anything here */
}

static gboolean clipboard_grab(SpiceMainChannel *main, guint selection,
                               guint32* types, guint32 ntypes, gpointer display)
{
    int m, n, i;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    GtkTargetEntry targets[SPICE_N_ELEMENTS(atom2agent)];
    gboolean target_selected[SPICE_N_ELEMENTS(atom2agent)] = { FALSE, };
    gboolean found;
    GtkClipboard* cb;

    cb = get_clipboard_from_selection(d, selection);
    g_return_val_if_fail(cb != NULL, FALSE);

    i = 0;
    for (n = 0; n < ntypes; ++n) {
        found = FALSE;
        for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
            if (atom2agent[m].vdagent == types[n] && !target_selected[m]) {
                found = TRUE;
                g_return_val_if_fail(i < SPICE_N_ELEMENTS(atom2agent), FALSE);
                targets[i].target = (gchar*)atom2agent[m].xatom;
                targets[i].flags = 0;
                targets[i].info = m;
                target_selected[m] = TRUE;
                i += 1;
            }
        }
        if (!found) {
            g_warning("clipboard: couldn't find a matching type for: %d", types[n]);
        }
    }

    g_free(d->clip_targets[selection]);
    d->nclip_targets[selection] = i;
    d->clip_targets[selection] = g_memdup(targets, sizeof(GtkTargetEntry) * i);
    /* Receiving a grab implies we've released our own grab */
    d->clip_grabbed[selection] = FALSE;

    if (!d->auto_clipboard_enable || d->nclip_targets[selection] == 0)
        goto skip_grab_clipboard;

    if (!gtk_clipboard_set_with_data(cb, targets, i,
                                     clipboard_get, clipboard_clear, display)) {
        g_warning("clipboard grab failed");
        return FALSE;
    }
    d->clipboard_selfgrab_pending[selection] = TRUE;
    d->clipboard_by_guest[selection] = TRUE;
    d->clip_hasdata[selection] = FALSE;

skip_grab_clipboard:
    return TRUE;
}

static void clipboard_received_cb(GtkClipboard *clipboard,
                                  GtkSelectionData *selection_data,
                                  gpointer display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gint len = 0, m;
    guint32 type = VD_AGENT_CLIPBOARD_NONE;
    gchar* name;
    GdkAtom atom;

    g_return_if_fail(get_selection_from_clipboard(d, clipboard) != -1);

    len = gtk_selection_data_get_length(selection_data);
    if (len == -1) {
        SPICE_DEBUG("empty clipboard");
        len = 0;
    } else if (len == 0) {
        SPICE_DEBUG("TODO: what should be done here?");
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

    spice_main_clipboard_selection_notify(d->main, get_selection_from_clipboard(d, clipboard),
        type, gtk_selection_data_get_data(selection_data), len);
}

static gboolean clipboard_request(SpiceMainChannel *main, guint selection,
                                  guint type, gpointer display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int m;
    GdkAtom atom;

    for (m = 0; m < SPICE_N_ELEMENTS(atom2agent); m++) {
        if (atom2agent[m].vdagent == type)
            break;
    }

    g_return_val_if_fail(m < SPICE_N_ELEMENTS(atom2agent), FALSE);

    atom = gdk_atom_intern_static_string(atom2agent[m].xatom);
    gtk_clipboard_request_contents(get_clipboard_from_selection(d, selection), atom,
                                   clipboard_received_cb, display);

    return TRUE;
}

static void clipboard_release(SpiceMainChannel *main, guint selection, gpointer data)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(data);
    GtkClipboard* clipboard = get_clipboard_from_selection(d, selection);
    if (!clipboard)
        return;

    d->nclip_targets[selection] = 0;

    if (!d->clipboard_by_guest[selection])
        return;
    gtk_clipboard_clear(clipboard);
    d->clipboard_by_guest[selection] = FALSE;
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
                         G_CALLBACK(mouse_update), display);
        mouse_update(channel, display);
        if (id != d->channel_id)
            return;
        g_signal_connect(channel, "main-clipboard-selection-grab",
                         G_CALLBACK(clipboard_grab), display);
        g_signal_connect(channel, "main-clipboard-selection-request",
                         G_CALLBACK(clipboard_request), display);
        g_signal_connect(channel, "main-clipboard-selection-release",
                         G_CALLBACK(clipboard_release), display);
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
    SpiceDisplay *display;
    SpiceDisplayPrivate *d;
    GList *list;
    GList *it;

    display = g_object_new(SPICE_TYPE_DISPLAY, NULL);
    d = SPICE_DISPLAY_GET_PRIVATE(display);
    d->session = g_object_ref(session);
    d->channel_id = id;

    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), display);
    g_signal_connect(session, "channel-destroy",
                     G_CALLBACK(channel_destroy), display);
    list = spice_session_get_channels(session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(session, it->data, (gpointer*)display);
    }
    g_list_free(list);

    return display;
}

/**
 * spice_display_mouse_ungrab:
 * @display:
 *
 * Ungrab the mouse.
 **/
void spice_display_mouse_ungrab(SpiceDisplay *display)
{
    try_mouse_ungrab(GTK_WIDGET(display));
}

/**
 * spice_display_copy_to_guest:
 * @display:
 *
 * Copy client-side clipboard to guest clipboard.
 **/
void spice_display_copy_to_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    if (d->clip_hasdata[selection] && !d->clip_grabbed[selection]) {
        gtk_clipboard_request_targets(d->clipboard, clipboard_get_targets, display);
    }
}

void spice_display_paste_from_guest(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int selection = VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD;

    if (d->nclip_targets[selection] == 0) {
        g_warning("Guest clipboard is not available.");
        return;
    }

    if (!gtk_clipboard_set_with_data(d->clipboard,
                                     d->clip_targets[selection], d->nclip_targets[selection],
                                     clipboard_get, clipboard_clear, display)) {
        g_warning("Clipboard grab failed");
        return;
    }
    d->clipboard_selfgrab_pending[selection] = TRUE;
    d->clipboard_by_guest[selection] = TRUE;
    d->clip_hasdata[selection] = FALSE;
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
