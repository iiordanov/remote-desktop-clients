/**
 * Copyright (C) 2013 Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include <jni.h>
#include <android/bitmap.h>

#define ANDROID_SERVICE_C
#include "android-service.h"

#include "android-spice-widget.h"
#include "android-spicy.h"


void spice_session_setup(JNIEnv *env, SpiceSession *session, jstring h, jstring p, jstring tp, jstring pw, jstring cf, jstring cs) {
    const char *host = NULL;
    const char *port = NULL;
    const char *tls_port = NULL;
    const char *password = NULL;
    const char *ca_file = NULL;
    const char *cert_subj = NULL;

    host = (*env)->GetStringUTFChars(env, h, NULL);
    port = (*env)->GetStringUTFChars(env, p, NULL);
    tls_port  = (*env)->GetStringUTFChars(env, tp, NULL);
    password  = (*env)->GetStringUTFChars(env, pw, NULL);
    ca_file   = (*env)->GetStringUTFChars(env, cf, NULL);
    cert_subj = (*env)->GetStringUTFChars(env, cs, NULL);

    g_return_if_fail(SPICE_IS_SESSION(session));

    if (host)
        g_object_set(session, "host", host, NULL);
    // If we receive "-1" for a port, we assume the port is not set.
    if (port && strcmp (port, "-1") != 0)
       g_object_set(session, "port", port, NULL);
    if (tls_port && strcmp (tls_port, "-1") != 0)
        g_object_set(session, "tls-port", tls_port, NULL);
    if (password)
        g_object_set(session, "password", password, NULL);
    if (ca_file)
        g_object_set(session, "ca-file", ca_file, NULL);
    if (cert_subj)
        g_object_set(session, "cert-subject", cert_subj, NULL);
}


static void signal_handler(int signal, siginfo_t *info, void *reserved) {
    kill(getpid(), SIGKILL);
}


JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceClientDisconnect (JNIEnv * env, jobject  obj) {
    maintainConnection = FALSE;

    if (g_main_loop_is_running (mainloop))
        g_main_loop_quit (mainloop);
}


JNIEXPORT jint JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceClientConnect (JNIEnv *env, jobject obj, jstring h, jstring p,
                                                                        jstring tp, jstring pw, jstring cf, jstring cs, jboolean sound)
{
    int result = 0;
    maintainConnection = TRUE;
    soundEnabled = sound;

    // Get a reference to the JVM to get JNIEnv from in (other) threads.
    jint rs = (*env)->GetJavaVM(env, &jvm);
    if (rs != JNI_OK) {
        __android_log_write(6, "spicy", "ERROR: Could not obtain jvm reference.");
        return 255;
    }

    // Find the jclass reference and get a Global reference for it for use in other threads.
    jclass local_class  = (*env)->FindClass (env, "com/iiordanov/aSPICE/SpiceCommunicator");
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
    spice_session_setup(env, conn->session, h, p, tp, pw, cf, cs);

    //watch_stdin();

    connection_connect(conn);
    if (connections > 0) {
        g_main_loop_run(mainloop);
        connection_disconnect(conn);
        g_object_unref(mainloop);
        __android_log_write(6, "spicy", "Exiting main loop.");
    } else {
        __android_log_write(6, "spicy", "Wrong hostname, port, or password.");
        result = 2;
    }

    jvm                  = NULL;
    jni_connector_class  = NULL;
    jni_settings_changed = NULL;
    jni_graphics_update  = NULL;
    return result;
}


JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    struct sigaction handler;
    memset(&handler, 0, sizeof(handler));
    handler.sa_sigaction = signal_handler;
    handler.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &handler, NULL);
    sigaction(SIGABRT, &handler, NULL);
    sigaction(SIGBUS, &handler, NULL);
    sigaction(SIGFPE, &handler, NULL);
    sigaction(SIGSEGV, &handler, NULL);
    sigaction(SIGSTKFLT, &handler, NULL);
    return(JNI_VERSION_1_6);
}
