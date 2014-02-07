/**
 * Copyright (C) 2013 Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "android-spicy.h"
#include "android-spice-widget.h"
#include "virt-viewer-file.h"

#define PTRFLAGS_DOWN 0x8000

#ifdef ANDROID_SERVICE_C
	SpiceDisplay*   global_display = NULL;
	spice_connection*  global_conn = NULL;
	JavaVM*                    jvm = NULL;
	jclass     jni_connector_class = NULL;
	jmethodID jni_settings_changed = NULL;
	jmethodID  jni_graphics_update = NULL;
	GMainLoop            *mainloop = NULL;
	int                connections = 0;
	gboolean          soundEnabled = FALSE;
	const gchar*         oVirtUser = NULL;
	const gchar*     oVirtPassword = NULL;
#else
	extern SpiceDisplay*   global_display;
	extern spice_connection*  global_conn;
	extern JavaVM*                    jvm;
	extern jclass     jni_connector_class;
	extern jmethodID jni_settings_changed;
	extern jmethodID  jni_graphics_update;
	extern GMainLoop*            mainloop;
	extern int                connections;
	extern gboolean          soundEnabled;
	extern gchar*               oVirtUser;
	extern gchar*           oVirtPassword;
#endif

gboolean attachThreadToJvm (JNIEnv** env);
void detachThreadFromJvm ();

int spiceClientConnect (const gchar *h, const gchar *p, const gchar *tp,
		                   const gchar *pw, const gchar *cf, GByteArray *cc,
                           const gchar *cs, const gboolean sound);

int spiceClientConnectVv (VirtViewerFile *vv_file, const gboolean sound);

int connectSession (spice_connection *conn);

int SpiceClientConnect(JNIEnv *env, jobject obj, const gchar *h, const gchar *p,
                           const gchar *tp, const gchar *pw, const gchar *cf, GByteArray *cc, const gchar *cs,
                           const gboolean sound, VirtViewerFile *vv_file);
int CreateOvirtSession(JNIEnv *env, jobject obj, const gchar *uri, const gchar *user, const gchar *password,
                          const gchar *ovirt_ca_file, const gboolean sound, const gboolean sslStrict);

extern void __android_log_write(int, char*, char*);

