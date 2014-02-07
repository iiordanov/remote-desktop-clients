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
#include "spice-channel-priv.h"
#include "usb-device-manager.h"

static GStrv disable_effects = NULL;
static gint color_depth = 0;
static char *ca_file = NULL;
static char *host_subject = NULL;
static char *smartcard_db = NULL;
static char *smartcard_certificates = NULL;
static char *usbredir_auto_redirect_filter = NULL;
static char *usbredir_redirect_on_connect = NULL;
static gboolean smartcard = FALSE;
static gboolean disable_audio = FALSE;
static gboolean disable_usbredir = FALSE;
static gint cache_size = 0;
static gint glz_window_size = 0;
static gchar *secure_channels = NULL;

G_GNUC_NORETURN
static void option_version(void)
{
    g_print(PACKAGE_STRING "\n");
    exit(0);
}

static gboolean option_debug(void)
{
    spice_util_set_debug(TRUE);
    return TRUE;
}

static gboolean parse_color_depth(const gchar *option_name, const gchar *value,
                                  gpointer data, GError **error)
{
    unsigned long parsed_depth;
    char *end;

    if (option_name == NULL) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, _("missing color depth, must be 16 or 32"));
        return FALSE;
    }

    parsed_depth = strtoul(value, &end, 0);
    if (*end != '\0')
        goto error;

    if ((parsed_depth != 16) && (parsed_depth != 32))
        goto error;

    color_depth = parsed_depth;

    return TRUE;

error:
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, _("invalid color depth (%s), must be 16 or 32"), value);
    return FALSE;
}

static gboolean parse_disable_effects(const gchar *option_name, const gchar *value,
                                      gpointer data, GError **error)
{
    GStrv it;

    disable_effects = g_strsplit(value, ",", -1);
    for (it = disable_effects; *it != NULL; it++) {
        if ((g_strcmp0(*it, "wallpaper") != 0)
             && (g_strcmp0(*it, "font-smooth") != 0)
             && (g_strcmp0(*it, "animation") != 0)
             && (g_strcmp0(*it, "all") != 0)) {
            /* Translators: do not translate 'wallpaper', 'font-smooth',
             * 'animation', 'all' as the user must use these values with the
             * --spice-disable-effects command line option
             */
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                    _("invalid effect name (%s), must be 'wallpaper', 'font-smooth', 'animation' or 'all'"), *it);
            g_strfreev(disable_effects);
            disable_effects = NULL;
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean parse_secure_channels(const gchar *option_name, const gchar *value,
                                      gpointer data, GError **error)
{
    gint i;
    gchar **channels = g_strsplit(value, ",", -1);

    g_return_val_if_fail(channels != NULL, FALSE);

    for (i = 0; channels[i]; i++) {
        if (g_strcmp0(channels[i], "all") == 0)
            continue;

        if (spice_channel_string_to_type(channels[i]) == -1) {
            gchar *supported = spice_channel_supported_string();
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                        _("invalid channel name (%s), valid names: all, %s"),
                        channels[i], supported);
            g_free(supported);
            return FALSE;
        }
    }

    g_strfreev(channels);

    secure_channels = g_strdup(value);

    return TRUE;
}


static gboolean parse_usbredir_filter(const gchar *option_name,
                                      const gchar *value,
                                      gpointer data, GError **error)

{
    g_warning("--spice-usbredir-filter is deprecated, please use --spice-usbredir-auto-redirect-filter instead");
    g_free(usbredir_auto_redirect_filter);
    usbredir_auto_redirect_filter = g_strdup(value);
    return TRUE;
}


/**
 * spice_get_option_group: (skip)
 *
 * Returns: (transfer full): a #GOptionGroup for the commandline
 * arguments specific to Spice.  You have to call
 * spice_set_session_option() after to set the options on a
 * #SpiceSession.
 **/
GOptionGroup* spice_get_option_group(void)
{
    const GOptionEntry entries[] = {
        { "spice-secure-channels", '\0', 0, G_OPTION_ARG_CALLBACK, parse_secure_channels,
          N_("Force the specified channels to be secured"), "<main,display,inputs,...,all>" },
        { "spice-disable-effects", '\0', 0, G_OPTION_ARG_CALLBACK, parse_disable_effects,
          N_("Disable guest display effects"), "<wallpaper,font-smooth,animation,all>" },
        { "spice-color-depth", '\0', 0, G_OPTION_ARG_CALLBACK, parse_color_depth,
          N_("Guest display color depth"), "<16,32>" },
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
        /* Backward compats version of spice-usbredir-auto-redirect-filter */
        { "spice-usbredir-filter", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, parse_usbredir_filter,
          NULL, NULL },
        { "spice-usbredir-auto-redirect-filter", '\0', 0, G_OPTION_ARG_STRING, &usbredir_auto_redirect_filter,
          N_("Filter selecting USB devices to be auto-redirected when plugged in"), N_("<filter-string>") },
        { "spice-usbredir-redirect-on-connect", '\0', 0, G_OPTION_ARG_STRING, &usbredir_redirect_on_connect,
          N_("Filter selecting USB devices to redirect on connect"), N_("<filter-string>") },
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
        g_object_set(session, "disable-effects", disable_effects, NULL);
    }

    if (secure_channels) {
        GStrv channels;
        channels = g_strsplit(secure_channels, ",", -1);
        if (channels)
            g_object_set(session, "secure-channels", channels, NULL);
        g_strfreev(channels);
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
    if (usbredir_auto_redirect_filter) {
        SpiceUsbDeviceManager *m = spice_usb_device_manager_get(session, NULL);
        if (m)
            g_object_set(m, "auto-connect-filter",
                         usbredir_auto_redirect_filter, NULL);
    }
    if (usbredir_redirect_on_connect) {
        SpiceUsbDeviceManager *m = spice_usb_device_manager_get(session, NULL);
        if (m)
            g_object_set(m, "redirect-on-connect",
                         usbredir_redirect_on_connect, NULL);
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
