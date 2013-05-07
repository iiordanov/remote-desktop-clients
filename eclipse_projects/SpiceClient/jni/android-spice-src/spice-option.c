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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include "spice-session.h"
#include "spice-util.h"
#include "usb-device-manager.h"

static gchar *disable_effects = NULL;
static gint color_depth = 0;
static char *ca_file = NULL;
static char *host_subject = NULL;
static char *smartcard_db = NULL;
static char *smartcard_certificates = NULL;
static char *usbredir_filter = NULL;
static gboolean smartcard = FALSE;
static gboolean disable_audio = FALSE;
static gboolean disable_usbredir = FALSE;
static gint cache_size = 0;
static gint glz_window_size = 0;

static void option_version(void)
{
    g_print(PACKAGE_STRING "\n");
    exit(0);
}

static void option_debug(void)
{
    spice_util_set_debug(TRUE);
}

/**
 * spice_get_option_group:
 *
 * Returns: (transfer full): a #GOptionGroup for the commandline
 * arguments specific to Spice.  You have to call
 * spice_set_session_option() after to set the options on a
 * #SpiceSession.
 **/
GOptionGroup* spice_get_option_group(void)
{
    const GOptionEntry entries[] = {
        { "spice-disable-effects", '\0', 0, G_OPTION_ARG_STRING, &disable_effects,
          N_("Disable guest display effects"), N_("<wallpaper,font-smooth,animation,all>") },
        { "spice-color-depth", '\0', 0, G_OPTION_ARG_INT, &color_depth,
          N_("Guest display color depth"), N_("<16,32>") },
        { "spice-ca-file", '\0', 0, G_OPTION_ARG_FILENAME, &ca_file,
          N_("Truststore file for secure connections"), N_("<file>") },
        { "spice-host-subject", '\0', 0, G_OPTION_ARG_STRING, &host_subject,
          N_("Subject of the host certificate (field=value pairs separated by commas)"), N_("<host-subject>") },
        { "spice-disable-audio", '\0', 0, G_OPTION_ARG_NONE, &disable_audio,
          N_("Disable audio support"), NULL },
        { "spice-smartcard", '\0', 0, G_OPTION_ARG_NONE, &smartcard,
          N_("Enable smartcard support"), NULL },
        { "spice-smartcard-certificates", '\0', 0, G_OPTION_ARG_STRING, &smartcard_certificates,
          N_("Certificates to use for software smartcards (field=values separated by commas)"), N_("<certificates>") },
        { "spice-smartcard-db", '\0', 0, G_OPTION_ARG_STRING, &smartcard_db,
          N_("Path to the local certificate database to use for software smartcard certificates"), N_("<certificate-db>") },
        { "spice-disable-usbredir", '\0', 0, G_OPTION_ARG_NONE, &disable_usbredir,
          N_("Disable USB redirection support"), NULL },
        { "spice-usbredir-filter", '\0', 0, G_OPTION_ARG_STRING, &usbredir_filter,
          N_("Filter for excluding USB devices from auto redirection"), N_("<filter-string>") },
        { "spice-cache-size", '\0', 0, G_OPTION_ARG_INT, &cache_size,
          N_("Image cache size"), N_("<bytes>") },
        { "spice-glz-window-size", '\0', 0, G_OPTION_ARG_INT, &glz_window_size,
          N_("Glz compression history size"), N_("<bytes>") },

        { "spice-debug", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_debug,
          N_("Enable Spice-GTK debugging"), NULL },
        { "spice-gtk-version", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_version,
          N_("Display Spice-GTK version information"), NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    GOptionGroup *grp;

    grp = g_option_group_new("spice", _("Spice Options:"), _("Show Spice Options"), NULL, NULL);
    g_option_group_add_entries(grp, entries);

    return grp;
}

/**
 * spice_set_session_option:
 * @session: a #SpiceSession to set option upon
 *
 * Set various properties on @session, according to the commandline
 * arguments given to spice_get_option_group() option group.
 **/
void spice_set_session_option(SpiceSession *session)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    if (ca_file == NULL) {
        const char *homedir = g_getenv("HOME");
        if (!homedir)
            homedir = g_get_home_dir();
        ca_file = g_strdup_printf("%s/.spicec/spice_truststore.pem", homedir);
    }

    if (disable_effects) {
            GStrv effects;
            effects = g_strsplit(disable_effects, ",", -1);
            if (effects)
                g_object_set(session, "disable-effects", effects, NULL);
            g_strfreev(effects);
    }
    if (color_depth)
        g_object_set(session, "color-depth", color_depth, NULL);
    if (ca_file)
        g_object_set(session, "ca-file", ca_file, NULL);
    if (host_subject)
        g_object_set(session, "cert-subject", host_subject, NULL);
    if (smartcard) {
        g_object_set(session, "enable-smartcard", smartcard, NULL);
        if (smartcard_certificates) {
            GStrv certs_strv;
            certs_strv = g_strsplit(smartcard_certificates, ",", -1);
            if (certs_strv)
                g_object_set(session, "smartcard-certificates", certs_strv, NULL);
            g_strfreev(certs_strv);
        }
        if (smartcard_db)
            g_object_set(session, "smartcard-db", smartcard_db, NULL);
    }
    if (usbredir_filter) {
        SpiceUsbDeviceManager *m = spice_usb_device_manager_get(session, NULL);
        if (m)
            g_object_set(m, "auto-connect-filter", usbredir_filter, NULL);
    }
    if (disable_usbredir)
        g_object_set(session, "enable-usbredir", FALSE, NULL);
    if (disable_audio)
        g_object_set(session, "enable-audio", FALSE, NULL);
    if (cache_size)
        g_object_set(session, "cache-size", cache_size, NULL);
    if (glz_window_size)
        g_object_set(session, "glz-window-size", glz_window_size, NULL);
}
