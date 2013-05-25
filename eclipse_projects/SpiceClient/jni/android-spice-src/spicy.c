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
void spice_session_setup(JNIEnv *env, SpiceSession *session, jstring h, jstring p, jstring pw);

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
static spice_window *create_spice_window(spice_connection *conn, int id, SpiceChannel *channel)
{
    struct spice_window *win;

    win = g_new0(struct spice_window, 1);
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

static void main_channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                               gpointer data)
{
    spice_connection *conn = data;
    char password[64];
    int rc = 0;

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
    //                     G_CALLBACK(usb_connect_failed), NULL);
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

void Java_com_keqisoft_android_spice_socket_Connector_AndroidSpicecDisconnect(JNIEnv * env, jobject  obj) {
	maintainConnection = FALSE;
	// TODO: For some reason, connection_disconnect does not work properly.
	// Not calling it may leave a connection running.
	//connection_disconnect(conn);
    if (g_main_loop_is_running (mainloop))
        g_main_loop_quit (mainloop);
}

jint Java_com_keqisoft_android_spice_socket_Connector_AndroidSpicec(JNIEnv *env, jobject obj, jstring h, jstring p, jstring pw)
{
    int result = 0;
    maintainConnection = TRUE;

	// Get a reference to the JVM to get JNIEnv from in (other) threads.
    jint rs = (*env)->GetJavaVM(env, &jvm);
    if (rs != JNI_OK) {
    	__android_log_write(6, "spicy", "ERROR: Could not obtain jvm reference.");
    	return 255;
    }

    // Find the jclass reference and get a Global reference for it for use in other threads.
    jclass local_class  = (*env)->FindClass (env, "com/keqisoft/android/spice/socket/Connector");
	jni_connector_class = (jclass)((*env)->NewGlobalRef(env, local_class));

	// Get global method IDs for callback methods.
	jni_settings_changed = (*env)->GetStaticMethodID (env, jni_connector_class, "OnSettingsChanged", "(IIII)V");
	jni_graphics_update  = (*env)->GetStaticMethodID (env, jni_connector_class, "OnGraphicsUpdate", "(IIIII)V");

    g_thread_init(NULL);
    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    spice_connection *conn;
    conn = connection_new();
    //spice_set_session_option(conn->session);
    spice_session_setup(env, conn->session, h, p, pw);
    connection_connect(conn);

    if (connections > 0) {
        g_main_loop_run(mainloop);
        connection_disconnect(conn);
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

void spice_session_setup(JNIEnv *env, SpiceSession *session, jstring h, jstring p, jstring pw)
{
	const char *host = NULL;
	const char *port = NULL;
	const char *tls_port = NULL;
	const char *password = NULL;
	host = (*env)->GetStringUTFChars(env, h, NULL);
	port = (*env)->GetStringUTFChars(env, p, NULL);
	password = (*env)->GetStringUTFChars(env, pw, NULL);

    g_return_if_fail(SPICE_IS_SESSION(session));

    __android_log_write(6, "spicy", host);
    __android_log_write(6, "spicy", port);
    __android_log_write(6, "spicy", password);

    if (host)
        g_object_set(session, "host", host, NULL);
    if (port)
        g_object_set(session, "port", port, NULL);
    // TODO: Add TLS support.
    if (tls_port)
        g_object_set(session, "tls-port", tls_port, NULL);
    if (password)
        g_object_set(session, "password", password, NULL);
}

