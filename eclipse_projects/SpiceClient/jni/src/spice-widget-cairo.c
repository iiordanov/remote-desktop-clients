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
#include "spice-widget.h"
#include "spice-widget-priv.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Some compatibility defines to let us build on both Gtk2 and Gtk3 */
#if GTK_CHECK_VERSION (2, 91, 0)

static inline void gdk_drawable_get_size(GdkWindow *w, gint *ww, gint *wh)
{
       *ww = gdk_window_get_width(w);
       *wh = gdk_window_get_height(w);
}
#endif

G_GNUC_INTERNAL
int spicex_image_create(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->format == SPICE_SURFACE_FMT_16_555 ||
        d->format == SPICE_SURFACE_FMT_16_565) {
        d->convert = TRUE;
        d->data = g_malloc0(d->height * d->stride); /* pixels are 32 bits */
    } else {
        d->convert = FALSE;
    }

    d->ximage = cairo_image_surface_create_for_data
        (d->data, CAIRO_FORMAT_RGB24, d->width, d->height, d->stride);

    return 0;
}

G_GNUC_INTERNAL
void spicex_image_destroy(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->ximage) {
        cairo_surface_finish(d->ximage);
        d->ximage = NULL;
    }
    if (d->convert && d->data) {
        g_free(d->data);
        d->data = NULL;
    }
}

G_GNUC_INTERNAL
void spicex_draw_event(SpiceDisplay *display, cairo_t *cr)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int fbw = d->width, fbh = d->height;
    int ww, wh;

    gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);

    /* If we don't have a pixmap, or we're not scaling, then
       we need to fill with background color */
    if (!d->ximage ||
        !d->allow_scaling) {
        cairo_rectangle(cr, 0, 0, ww, wh);
        /* Optionally cut out the inner area where the pixmap
           will be drawn. This avoids 'flashing' since we're
           not double-buffering. Note we're using the undocumented
           behaviour of drawing the rectangle from right to left
           to cut out the whole */
        if (d->ximage)
            cairo_rectangle(cr, d->mx + fbw, d->my,
                            -1 * fbw, fbh);
        cairo_fill(cr);
    }

    /* Draw the display */
    if (d->ximage) {
        if (d->allow_scaling) {
            double sx, sy;
            spice_display_get_scaling(display, &sx, &sy);
            cairo_scale(cr, sx, sy);
            cairo_set_source_surface(cr, d->ximage, 0, 0);
        } else {
            cairo_set_source_surface(cr, d->ximage, d->mx, d->my);
        }
        cairo_paint(cr);

        if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER) {
            GdkPixbuf *image = d->mouse_pixbuf;
            if (image != NULL) {
                gdk_cairo_set_source_pixbuf(cr, image,
                                            d->mx + d->mouse_guest_x - d->mouse_hotspot.x,
                                            d->my + d->mouse_guest_y - d->mouse_hotspot.y);
                cairo_paint(cr);
            }
        }
    }
}

#if ! GTK_CHECK_VERSION (2, 91, 0)
G_GNUC_INTERNAL
void spicex_expose_event(SpiceDisplay *display, GdkEventExpose *expose)
{
    cairo_t *cr;

    cr = gdk_cairo_create(gtk_widget_get_window(GTK_WIDGET(display)));
    cairo_rectangle(cr,
                    expose->area.x,
                    expose->area.y,
                    expose->area.width,
                    expose->area.height);
    cairo_clip(cr);

    spicex_draw_event(display, cr);

    cairo_destroy(cr);
}
#endif

G_GNUC_INTERNAL
void spicex_image_invalidate(SpiceDisplay *display,
                             gint *x, gint *y, gint *w, gint *h)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int ww, wh;

    gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);

    if (d->allow_scaling) {
        double sx, sy;

        /* Scale the exposed region */
        sx = (double)ww / (double)d->width;
        sy = (double)wh / (double)d->height;

        *x *= sx;
        *y *= sy;
        *w *= sx;
        *h *= sy;

        /* FIXME: same hack as gtk-vnc */
        /* Without this, we get horizontal & vertical line artifacts
         * when drawing. This "fix" is somewhat dubious though. The
         * true mistake & fix almost certainly lies elsewhere.
         */
        *x -= 2;
        *y -= 2;
        *w += 4;
        *h += 4;
    } else {
        /* Offset the Spice region to produce expose region */
        *x += d->mx;
        *y += d->my;
    }
}

G_GNUC_INTERNAL
gboolean spicex_is_scaled(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    return d->allow_scaling;
}
