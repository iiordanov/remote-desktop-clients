/* gnome-rr.h
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */
#ifndef GNOME_RR_H
#define GNOME_RR_H

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#error    GnomeRR is unstable API. You must define GNOME_DESKTOP_USE_UNSTABLE_API before including gnomerr.h
#endif

#include <glib.h>
#include <gdk/gdk.h>

typedef struct GnomeRRScreenPrivate GnomeRRScreenPrivate;
typedef struct GnomeRROutput GnomeRROutput;
typedef struct GnomeRRCrtc GnomeRRCrtc;
typedef struct GnomeRRMode GnomeRRMode;

typedef struct {
	GObject parent;

	GnomeRRScreenPrivate* priv;
} GnomeRRScreen;

typedef struct {
	GObjectClass parent_class;

        void (* changed) (void);
} GnomeRRScreenClass;

typedef enum
{
    GNOME_RR_ROTATION_0 =	(1 << 0),
    GNOME_RR_ROTATION_90 =	(1 << 1),
    GNOME_RR_ROTATION_180 =	(1 << 2),
    GNOME_RR_ROTATION_270 =	(1 << 3),
    GNOME_RR_REFLECT_X =	(1 << 4),
    GNOME_RR_REFLECT_Y =	(1 << 5)
} GnomeRRRotation;

/* Error codes */

#define GNOME_RR_ERROR (gnome_rr_error_quark ())

GQuark gnome_rr_error_quark (void);

typedef enum {
    GNOME_RR_ERROR_UNKNOWN,		/* generic "fail" */
    GNOME_RR_ERROR_NO_RANDR_EXTENSION,	/* RANDR extension is not present */
    GNOME_RR_ERROR_RANDR_ERROR,		/* generic/undescribed error from the underlying XRR API */
    GNOME_RR_ERROR_BOUNDS_ERROR,	/* requested bounds of a CRTC are outside the maximum size */
    GNOME_RR_ERROR_CRTC_ASSIGNMENT,	/* could not assign CRTCs to outputs */
    GNOME_RR_ERROR_NO_MATCHING_CONFIG,	/* none of the saved configurations matched the current configuration */
} GnomeRRError;

#define GNOME_RR_CONNECTOR_TYPE_PANEL "Panel"  /* This is a laptop's built-in LCD */

#define GNOME_TYPE_RR_SCREEN                  (gnome_rr_screen_get_type())
#define GNOME_RR_SCREEN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_SCREEN, GnomeRRScreen))
#define GNOME_IS_RR_SCREEN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_SCREEN))
#define GNOME_RR_SCREEN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_RR_SCREEN, GnomeRRScreenClass))
#define GNOME_IS_RR_SCREEN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_RR_SCREEN))
#define GNOME_RR_SCREEN_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_RR_SCREEN, GnomeRRScreenClass))

#define GNOME_TYPE_RR_OUTPUT (gnome_rr_output_get_type())
#define GNOME_TYPE_RR_CRTC   (gnome_rr_crtc_get_type())
#define GNOME_TYPE_RR_MODE   (gnome_rr_mode_get_type())

GType gnome_rr_screen_get_type (void);
GType gnome_rr_output_get_type (void);
GType gnome_rr_crtc_get_type (void);
GType gnome_rr_mode_get_type (void);

/* GnomeRRScreen */
GnomeRRScreen * gnome_rr_screen_new                (GdkScreen             *screen,
						    GError               **error);
GnomeRROutput **gnome_rr_screen_list_outputs       (GnomeRRScreen         *screen);
GnomeRRCrtc **  gnome_rr_screen_list_crtcs         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_modes         (GnomeRRScreen         *screen);
GnomeRRMode **  gnome_rr_screen_list_clone_modes   (GnomeRRScreen	  *screen);
void            gnome_rr_screen_set_size           (GnomeRRScreen         *screen,
						    int                    width,
						    int                    height,
						    int                    mm_width,
						    int                    mm_height);
GnomeRRCrtc *   gnome_rr_screen_get_crtc_by_id     (GnomeRRScreen         *screen,
						    guint32                id);
gboolean        gnome_rr_screen_refresh            (GnomeRRScreen         *screen,
						    GError               **error);
GnomeRROutput * gnome_rr_screen_get_output_by_id   (GnomeRRScreen         *screen,
						    guint32                id);
GnomeRROutput * gnome_rr_screen_get_output_by_name (GnomeRRScreen         *screen,
						    const char            *name);
void            gnome_rr_screen_get_ranges         (GnomeRRScreen         *screen,
						    int                   *min_width,
						    int                   *max_width,
						    int                   *min_height,
						    int                   *max_height);
void            gnome_rr_screen_get_timestamps     (GnomeRRScreen         *screen,
						    guint32               *change_timestamp_ret,
						    guint32               *config_timestamp_ret);

void            gnome_rr_screen_set_primary_output (GnomeRRScreen         *screen,
                                                    GnomeRROutput         *output);

GnomeRRMode   **gnome_rr_screen_create_clone_modes (GnomeRRScreen *screen);

/* GnomeRROutput */
guint32         gnome_rr_output_get_id             (GnomeRROutput         *output);
const char *    gnome_rr_output_get_name           (GnomeRROutput         *output);
gboolean        gnome_rr_output_is_connected       (GnomeRROutput         *output);
int             gnome_rr_output_get_size_inches    (GnomeRROutput         *output);
int             gnome_rr_output_get_width_mm       (GnomeRROutput         *outout);
int             gnome_rr_output_get_height_mm      (GnomeRROutput         *output);
const guint8 *  gnome_rr_output_get_edid_data      (GnomeRROutput         *output);
GnomeRRCrtc **  gnome_rr_output_get_possible_crtcs (GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_output_get_current_mode   (GnomeRROutput         *output);
GnomeRRCrtc *   gnome_rr_output_get_crtc           (GnomeRROutput         *output);
const char *    gnome_rr_output_get_connector_type (GnomeRROutput         *output);
gboolean        gnome_rr_output_is_laptop          (GnomeRROutput         *output);
void            gnome_rr_output_get_position       (GnomeRROutput         *output,
						    int                   *x,
						    int                   *y);
gboolean        gnome_rr_output_can_clone          (GnomeRROutput         *output,
						    GnomeRROutput         *clone);
GnomeRRMode **  gnome_rr_output_list_modes         (GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_output_get_preferred_mode (GnomeRROutput         *output);
gboolean        gnome_rr_output_supports_mode      (GnomeRROutput         *output,
						    GnomeRRMode           *mode);
gboolean        gnome_rr_output_get_is_primary     (GnomeRROutput         *output);

/* GnomeRRMode */
guint32         gnome_rr_mode_get_id               (GnomeRRMode           *mode);
guint           gnome_rr_mode_get_width            (GnomeRRMode           *mode);
guint           gnome_rr_mode_get_height           (GnomeRRMode           *mode);
int             gnome_rr_mode_get_freq             (GnomeRRMode           *mode);

/* GnomeRRCrtc */
guint32         gnome_rr_crtc_get_id               (GnomeRRCrtc           *crtc);

#ifndef GNOME_DISABLE_DEPRECATED
gboolean        gnome_rr_crtc_set_config           (GnomeRRCrtc           *crtc,
						    int                    x,
						    int                    y,
						    GnomeRRMode           *mode,
						    GnomeRRRotation        rotation,
						    GnomeRROutput        **outputs,
						    int                    n_outputs,
						    GError               **error);
#endif

gboolean        gnome_rr_crtc_set_config_with_time (GnomeRRCrtc           *crtc,
						    guint32                timestamp,
						    int                    x,
						    int                    y,
						    GnomeRRMode           *mode,
						    GnomeRRRotation        rotation,
						    GnomeRROutput        **outputs,
						    int                    n_outputs,
						    GError               **error);
gboolean        gnome_rr_crtc_can_drive_output     (GnomeRRCrtc           *crtc,
						    GnomeRROutput         *output);
GnomeRRMode *   gnome_rr_crtc_get_current_mode     (GnomeRRCrtc           *crtc);
void            gnome_rr_crtc_get_position         (GnomeRRCrtc           *crtc,
						    int                   *x,
						    int                   *y);
GnomeRRRotation gnome_rr_crtc_get_current_rotation (GnomeRRCrtc           *crtc);
GnomeRRRotation gnome_rr_crtc_get_rotations        (GnomeRRCrtc           *crtc);
gboolean        gnome_rr_crtc_supports_rotation    (GnomeRRCrtc           *crtc,
						    GnomeRRRotation        rotation);

gboolean        gnome_rr_crtc_get_gamma            (GnomeRRCrtc           *crtc,
						    int                   *size,
						    unsigned short       **red,
						    unsigned short       **green,
						    unsigned short       **blue);
void            gnome_rr_crtc_set_gamma            (GnomeRRCrtc           *crtc,
						    int                    size,
						    unsigned short        *red,
						    unsigned short        *green,
						    unsigned short        *blue);
#endif /* GNOME_RR_H */
