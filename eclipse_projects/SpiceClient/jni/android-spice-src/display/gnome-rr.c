/* gnome-rr.c
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

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "../glib-compat.h"

#if defined(HAVE_X11)
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include "gnome-rr-x11.h"
#elif defined(HAVE_WINDOWS)
#include "gnome-rr-windows.h"
#else
#include "gnome-rr-generic.h"
#endif

#undef GNOME_DISABLE_DEPRECATED
#include "gnome-rr.h"
#include "gnome-rr-config.h"

#include "gnome-rr-private.h"

enum {
    SCREEN_PROP_0,
    SCREEN_PROP_GDK_SCREEN,
    SCREEN_PROP_LAST,
};

enum {
    SCREEN_CHANGED,
    SCREEN_SIGNAL_LAST,
};

static gint screen_signals[SCREEN_SIGNAL_LAST];
static GParamSpec *screen_properties[SCREEN_PROP_LAST];

/* GnomeRRCrtc */
static GnomeRRCrtc *  crtc_copy         (const GnomeRRCrtc  *from);
static void           crtc_free         (GnomeRRCrtc        *crtc);

static GnomeRROutput *output_copy       (const GnomeRROutput *from);
static void           output_free       (GnomeRROutput      *output);

static GnomeRRMode *  mode_copy         (const GnomeRRMode  *from);
static void           mode_free         (GnomeRRMode        *mode);

static void gnome_rr_screen_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void gnome_rr_screen_get_property (GObject*, guint, GValue*, GParamSpec*);
static void gnome_rr_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GnomeRRScreen, gnome_rr_screen, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gnome_rr_screen_initable_iface_init))

G_DEFINE_BOXED_TYPE (GnomeRRCrtc, gnome_rr_crtc, crtc_copy, crtc_free)
G_DEFINE_BOXED_TYPE (GnomeRROutput, gnome_rr_output, output_copy, output_free)
G_DEFINE_BOXED_TYPE (GnomeRRMode, gnome_rr_mode, mode_copy, mode_free)

/* Errors */

/**
 * gnome_rr_error_quark:
 *
 * Returns the #GQuark that will be used for #GError values returned by the
 * GnomeRR API.
 *
 * Return value: a #GQuark used to identify errors coming from the GnomeRR API.
 */
GQuark
gnome_rr_error_quark (void)
{
    return g_quark_from_static_string ("gnome-rr-error-quark");
}

/* Screen */
GnomeRROutput *
gnome_rr_output_by_id (ScreenInfo *info, RROutput id)
{
    GnomeRROutput **output;

    g_return_val_if_fail (info != NULL, NULL);

    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }

    return NULL;
}

GnomeRRCrtc *
crtc_by_id (ScreenInfo *info, RRCrtc id)
{
    GnomeRRCrtc **crtc;

    if (!info)
        return NULL;

    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }

    return NULL;
}

GnomeRRMode *
mode_by_id (ScreenInfo *info, RRMode id)
{
    GnomeRRMode **mode;

    g_return_val_if_fail (info != NULL, NULL);

    for (mode = info->modes; *mode; ++mode)
    {
	if ((*mode)->id == id)
	    return *mode;
    }

    return NULL;
}

void
screen_info_free (ScreenInfo *info)
{
    GnomeRROutput **output;
    GnomeRRCrtc **crtc;
    GnomeRRMode **mode;

    g_return_if_fail (info != NULL);

#ifdef HAVE_RANDR
    if (info->resources)
    {
	XRRFreeScreenResources (info->resources);

	info->resources = NULL;
    }
#endif

    if (info->outputs)
    {
	for (output = info->outputs; *output; ++output)
	    output_free (*output);
	g_free (info->outputs);
    }

    if (info->crtcs)
    {
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_free (*crtc);
	g_free (info->crtcs);
    }

    if (info->modes)
    {
	for (mode = info->modes; *mode; ++mode)
	    mode_free (*mode);
	g_free (info->modes);
    }

    if (info->clone_modes)
    {
	/* The modes themselves were freed above */
	g_free (info->clone_modes);
    }

    g_free (info);
}

gboolean
has_similar_mode (GnomeRROutput *output, GnomeRRMode *mode)
{
    int i;
    GnomeRRMode **modes = gnome_rr_output_list_modes (output);
    int width = gnome_rr_mode_get_width (mode);
    int height = gnome_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	GnomeRRMode *m = modes[i];

	if (gnome_rr_mode_get_width (m) == width	&&
	    gnome_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

void
gather_clone_modes (ScreenInfo *info)
{
    int i;
    GPtrArray *result = g_ptr_array_new ();

    for (i = 0; info->outputs[i] != NULL; ++i)
    {
	int j;
	GnomeRROutput *output1, *output2;

	output1 = info->outputs[i];

	if (!output1->connected)
	    continue;

	for (j = 0; output1->modes[j] != NULL; ++j)
	{
	    GnomeRRMode *mode = output1->modes[j];
	    gboolean valid;
	    int k;

	    valid = TRUE;
	    for (k = 0; info->outputs[k] != NULL; ++k)
	    {
		output2 = info->outputs[k];

		if (!output2->connected)
		    continue;

		if (!has_similar_mode (output2, mode))
		{
		    valid = FALSE;
		    break;
		}
	    }

	    if (valid)
		g_ptr_array_add (result, mode);
	}
    }

    g_ptr_array_add (result, NULL);

    info->clone_modes = (GnomeRRMode **)g_ptr_array_free (result, FALSE);
}

ScreenInfo *
screen_info_new (GnomeRRScreen *screen, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);

    g_return_val_if_fail (screen != NULL, NULL);

    info->outputs = NULL;
    info->crtcs = NULL;
    info->modes = NULL;
    info->screen = screen;

    if (fill_out_screen_info (screen, info, needs_reprobe, error))
    {
	return info;
    }
    else
    {
	screen_info_free (info);
	return NULL;
    }
}

gboolean
screen_update (GnomeRRScreen *screen, gboolean force_callback, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info;
    gboolean changed = FALSE;

    g_return_val_if_fail (screen != NULL, FALSE);

    info = screen_info_new (screen, needs_reprobe, error);
    if (!info)
	    return FALSE;

#ifdef HAVE_RANDR
    if (info->resources->configTimestamp != screen->priv->info->resources->configTimestamp)
	    changed = TRUE;
#endif

    screen_info_free (screen->priv->info);

    screen->priv->info = info;

    if (changed || force_callback)
        g_signal_emit (G_OBJECT (screen), screen_signals[SCREEN_CHANGED], 0);

    return changed;
}

static gboolean
gnome_rr_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (initable);
    GnomeRRScreenPrivate *priv = self->priv;

    priv->info = screen_info_new (self, TRUE, error);
    g_return_val_if_fail (priv->info != NULL, FALSE);

    return TRUE;
}

static void
gnome_rr_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_rr_screen_initable_init;
}

static void
gnome_rr_screen_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (gobject);
    GnomeRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        priv->gdk_screen = g_value_get_object (value);
        g_object_notify (gobject, "gdk-screen");
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

static void
gnome_rr_screen_get_property (GObject *gobject, guint property_id, GValue *value, GParamSpec *property)
{
    GnomeRRScreen *self = GNOME_RR_SCREEN (gobject);

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        g_value_set_object (value, gnome_rr_screen_get_gdk_screen (self));
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

static void
gnome_rr_screen_finalize (GObject *gobject)
{
    GnomeRRScreen *screen = GNOME_RR_SCREEN (gobject);

    if (screen->priv->info)
        screen_info_free (screen->priv->info);

    G_OBJECT_CLASS (gnome_rr_screen_parent_class)->finalize (gobject);
}

void
gnome_rr_screen_class_init (GnomeRRScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    g_type_class_add_private (klass, sizeof (GnomeRRScreenPrivate));

    gobject_class->set_property = gnome_rr_screen_set_property;
    gobject_class->get_property = gnome_rr_screen_get_property;
    gobject_class->finalize = gnome_rr_screen_finalize;

    screen_properties[SCREEN_PROP_GDK_SCREEN] = g_param_spec_object (
            "gdk-screen",
            "GDK Screen",
            "The GDK Screen represented by this GnomeRRScreen",
            GDK_TYPE_SCREEN,
            G_PARAM_READWRITE |
            G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS);

    g_object_class_install_property(
            gobject_class,
            SCREEN_PROP_GDK_SCREEN,
            screen_properties[SCREEN_PROP_GDK_SCREEN]);

    screen_signals[SCREEN_CHANGED] = g_signal_new("changed",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GnomeRRScreenClass, changed),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
	    0);
}

void
gnome_rr_screen_init (GnomeRRScreen *self)
{
    GnomeRRScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_RR_SCREEN, GnomeRRScreenPrivate);
    self->priv = priv;

    priv->gdk_screen = NULL;
    priv->info = NULL;
}

/**
 * gnome_rr_screen_new:
 * Creates a new #GnomeRRScreen instance
 *
 * @screen: the #GdkScreen on which to operate
 * @error: will be set if screen could not be created
 *
 * Returns: a new #GnomeRRScreen instance or NULL if screen could not be created,
 * for instance if the driver does not support Xrandr 1.2
 */
GnomeRRScreen *
gnome_rr_screen_new (GdkScreen *screen,
		     GError **error)
{
    /* FIXME: _gnome_desktop_init_i18n (); */
    return g_initable_new (
#if defined(HAVE_X11)
                           GNOME_TYPE_RR_X11_SCREEN,
#elif defined(HAVE_WINDOWS)
                           GNOME_TYPE_RR_WINDOWS_SCREEN,
#else
                           GNOME_TYPE_RR_GENERIC_SCREEN,
#endif
                           NULL, error, "gdk-screen", screen, NULL);
}

/**
 * gnome_rr_screen_get_ranges:
 *
 * Get the ranges of the screen
 * @screen: a #GnomeRRScreen
 * @min_width: (out): the minimum width
 * @max_width: (out): the maximum width
 * @min_height: (out): the minimum height
 * @max_height: (out): the maximum height
 */
void
gnome_rr_screen_get_ranges (GnomeRRScreen *screen,
			    int	          *min_width,
			    int	          *max_width,
			    int           *min_height,
			    int	          *max_height)
{
    GnomeRRScreenPrivate *priv;

    g_return_if_fail (GNOME_IS_RR_SCREEN (screen));

    priv = screen->priv;

    if (min_width)
	*min_width = priv->info->min_width;

    if (max_width)
	*max_width = priv->info->max_width;

    if (min_height)
	*min_height = priv->info->min_height;

    if (max_height)
	*max_height = priv->info->max_height;
}

/**
 * gnome_rr_screen_get_timestamps:
 * @screen: a #GnomeRRScreen
 * @change_timestamp_ret: (out): Location in which to store the timestamp at which the RANDR configuration was last changed
 * @config_timestamp_ret: (out): Location in which to store the timestamp at which the RANDR configuration was last obtained
 *
 * Queries the two timestamps that the X RANDR extension maintains.  The X
 * server will prevent change requests for stale configurations, those whose
 * timestamp is not equal to that of the latest request for configuration.  The
 * X server will also prevent change requests that have an older timestamp to
 * the latest change request.
 */
void
gnome_rr_screen_get_timestamps (GnomeRRScreen *screen,
				guint32       *change_timestamp_ret,
				guint32       *config_timestamp_ret)
{
    GnomeRRScreenPrivate *priv G_GNUC_UNUSED;

    g_return_if_fail (GNOME_IS_RR_SCREEN (screen));

    priv = screen->priv;

#ifdef HAVE_RANDR
    if (change_timestamp_ret)
	*change_timestamp_ret = priv->info->resources->timestamp;

    if (config_timestamp_ret)
	*config_timestamp_ret = priv->info->resources->configTimestamp;
#endif
}

/**
 * gnome_rr_screen_refresh:
 * @screen: a #GnomeRRScreen
 * @error: location to store error, or %NULL
 *
 * Refreshes the screen configuration, and calls the screen's callback if it
 * exists and if the screen's configuration changed.
 *
 * Return value: TRUE if the screen's configuration changed; otherwise, the
 * function returns FALSE and a NULL error if the configuration didn't change,
 * or FALSE and a non-NULL error if there was an error while refreshing the
 * configuration.
 */
gboolean
gnome_rr_screen_refresh (GnomeRRScreen *screen,
			 GError       **error)
{
    gboolean refreshed;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#ifdef HAVE_X11
    gdk_x11_display_grab (gdk_screen_get_display (screen->priv->gdk_screen));
#endif

    refreshed = screen_update (screen, FALSE, TRUE, error);

#ifdef HAVE_X11
    gnome_rr_x11_screen_force_timestamp_update (GNOME_RR_X11_SCREEN (screen)); /* this is to keep other clients from thinking that the X server re-detected things by itself - bgo#621046 */
    gdk_x11_display_ungrab (gdk_screen_get_display (screen->priv->gdk_screen));
#endif

    return refreshed;
}

/**
 * gnome_rr_screen_list_modes:
 *
 * List available XRandR modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRMode **
gnome_rr_screen_list_modes (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->modes;
}

/**
 * gnome_rr_screen_list_clone_modes:
 *
 * List available XRandR clone modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRMode **
gnome_rr_screen_list_clone_modes   (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->clone_modes;
}

/**
 * gnome_rr_screen_list_crtcs:
 *
 * List all CRTCs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRRCrtc **
gnome_rr_screen_list_crtcs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->crtcs;
}

/**
 * gnome_rr_screen_list_outputs:
 *
 * List all outputs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GnomeRROutput **
gnome_rr_screen_list_outputs (GnomeRRScreen *screen)
{
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->outputs;
}

/**
 * gnome_rr_screen_get_crtc_by_id:
 *
 * Returns: (transfer none): the CRTC identified by @id
 */
GnomeRRCrtc *
gnome_rr_screen_get_crtc_by_id (GnomeRRScreen *screen,
				guint32        id)
{
    GnomeRRCrtc **crtcs;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    crtcs = screen->priv->info->crtcs;

    for (i = 0; crtcs[i] != NULL; ++i)
    {
	if (crtcs[i]->id == id)
	    return crtcs[i];
    }

    return NULL;
}

/**
 * gnome_rr_screen_get_output_by_id:
 *
 * Returns: (transfer none): the output identified by @id
 */
GnomeRROutput *
gnome_rr_screen_get_output_by_id (GnomeRRScreen *screen,
				  guint32        id)
{
    GnomeRROutput **outputs;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    outputs = screen->priv->info->outputs;

    for (i = 0; outputs[i] != NULL; ++i)
    {
	if (outputs[i]->id == id)
	    return outputs[i];
    }

    return NULL;
}

/* GnomeRROutput */
GnomeRROutput *
output_new (ScreenInfo *info, RROutput id)
{
    GnomeRROutput *output = g_slice_new0 (GnomeRROutput);

    output->id = id;
    output->info = info;

    return output;
}

static GnomeRROutput*
output_copy (const GnomeRROutput *from)
{
    GPtrArray *array;
    GnomeRRCrtc **p_crtc;
    GnomeRROutput **p_output;
    GnomeRRMode **p_mode;
    GnomeRROutput *output = g_slice_new0 (GnomeRROutput);

    output->id = from->id;
    output->info = from->info;
    output->name = g_strdup (from->name);
    output->current_crtc = from->current_crtc;
    output->width_mm = from->width_mm;
    output->height_mm = from->height_mm;
    output->connected = from->connected;
    output->n_preferred = from->n_preferred;
    output->connector_type = g_strdup (from->connector_type);

    array = g_ptr_array_new ();
    for (p_crtc = from->possible_crtcs; *p_crtc != NULL; p_crtc++)
    {
        g_ptr_array_add (array, *p_crtc);
    }
    output->possible_crtcs = (GnomeRRCrtc**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->clones; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    output->clones = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_mode = from->modes; *p_mode != NULL; p_mode++)
    {
        g_ptr_array_add (array, *p_mode);
    }
    output->modes = (GnomeRRMode**) g_ptr_array_free (array, FALSE);

    output->edid_size = from->edid_size;
    output->edid_data = g_memdup (from->edid_data, from->edid_size);

    return output;
}

static void
output_free (GnomeRROutput *output)
{
    g_free (output->clones);
    g_free (output->modes);
    g_free (output->possible_crtcs);
    g_free (output->edid_data);
    g_free (output->name);
    g_free (output->connector_type);
    g_slice_free (GnomeRROutput, output);
}

guint32
gnome_rr_output_get_id (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, 0);

    return output->id;
}

const guint8 *
gnome_rr_output_get_edid_data (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->edid_data;
}

/**
 * gnome_rr_screen_get_output_by_id:
 *
 * Returns: (transfer none): the output identified by @name
 */
GnomeRROutput *
gnome_rr_screen_get_output_by_name (GnomeRRScreen *screen,
				    const char    *name)
{
    int i;

    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    for (i = 0; screen->priv->info->outputs[i] != NULL; ++i)
    {
	GnomeRROutput *output = screen->priv->info->outputs[i];

	if (strcmp (output->name, name) == 0)
	    return output;
    }

    return NULL;
}

GnomeRRCrtc *
gnome_rr_output_get_crtc (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->current_crtc;
}

/* Returns NULL if the ConnectorType property is not available */
const char *
gnome_rr_output_get_connector_type (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->connector_type;
}

gboolean
gnome_rr_output_is_laptop (GnomeRROutput *output)
{
    const char *connector_type;

    g_return_val_if_fail (output != NULL, FALSE);

    if (!output->connected)
	return FALSE;

    /* The ConnectorType property is present in RANDR 1.3 and greater */

    connector_type = gnome_rr_output_get_connector_type (output);
    if (connector_type && strcmp (connector_type, GNOME_RR_CONNECTOR_TYPE_PANEL) == 0)
	return TRUE;

    /* Older versions of RANDR - this is a best guess, as @#$% RANDR doesn't have standard output names,
     * so drivers can use whatever they like.
     */

    if (output->name
	&& (strstr (output->name, "lvds") ||  /* Most drivers use an "LVDS" prefix... */
	    strstr (output->name, "LVDS") ||
	    strstr (output->name, "Lvds") ||
	    strstr (output->name, "LCD")))    /* ... but fglrx uses "LCD" in some versions.  Shoot me now, kthxbye. */
	return TRUE;

    return FALSE;
}

GnomeRRMode *
gnome_rr_output_get_current_mode (GnomeRROutput *output)
{
    GnomeRRCrtc *crtc;

    g_return_val_if_fail (output != NULL, NULL);

    if ((crtc = gnome_rr_output_get_crtc (output)))
	return gnome_rr_crtc_get_current_mode (crtc);

    return NULL;
}

void
gnome_rr_output_get_position (GnomeRROutput   *output,
			      int             *x,
			      int             *y)
{
    GnomeRRCrtc *crtc;

    g_return_if_fail (output != NULL);

    if ((crtc = gnome_rr_output_get_crtc (output)))
	gnome_rr_crtc_get_position (crtc, x, y);
}

const char *
gnome_rr_output_get_name (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, "");
    return output->name;
}

int
gnome_rr_output_get_width_mm (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);
    return output->width_mm;
}

int
gnome_rr_output_get_height_mm (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);
    return output->height_mm;
}

GnomeRRMode *
gnome_rr_output_get_preferred_mode (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    if (output->n_preferred)
	return output->modes[0];

    return NULL;
}

GnomeRRMode **
gnome_rr_output_list_modes (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

gboolean
gnome_rr_output_is_connected (GnomeRROutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);
    return output->connected;
}

gboolean
gnome_rr_output_supports_mode (GnomeRROutput *output,
			       GnomeRRMode   *mode)
{
    int i;

    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);

    for (i = 0; output->modes[i] != NULL; ++i)
    {
	if (output->modes[i] == mode)
	    return TRUE;
    }

    return FALSE;
}

gboolean
gnome_rr_output_can_clone (GnomeRROutput *output,
			   GnomeRROutput *clone)
{
    int i;

    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (clone != NULL, FALSE);

    for (i = 0; output->clones[i] != NULL; ++i)
    {
	if (output->clones[i] == clone)
	    return TRUE;
    }

    return FALSE;
}

gboolean
gnome_rr_output_get_is_primary (GnomeRROutput *output)
{
#ifdef HAVE_RANDR
    return output->info->primary == output->id;
#else
    return FALSE;
#endif
}

void
gnome_rr_screen_set_primary_output (GnomeRRScreen *screen,
                                    GnomeRROutput *output)
{
    g_return_if_fail (GNOME_IS_RR_SCREEN (screen));

    screen_set_primary_output (screen, output);
}

#ifndef GNOME_DISABLE_DEPRECATED_SOURCE
gboolean
gnome_rr_crtc_set_config (GnomeRRCrtc      *crtc,
			  int               x,
			  int               y,
			  GnomeRRMode      *mode,
			  GnomeRRRotation   rotation,
			  GnomeRROutput   **outputs,
			  int               n_outputs,
			  GError          **error)
{
    return gnome_rr_crtc_set_config_with_time (crtc, GDK_CURRENT_TIME, x, y, mode, rotation, outputs, n_outputs, error);
}
#endif

GnomeRRMode *
gnome_rr_crtc_get_current_mode (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);

    return crtc->current_mode;
}

guint32
gnome_rr_crtc_get_id (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, 0);

    return crtc->id;
}

gboolean
gnome_rr_crtc_can_drive_output (GnomeRRCrtc   *crtc,
				GnomeRROutput *output)
{
    int i;

    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (output != NULL, FALSE);

    for (i = 0; crtc->possible_outputs[i] != NULL; ++i)
    {
	if (crtc->possible_outputs[i] == output)
	    return TRUE;
    }

    return FALSE;
}

/* FIXME: merge with get_mode()? */
void
gnome_rr_crtc_get_position (GnomeRRCrtc *crtc,
			    int         *x,
			    int         *y)
{
    g_return_if_fail (crtc != NULL);

    if (x)
	*x = crtc->x;

    if (y)
	*y = crtc->y;
}

/* FIXME: merge with get_mode()? */
GnomeRRRotation
gnome_rr_crtc_get_current_rotation (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, GNOME_RR_ROTATION_0);
    return crtc->current_rotation;
}

GnomeRRRotation
gnome_rr_crtc_get_rotations (GnomeRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, GNOME_RR_ROTATION_0);
    return crtc->rotations;
}

gboolean
gnome_rr_crtc_supports_rotation (GnomeRRCrtc *   crtc,
				 GnomeRRRotation rotation)
{
    g_return_val_if_fail (crtc != NULL, FALSE);
    return (crtc->rotations & rotation);
}

GnomeRRCrtc *
crtc_new (ScreenInfo *info, RROutput id)
{
    GnomeRRCrtc *crtc = g_slice_new0 (GnomeRRCrtc);

    crtc->id = id;
    crtc->info = info;

    return crtc;
}

static GnomeRRCrtc *
crtc_copy (const GnomeRRCrtc *from)
{
    GnomeRROutput **p_output;
    GPtrArray *array;
    GnomeRRCrtc *to = g_slice_new0 (GnomeRRCrtc);

    to->info = from->info;
    to->id = from->id;
    to->current_mode = from->current_mode;
    to->x = from->x;
    to->y = from->y;
    to->current_rotation = from->current_rotation;
    to->rotations = from->rotations;
    to->gamma_size = from->gamma_size;

    array = g_ptr_array_new ();
    for (p_output = from->current_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->current_outputs = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->possible_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->possible_outputs = (GnomeRROutput**) g_ptr_array_free (array, FALSE);

    return to;
}

static void
crtc_free (GnomeRRCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_slice_free (GnomeRRCrtc, crtc);
}

/* GnomeRRMode */
GnomeRRMode *
mode_new (ScreenInfo *info, RRMode id)
{
    GnomeRRMode *mode = g_slice_new0 (GnomeRRMode);

    mode->id = id;
    mode->info = info;

    return mode;
}

guint32
gnome_rr_mode_get_id (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->id;
}

guint
gnome_rr_mode_get_width (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

int
gnome_rr_mode_get_freq (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->freq) / 1000;
}

guint
gnome_rr_mode_get_height (GnomeRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

static GnomeRRMode *
mode_copy (const GnomeRRMode *from)
{
    GnomeRRMode *to = g_slice_new0 (GnomeRRMode);

    to->id = from->id;
    to->info = from->info;
    to->name = g_strdup (from->name);
    to->width = from->width;
    to->height = from->height;
    to->freq = from->freq;

    return to;
}

static void
mode_free (GnomeRRMode *mode)
{
    g_free (mode->name);
    g_slice_free (GnomeRRMode, mode);
}

GdkScreen *
gnome_rr_screen_get_gdk_screen (GnomeRRScreen *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return self->priv->gdk_screen;
}
