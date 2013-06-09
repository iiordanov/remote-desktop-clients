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


void copy_pixel_buffer(uchar* dest, uchar* source, int x, int y, int width, int height, int buffwidth, int buffheight, int bpp) {
	//char buf[100];
    //snprintf (buf, 100, "Drawing x: %d, y: %d, w: %d, h: %d, wBuf: %d, hBuf: %d, bpp: %d", x, y, width, height, wBuf, hBuf, bpp);
	//__android_log_write(6, "android-spice", buf);
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


gboolean update_bitmap (JNIEnv* env, jobject* bitmap, void *source, gint x, gint y, gint width, gint height, gint sourceWidth, gint sourceHeight) {
	void* pixels;
	if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
		__android_log_write(6, "android-spice", "AndroidBitmap_lockPixels() failed!");
		return FALSE;
	}
	//__android_log_write(6, "android-spice", "Copying new data into pixels.");
	copy_pixel_buffer(pixels, source, x, y, width, height, sourceWidth, sourceHeight, 4);
	AndroidBitmap_unlockPixels(env, bitmap);
	return TRUE;
}


int win32key2spice (int keycode)
{
	int newKeyCode = keymap_win322xtkbd[keycode];
	/*
	char buf[100];
    snprintf (buf, 100, "Converted win32 key: %d to linux key: %d", keycode, newKeyCode);
	__android_log_write(6, "android-spice", buf);
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
    		__android_log_write(6, "android-spice", "ERROR: Could not attach current thread to jvm.");
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

JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceSetBitmap(JNIEnv* env, jobject obj, jobject bitmap) {
	__android_log_write(6, "android-spice", "Setting new jbitmap from Java.");
	jbitmap = bitmap;

	// TODO: Can I lock all the pixels and make every 4th uint8 0xFF now and avoid doing so when
	// copying the pixels? It should give me a 25% performance boost when copying the pixels.
}


JNIEXPORT void JNICALL
Java_com_iiordanov_aSPICE_SpiceCommunicator_SpiceKeyEvent(JNIEnv * env, jobject  obj, jboolean down, jint hardware_keycode) {
    SpiceDisplay* display = global_display;
    spice_display* d = SPICE_DISPLAY_GET_PRIVATE(display);
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
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    //char buf[60];
    //snprintf (buf, 60, "Pointer event: %d at x: %d, y: %d", type, x, y);
    //__android_log_write(6, "android-spice", buf);

    if (!d->inputs || (x >= 0 && x < d->width && y >= 0 && y < d->height)) {

		gboolean down = (type & PTRFLAGS_DOWN) != 0;
		int mouseButton = type &~ PTRFLAGS_DOWN;
		int newMask = update_mask (mouseButton, down);

		spice_inputs_position(d->inputs, x, y, d->channel_id, newMask);
/*		// TODO: Figure out why this code isn't working.
		gint dx;
		gint dy;
	    switch (d->mouse_mode) {
	    case SPICE_MOUSE_MODE_CLIENT:
			spice_inputs_position(d->inputs, button->x, button->y, d->channel_id, newMask);
	        break;
	    case SPICE_MOUSE_MODE_SERVER:
	        dx = d->mouse_last_x != -1 ? button->x - d->mouse_last_x : 0;
	        dy = d->mouse_last_y != -1 ? button->y - d->mouse_last_y : 0;
	        spice_inputs_motion(d->inputs, dx, dy, newMask);
	        d->mouse_last_x = button->x;
	        d->mouse_last_y = button->y;
	        break;
	    default:
	        g_warn_if_reached();
	        break;
	    }
*/

		if (mouseButton != SPICE_MOUSE_BUTTON_INVALID) {
			if (down) {
			    //__android_log_write(6, "android-spice", "Button press");
				spice_inputs_button_press(d->inputs, mouseButton, newMask);
			} else {
			    //__android_log_write(6, "android-spice", "Button release");
			    // This sleep is an ugly hack to prevent stuck buttons after a drag/drop gesture.
			    usleep(50000);
				spice_inputs_button_release(d->inputs, mouseButton, newMask);
			}
		}
    }
}


/* Callbacks to the UI layer to request invalidation or a new bitmap. */

void uiCallbackInvalidate (spice_display *d, gint x, gint y, gint w, gint h) {
    JNIEnv* env;
    bool attached = attachThreadToJvm (&env);

    // Draw the new data into the Java bitmap object.
    update_bitmap(env, jbitmap, d->data, x, y, w, h, d->width, d->height);

    // Tell the UI that it needs to redraw the bitmap.
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
