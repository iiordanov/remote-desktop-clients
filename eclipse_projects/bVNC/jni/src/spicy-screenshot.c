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
static const char *outf      = "spicy-screenshot.ppm";
static gboolean version = FALSE;

/* state */
static SpiceSession  *session;
static GMainLoop     *mainloop;

enum SpiceSurfaceFmt d_format;
gint                 d_width, d_height, d_stride;
gpointer             d_data;

/* ------------------------------------------------------------------ */

static void primary_create(SpiceChannel *channel, gint format,
                           gint width, gint height, gint stride,
                           gint shmid, gpointer imgdata, gpointer data)
{
    SPICE_DEBUG("%s: %dx%d, format %d", __FUNCTION__, width, height, format);
    d_format = format;
    d_width  = width;
    d_height = height;
    d_stride = stride;
    d_data   = imgdata;
}

static int write_ppm_32(void)
{
    FILE *fp;
    uint8_t *p;
    int n;

    fp = fopen(outf,"w");
    if (NULL == fp) {
	fprintf(stderr, _("%s: can't open %s: %s\n"), g_get_prgname(), outf, strerror(errno));
	return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n",
            d_width, d_height);
    n = d_width * d_height;
    p = d_data;
    while (n > 0) {
        fputc(p[2], fp);
        fputc(p[1], fp);
        fputc(p[0], fp);
        p += 4;
        n--;
    }
    fclose(fp);
    return 0;
}

static void invalidate(SpiceChannel *channel,
                       gint x, gint y, gint w, gint h, gpointer *data)
{
    int rc;

    switch (d_format) {
    case SPICE_SURFACE_FMT_32_xRGB:
        rc = write_ppm_32();
        break;
    default:
        fprintf(stderr, _("unsupported spice surface format %d\n"), d_format);
        rc = -1;
        break;
    }
    if (rc == 0)
        fprintf(stderr, _("wrote screen shot to %s\n"), outf);
    g_main_loop_quit(mainloop);
}

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
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(main_channel_event), data);
        return;
    }

    if (!SPICE_IS_DISPLAY_CHANNEL(channel))
        return;

    g_object_get(channel, "channel-id", &id, NULL);
    if (id != 0)
        return;

    g_signal_connect(channel, "display-primary-create",
                     G_CALLBACK(primary_create), NULL);
    g_signal_connect(channel, "display-invalidate",
                     G_CALLBACK(invalidate), NULL);
    spice_channel_connect(channel);
}

/* ------------------------------------------------------------------ */

static GOptionEntry app_entries[] = {
    {
        .long_name        = "out-file",
        .short_name       = 'o',
        .arg              = G_OPTION_ARG_FILENAME,
        .arg_data         = &outf,
        .description      = N_("Output file name (default spicy-screenshot.ppm)"),
        .arg_description  = N_("<filename>"),
    },
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

int main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;

    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    /* parse opts */
    context = g_option_context_new(_(" - make screen shots"));
    g_option_context_set_summary(context, _("A Spice server client to take screenshots in ppm format."));
    g_option_context_set_description(context, _("Report bugs to " PACKAGE_BUGREPORT "."));
    g_option_context_set_main_group(context, spice_cmdline_get_option_group());
    g_option_context_add_main_entries(context, app_entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print(_("option parsing failed: %s\n"), error->message);
        exit(1);
    }

    if (version) {
        g_print("%s " PACKAGE_VERSION "\n", g_get_prgname());
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
    return 0;
}
