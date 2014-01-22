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
#include "android-spice-widget-priv.h"
#include "win32keymap.h"
#include "android-io.h"
#include "android-service.h"


void updatePixels (uchar* dest, uchar* source, int x, int y, int width, int height, int buffwidth, int buffheight, int bpp) {
    //char buf[100];
    //snprintf (buf, 100, "Drawing x: %d, y: %d, w: %d, h: %d, wBuf: %d, hBuf: %d, bpp: %d", x, y, width, height, wBuf, hBuf, bpp);
    //__android_log_write(6, "android-io", buf);
    int plen = width * bpp;
    int slen = buffwidth * bpp;
    uchar *sourcepix = (uchar*) &source[(slen * y) + (x * bpp)];
    uchar *destpix   = (uchar*) &dest[(slen * y) + (x * bpp)];

    if (bpp == 4) {
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width * 4; j += 4) {
                destpix[j + 0] = sourcepix[j + 2];
                destpix[j + 1] = sourcepix[j + 1];
                destpix[j + 2] = sourcepix[j + 0];
                destpix[j + 3] = 0xFF;
            }
            sourcepix = sourcepix + slen;
            destpix   = destpix + slen;
        }
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(destpix, sourcepix, plen);
            sourcepix = sourcepix + slen;
            destpix   = destpix + slen;
        }
    }
}

JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_UpdateBitmap (JNIEnv* env, jobject obj, jobject bitmap, gint x, gint y, gint width, gint height) {
    void* pixels;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(global_display);

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        __android_log_write(6, "android-io", "AndroidBitmap_lockPixels() failed!");
        return;
    }
    //__android_log_write(6, "android-io", "Copying new data into pixels.");
    updatePixels (pixels, d->data, x, y, width, height, d->width, d->height, 4);
    AndroidBitmap_unlockPixels(env, bitmap);
}


int win32key2spice (int keycode)
{
    int newKeyCode = keymap_win322xtkbd[keycode];
    /*
    char buf[100];
    snprintf (buf, 100, "Converted win32 key: %d to linux key: %d", keycode, newKeyCode);
    __android_log_write(6, "android-io", buf);
    */
    return newKeyCode;
}

inline bool attachThreadToJvm (JNIEnv** env) {
    bool attached = false;
    int rs2 = 0;
    int rs1 = (*jvm)->GetEnv(jvm, (void**)env, JNI_VERSION_1_6);
    switch (rs1) {
    case JNI_OK:
        break;
    case JNI_EDETACHED:
        rs2 = (*jvm)->AttachCurrentThread(jvm, env, NULL);
        if (rs2 != JNI_OK) {
            __android_log_write(6, "android-io", "ERROR: Could not attach current thread to jvm.");
        } else {
            attached = true;
        }
        break;
    }

    return attached;
}

inline void detachThreadFromJvm () {
    (*jvm)->DetachCurrentThread(jvm);
}

bool attachThreadToJvm (JNIEnv** env);
void detachThreadFromJvm ();


static int update_mask (int button, gboolean down) {
    static int mask;
    int update = 0;
    if (button == SPICE_MOUSE_BUTTON_LEFT)
        update = SPICE_MOUSE_BUTTON_MASK_LEFT;
    else if (button == SPICE_MOUSE_BUTTON_MIDDLE)
        update = SPICE_MOUSE_BUTTON_MASK_MIDDLE;
    else if (button == SPICE_MOUSE_BUTTON_RIGHT)
        update = SPICE_MOUSE_BUTTON_MASK_RIGHT;
    if (down) {
        mask |= update;
    } else {
        mask &= ~update;
    }
    return mask;
}


/* JNI functions related to input (keyboard, mouse), and output (display). */
/***************************************************************************/


JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceRequestResolution(JNIEnv* env, jobject obj, jint x, jint y) {
    SpiceDisplay* display = global_display;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);

    spice_main_update_display(d->main, get_display_id(display), 0, 0, x, y, TRUE);
    spice_main_set_display_enabled(d->main, -1, TRUE);
    // TODO: Sending the monitor config right away may be causing guest OS to shut down.
    /*
    if (spice_main_send_monitor_config(d->main)) {
        __android_log_write(6, "android-io", "Successfully sent monitor config");
    } else {
        __android_log_write(6, "android-io", "Failed to send monitor config");
    }*/
}


JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceKeyEvent(JNIEnv * env, jobject  obj, jboolean down, jint hardware_keycode) {
    SpiceDisplay* display = global_display;
    SpiceDisplayPrivate* d = SPICE_DISPLAY_GET_PRIVATE(display);
    int scancode;

    SPICE_DEBUG("%s %s: keycode: %d", __FUNCTION__, "Key", hardware_keycode);

    if (!d->inputs)
        return;

    scancode = win32key2spice(hardware_keycode);
    //scancode = hardware_keycode;
    if (down) {
        send_key(display, scancode, 1);
    } else {
        send_key(display, scancode, 0);
    }
}


JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceButtonEvent(JNIEnv * env, jobject  obj, jint x, jint y, jint metaState, jint type) {
    SpiceDisplay* display = global_display;
    SpiceDisplayPrivate *d = SPICE_DISPLAY_GET_PRIVATE(display);
    //char buf[60];
    //snprintf (buf, 60, "Pointer event: %d at x: %d, y: %d", type, x, y);
    //__android_log_write(6, "android-io", buf);

    if (!d->inputs || (x >= 0 && x < d->width && y >= 0 && y < d->height)) {

        gboolean down = (type & PTRFLAGS_DOWN) != 0;
        int mouseButton = type &~ PTRFLAGS_DOWN;
        int newMask = update_mask (mouseButton, down);

        gint dx;
        gint dy;
        switch (d->mouse_mode) {
        case SPICE_MOUSE_MODE_CLIENT:
            //__android_log_write(6, "android-io", "spice mouse mode client");
            spice_inputs_position(d->inputs, x, y, d->channel_id, newMask);
            break;
        case SPICE_MOUSE_MODE_SERVER:
            //__android_log_write(6, "android-io", "spice mouse mode server");
            dx = d->mouse_last_x != -1 ? x - d->mouse_last_x : 0;
            dy = d->mouse_last_y != -1 ? y - d->mouse_last_y : 0;
            spice_inputs_motion(d->inputs, dx, dy, newMask);
            d->mouse_last_x = x;
            d->mouse_last_y = y;
            break;
        default:
            g_warn_if_reached();
            break;
        }

        if (mouseButton != SPICE_MOUSE_BUTTON_INVALID) {
            if (down) {
                //__android_log_write(6, "android-io", "Button press");
                spice_inputs_button_press(d->inputs, mouseButton, newMask);
            } else {
                //__android_log_write(6, "android-io", "Button release");
                // This sleep is an ugly hack to prevent stuck buttons after a drag/drop gesture.
                usleep(50000);
                spice_inputs_button_release(d->inputs, mouseButton, newMask);
            }
        }
    }
}


/* Callbacks to the UI layer to draw screen updates and invalidate part of the screen,
 * or to request a new bitmap. */

void uiCallbackInvalidate (SpiceDisplayPrivate *d, gint x, gint y, gint w, gint h) {
    JNIEnv* env;
    bool attached = attachThreadToJvm (&env);

    // Tell the UI that it needs to send in the bitmap to be updated and to redraw.
    (*env)->CallStaticVoidMethod(env, jni_connector_class, jni_graphics_update, 0, x, y, w, h);

    if (attached) {
        detachThreadFromJvm ();
    }
}

void uiCallbackSettingsChanged (gint instance, gint width, gint height, gint bpp) {
    JNIEnv* env;
    bool attached = attachThreadToJvm (&env);

    // Ask for a new bitmap from the UI.
    (*env)->CallStaticVoidMethod(env, jni_connector_class, jni_settings_changed, instance, width, height, bpp);

    if (attached) {
        detachThreadFromJvm ();
    }
}
