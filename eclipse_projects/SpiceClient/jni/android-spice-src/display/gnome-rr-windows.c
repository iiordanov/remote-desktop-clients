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
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <windef.h>
#include <wingdi.h>

#undef GNOME_DISABLE_DEPRECATED
#include "gnome-rr.h"
#include "gnome-rr-config.h"
#include "gnome-rr-private.h"
#include "gnome-rr-windows.h"

struct GnomeRRWindowsScreenPrivate
{
    RRMode rrmode_id;
    DISPLAY_DEVICE device;
};

static void gnome_rr_windows_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GnomeRRWindowsScreen, gnome_rr_windows_screen, GNOME_TYPE_RR_SCREEN,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gnome_rr_windows_screen_initable_iface_init))

static gboolean
gnome_rr_windows_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GInitableIface *iface, *parent_iface;

    iface = G_TYPE_INSTANCE_GET_INTERFACE(initable, G_TYPE_INITABLE, GInitableIface);
    parent_iface = g_type_interface_peek_parent(iface);

    if (!parent_iface->init (initable, canc, error))
      return FALSE;

    return TRUE;
}

static void
gnome_rr_windows_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gnome_rr_windows_screen_initable_init;
}

static void
gnome_rr_windows_screen_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (gnome_rr_windows_screen_parent_class)->finalize (gobject);
}

static void
gnome_rr_windows_screen_class_init (GnomeRRWindowsScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GnomeRRWindowsScreenPrivate));

    gobject_class->finalize = gnome_rr_windows_screen_finalize;
}

static void
gnome_rr_windows_screen_init (GnomeRRWindowsScreen *self)
{
    GnomeRRWindowsScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
        GNOME_TYPE_RR_WINDOWS_SCREEN, GnomeRRWindowsScreenPrivate);

    self->priv = priv;
}

static int
display_frequency_to_freq (DWORD f)
{
    return ((f <= 1 ? 60 : f) * 1000);
}

GnomeRRMode *
lookup_mode (GPtrArray *a, DEVMODE *mode, int flags)
{
    guint len = a->len;
    GnomeRRMode **modes = (GnomeRRMode **)a->pdata;

    for (len = a->len; len > 0; len--) {
        GnomeRRMode *m = modes[len - 1];

        if (m->width != mode->dmPelsWidth)
            continue;
        if (m->height != mode->dmPelsHeight)
            continue;
        if (m->freq != display_frequency_to_freq (mode->dmDisplayFrequency))
            continue;
        return m;
    }

    return NULL;
}

#if(WINVER >= 0x0501)
typedef struct
{
    DWORD o;
    GnomeRRRotation rot;
} RotationMap;

static const RotationMap rotation_map[] =
{
    { DMDO_DEFAULT, GNOME_RR_ROTATION_0 },
    { DMDO_90, GNOME_RR_ROTATION_90 },
    { DMDO_180, GNOME_RR_ROTATION_180 },
    { DMDO_270, GNOME_RR_ROTATION_270 },
};

static GnomeRRRotation
gnome_rr_rotation_from_orientation (DWORD o)
{
    int i;
    GnomeRRRotation result = 0;

    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (o == rotation_map[i].o)
	    result |= rotation_map[i].rot;
    }

    return result;
}

static DWORD
orientation_from_rotation (GnomeRRRotation r)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].rot)
	    return rotation_map[i].o;
    }

    g_return_val_if_reached (DMDO_DEFAULT);
}
#endif


GnomeRRMode*
screen_mode_new (GnomeRRScreen *screen, ScreenInfo *info, DEVMODE *m)
{
    GnomeRRWindowsScreen *self = GNOME_RR_WINDOWS_SCREEN (screen);
    GnomeRRWindowsScreenPrivate *priv = self->priv;
    GnomeRRMode *mode;

    g_return_val_if_fail (m != NULL, NULL);

    mode = mode_new (info, priv->rrmode_id++);
    mode->mode = *m;
    mode->width = m->dmPelsWidth;
    mode->height = m->dmPelsHeight;
    mode->freq = display_frequency_to_freq (m->dmDisplayFrequency);

    return mode;
}

gboolean
fill_out_screen_info (GnomeRRScreen *screen, ScreenInfo *info,
                      gboolean needs_reprobe, GError **error)
{
    DWORD device_id;
    DISPLAY_DEVICE device;
    GPtrArray *crtcs, *outputs, *modes, *omodes;;

    device.cb = sizeof(device);

    crtcs = g_ptr_array_new ();
    outputs = g_ptr_array_new ();
    modes = g_ptr_array_new ();

    for (device_id = 0; ; device_id++) {
        GnomeRRCrtc *crtc;
        GnomeRRMode *rrmode;
        GnomeRROutput *output;
        DEVMODE mode = { {0, }, };
        DWORD iModeNum;

        mode.dmSize = sizeof (DEVMODE);

        if (!EnumDisplayDevices (NULL, device_id, &device, 0))
            break;

#if DEBUG
        g_debug ("%s - %s, primary: %s", device.DeviceName, device.DeviceString,
                 device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE ? "yes" : "no");
#endif
        if (device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) /* FIXME: == clone output? */
            continue;

        crtc = crtc_new (info, device_id);
        crtc->rotations = GNOME_RR_ROTATION_0;
        g_ptr_array_add (crtcs, crtc);

        output = output_new (info, device_id);
        g_ptr_array_add (outputs, output);
        omodes = g_ptr_array_new ();

        output->name = g_strdup (device.DeviceName);
        output->current_crtc = crtc;
        output->possible_crtcs = g_new0 (GnomeRRCrtc*, 2);
        output->possible_crtcs[0] = crtc;
        output->clones = g_new0 (GnomeRROutput*, 1); /* FIXME */
        /* FIXME: could be a seperate active display? */
        output->connected =
            device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;

        /* Populate modes for this display/crtc */
        for (iModeNum = 0; ; iModeNum++) {
#if (_WIN32_WINNT >= 0x0500 || _WIN32_WINDOWS >= 0x0410) && defined(EDS_ROTATEDMODE)
            if (!EnumDisplaySettingsEx (device.DeviceName, iModeNum, &mode, EDS_ROTATEDMODE))
#else
            if (!EnumDisplaySettings (device.DeviceName, iModeNum, &mode))
#endif
                break;

#if DEBUG
            g_debug ("pos: %ldx%ld, size: %ldx%ld, rot: %ld depth: %ld freq: %ld",
                     mode.dmPosition.x, mode.dmPosition.y,
                     mode.dmPelsWidth, mode.dmPelsHeight,
                     mode.dmDisplayOrientation, mode.dmBitsPerPel,
                     mode.dmDisplayFrequency);
#endif

            rrmode = screen_mode_new (screen, info, &mode);
            g_ptr_array_add (modes, rrmode);
            g_ptr_array_add (omodes, rrmode);
#if (_WIN32_WINNT >= 0x0500 || _WIN32_WINDOWS >= 0x0410)
            /* NOTE: windows does rotation at mode level... */
            crtc->rotations |= gnome_rr_rotation_from_orientation (mode.dmDisplayOrientation);
#endif
        }

        {
            /* Current mode settings for this display/crtc */
#if (_WIN32_WINNT >= 0x0500 || _WIN32_WINDOWS >= 0x0410) && defined(EDS_ROTATEDMODE)
            if (!EnumDisplaySettingsEx (device.DeviceName, ENUM_CURRENT_SETTINGS, &mode, EDS_ROTATEDMODE))
#else
            if (!EnumDisplaySettings (device.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
#endif
            {
                g_warn_if_reached ();
                continue;
            }

            crtc->current_mode = lookup_mode (omodes, &mode, 0);
            if (!crtc->current_mode) {
                g_warn_if_reached ();

                rrmode = screen_mode_new (screen, info, &mode);
                g_ptr_array_add (modes, rrmode);
                g_ptr_array_add (omodes, rrmode);
                crtc->current_mode = lookup_mode (outputs, &mode, 0);
            }
            g_return_val_if_fail (crtc->current_mode != NULL, FALSE);

            crtc->x = mode.dmPosition.x;
            crtc->y = mode.dmPosition.y;

#if(WINVER >= 0x0501)
            crtc->current_rotation = gnome_rr_rotation_from_orientation (mode.dmDisplayOrientation);
#else
            crtc->current_rotation = GNOME_RR_ROTATION_0;
#endif

            crtc->current_outputs = g_new0 (GnomeRROutput*, 2);
            crtc->current_outputs[0] = output;
            crtc->possible_outputs = g_new0 (GnomeRROutput*, 2);
            crtc->possible_outputs[0] = output;
        }

        /* FIXME: vista has GetMonitorDisplayAreaSize, not mingw */
        /* see also EDID hack on stackoverflow if necessary */
        output->width_mm = 0;
        output->height_mm = 0;
        g_ptr_array_add (omodes, NULL);
        output->modes = (GnomeRRMode **)g_ptr_array_free (omodes, FALSE);
    }

    g_ptr_array_add (modes, NULL);
    info->modes = (GnomeRRMode **)g_ptr_array_free (modes, FALSE);

    g_ptr_array_add (crtcs, NULL);
    info->crtcs = (GnomeRRCrtc **)g_ptr_array_free (crtcs, FALSE);

    g_ptr_array_add (outputs, NULL);
    info->outputs = (GnomeRROutput **)g_ptr_array_free (outputs, FALSE);

    if (needs_reprobe) {
        /* FIXME: all wrong.. perhaps size of primary monitor & sum of monitor sizes*/
        info->min_width = 0;
        info->min_height = 0;
        info->max_width = MAX(GetSystemMetrics (SM_CXVIRTUALSCREEN), 8192);
        info->max_height = MAX(GetSystemMetrics (SM_CYVIRTUALSCREEN), 8192);
    } else {
        gnome_rr_screen_get_ranges (info->screen,
                                    &(info->min_width),
                                    &(info->max_width),
                                    &(info->min_height),
                                    &(info->max_height));
    }

    gather_clone_modes (info); /* FIXME: or do it diffently? */

    return TRUE;
}

static const gchar*
get_display_change_error (LONG ret)
{
    switch (ret) {
    case DISP_CHANGE_SUCCESSFUL:
        return "The settings change was successful.";
    case DISP_CHANGE_BADDUALVIEW:
        return "The settings change was unsuccessful because the system is DualView capable.";
    case DISP_CHANGE_BADFLAGS:
        return "An invalid set of flags was passed in.";
    case DISP_CHANGE_BADMODE:
        return "The graphics mode is not supported.";
    case DISP_CHANGE_BADPARAM:
        return "An invalid parameter was passed in. This can include an invalid flag or combination of flags.";
    case DISP_CHANGE_FAILED:
        return "The display driver failed the specified graphics mode.";
    case DISP_CHANGE_NOTUPDATED:
        return "Unable to write settings to the registry.";
    case DISP_CHANGE_RESTART:
        return "The computer must be restarted for the graphics mode to work.";
    }

    g_return_val_if_reached ("unknown display change error");
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
    int n;
    DEVMODE m;
    LONG ret;

    g_return_val_if_fail (n_outputs <= 1, FALSE); /* FIXME */
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (mode != NULL || outputs == NULL || n_outputs == 0, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (mode == NULL) /* FIXME: turn off crtc? */
        return TRUE;

    m = mode->mode;

    m.dmFields |= DM_POSITION;
    m.dmPosition.x = x;
    m.dmPosition.y = y;
#if(WINVER >= 0x0501)
    m.dmFields |= DM_DISPLAYORIENTATION;
    m.dmDisplayOrientation = orientation_from_rotation (rotation);
#endif
    /* TODO: deal with removed outputs and refresh stucts */

    for (n = 0; n < n_outputs; ++n) {
        GnomeRROutput *o = outputs[n];

        ret = ChangeDisplaySettingsEx(o->name, &m, NULL, CDS_FULLSCREEN, NULL);
        if (ret != DISP_CHANGE_SUCCESSFUL) {
            g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_UNKNOWN,
                         /* Translators: a CRTC is a CRT Controller (this is X terminology). */
                         _("could not set the output configuration %s"),
                         get_display_change_error (ret));
            return FALSE;
        }
    }

    return TRUE;
}

void
gnome_rr_screen_set_size (GnomeRRScreen *self,
			  int	      width,
			  int       height,
			  int       mm_width,
			  int       mm_height)
{
    g_return_if_fail (GNOME_IS_RR_WINDOWS_SCREEN (self));

    /* there is nothing we can do here, windows seems to compute
       virtual screen size itself */
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

    g_warning ("Unimplemented on Windows");
}

gboolean
gnome_rr_crtc_get_gamma (GnomeRRCrtc *crtc, int *size,
			 unsigned short **red, unsigned short **green,
			 unsigned short **blue)
{
    g_return_val_if_fail (crtc != NULL, FALSE);

    g_warning ("Unimplemented on Windows");
    return FALSE;
}

void
screen_set_primary_output (GnomeRRScreen *screen, GnomeRROutput *output)
{
}
