/* gnome-rr-private.h
 *
 * Copyright 2007, 2008, Red Hat, Inc.
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
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#ifndef GNOME_RR_PRIVATE_H
#define GNOME_RR_PRIVATE_H

#include <config.h>
#include <gtk/gtk.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <winuser.h>
#endif

#ifndef HAVE_RANDR
/* This is to avoid a ton of ifdefs wherever we use a type from libXrandr */
typedef int RROutput;
typedef int RRCrtc;
typedef int RRMode;
typedef int Rotation;
#define RR_Rotate_0		1
#define RR_Rotate_90		2
#define RR_Rotate_180		4
#define RR_Rotate_270		8
#define RR_Reflect_X		16
#define RR_Reflect_Y		32
#endif

typedef struct ScreenInfo ScreenInfo;

struct ScreenInfo
{
    int			min_width;
    int			max_width;
    int			min_height;
    int			max_height;

    GnomeRROutput **	outputs;
    GnomeRRCrtc **	crtcs;
    GnomeRRMode **	modes;

    GnomeRRScreen *	screen;

    GnomeRRMode **	clone_modes;

#ifdef HAVE_RANDR
    XRRScreenResources *resources;
    RROutput            primary;
#endif
};

struct GnomeRRScreenPrivate
{
    GdkScreen *			gdk_screen;
    ScreenInfo *		info;
};

struct GnomeRROutputInfoPrivate
{
    char *		name;

    gboolean		on;
    int			width;
    int			height;
    int			rate;
    int			x;
    int			y;
    GnomeRRRotation	rotation;

    gboolean		connected;
    gchar		vendor[4];
    guint		product;
    guint		serial;
    double		aspect;
    int			pref_width;
    int			pref_height;
    char *		display_name;
    gboolean            primary;
};

struct GnomeRRConfigPrivate
{
  gboolean              clone;
  GnomeRRScreen *       screen;
  GnomeRROutputInfo **  outputs;
};

struct GnomeRROutput
{
    ScreenInfo *	info;
    RROutput		id;

    char *		name;
    GnomeRRCrtc *	current_crtc;
    gboolean		connected;
    gulong		width_mm;
    gulong		height_mm;
    GnomeRRCrtc **	possible_crtcs;
    GnomeRROutput **	clones;
    GnomeRRMode **	modes;
    int			n_preferred;
    guint8 *		edid_data;
    int                 edid_size;
    char *              connector_type;
};

struct GnomeRROutputWrap
{
    RROutput		id;
};

struct GnomeRRCrtc
{
    ScreenInfo *	info;
    RRCrtc		id;

    GnomeRRMode *	current_mode;
    GnomeRROutput **	current_outputs;
    GnomeRROutput **	possible_outputs;
    int			x;
    int			y;

    GnomeRRRotation	current_rotation;
    GnomeRRRotation	rotations;
    int			gamma_size;
};

struct GnomeRRMode
{
    ScreenInfo *	info;
    RRMode		id;
    char *		name;
    int			width;
    int			height;
    int			freq;		/* in mHz */
#ifdef WIN32
    DEVMODE             mode;
#endif
};

#if !GTK_CHECK_VERSION (2, 91, 0)
#define gdk_x11_window_get_xid  gdk_x11_drawable_get_xid
#define gdk_error_trap_pop_ignored gdk_error_trap_pop
#endif

G_GNUC_INTERNAL
GdkScreen *     gnome_rr_screen_get_gdk_screen          (GnomeRRScreen *self);
G_GNUC_INTERNAL
GnomeRROutput * gnome_rr_output_by_id                   (ScreenInfo *info, RROutput id);
G_GNUC_INTERNAL
GnomeRRCrtc *   crtc_by_id                              (ScreenInfo *info, RRCrtc id);
G_GNUC_INTERNAL
GnomeRRMode *   mode_by_id                              (ScreenInfo *info, RRMode id);

G_GNUC_INTERNAL
ScreenInfo *    screen_info_new                         (GnomeRRScreen *screen, gboolean needs_reprobe,
                                                         GError **error);
G_GNUC_INTERNAL
gboolean        screen_update                           (GnomeRRScreen *screen, gboolean force_callback,
                                                         gboolean needs_reprobe, GError **error);
G_GNUC_INTERNAL
gboolean        fill_out_screen_info                    (GnomeRRScreen *screen, ScreenInfo *info,
                                                         gboolean needs_reprobe, GError **error);
G_GNUC_INTERNAL
void            screen_set_primary_output               (GnomeRRScreen *screen, GnomeRROutput *output);
G_GNUC_INTERNAL
GnomeRRCrtc *   crtc_new                                (ScreenInfo *info, RRCrtc id);

/* GnomeRROutput */
G_GNUC_INTERNAL
GnomeRROutput * output_new                              (ScreenInfo *info, RROutput id);
G_GNUC_INTERNAL
GnomeRRMode *   mode_new                                (ScreenInfo *info, RRMode id);
G_GNUC_INTERNAL
void            screen_info_free                        (ScreenInfo *info);
G_GNUC_INTERNAL
void            gather_clone_modes                      (ScreenInfo *info);



#endif
