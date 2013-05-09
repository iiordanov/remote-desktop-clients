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
#ifndef __SPICE_WIDGET_PRIV_H__
#define __SPICE_WIDGET_PRIV_H__

G_BEGIN_DECLS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_X11
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <gdk/gdkx.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include "spice-widget.h"
#include "spice-common.h"
#include <spice/vd_agent.h>

#define SPICE_DISPLAY_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_DISPLAY, SpiceDisplayPrivate))

#define CLIPBOARD_LAST (VD_AGENT_CLIPBOARD_SELECTION_SECONDARY + 1)

struct _SpiceDisplayPrivate {
    gint                    channel_id;

    /* options */
    bool                    keyboard_grab_enable;
    bool                    mouse_grab_enable;
    bool                    resize_guest_enable;
    bool                    auto_clipboard_enable;

    /* state */
    enum SpiceSurfaceFmt    format;
    gint                    width, height, stride;
    gint                    shmid;
    gpointer                data_origin; /* the original display image data */
    gpointer                data; /* converted if necessary to 32 bits */

    gint                    ww, wh, mx, my;

    bool                    convert;
    bool                    have_mitshm;
    gboolean                allow_scaling;

    /* TODO: make a display object instead? */
#ifdef WITH_X11
    Display                 *dpy;
    XVisualInfo             *vi;
    XImage                  *ximage;
    XShmSegmentInfo         *shminfo;
    GC                      gc;
#else
    cairo_surface_t         *ximage;
#endif

    GtkClipboard            *clipboard;
    GtkClipboard            *clipboard_primary;
    GtkTargetEntry          *clip_targets[CLIPBOARD_LAST];
    guint                   nclip_targets[CLIPBOARD_LAST];
    bool                    clip_hasdata[CLIPBOARD_LAST];
    bool                    clip_grabbed[CLIPBOARD_LAST];
    gboolean                clipboard_by_guest[CLIPBOARD_LAST];
    gboolean                clipboard_selfgrab_pending[CLIPBOARD_LAST];

    SpiceSession            *session;
    SpiceMainChannel        *main;
    SpiceChannel            *display;
    SpiceCursorChannel      *cursor;
    SpiceInputsChannel      *inputs;
    SpiceSmartcardChannel   *smartcard;

    enum SpiceMouseMode     mouse_mode;
    int                     mouse_grab_active;
    bool                    mouse_have_pointer;
    GdkCursor               *mouse_cursor;
    GdkCursor               *show_cursor;
    int                     mouse_last_x;
    int                     mouse_last_y;
    int                     mouse_guest_x;
    int                     mouse_guest_y;

    bool                    keyboard_grab_active;
    bool                    keyboard_have_focus;
    int                     keyboard_grab_count;
    time_t                  keyboard_grab_time;

    const guint16 const     *keycode_map;
    size_t                  keycode_maplen;
    uint32_t                key_state[512 / 32];
    SpiceGrabSequence         *grabseq; /* the configured key sequence */
    gboolean                *activeseq; /* the currently pressed keys */
    gint                    mark;
#ifdef WIN32
    HHOOK                   keyboard_hook;
#endif
};

int      spicex_image_create                 (SpiceDisplay *display);
void     spicex_image_destroy                (SpiceDisplay *display);
void     spicex_image_invalidate             (SpiceDisplay *display, gint *x, gint *y, gint *w, gint *h);
#if GTK_CHECK_VERSION (2, 91, 0)
void     spicex_draw_event                   (SpiceDisplay *display, cairo_t *cr);
#else
void     spicex_expose_event                 (SpiceDisplay *display, GdkEventExpose *ev);
#endif
gboolean spicex_is_scaled                    (SpiceDisplay *display);

G_END_DECLS

#endif
