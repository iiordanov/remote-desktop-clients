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

#include "android-spice-widget.h"

#define PTRFLAGS_DOWN 0x8000

#ifdef ANDROID_SERVICE_C
    SpiceDisplay* global_display   = NULL;
    gboolean  maintainConnection   = TRUE;
    JavaVM*   jvm                  = NULL;
    jclass    jni_connector_class  = NULL;
    jmethodID jni_settings_changed = NULL;
    jmethodID jni_graphics_update  = NULL;
    GMainLoop            *mainloop = NULL;
    int                connections = 0;
    gboolean soundEnabled          = FALSE;
#else
    extern SpiceDisplay* global_display;
    extern gboolean  maintainConnection;
    extern JavaVM*   jvm;
    extern jclass    jni_connector_class;
    extern jmethodID jni_settings_changed;
    extern jmethodID jni_graphics_update;
    extern GMainLoop *mainloop;
    extern int       connections;
    extern gboolean soundEnabled;
#endif
