/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* gnome-rr.c
 *
 * Copyright 2011, Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gtk/gtk.h>

#undef GNOME_DISABLE_DEPRECATED
#include "gnome-rr.h"
#include "gnome-rr-config.h"
#include "gnome-rr-private.h"
#include "gnome-rr-generic.h"

struct GnomeRRGenericScreenPrivate
{
    RRMode rrmode_id;
};

static void gnome_rr_generic_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GnomeRRGenericScreen, gnome_rr_generic_screen, GNOME_TYPE_RR_SCREEN,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gnome_rr_generic_screen_initable_iface_init))

static gboolean
gnome_rr_generic_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GInitableIface *iface, *parent_iface;

    iface = G_TYPE_INSTANCE_GET_INTERFACE(initable, G_TYPE_INITABLE, GInitableIface);
    parent_iface = g_type_interface_peek_parent(iface);

    if (!parent_iface->init (initable, canc, error))
      return FALSE;

    return TRUE;
}

static void
gnome_rr_generic_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_rr_generic_screen_initable_init;
}

static void
gnome_rr_generic_screen_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (gnome_rr_generic_screen_parent_class)->finalize (gobject);
}

static void
gnome_rr_generic_screen_class_init (GnomeRRGenericScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GnomeRRGenericScreenPrivate));

    gobject_class->finalize = gnome_rr_generic_screen_finalize;
}

static void
gnome_rr_generic_screen_init (GnomeRRGenericScreen *self)
{
    GnomeRRGenericScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
        GNOME_TYPE_RR_GENERIC_SCREEN, GnomeRRGenericScreenPrivate);

    self->priv = priv;
}

gboolean
fill_out_screen_info (GnomeRRScreen *screen, ScreenInfo *info,
                      gboolean needs_reprobe, GError **error)
{
    GdkScreen *gdk_screen;
    GPtrArray *crtcs, *outputs, *modes;
    guint i;

    g_object_get(G_OBJECT(screen), "gdk-screen", &gdk_screen, NULL);

    crtcs = g_ptr_array_new ();
    outputs = g_ptr_array_new ();
    modes = g_ptr_array_new ();

    for (i = 0; i < gdk_screen_get_n_monitors(gdk_screen); i++) {
        GdkRectangle monitor_geometry;
        GnomeRRCrtc *crtc;
        GnomeRROutput *output;

        crtc = crtc_new(info, i);
        crtc->rotations = GNOME_RR_ROTATION_0;
        g_ptr_array_add(crtcs, crtc);

        gdk_screen_get_monitor_geometry(gdk_screen, i, &monitor_geometry);
        crtc->x = monitor_geometry.x;
        crtc->y = monitor_geometry.y;
        crtc->current_rotation = GNOME_RR_ROTATION_0;

        output = output_new(info, i);
        output->name = gdk_screen_get_monitor_plug_name(gdk_screen, i);
        output->current_crtc = crtc;
        output->possible_crtcs = g_new0 (GnomeRRCrtc*, 2);
        output->possible_crtcs[0] = crtc;
        output->connected = TRUE;
        output->width_mm = gdk_screen_get_monitor_width_mm(gdk_screen, i);
        output->height_mm = gdk_screen_get_monitor_height_mm(gdk_screen, i);

        crtc->current_mode = mode_new(info, i);
        crtc->current_mode->width = monitor_geometry.x;
        crtc->current_mode->height = monitor_geometry.y;
        output->modes = g_new0(GnomeRRMode*, 2);
        output->modes[0] = crtc->current_mode;
        g_ptr_array_add(modes, crtc->current_mode);

        crtc->current_outputs = g_new0(GnomeRROutput*, 2);
        crtc->current_outputs[0] = output;
        crtc->possible_outputs = g_new0(GnomeRROutput*, 2);
        crtc->possible_outputs[0] = output;
    }

    info->min_width = 0;
    info->min_height = 0;
    info->max_width = gdk_screen_get_width(gdk_screen);
    info->max_height = gdk_screen_get_height(gdk_screen);

    g_ptr_array_add (modes, NULL);
    info->modes = (GnomeRRMode **)g_ptr_array_free (modes, FALSE);

    g_ptr_array_add (crtcs, NULL);
    info->crtcs = (GnomeRRCrtc **)g_ptr_array_free (crtcs, FALSE);

    g_ptr_array_add (outputs, NULL);
    info->outputs = (GnomeRROutput **)g_ptr_array_free (outputs, FALSE);

    g_object_unref(G_OBJECT(gdk_screen));

    return TRUE;
}

gboolean
gnome_rr_crtc_set_config_with_time (GnomeRRCrtc      *crtc,
				    guint32           timestamp,
				    int               x,
				    int               y,
				    GnomeRRMode      *mode,
				    GnomeRRRotation   rotation,
				    GnomeRROutput   **outputs,
				    int               n_outputs,
				    GError          **error)
{
    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_UNKNOWN,
            /* Translators: a CRTC is a CRT Controller (this is X terminology). */
            _("could not set the output configuration"));

    return FALSE;
}

void
gnome_rr_screen_set_size (GnomeRRScreen *self,
			  int       width,
			  int       height,
			  int       mm_width,
			  int       mm_height)
{
    g_warning ("Unimplemented on MacOS X");
}

void
gnome_rr_crtc_set_gamma (GnomeRRCrtc *crtc, int size,
			 unsigned short *red,
			 unsigned short *green,
			 unsigned short *blue)
{
    g_return_if_fail (crtc != NULL);
    g_return_if_fail (red != NULL);
    g_return_if_fail (green != NULL);
    g_return_if_fail (blue != NULL);

    g_warning ("Unimplemented on MacOS X");
}

gboolean
gnome_rr_crtc_get_gamma (GnomeRRCrtc *crtc, int *size,
			 unsigned short **red, unsigned short **green,
			 unsigned short **blue)
{
    g_return_val_if_fail (crtc != NULL, FALSE);

    g_warning ("Unimplemented on MacOS X");
    return FALSE;
}

void
screen_set_primary_output (GnomeRRScreen *screen, GnomeRROutput *output)
{
    g_warning ("Unimplemented on MacOS X");
}
