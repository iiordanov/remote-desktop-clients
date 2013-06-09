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

    if (d->ximage != NULL)
        return 0;

    if (d->format == SPICE_SURFACE_FMT_16_555 ||
        d->format == SPICE_SURFACE_FMT_16_565) {
        d->convert = TRUE;
        d->data = g_malloc0(d->area.width * d->area.height * 4);

        d->ximage = cairo_image_surface_create_for_data
            (d->data, CAIRO_FORMAT_RGB24, d->area.width, d->area.height, d->area.width * 4);

    } else {
        d->convert = FALSE;

        d->ximage = cairo_image_surface_create_for_data
            (d->data, CAIRO_FORMAT_RGB24, d->width, d->height, d->stride);
    }

    return 0;
}

G_GNUC_INTERNAL
void spicex_image_destroy(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->ximage) {
        cairo_surface_destroy(d->ximage);
        d->ximage = NULL;
    }
    if (d->convert && d->data) {
        g_free(d->data);
        d->data = NULL;
    }
    d->convert = FALSE;
}

#if !GTK_CHECK_VERSION (3, 0, 0)
#define cairo_rectangle_int_t GdkRectangle
#define cairo_region_t GdkRegion
#define cairo_region_create_rectangle gdk_region_rectangle
#define cairo_region_subtract_rectangle(_dest,_rect) { GdkRegion *_region = gdk_region_rectangle (_rect); gdk_region_subtract (_dest, _region); gdk_region_destroy (_region); }
#define cairo_region_destroy gdk_region_destroy
#endif

G_GNUC_INTERNAL
void spicex_draw_event(SpiceDisplay *display, cairo_t *cr)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    cairo_rectangle_int_t rect;
    cairo_region_t *region;
    double s;
    int x, y;
    int ww, wh;
    int w, h;

    spice_display_get_scaling(display, &s, &x, &y, &w, &h);

    gdk_drawable_get_size(gtk_widget_get_window(GTK_WIDGET(display)), &ww, &wh);

    /* We need to paint the bg color around the image */
    rect.x = 0;
    rect.y = 0;
    rect.width = ww;
    rect.height = wh;
    region = cairo_region_create_rectangle(&rect);

    /* Optionally cut out the inner area where the pixmap
       will be drawn. This avoids 'flashing' since we're
       not double-buffering. */
    if (d->ximage) {
        rect.x = x;
        rect.y = y;
        rect.width = w;
        rect.height = h;
        cairo_region_subtract_rectangle(region, &rect);
    }

    gdk_cairo_region (cr, region);
    cairo_region_destroy (region);

    /* Need to set a real solid color, because the default is usually
       transparent these days, and non-double buffered windows can't
       render transparently */
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_fill(cr);

    /* Draw the display */
    if (d->ximage) {
        cairo_translate(cr, x, y);
        cairo_rectangle(cr, 0, 0, w, h);
        cairo_scale(cr, s, s);
        if (!d->convert)
            cairo_translate(cr, -d->area.x, -d->area.y);
        cairo_set_source_surface(cr, d->ximage, 0, 0);
        cairo_fill(cr);

        if (d->mouse_mode == SPICE_MOUSE_MODE_SERVER &&
            d->mouse_guest_x != -1 && d->mouse_guest_y != -1 &&
            !d->show_cursor) {
            GdkPixbuf *image = d->mouse_pixbuf;
            if (image != NULL) {
                gdk_cairo_set_source_pixbuf(cr, image,
                                            d->mouse_guest_x - d->mouse_hotspot.x,
                                            d->mouse_guest_y - d->mouse_hotspot.y);
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
gboolean spicex_is_scaled(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    return d->allow_scaling;
}
