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
#ifndef __SPICE_CLIENT_WIDGET_H__
#define __SPICE_CLIENT_WIDGET_H__

#include "spice-client.h"

#include <gtk/gtk.h>
#include "spice-grabsequence.h"
#include "spice-widget-enums.h"
#include "spice-util.h"
#include "spice-gtk-session.h"

G_BEGIN_DECLS

#define SPICE_TYPE_DISPLAY            (spice_display_get_type())
#define SPICE_DISPLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_DISPLAY, SpiceDisplay))
#define SPICE_DISPLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_DISPLAY, SpiceDisplayClass))
#define SPICE_IS_DISPLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_DISPLAY))
#define SPICE_IS_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_DISPLAY))
#define SPICE_DISPLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_DISPLAY, SpiceDisplayClass))


typedef struct _SpiceDisplay SpiceDisplay;
typedef struct _SpiceDisplayClass SpiceDisplayClass;
typedef struct _SpiceDisplayPrivate SpiceDisplayPrivate;

struct _SpiceDisplay {
    GtkDrawingArea parent;
    SpiceDisplayPrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpiceDisplayClass {
    GtkDrawingAreaClass parent_class;

    /* signals */
    void (*mouse_grab)(SpiceChannel *channel, gint grabbed);
    void (*keyboard_grab)(SpiceChannel *channel, gint grabbed);

    /*< private >*/
    /*
     * If adding fields to this struct, remove corresponding
     * amount of padding to avoid changing overall struct size
     */
    gchar _spice_reserved[SPICE_RESERVED_PADDING];
};

typedef enum
{
	SPICE_DISPLAY_KEY_EVENT_PRESS = 1,
	SPICE_DISPLAY_KEY_EVENT_RELEASE = 2,
	SPICE_DISPLAY_KEY_EVENT_CLICK = 3,
} SpiceDisplayKeyEvent;

GType	        spice_display_get_type(void);

SpiceDisplay* spice_display_new(SpiceSession *session, int channel_id);
SpiceDisplay* spice_display_new_with_monitor(SpiceSession *session, gint channel_id, gint monitor_id);

void spice_display_mouse_ungrab(SpiceDisplay *display);
void spice_display_set_grab_keys(SpiceDisplay *display, SpiceGrabSequence *seq);
SpiceGrabSequence *spice_display_get_grab_keys(SpiceDisplay *display);
void spice_display_send_keys(SpiceDisplay *display, const guint *keyvals,
                             int nkeyvals, SpiceDisplayKeyEvent kind);
GdkPixbuf *spice_display_get_pixbuf(SpiceDisplay *display);

#ifndef SPICE_DISABLE_DEPRECATED
SPICE_DEPRECATED_FOR(spice_gtk_session_copy_to_guest)
void spice_display_copy_to_guest(SpiceDisplay *display);
SPICE_DEPRECATED_FOR(spice_gtk_session_paste_from_guest)
void spice_display_paste_from_guest(SpiceDisplay *display);
#endif

G_END_DECLS

#endif /* __SPICE_CLIENT_WIDGET_H__ */
