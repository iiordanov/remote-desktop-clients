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
#include <glib/gi18n.h>

#include "spice-client.h"
#include "spice-common.h"
#include "spice-cmdline.h"

static char *host;
static char *port;
static char *tls_port;
static char *password;
static char *uri;

static GOptionEntry spice_entries[] = {
    {
        .long_name        = "uri",
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &uri,
        .description      = N_("Spice server uri"),
        .arg_description  = N_("<uri>"),
    },{
        .long_name        = "host",
        .short_name       = 'h',
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &host,
        .description      = N_("Spice server address"),
        .arg_description  = N_("<host>"),
    },{
        .long_name        = "port",
        .short_name       = 'p',
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &port,
        .description      = N_("Spice server port"),
        .arg_description  = N_("<port>"),
    },{
        .long_name        = "secure-port",
        .short_name       = 's',
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &tls_port,
        .description      = N_("Spice server secure port"),
        .arg_description  = N_("<port>"),
    },{
        .long_name        = "password",
        .short_name       = 'w',
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &password,
        .description      = N_("Server password"),
        .arg_description  = N_("<password>"),
    },{
        /* end of list */
    }
};

GOptionGroup *spice_cmdline_get_option_group(void)
{
    GOptionGroup *grp;

    grp = g_option_group_new("spice",
                             _("Spice connection options:"),
                             _("Show Spice options"),
                             NULL, NULL);
    g_option_group_add_entries(grp, spice_entries);

    return grp;
}

void spice_cmdline_session_setup(SpiceSession *session)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    if (uri)
        g_object_set(session, "uri", uri, NULL);
    if (host)
        g_object_set(session, "host", host, NULL);
    if (port)
        g_object_set(session, "port", port, NULL);
    if (tls_port)
        g_object_set(session, "tls-port", tls_port, NULL);
    if (password)
        g_object_set(session, "password", password, NULL);
}
