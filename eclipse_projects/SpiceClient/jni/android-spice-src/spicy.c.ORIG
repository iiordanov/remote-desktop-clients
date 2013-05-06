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

#include <sys/stat.h>
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
};

struct spice_connection {
    SpiceSession     *session;
    spice_window     *wins[4];
    SpiceAudio       *audio;
    char             *mouse_state;
    char             *agent_state;
    int              channels;
    int              disconnecting;
};

//for android-workers threads
volatile GMainLoop* android_mainloop;
volatile JpegEncoder* android_jpeg_encoder;
volatile int android_task_ready;
volatile int android_task;
pthread_mutex_t android_mutex = PTHREAD_MUTEX_INITIALIZER;  
pthread_cond_t android_cond = PTHREAD_COND_INITIALIZER;  

static GMainLoop     *mainloop;
static int           connections;

static spice_connection *connection_new(void);
static void connection_connect(spice_connection *conn);
static void connection_disconnect(spice_connection *conn);
static void connection_destroy(spice_connection *conn);

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
static spice_window *create_spice_window(spice_connection *conn, int id)
{
    struct spice_window *win;

    win = malloc(sizeof(*win));
    if (NULL == win)
	return NULL;
    memset(win,0,sizeof(*win));
    win->id = id;
    win->conn = conn;
    g_message("create window (#%d)", win->id);

    win->spice = (spice_display_new(conn->session, id));
    return win;
}

static void destroy_spice_window(spice_window *win)
{
    SPICE_DEBUG("destroy window (#%d)", win->id);
    //gtk_widget_destroy(win->toplevel);
    free(win);
    exit(0);
}

/* ------------------------------------------------------------------ */

static void main_channel_event(SpiceChannel *channel, SpiceChannelEvent event,
	gpointer data)
{
    spice_connection *conn = data;
    char password[64];
    int rc;

    switch (event) {
	case SPICE_CHANNEL_OPENED:
	    g_message("main channel: opened");
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
	    //_("Please enter the spice server password"),
	    //password, sizeof(password), true);
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

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
	SPICE_DEBUG("new main channel");
	g_signal_connect(channel, "channel-event",
		G_CALLBACK(main_channel_event), conn);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
	if (id >= SPICE_N_ELEMENTS(conn->wins))
	    return;
	if (conn->wins[id] != NULL)
	    return;
	SPICE_DEBUG("new display channel (#%d)", id);
	conn->wins[id] = create_spice_window(conn, id);
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
	SPICE_DEBUG("new inputs channel");
    }

    //if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
    //if (conn->audio != NULL)
    //return;
    //SPICE_DEBUG("new audio channel");
    //conn->audio = spice_audio_new(s, NULL, NULL);
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
    //SPICE_DEBUG("zap audio channel");
    //if (conn->audio != NULL) {
    //g_object_unref(conn->audio);
    //conn->audio = NULL;
    //}
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

    conn = g_new0(spice_connection, 1);
    conn->session = spice_session_new();
    g_signal_connect(conn->session, "channel-new",
	    G_CALLBACK(channel_new), conn);
    g_signal_connect(conn->session, "channel-destroy",
	    G_CALLBACK(channel_destroy), conn);
    g_signal_connect(conn->session, "notify::migration-state",
	    G_CALLBACK(migration_state), conn);
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

#undef C_ANDROID
//#define C_ANDROID
#ifdef C_ANDROID
int spice_main(char* cmd)
#else
jint Java_com_keqisoft_android_spice_socket_Connector_AndroidSpicec(JNIEnv *env, jobject obj,jstring str)
#endif
{
    SPICE_DEBUG("libspicec started");
#ifndef C_ANDROID
    jboolean  b  = true;
    char cmd[128];
    memset(cmd, 0, sizeof(cmd));
    strcpy(cmd ,(char*)(*env)->GetStringUTFChars(env,str, &b));
#endif

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
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
	g_print (_("option parsing failed: %s\n"), error->message);
	exit (1);
    }

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    conn = connection_new();
    spice_cmdline_session_setup(conn->session);
    connection_connect(conn);

    pthread_t android_input, android_output;  
    int  iret1, iret2;  
    android_mainloop = mainloop;

    if (connections > 0)
    {
	SPICE_DEBUG("start I/O threads");
	//start the android workers threads
	iret1 = pthread_create( &android_input, NULL, (void*)android_spice_input, NULL);  
	iret2 = pthread_create( &android_output, NULL, (void*)android_spice_output, NULL);  
	//create jpeg_encoder for the jpg images to JAVA
	android_jpeg_encoder = jpeg_encoder_create();

	g_main_loop_run(mainloop);

	pthread_join(android_input, NULL);  
	pthread_join(android_output, NULL);   
	jpeg_encoder_destroy(android_jpeg_encoder);
	SPICE_DEBUG("stop I/O threads");
    }


    while(--argc)
	free(argv[argc]);
    free(argv);
#ifndef C_ANDROID
    (*env)->ReleaseStringUTFChars(env ,str, NULL);
#endif
    SPICE_DEBUG("libspicec.so over.");
    return 0;
}
