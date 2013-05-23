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
#include "config.h"
#endif
#include <glib/gi18n.h>

#include <sys/stat.h>
#define SPICY_C
#include "android-spice.h"
#include "jpeg_encoder.h"
#include "spice-common.h"
#include "spice-audio.h"
#include "spice-cmdline.h"
#include <jni.h>

enum {
    STATE_SCROLL_LOCK,
    STATE_CAPS_LOCK,
    STATE_NUM_LOCK,
    STATE_MAX,
};

typedef struct spice_window spice_window;
typedef struct spice_connection spice_connection;

struct spice_window {
    spice_connection *conn;
    int              id;
    SpiceDisplay      *spice;
    bool             fullscreen;
    bool             mouse_grabbed;
    SpiceChannel     *display_channel;
#ifdef WIN32
    gint             win_x;
    gint             win_y;
#endif
    bool             enable_accels_save;
    bool             enable_mnemonics_save;
};

struct spice_connection {
    SpiceSession     *session;
    spice_window     *wins[4];
    SpiceAudio       *audio;
    char             *mouse_state;
    char             *agent_state;
    gboolean         agent_connected;
    int              channels;
    int              disconnecting;
};

static GMainLoop     *mainloop = NULL;
static int           connections = 0;

static spice_connection *connection_new(void);
static void connection_connect(spice_connection *conn);
static void connection_disconnect(spice_connection *conn);
static void connection_destroy(spice_connection *conn);

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
static spice_window *create_spice_window(spice_connection *conn, int id, SpiceChannel *channel)
{
    struct spice_window *win;

    win = malloc(sizeof(*win));
    if (NULL == win)
	return NULL;
    memset(win,0,sizeof(*win));
    win->id = id;
    win->conn = conn;
    win->display_channel = channel;

    win->spice = (spice_display_new(conn->session, id));
    return win;
}

static void destroy_spice_window(spice_window *win)
{
    SPICE_DEBUG("destroy window (#%d)", win->id);
    //g_object_unref(win->ag);
    //g_object_unref(win->ui);
    //gtk_widget_destroy(win->toplevel);
    free(win);
}

/* ------------------------------------------------------------------ */
/*
static void recent_add(SpiceSession *session)
{
    GtkRecentManager *recent;
    GtkRecentData meta = {
        .mime_type    = "application/x-spice",
        .app_name     = "spicy",
        .app_exec     = "spicy --uri=%u",
    };
    char *uri;

    g_object_get(session, "uri", &uri, NULL);
    SPICE_DEBUG("%s: %s", __FUNCTION__, uri);

    g_return_if_fail(g_str_has_prefix(uri, "spice://"));

    recent = gtk_recent_manager_get_default();
    meta.display_name = uri + 8;
    if (!gtk_recent_manager_add_full(recent, uri, &meta))
        g_warning("Recent item couldn't be added successfully");

    g_free(uri);
}
*/
static void main_channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                               gpointer data)
{
    spice_connection *conn = data;
    char password[64];
    int rc;

    switch (event) {
    case SPICE_CHANNEL_OPENED:
        g_message("main channel: opened");
        //recent_add(conn->session);
        break;
    case SPICE_CHANNEL_SWITCHING:
        g_message("main channel: switching host");
        break;
    case SPICE_CHANNEL_CLOSED:
        /* this event is only sent if the channel was succesfully opened before */
        g_message("main channel: closed");
        connection_disconnect(conn);
        break;
    case SPICE_CHANNEL_ERROR_IO:
        connection_disconnect(conn);
        break;
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_CONNECT:
        g_message("main channel: failed to connect");
        //rc = connect_dialog(conn->session);
        if (rc == 0) {
            connection_connect(conn);
        } else {
            connection_disconnect(conn);
        }
        break;
    case SPICE_CHANNEL_ERROR_AUTH:
        g_warning("main channel: auth failure (wrong password?)");
        strcpy(password, "");
        /* FIXME i18 */
        //rc = ask_user(NULL, _("Authentication"),
        //              _("Please enter the spice server password"),
        //              password, sizeof(password), true);
        if (rc == 0) {
            g_object_set(conn->session, "password", password, NULL);
            connection_connect(conn);
        } else {
            connection_disconnect(conn);
        }
        break;
    default:
        /* TODO: more sophisticated error handling */
        g_warning("unknown main channel event: %d", event);
        /* connection_disconnect(conn); */
        break;
    }
}
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    conn->channels++;
    SPICE_DEBUG("new channel (#%d)", id);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("new main channel");
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(main_channel_event), conn);
        //g_signal_connect(channel, "main-mouse-update",
        //                 G_CALLBACK(main_mouse_update), conn);
        //g_signal_connect(channel, "main-agent-update",
        //                 G_CALLBACK(main_agent_update), conn);
        //main_mouse_update(channel, conn);
        //main_agent_update(channel, conn);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id >= SPICE_N_ELEMENTS(conn->wins))
            return;
        if (conn->wins[id] != NULL)
            return;
        SPICE_DEBUG("new display channel (#%d)", id);
        conn->wins[id] = create_spice_window(conn, id, channel);
        //g_signal_connect(channel, "display-mark",
        //                 G_CALLBACK(display_mark), conn->wins[id]);
        //update_auto_usbredir_sensitive(conn);
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        SPICE_DEBUG("new inputs channel");
        //g_signal_connect(channel, "inputs-modifiers",
        //                 G_CALLBACK(inputs_modifiers), conn);
    }

    //if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
    //    if (conn->audio != NULL)
    //        return;
    //    SPICE_DEBUG("new audio channel");
    //    conn->audio = spice_audio_new(s, NULL, NULL);
    //}

    //if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
    //    update_auto_usbredir_sensitive(conn);
    //}
}

static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("zap main channel");
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id >= SPICE_N_ELEMENTS(conn->wins))
            return;
        if (conn->wins[id] == NULL)
            return;
        SPICE_DEBUG("zap display channel (#%d)", id);
        destroy_spice_window(conn->wins[id]);
        conn->wins[id] = NULL;
    }

    //if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
    //    SPICE_DEBUG("zap audio channel");
    //    if (conn->audio != NULL) {
    //        g_object_unref(conn->audio);
    //        conn->audio = NULL;
    //    }
    //}

    //if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
    //    update_auto_usbredir_sensitive(conn);
    //}

    conn->channels--;
    if (conn->channels > 0) {
        return;
    }

    connection_destroy(conn);
}

static void migration_state(GObject *session,
                            GParamSpec *pspec, gpointer data)
{
    SpiceSessionMigration mig;

    g_object_get(session, "migration-state", &mig, NULL);
    if (mig == SPICE_SESSION_MIGRATION_SWITCHING)
        g_message("migrating session");
}

static spice_connection *connection_new(void)
{
    spice_connection *conn;
    //SpiceUsbDeviceManager *manager;

    conn = g_new0(spice_connection, 1);
    conn->session = spice_session_new();
    //conn->gtk_session = spice_gtk_session_get(conn->session);
    g_signal_connect(conn->session, "channel-new",
                     G_CALLBACK(channel_new), conn);
    g_signal_connect(conn->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), conn);
    g_signal_connect(conn->session, "notify::migration-state",
                     G_CALLBACK(migration_state), conn);

    //manager = spice_usb_device_manager_get(conn->session, NULL);
    //if (manager) {
    //    g_signal_connect(manager, "auto-connect-failed",
    //                     G_CALLBACK(auto_connect_failed), NULL);
    //}

    connections++;
    SPICE_DEBUG("%s (%d)", __FUNCTION__, connections);
    return conn;
}

static void connection_connect(spice_connection *conn)
{
    conn->disconnecting = false;
    spice_session_connect(conn->session);
}

static void connection_disconnect(spice_connection *conn)
{
    if (conn->disconnecting)
        return;
    conn->disconnecting = true;
    spice_session_disconnect(conn->session);
}

static void connection_destroy(spice_connection *conn)
{
    g_object_unref(conn->session);
    free(conn);

    connections--;
    SPICE_DEBUG("%s (%d)", __FUNCTION__, connections);
    if (connections > 0) {
        return;
    }

    g_main_loop_quit(mainloop);
}

/* ------------------------------------------------------------------ */

void cmd_parse(char* cmd,char** argv,int* argc)
{
    char* ch;
    char* loc;
    int i = 0;
    int len = 0;
    loc = ch = cmd;
    bool need_parse = false;

    while(*ch!='\0') {
	if(*ch!=' '&&*ch!='\t') {
	    if(!need_parse) {
		need_parse = true;
		loc = ch;
	    }
	    ch++;
	} else {
	    if(need_parse) {
		need_parse = false;
		len = ch-loc;
		argv[i] = (char*)malloc(len+1);
		memcpy(argv[i],loc,len);
		argv[i][len] = '\0';
		i++;
	    }
	    ch++;
	}
    }
    if(need_parse) {
	need_parse = false;
	len = ch-loc;
	argv[i] = (char*)malloc(len+1);
	memcpy(argv[i],loc,len);
	argv[i][len] = '\0';
	i++;
    }
    *argc = i;
}

void Java_com_keqisoft_android_spice_socket_Connector_AndroidSpicecDisconnect(JNIEnv * env, jobject  obj) {
	maintainConnection = FALSE;
	// TODO: For some reason, connection_disconnect does not work properly.
	// Not calling it may leave a connection running.
	//connection_disconnect(conn);
    if (g_main_loop_is_running (mainloop))
        g_main_loop_quit (mainloop);
}

jint Java_com_keqisoft_android_spice_socket_Connector_AndroidSpicec(JNIEnv *env, jobject obj, jstring str)
{
    SPICE_DEBUG("libspicec started");
    jboolean  b  = true;
    char cmd[128];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd ,(char*)(*env)->GetStringUTFChars(env,str, &b));

    jint rs = (*env)->GetJavaVM(env, &jvm);
    if (rs != JNI_OK) {
    	__android_log_write(6, "spicy", "ERROR: Could not obtain jvm reference.");
    	return 255;
    }

    jclass local_class  = (*env)->FindClass (env, "com/keqisoft/android/spice/socket/Connector");
	jni_connector_class = (jclass)((*env)->NewGlobalRef(env, local_class));

	jni_settings_changed = (*env)->GetStaticMethodID (env, jni_connector_class, "OnSettingsChanged", "(IIII)V");
	jni_graphics_update  = (*env)->GetStaticMethodID (env, jni_connector_class, "OnGraphicsUpdate", "(IIIII)V");

    SPICE_DEBUG("Got cmd:%s",cmd);
    char** argv = (char**)malloc(12*sizeof(char*));
    int argc;
    cmd_parse(cmd,argv,&argc);
    int dex;
    for(dex=0;dex<argc;dex++)
	SPICE_DEBUG("got item:size:%s:%d",argv[dex],strlen(argv[dex]));

    GError *error = NULL;
    GOptionContext *context;
    spice_connection *conn;

    g_thread_init(NULL);
    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
    /* parse opts */
    context = g_option_context_new(_("- spice client application"));
    g_option_context_add_group(context, spice_cmdline_get_option_group());

    int result = 0;
    maintainConnection = TRUE;

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print (_("option parsing failed: %s\n"), error->message);
        return 255;
    }

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    conn = connection_new();
    //spice_set_session_option(conn->session);
    spice_cmdline_session_setup(conn->session);
    connection_connect(conn);

    if (connections > 0) {
        g_main_loop_run(mainloop);

    connection_disconnect(conn);

    // unref was causing segfaults at one point.
    //g_main_loop_unref(mainloop);
	__android_log_write(6, "spicy", "Exiting main loop.");
    } else {
        __android_log_write(6, "spicy", "Wrong hostname, port, or password.");
        result = 2;
    }

	jvm                  = NULL;
	jni_connector_class  = NULL;
	jni_settings_changed = NULL;
	jni_graphics_update  = NULL;
	jbitmap              = NULL;
	jw = 0;
	jh = 0;
	return result;
}
