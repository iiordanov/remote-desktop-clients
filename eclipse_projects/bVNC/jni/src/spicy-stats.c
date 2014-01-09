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
#include <glib/gi18n.h>

#include "spice-client.h"
#include "spice-common.h"
#include "spice-cmdline.h"

/* config */
static gboolean version = FALSE;

/* state */
static SpiceSession  *session;
static GMainLoop     *mainloop;

/* ------------------------------------------------------------------ */
static void main_channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                               gpointer data)
{
    switch (event) {
    case SPICE_CHANNEL_OPENED:
        break;
    default:
        g_warning("main channel event: %d", event);
        g_main_loop_quit(mainloop);
    }
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer *data)
{
    int id;

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("new main channel");
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(main_channel_event), data);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        g_object_get(channel, "channel-id", &id, NULL);
        if (id != 0)
            return;
    }

    spice_channel_connect(channel);
}

/* ------------------------------------------------------------------ */

static GOptionEntry app_entries[] = {
    {
        .long_name        = "version",
        .arg              = G_OPTION_ARG_NONE,
        .arg_data         = &version,
        .description      = N_("Display version and quit"),
    },
    {
        /* end of list */
    }
};

static void
signal_handler(int signum)
{
    g_main_loop_quit(mainloop);
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;

    signal(SIGINT, signal_handler);

    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    /* parse opts */
    context = g_option_context_new(NULL);
    g_option_context_set_summary(context, _("A Spice client used for testing and measurements."));
    g_option_context_set_description(context, _("Report bugs to " PACKAGE_BUGREPORT "."));
    g_option_context_set_main_group(context, spice_cmdline_get_option_group());
    g_option_context_add_main_entries(context, app_entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print(_("option parsing failed: %s\n"), error->message);
        exit(1);
    }

    if (version) {
        g_print("spicy-stats " PACKAGE_VERSION "\n");
        exit(0);
    }

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    session = spice_session_new();
    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), NULL);
    spice_cmdline_session_setup(session);

    if (!spice_session_connect(session)) {
        fprintf(stderr, _("spice_session_connect failed\n"));
        exit(1);
    }

    g_main_loop_run(mainloop);
    {
        GList *iter, *list = spice_session_get_channels(session);
        gulong total_read_bytes;
        gint  channel_type;
        printf("total bytes read:\n");
        for (iter = list ; iter ; iter = iter->next) {
            g_object_get(iter->data,
                "total-read-bytes", &total_read_bytes,
                "channel-type", &channel_type,
                NULL);
            printf("%s: %lu\n",
                   spice_channel_type_to_string(channel_type),
                   total_read_bytes);
        }
        g_list_free(list);
    }
    return 0;
}
