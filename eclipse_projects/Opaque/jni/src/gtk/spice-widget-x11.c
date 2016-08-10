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
#include "config.h"

#include "spice-widget.h"
#include "spice-widget-priv.h"

#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

static bool no_mitshm;

static struct format_table {
    enum SpiceSurfaceFmt  spice;
    XVisualInfo           xvisual;
} format_table[] = {
    {
        .spice = SPICE_SURFACE_FMT_32_ARGB, /* FIXME: is that correct xvisual? */
        .xvisual = {
            .depth      = 24,
            .red_mask   = 0xff0000,
            .green_mask = 0x00ff00,
            .blue_mask  = 0x0000ff,
        },
    },{
        .spice = SPICE_SURFACE_FMT_32_xRGB,
        .xvisual = {
            .depth      = 24,
            .red_mask   = 0xff0000,
            .green_mask = 0x00ff00,
            .blue_mask  = 0x0000ff,
        },
    },{
        .spice = SPICE_SURFACE_FMT_16_555,
        .xvisual = {
            .depth      = 16,
            .red_mask   = 0x7c00,
            .green_mask = 0x03e0,
            .blue_mask  = 0x001f,
        },
    },{
        .spice = SPICE_SURFACE_FMT_16_565,
        .xvisual = {
            .depth      = 16,
            .red_mask   = 0xf800,
            .green_mask = 0x07e0,
            .blue_mask  = 0x001f,
        },
    }
};

static XVisualInfo *get_visual_for_format(GtkWidget *widget, enum SpiceSurfaceFmt format)
{
    GdkDrawable  *drawable = gtk_widget_get_window(widget);
    GdkDisplay   *display = gdk_drawable_get_display(drawable);
    GdkScreen    *screen = gdk_drawable_get_screen(drawable);
    XVisualInfo  template;
    int          found, i;
    XVisualInfo *vi;

    for (i = 0; i < SPICE_N_ELEMENTS(format_table); i++) {
        if (format == format_table[i].spice)
            break;
    }
    if (i == SPICE_N_ELEMENTS(format_table)) {
        g_warn_if_reached();
        return NULL;
    }

    template = format_table[i].xvisual;
    template.screen = gdk_x11_screen_get_screen_number(screen);
    vi = XGetVisualInfo(gdk_x11_display_get_xdisplay(display),
                        VisualScreenMask | VisualDepthMask |
                        VisualRedMaskMask | VisualGreenMaskMask | VisualBlueMaskMask,
                        &template, &found);
    return vi;
}

static XVisualInfo *get_visual_default(GtkWidget *widget)
{
    GdkDrawable  *drawable = gtk_widget_get_window(widget);
    GdkDisplay   *display = gdk_drawable_get_display(drawable);
    GdkScreen    *screen = gdk_drawable_get_screen(drawable);
    XVisualInfo  template;
    int          found;

    template.screen = gdk_x11_screen_get_screen_number(screen);
    return XGetVisualInfo(gdk_x11_display_get_xdisplay(display),
                          VisualScreenMask,
                          &template, &found);
}

static int catch_no_mitshm(Display * dpy, XErrorEvent * event)
{
    no_mitshm = true;
    return 0;
}

G_GNUC_INTERNAL
int spicex_image_create(SpiceDisplay *display)
{
    SpiceDisplayPrivate   *d = display->priv;

    if (d->ximage != NULL)
        return 0;

    GdkDrawable     *window = gtk_widget_get_window(GTK_WIDGET(display));
    GdkDisplay      *gtkdpy = gdk_drawable_get_display(window);
    void            *old_handler = NULL;
    XGCValues       gcval = {
        .foreground = 0,
        .background = 0,
    };

    d->dpy = gdk_x11_display_get_xdisplay(gtkdpy);
    d->convert = false;
    d->vi = get_visual_for_format(GTK_WIDGET(display), d->format);
    if (d->vi == NULL) {
        d->convert = true;
        d->vi = get_visual_default(GTK_WIDGET(display));
        d->vi = get_visual_for_format(GTK_WIDGET(display), SPICE_SURFACE_FMT_32_xRGB);
        g_return_val_if_fail(d->vi != NULL, 1);
    }
    if (d->convert) {
        d->data = g_malloc0(d->height * d->stride); /* pixels are 32 bits */
    }

    d->gc = XCreateGC(d->dpy, gdk_x11_drawable_get_xid(window),
                      GCForeground | GCBackground, &gcval);

    if (d->convert) /* do not use shm when doing color format conversion */
        goto xcreate;

    if (d->have_mitshm && d->shmid != -1) {
        if (!XShmQueryExtension(d->dpy)) {
            goto shm_fail;
        }
        no_mitshm = false;
        old_handler = XSetErrorHandler(catch_no_mitshm);
        d->shminfo = g_new0(XShmSegmentInfo, 1);
        d->ximage = XShmCreateImage(d->dpy, d->vi->visual, d->vi->depth,
                                    ZPixmap, d->data, d->shminfo, d->width, d->height);
        if (d->ximage == NULL)
            goto shm_fail;
        d->shminfo->shmaddr = d->data;
        d->shminfo->shmid = d->shmid;
        d->shminfo->readOnly = false;
        XShmAttach(d->dpy, d->shminfo);
        XSync(d->dpy, False);
        shmctl(d->shmid, IPC_RMID, 0);
        if (no_mitshm)
            goto shm_fail;
        XSetErrorHandler(old_handler);
        return 0;
    }

 shm_fail:
    d->have_mitshm = false;
    g_free(d->shminfo);
    d->shminfo = NULL;
    if (old_handler)
        XSetErrorHandler(old_handler);
 xcreate:
    d->ximage = XCreateImage(d->dpy, d->vi->visual, d->vi->depth, ZPixmap, 0,
                             d->data, d->width, d->height, 32, d->stride);
    return 0;
}

G_GNUC_INTERNAL
void spicex_image_destroy(SpiceDisplay *display)
{
    SpiceDisplayPrivate *d = display->priv;

    if (d->ximage) {
        /* avoid XDestroy to free shared memory, owned and freed by
           channel-display itself */
        if (d->ximage->data == d->data_origin)
            d->ximage->data = NULL;
        XDestroyImage(d->ximage);
        d->ximage = NULL;
        if (d->convert)
            d->data = 0;
    }
    if (d->shminfo) {
        XShmDetach(d->dpy, d->shminfo);
        free(d->shminfo);
        d->shminfo = NULL;
    }
    if (d->gc) {
        XFreeGC(d->dpy, d->gc);
        d->gc = NULL;
    }
    if (d->convert && d->data) {
        g_free(d->data);
        d->data = NULL;
    }
}

G_GNUC_INTERNAL
void spicex_expose_event(SpiceDisplay *display, GdkEventExpose *expose)
{
    GdkDrawable *window = gtk_widget_get_window(GTK_WIDGET(display));
    SpiceDisplayPrivate *d = display->priv;
    int x, y, w, h;

    spice_display_get_scaling(display, NULL, &x, &y, &w, &h);

    if (expose->area.x >= x &&
        expose->area.y >= y &&
        expose->area.x + expose->area.width  <= x + w &&
        expose->area.y + expose->area.height <= y + h) {
        /* area is completely inside the guest screen -- blit it */
        if (d->have_mitshm && d->shminfo) {
            XShmPutImage(d->dpy, gdk_x11_drawable_get_xid(window),
                         d->gc, d->ximage,
                         d->area.x + expose->area.x - x, d->area.y + expose->area.y - y,
                         expose->area.x, expose->area.y,
                         expose->area.width, expose->area.height,
                         true);
        } else {
            XPutImage(d->dpy, gdk_x11_drawable_get_xid(window),
                      d->gc, d->ximage,
                      d->area.x + expose->area.x - x, d->area.y + expose->area.y - y,
                      expose->area.x, expose->area.y,
                      expose->area.width, expose->area.height);
        }
    } else {
        /* complete window update */
        if (d->ww > d->area.width || d->wh > d->area.height) {
            int x1 = x;
            int x2 = x + w;
            int y1 = y;
            int y2 = y + h;
            XFillRectangle(d->dpy, gdk_x11_drawable_get_xid(window),
                           d->gc, 0, 0, x1, d->wh);
            XFillRectangle(d->dpy, gdk_x11_drawable_get_xid(window),
                           d->gc, x2, 0, d->ww - x2, d->wh);
            XFillRectangle(d->dpy, gdk_x11_drawable_get_xid(window),
                           d->gc, 0, 0, d->ww, y1);
            XFillRectangle(d->dpy, gdk_x11_drawable_get_xid(window),
                           d->gc, 0, y2, d->ww, d->wh - y2);
        }
        if (d->have_mitshm && d->shminfo) {
            XShmPutImage(d->dpy, gdk_x11_drawable_get_xid(window),
                         d->gc, d->ximage,
                         d->area.x, d->area.y, x, y, w, h,
                         true);
        } else {
            XPutImage(d->dpy, gdk_x11_drawable_get_xid(window),
                      d->gc, d->ximage,
                      d->area.x, d->area.y, x, y, w, h);
        }
    }
}

G_GNUC_INTERNAL
gboolean spicex_is_scaled(SpiceDisplay *display)
{
    return FALSE; /* backend doesn't support scaling yet */
}
