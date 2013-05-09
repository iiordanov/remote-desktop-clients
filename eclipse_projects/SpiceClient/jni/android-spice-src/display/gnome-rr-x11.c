/* gnome-rr-x11.c
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

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

#include <gtk/gtk.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#undef GNOME_DISABLE_DEPRECATED
#include "gnome-rr.h"
#include "gnome-rr-x11.h"
#include "gnome-rr-config.h"

#include "gnome-rr-private.h"

struct GnomeRRX11ScreenPrivate
{
    Display *			xdisplay;
    Screen *			xscreen;
    Window			xroot;

    int				randr_event_base;
    int				rr_major_version;
    int				rr_minor_version;
    Atom                        connector_type_atom;
};

#define DISPLAY(o) (GNOME_RR_X11_SCREEN((o)->info->screen)->priv->xdisplay)

#ifdef HAVE_RANDR
#define RANDR_LIBRARY_IS_AT_LEAST_1_3 (RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3))
#else
#define RANDR_LIBRARY_IS_AT_LEAST_1_3 0
#endif

#define SERVERS_RANDR_IS_AT_LEAST_1_3(priv) (priv->rr_major_version > 1 || (priv->rr_major_version == 1 && priv->rr_minor_version >= 3))

static gboolean output_initialize (GnomeRROutput *output, XRRScreenResources *res, GError **error);
static void gnome_rr_x11_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GnomeRRX11Screen, gnome_rr_x11_screen, GNOME_TYPE_RR_SCREEN,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gnome_rr_x11_screen_initable_iface_init))

static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
		 GdkEvent *event,
		 gpointer data)
{
#ifdef HAVE_RANDR
    GnomeRRX11Screen *screen = data;
    GnomeRRX11ScreenPrivate *priv = screen->priv;
    XEvent *e = xevent;
    int event_num;

    if (!e)
	return GDK_FILTER_CONTINUE;

    event_num = e->type - priv->randr_event_base;

    if (event_num == RRScreenChangeNotify) {
	/* We don't reprobe the hardware; we just fetch the X server's latest
	 * state.  The server already knows the new state of the outputs; that's
	 * why it sent us an event!
	 */
        screen_update (GNOME_RR_SCREEN (screen), TRUE, FALSE, NULL); /* NULL-GError */
#if 0
	/* Enable this code to get a dialog showing the RANDR timestamps, for debugging purposes */
	{
	    GtkWidget *dialog;
	    XRRScreenChangeNotifyEvent *rr_event;
	    static int dialog_num;

	    rr_event = (XRRScreenChangeNotifyEvent *) e;

	    dialog = gtk_message_dialog_new (NULL,
					     0,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_CLOSE,
					     "RRScreenChangeNotify timestamps (%d):\n"
					     "event change: %u\n"
					     "event config: %u\n"
					     "event serial: %lu\n"
					     "----------------------"
					     "screen change: %u\n"
					     "screen config: %u\n",
					     dialog_num++,
					     (guint32) rr_event->timestamp,
					     (guint32) rr_event->config_timestamp,
					     rr_event->serial,
					     (guint32) priv->info->resources->timestamp,
					     (guint32) priv->info->resources->configTimestamp);
	    g_signal_connect (dialog, "response",
			      G_CALLBACK (gtk_widget_destroy), NULL);
	    gtk_widget_show (dialog);
	}
#endif
    }
#if 0
    /* WHY THIS CODE IS DISABLED:
     *
     * Note that in gnome_rr_screen_new(), we only select for
     * RRScreenChangeNotifyMask.  We used to select for other values in
     * RR*NotifyMask, but we weren't really doing anything useful with those
     * events.  We only care about "the screens changed in some way or another"
     * for now.
     *
     * If we ever run into a situtation that could benefit from processing more
     * detailed events, we can enable this code again.
     *
     * Note that the X server sends RRScreenChangeNotify in conjunction with the
     * more detailed events from RANDR 1.2 - see xserver/randr/randr.c:TellChanged().
     */
    else if (event_num == RRNotify)
    {
	/* Other RandR events */

	XRRNotifyEvent *event = (XRRNotifyEvent *)e;

	/* Here we can distinguish between RRNotify events supported
	 * since RandR 1.2 such as RRNotify_OutputProperty.  For now, we
	 * don't have anything special to do for particular subevent types, so
	 * we leave this as an empty switch().
	 */
	switch (event->subtype)
	{
	default:
	    break;
	}

	/* No need to reprobe hardware here */
	screen_update (GNOME_RR_SCREEN (screen), TRUE, FALSE, NULL); /* NULL-GError */
    }
#endif

#endif /* HAVE_RANDR */

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

static gboolean
gnome_rr_x11_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GInitableIface *iface, *parent_iface;

    iface = G_TYPE_INSTANCE_GET_INTERFACE(initable, G_TYPE_INITABLE, GInitableIface);
    parent_iface = g_type_interface_peek_parent(iface);

#ifdef HAVE_RANDR
    GnomeRRX11Screen *self = GNOME_RR_X11_SCREEN (initable);
    GnomeRRX11ScreenPrivate *priv = self->priv;
    Display *dpy = GDK_SCREEN_XDISPLAY (gnome_rr_screen_get_gdk_screen (GNOME_RR_SCREEN (self)));
    int event_base;
    int ignore;
    GdkWindow *root;

    root = gdk_screen_get_root_window (gnome_rr_screen_get_gdk_screen (GNOME_RR_SCREEN (self)));
    priv->connector_type_atom = XInternAtom (dpy, "ConnectorType", FALSE);

    if (XRRQueryExtension (dpy, &event_base, &ignore))
    {
        priv->randr_event_base = event_base;

        XRRQueryVersion (dpy, &priv->rr_major_version, &priv->rr_minor_version);
        if (priv->rr_major_version < 1 || (priv->rr_major_version == 1 && priv->rr_minor_version < 2)) {
            g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_NO_RANDR_EXTENSION,
                    "RANDR extension is too old (must be at least 1.2)");
            return FALSE;
        }

        if (!parent_iface->init (initable, canc, error))
            return FALSE;

        XRRSelectInput (priv->xdisplay,
                priv->xroot,
                RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (
                gdk_screen_get_display (gnome_rr_screen_get_gdk_screen (GNOME_RR_SCREEN (self))),
                event_base,
                RRNotify + 1);
        gdk_window_add_filter (root, screen_on_event, self);

        return TRUE;
    }
    else
    {
#endif /* HAVE_RANDR */
    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_NO_RANDR_EXTENSION,
             _("RANDR extension is not present"));

    return FALSE;
#ifdef HAVE_RANDR
   }
#endif
}

static void
gnome_rr_x11_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_rr_x11_screen_initable_init;
}

static void
gnome_rr_x11_screen_finalize (GObject *gobject)
{
    GnomeRRX11Screen *screen = GNOME_RR_X11_SCREEN (gobject);
    GdkWindow *root;

    root = gdk_screen_get_root_window (gnome_rr_screen_get_gdk_screen (GNOME_RR_SCREEN (screen)));
    gdk_window_remove_filter (root, screen_on_event, screen);

    G_OBJECT_CLASS (gnome_rr_x11_screen_parent_class)->finalize (gobject);
}

static void
gnome_rr_x11_screen_class_init (GnomeRRX11ScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GnomeRRX11ScreenPrivate));

    gobject_class->finalize = gnome_rr_x11_screen_finalize;
}

static void
on_gdk_screen_changed (GnomeRRX11Screen *self, G_GNUC_UNUSED gpointer data)
{
    GnomeRRX11ScreenPrivate *priv = self->priv;
    GdkScreen *gdk_screen;
    GdkWindow *root;

    gdk_screen = gnome_rr_screen_get_gdk_screen (GNOME_RR_SCREEN (self));
    root = gdk_screen_get_root_window (gdk_screen);

    priv->xroot = gdk_x11_window_get_xid (root);
    priv->xdisplay = GDK_SCREEN_XDISPLAY (gdk_screen);
    priv->xscreen = gdk_x11_screen_get_xscreen (gdk_screen);
}

static void
gnome_rr_x11_screen_init (GnomeRRX11Screen *self)
{
    GnomeRRX11ScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_RR_X11_SCREEN, GnomeRRX11ScreenPrivate);

    self->priv = priv;
    priv->xdisplay = NULL;
    priv->xroot = None;
    priv->xscreen = NULL;
    priv->rr_major_version = 0;
    priv->rr_minor_version = 0;

    /* no need to unregister this handler: it's self - instead of notify signal, we could override the prop */
    g_signal_connect (self, "notify::gdk-screen", G_CALLBACK (on_gdk_screen_changed), NULL);
}

void
gnome_rr_screen_set_size (GnomeRRScreen *self,
			  int	      width,
			  int       height,
			  int       mm_width,
			  int       mm_height)
{
    g_return_if_fail (GNOME_IS_RR_X11_SCREEN (self));

#ifdef HAVE_RANDR
    GnomeRRX11ScreenPrivate *priv = GNOME_RR_X11_SCREEN (self)->priv;

    gdk_error_trap_push ();
    XRRSetScreenSize (priv->xdisplay, priv->xroot,
		      width, height, mm_width, mm_height);
    gdk_error_trap_pop_ignored ();
#endif
}

#ifdef HAVE_RANDR
/* GnomeRRCrtc */
typedef struct
{
    Rotation xrot;
    GnomeRRRotation rot;
} RotationMap;

static const RotationMap rotation_map[] =
{
    { RR_Rotate_0, GNOME_RR_ROTATION_0 },
    { RR_Rotate_90, GNOME_RR_ROTATION_90 },
    { RR_Rotate_180, GNOME_RR_ROTATION_180 },
    { RR_Rotate_270, GNOME_RR_ROTATION_270 },
    { RR_Reflect_X, GNOME_RR_REFLECT_X },
    { RR_Reflect_Y, GNOME_RR_REFLECT_Y },
};

static GnomeRRRotation
gnome_rr_rotation_from_xrotation (Rotation r)
{
    int i;
    GnomeRRRotation result = 0;

    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].xrot)
	    result |= rotation_map[i].rot;
    }

    return result;
}

static Rotation
xrotation_from_rotation (GnomeRRRotation r)
{
    int i;
    Rotation result = 0;

    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].rot)
	    result |= rotation_map[i].xrot;
    }

    return result;
}

static gboolean
crtc_initialize (GnomeRRCrtc        *crtc,
		 XRRScreenResources *res,
		 GError            **error)
{
    XRRCrtcInfo *info = XRRGetCrtcInfo (DISPLAY (crtc), res, crtc->id);
    GPtrArray *a;
    int i;

#if 0
    g_print ("CRTC %lx Timestamp: %u\n", crtc->id, (guint32)info->timestamp);
#endif

    if (!info)
    {
	/* FIXME: We need to reaquire the screen resources */
	/* FIXME: can we actually catch BadRRCrtc, and does it make sense to emit that? */

	/* Translators: CRTC is a CRT Controller (this is X terminology).
	 * It is *very* unlikely that you'll ever get this error, so it is
	 * only listed for completeness. */
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_RANDR_ERROR,
		     _("could not get information about CRTC %d"),
		     (int) crtc->id);
	return FALSE;
    }

    /* GnomeRRMode */
    crtc->current_mode = mode_by_id (crtc->info, info->mode);

    crtc->x = info->x;
    crtc->y = info->y;

    /* Current outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->noutput; ++i)
    {
	GnomeRROutput *output = gnome_rr_output_by_id (crtc->info, info->outputs[i]);

	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->current_outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);

    /* Possible outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->npossible; ++i)
    {
	GnomeRROutput *output = gnome_rr_output_by_id (crtc->info, info->possible[i]);

	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->possible_outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);

    /* Rotations */
    crtc->current_rotation = gnome_rr_rotation_from_xrotation (info->rotation);
    crtc->rotations = gnome_rr_rotation_from_xrotation (info->rotations);

    XRRFreeCrtcInfo (info);

    /* get an store gamma size */
    crtc->gamma_size = XRRGetCrtcGammaSize (DISPLAY (crtc), crtc->id);

    return TRUE;
}

static void
mode_initialize (GnomeRRMode *mode, XRRModeInfo *info)
{
    g_return_if_fail (mode != NULL);
    g_return_if_fail (info != NULL);

    mode->name = g_strdup (info->name);
    mode->width = info->width;
    mode->height = info->height;
    mode->freq = ((info->dotClock / (double)info->hTotal) / info->vTotal + 0.5) * 1000;
}

static gboolean
fill_screen_info_from_resources (ScreenInfo *info,
				 XRRScreenResources *resources,
				 GError **error)
{
    int i;
    GPtrArray *a;
    GnomeRRCrtc **crtc;
    GnomeRROutput **output;

    info->resources = resources;

    /* We create all the structures before initializing them, so
     * that they can refer to each other.
     */
    a = g_ptr_array_new ();
    for (i = 0; i < resources->ncrtc; ++i)
    {
	GnomeRRCrtc *crtc = crtc_new (info, resources->crtcs[i]);

	g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    info->crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0; i < resources->noutput; ++i)
    {
	GnomeRROutput *output = output_new (info, resources->outputs[i]);

	g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    info->outputs = (GnomeRROutput **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0;  i < resources->nmode; ++i)
    {
	GnomeRRMode *mode = mode_new (info, resources->modes[i].id);

	g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    info->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);

    /* Initialize */
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if (!crtc_initialize (*crtc, resources, error))
	    return FALSE;
    }

    for (output = info->outputs; *output; ++output)
    {
	if (!output_initialize (*output, resources, error))
	    return FALSE;
    }

    for (i = 0; i < resources->nmode; ++i)
    {
	GnomeRRMode *mode = mode_by_id (info, resources->modes[i].id);

	mode_initialize (mode, &(resources->modes[i]));
    }

    gather_clone_modes (info);

    return TRUE;
}
#endif /* HAVE_RANDR */

gboolean
fill_out_screen_info (GnomeRRScreen *screen,
		      ScreenInfo *info,
		      gboolean needs_reprobe,
		      GError **error)
{
    GnomeRRX11ScreenPrivate *priv = GNOME_RR_X11_SCREEN (screen)->priv;
    Display *xdisplay = priv->xdisplay;
    Window xroot = priv->xroot;
#ifdef HAVE_RANDR
    XRRScreenResources *resources;

    g_return_val_if_fail (xdisplay != NULL, FALSE);
    g_return_val_if_fail (info != NULL, FALSE);

    /* First update the screen resources */

    if (needs_reprobe)
        resources = XRRGetScreenResources (xdisplay, xroot);
    else
    {
	/* XRRGetScreenResourcesCurrent is less expensive than
	 * XRRGetScreenResources, however it is available only
	 * in RandR 1.3 or higher
	 */
#if RANDR_LIBRARY_IS_AT_LEAST_1_3
        if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv))
            resources = XRRGetScreenResourcesCurrent (xdisplay, xroot);
        else
            resources = XRRGetScreenResources (xdisplay, xroot);
#else
        resources = XRRGetScreenResources (xdisplay, xroot);
#endif
    }

    if (resources)
    {
	if (!fill_screen_info_from_resources (info, resources, error))
	    return FALSE;
    }
    else
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_RANDR_ERROR,
		     /* Translators: a CRTC is a CRT Controller (this is X terminology). */
		     _("could not get the screen resources (CRTCs, outputs, modes)"));
	return FALSE;
    }

    /* Then update the screen size range.  We do this after XRRGetScreenResources() so that
     * the X server will already have an updated view of the outputs.
     */

    if (needs_reprobe) {
	gboolean success;

        gdk_error_trap_push ();
	success = XRRGetScreenSizeRange (xdisplay, xroot,
					 &(info->min_width),
					 &(info->min_height),
					 &(info->max_width),
					 &(info->max_height));
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_UNKNOWN,
			 _("unhandled X error while getting the range of screen sizes"));
	    return FALSE;
	}

	if (!success) {
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_RANDR_ERROR,
			 _("could not get the range of screen sizes"));
            return FALSE;
        }
    }
    else
    {
        gnome_rr_screen_get_ranges (info->screen,
					 &(info->min_width),
					 &(info->max_width),
					 &(info->min_height),
					 &(info->max_height));
    }

    info->primary = None;
#if RANDR_LIBRARY_IS_AT_LEAST_1_3
    if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv)) {
        gdk_error_trap_push ();
        info->primary = XRRGetOutputPrimary (xdisplay, xroot);
        gdk_error_trap_pop_ignored ();
    }
#endif

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_RANDR */
}

static guint8 *
get_property (Display *dpy,
	      RROutput output,
	      Atom atom,
	      int *len)
{
#ifdef HAVE_RANDR
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    guint8 *result;

    XRRGetOutputProperty (dpy, output, atom,
			  0, 100, False, False,
			  AnyPropertyType,
			  &actual_type, &actual_format,
			  &nitems, &bytes_after, &prop);

    if (actual_type == XA_INTEGER && actual_format == 8)
    {
	result = g_memdup (prop, nitems);
	if (len)
	    *len = nitems;
    }
    else
    {
	result = NULL;
    }

    XFree (prop);

    return result;
#else
    return NULL;
#endif /* HAVE_RANDR */
}

static guint8 *
read_edid_data (GnomeRROutput *output, int *len)
{
    Atom edid_atom;
    guint8 *result;

    edid_atom = XInternAtom (DISPLAY (output), "EDID", FALSE);
    result = get_property (DISPLAY (output),
			   output->id, edid_atom, len);

    if (!result)
    {
	edid_atom = XInternAtom (DISPLAY (output), "EDID_DATA", FALSE);
	result = get_property (DISPLAY (output),
			       output->id, edid_atom, len);
    }

    if (result)
    {
	if (*len % 128 == 0)
	    return result;
	else
	    g_free (result);
    }

    return NULL;
}

static char *
x11_get_connector_type_string (GnomeRROutput *output)
{
#ifdef HAVE_RANDR
    char *result;
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    Atom connector_type;
    char *connector_type_str;
    GnomeRRX11ScreenPrivate *priv = GNOME_RR_X11_SCREEN (output->info->screen)->priv;

    result = NULL;

    if (XRRGetOutputProperty (DISPLAY (output), output->id, priv->connector_type_atom,
			      0, 100, False, False,
			      AnyPropertyType,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop) != Success)
	return NULL;

    if (!(actual_type == XA_ATOM && actual_format == 32 && nitems == 1))
	goto out;

    connector_type = *((Atom *) prop);

    connector_type_str = XGetAtomName (DISPLAY (output), connector_type);
    if (connector_type_str) {
	result = g_strdup (connector_type_str); /* so the caller can g_free() it */
	XFree (connector_type_str);
    }

out:

    XFree (prop);

    return result;
#else
    return NULL;
#endif
}

static gboolean
output_initialize (GnomeRROutput *output, XRRScreenResources *res, GError **error)
{
    XRROutputInfo *info = XRRGetOutputInfo (
	DISPLAY (output), res, output->id);
    GPtrArray *a;
    int i;

#if 0
    g_print ("Output %lx Timestamp: %u\n", output->id, (guint32)info->timestamp);
#endif

    if (!info || !output->info)
    {
	/* FIXME: see the comment in crtc_initialize() */
	/* Translators: here, an "output" is a video output */
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_RANDR_ERROR,
		     _("could not get information about output %d"),
		     (int) output->id);
	return FALSE;
    }

    output->name = g_strdup (info->name); /* FIXME: what is nameLen used for? */
    output->current_crtc = crtc_by_id (output->info, info->crtc);
    output->width_mm = info->mm_width;
    output->height_mm = info->mm_height;
    output->connected = (info->connection == RR_Connected);
    output->connector_type = x11_get_connector_type_string (output);

    /* Possible crtcs */
    a = g_ptr_array_new ();

    for (i = 0; i < info->ncrtc; ++i)
    {
	GnomeRRCrtc *crtc = crtc_by_id (output->info, info->crtcs[i]);

	if (crtc)
	    g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (GnomeRRCrtc **)g_ptr_array_free (a, FALSE);

    /* Clones */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nclone; ++i)
    {
	GnomeRROutput *gnome_rr_output = gnome_rr_output_by_id (output->info, info->clones[i]);

	if (gnome_rr_output)
	    g_ptr_array_add (a, gnome_rr_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (GnomeRROutput **)g_ptr_array_free (a, FALSE);

    /* Modes */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nmode; ++i)
    {
	GnomeRRMode *mode = mode_by_id (output->info, info->modes[i]);

	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (GnomeRRMode **)g_ptr_array_free (a, FALSE);

    output->n_preferred = info->npreferred;

    /* Edid data */
    output->edid_data = read_edid_data (output, &output->edid_size);

    XRRFreeOutputInfo (info);

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
#ifdef HAVE_RANDR
    ScreenInfo *info;
    GArray *output_ids;
    Status status;
    gboolean result;
    int i;

    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (mode != NULL || outputs == NULL || n_outputs == 0, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    info = crtc->info;

    if (mode)
    {
	if (x + mode->width > info->max_width
	    || y + mode->height > info->max_height)
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_BOUNDS_ERROR,
			 /* Translators: the "position", "size", and "maximum"
			  * words here are not keywords; please translate them
			  * as usual.  A CRTC is a CRT Controller (this is X terminology) */
			 _("requested position/size for CRTC %d is outside the allowed limit: "
			   "position=(%d, %d), size=(%d, %d), maximum=(%d, %d)"),
			 (int) crtc->id,
			 x, y,
			 mode->width, mode->height,
			 info->max_width, info->max_height);
	    return FALSE;
	}
    }

    output_ids = g_array_new (FALSE, FALSE, sizeof (RROutput));

    if (outputs)
    {
	for (i = 0; i < n_outputs; ++i)
	    g_array_append_val (output_ids, outputs[i]->id);
    }

    status = XRRSetCrtcConfig (DISPLAY (crtc), info->resources, crtc->id,
			       timestamp,
			       x, y,
			       mode ? mode->id : None,
			       xrotation_from_rotation (rotation),
			       (RROutput *)output_ids->data,
			       output_ids->len);

    g_array_free (output_ids, TRUE);

    if (status == RRSetConfigSuccess)
	result = TRUE;
    else {
	result = FALSE;
	/* Translators: CRTC is a CRT Controller (this is X terminology).
	 * It is *very* unlikely that you'll ever get this error, so it is
	 * only listed for completeness. */
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_RANDR_ERROR,
		     _("could not set the configuration for CRTC %d"),
		     (int) crtc->id);
    }

    return result;
#else
    return FALSE;
#endif /* HAVE_RANDR */
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

#ifdef HAVE_RANDR
    int copy_size;
    XRRCrtcGamma *gamma;

    if (size != crtc->gamma_size)
	return;

    gamma = XRRAllocGamma (crtc->gamma_size);

    copy_size = crtc->gamma_size * sizeof (unsigned short);
    memcpy (gamma->red, red, copy_size);
    memcpy (gamma->green, green, copy_size);
    memcpy (gamma->blue, blue, copy_size);

    XRRSetCrtcGamma (DISPLAY (crtc), crtc->id, gamma);
    XRRFreeGamma (gamma);
#endif /* HAVE_RANDR */
}

gboolean
gnome_rr_crtc_get_gamma (GnomeRRCrtc *crtc, int *size,
			 unsigned short **red, unsigned short **green,
			 unsigned short **blue)
{
    g_return_val_if_fail (crtc != NULL, FALSE);

#ifdef HAVE_RANDR
    int copy_size;
    unsigned short *r, *g, *b;
    XRRCrtcGamma *gamma;

    gamma = XRRGetCrtcGamma (DISPLAY (crtc), crtc->id);
    if (!gamma)
	return FALSE;

    copy_size = crtc->gamma_size * sizeof (unsigned short);

    if (red) {
	r = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (r, gamma->red, copy_size);
	*red = r;
    }

    if (green) {
	g = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (g, gamma->green, copy_size);
	*green = g;
    }

    if (blue) {
	b = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (b, gamma->blue, copy_size);
	*blue = b;
    }

    XRRFreeGamma (gamma);

    if (size)
	*size = crtc->gamma_size;

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_RANDR */
}

gboolean
gnome_rr_x11_screen_force_timestamp_update (GnomeRRX11Screen *self)
{
#ifdef HAVE_RANDR
    GnomeRRX11ScreenPrivate *priv = self->priv;
    GnomeRRCrtc *crtc;
    XRRCrtcInfo *current_info;
    Status status;
    gboolean timestamp_updated;
    ScreenInfo *info;

    timestamp_updated = FALSE;

    info = GNOME_RR_SCREEN(self)->priv->info;
    crtc = info->crtcs[0];

    if (crtc == NULL)
	goto out;

    current_info = XRRGetCrtcInfo (priv->xdisplay,
				   info->resources,
				   crtc->id);

    if (current_info == NULL)
	goto out;

    gdk_error_trap_push ();
    status = XRRSetCrtcConfig (priv->xdisplay,
			       info->resources,
			       crtc->id,
			       current_info->timestamp,
			       current_info->x,
			       current_info->y,
			       current_info->mode,
			       current_info->rotation,
			       current_info->outputs,
			       current_info->noutput);

    XRRFreeCrtcInfo (current_info);

    gdk_flush ();
    if (gdk_error_trap_pop ())
	goto out;

    if (status == RRSetConfigSuccess)
	timestamp_updated = TRUE;
out:
    return timestamp_updated;
#else
    return FALSE;
#endif
}

void
screen_set_primary_output (GnomeRRScreen *screen, GnomeRROutput *output)
{
#if RANDR_LIBRARY_IS_AT_LEAST_1_3
    GnomeRRX11ScreenPrivate *priv = GNOME_RR_X11_SCREEN (screen)->priv;
    RROutput id;

    if (output)
        id = output->id;
    else
        id = None;

    if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv))
        XRRSetOutputPrimary (priv->xdisplay, priv->xroot, id);
#endif
}
