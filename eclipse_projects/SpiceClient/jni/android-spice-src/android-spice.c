/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011  Keqisoft,Co,Ltd,Shanghai,China
   Copyright (C) 2011  Shuxiang Lin (shohyanglim@gmail.com)

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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "android-spice.h"
#include "android-spice-priv.h"
#include "androidkeymap.c"
#include "win32keymap.h"
#include <jni.h>
#include <android/bitmap.h>


G_DEFINE_TYPE(SpiceDisplay, spice_display, SPICE_TYPE_CHANNEL);
static SpiceDisplay* android_display;
int android_drop_show;


JNIEXPORT void JNICALL
Java_com_keqisoft_android_spice_socket_Connector_AndroidSetBitmap(JNIEnv* env, jobject obj, jobject bitmap) {
	__android_log_write(6, "android-spice", "Setting new jbitmap from Java.");
	jbitmap = bitmap;
}

typedef unsigned char UINT8;

void copy_pixel_buffer(UINT8* dstBuf, UINT8* srcBuf, int x, int y, int width, int height, int wBuf, int hBuf, int bpp)
{
	//char buf[100];
    //snprintf (buf, 100, "Drawing x: %d, y: %d, w: %d, h: %d, wBuf: %d, hBuf: %d, bpp: %d", x, y, width, height, wBuf, hBuf, bpp);
	//__android_log_write(6, "android-spice", buf);
	int i, j;
	int length;
	int scanline;
	UINT8 *dstp, *srcp;

	length = width * bpp;
	scanline = wBuf * bpp;

	srcp = (UINT8*) &srcBuf[(scanline * y) + (x * bpp)];
	dstp = (UINT8*) &dstBuf[(scanline * y) + (x * bpp)];

	if (bpp == 4) {
		for (i = 0; i < height; i++) {
			for (j = 0; j < width * 4; j += 4) {
				// ARGB <-> ABGR
				dstp[j + 0] = srcp[j + 2];
				dstp[j + 1] = srcp[j + 1];
				dstp[j + 2] = srcp[j + 0];
				dstp[j + 3] = 0xFF;
			}
			srcp += scanline;
			dstp += scanline;
		}
	} else {
		for (i = 0; i < height; i++) {
			memcpy(dstp, srcp, length);
			srcp += scanline;
			dstp += scanline;
		}
	}
}

gboolean update_bitmap (JNIEnv* env, jobject* bitmap, void *source, gint x, gint y, gint width, gint height, gint sourceWidth, gint sourceHeight) {
	int ret;
	void* pixels;

	//AndroidBitmapInfo info;
	//if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
	//	__android_log_write(6, "android-spice", "AndroidBitmap_getInfo() failed!");
	//	//DEBUG_ANDROID("AndroidBitmap_getInfo() failed ! error=%d", ret);
	//	return FALSE;
	//}

	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
		__android_log_write(6, "android-spice", "AndroidBitmap_lockPixels() failed!");
		//DEBUG_ANDROID("AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return FALSE;
	}
	//__android_log_write(6, "android-spice", "Copying new data into pixels.");
	copy_pixel_buffer(pixels, source, x, y, width, height, sourceWidth, sourceHeight, 4);

	AndroidBitmap_unlockPixels(env, bitmap);

	return TRUE;
}

static void disconnect_main(SpiceDisplay *display);
static void disconnect_display(SpiceDisplay *display);
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data);

/* ---------------------------------------------------------------- */


static void spice_display_dispose(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("spice display dispose");

    disconnect_main(display);
    disconnect_display(display);
    //disconnect_cursor(display);

    //if (d->clipboard) {
    //    g_signal_handlers_disconnect_by_func(d->clipboard, G_CALLBACK(clipboard_owner_change),
    //                                         display);
    //    d->clipboard = NULL;
    //}

    //if (d->clipboard_primary) {
    //    g_signal_handlers_disconnect_by_func(d->clipboard_primary, G_CALLBACK(clipboard_owner_change),
    //                                         display);
    //    d->clipboard_primary = NULL;
    //}
    if (d->session) {
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_new),
                                             display);
        g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_destroy),
                                             display);
        g_object_unref(d->session);
        d->session = NULL;
    }
}

static void spice_display_finalize(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i;

    SPICE_DEBUG("Finalize spice display");

    G_OBJECT_CLASS(spice_display_parent_class)->finalize(obj);
}


static void spice_display_class_init(SpiceDisplayClass *klass)
{
    g_type_class_add_private(klass, sizeof(spice_display));
}


static void spice_display_init(SpiceDisplay *display)
{
    android_display = display;
    spice_display *d;

    d = display->priv = SPICE_DISPLAY_GET_PRIVATE(display);
    memset(d, 0, sizeof(*d));
    d->have_mitshm = true;
    d->mouse_last_x = -1;
    d->mouse_last_y = -1;
}


/* ---------------------------------------------------------------- */

#define CONVERT_0565_TO_0888(s)                                         \
    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) |                       \
     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) |                   \
     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#define CONVERT_0565_TO_8888(s) (CONVERT_0565_TO_0888(s) | 0xff000000)

#define CONVERT_0555_TO_0888(s)                                         \
    (((((s) & 0x001f) << 3) | (((s) & 0x001c) >> 2)) |                  \
     ((((s) & 0x03e0) << 6) | (((s) & 0x0380) << 1)) |                  \
     ((((s) & 0x7c00) << 9) | ((((s) & 0x7000)) << 4)))

#define CONVERT_0555_TO_8888(s) (CONVERT_0555_TO_0888(s) | 0xff000000)

static gboolean do_color_convert(SpiceDisplay *display,
                                 gint x, gint y, gint w, gint h)
{
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int i, j, maxy, maxx, miny, minx;
    guint32 *dest = d->data;
    guint16 *src = d->data_origin;

    if (!d->convert)
        return true;

    g_return_val_if_fail(d->format == SPICE_SURFACE_FMT_16_555 ||
                         d->format == SPICE_SURFACE_FMT_16_565, false);

    miny = MAX(y, 0);
    minx = MAX(x, 0);
    maxy = MIN(y + h, d->height);
    maxx = MIN(x + w, d->width);

    dest +=  (d->stride / 4) * miny;
    src += (d->stride / 2) * miny;

    if (d->format == SPICE_SURFACE_FMT_16_555) {
        for (j = miny; j < maxy; j++) {
            for (i = minx; i < maxx; i++) {
                dest[i] = CONVERT_0555_TO_0888(src[i]);
            }

            dest += d->stride / 4;
            src += d->stride / 2;
        }
    } else if (d->format == SPICE_SURFACE_FMT_16_565) {
        for (j = miny; j < maxy; j++) {
            for (i = minx; i < maxx; i++) {
                dest[i] = CONVERT_0565_TO_0888(src[i]);
            }

            dest += d->stride / 4;
            src += d->stride / 2;
        }
    }

    return true;
}

/* ---------------------------------------------------------------- */

static void send_key(SpiceDisplay *display, int scancode, int down)
{
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    uint32_t i, b, m;

    if (!d->inputs)
        return;

    i = scancode / 32;
    b = scancode % 32;
    m = (1 << b);
    g_return_if_fail(i < SPICE_N_ELEMENTS(d->key_state));

    if (down) {
        spice_inputs_key_press(d->inputs, scancode);
        d->key_state[i] |= m;
    } else {
        if (!(d->key_state[i] & m)) {
            return;
        }
        spice_inputs_key_release(d->inputs, scancode);
        d->key_state[i] &= ~m;
    }
}

int win32key2spice (int keycode)
{
	int newKeyCode = keymap_win322xtkbd[keycode];
	//char buf[60];
    //snprintf (buf, 60, "Converted win32 key: %d to linux key: %d", keycode, newKeyCode);
	//__android_log_write(6, "android-spice", buf);
    return newKeyCode;
}

int androidkey2spice(int keycode) {
    return keymap_android[keycode];
}

JNIEXPORT void JNICALL
Java_com_keqisoft_android_spice_socket_Connector_AndroidKeyEvent(JNIEnv * env, jobject  obj, jboolean down, jint hardware_keycode) {
    SpiceDisplay* display = android_display;
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


JNIEXPORT void JNICALL
Java_com_keqisoft_android_spice_socket_Connector_AndroidButtonEvent(JNIEnv * env, jobject  obj, jint x, jint y, jint metaState, jint type) {
    SpiceDisplay* display = android_display;
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

/* ---------------------------------------------------------------- */


static void primary_create(SpiceChannel *channel, gint format, gint width, gint height, gint stride, gint shmid, gpointer imgdata, gpointer data) {
	__android_log_write(6, "android-spice", "primary_create");

    //fprintf(stderr,"%s:%s:%d:%p\n",__FILE__,__FUNCTION__,__LINE__,(char*)data);
    SpiceDisplay *display = data;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    gboolean set_display = FALSE;

    d->format = format;
    d->stride = stride;
    d->shmid  = shmid;
    d->data_origin = d->data = imgdata;

    SPICE_DEBUG("%s:%s:%d:%p\n\t%d:%d\n",__FILE__, __FUNCTION__,__LINE__,(char*)d->data,width,height);
    if (d->width != width || d->height != height) {
	if (d->width != 0 && d->height != 0)
	    set_display = TRUE;
	d->width  = width;
	d->height = height;
	if (!d->resize_guest_enable) {
	}
    }

    JNIEnv* env;
    bool attached = false;
    int rs2 = 0;
    int rs1 = (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_6);
    switch (rs1) {
    case JNI_OK:
    	break;
    case JNI_EDETACHED:
    	rs2 = (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    	if (rs2 != JNI_OK) {
    		__android_log_write(6, "android-spice", "ERROR: Could not attach current thread to jvm.");
    		return;
    	}
    	attached = true;
    	break;
    }

	// Ask for a new bitmap from the UI.
	(*env)->CallStaticVoidMethod(env, jni_connector_class, jni_settings_changed, 0, width, height, 0);

    if (attached) {
    	(*jvm)->DetachCurrentThread(jvm);
    }

    /*
    JNIEnv* env = NULL;
   	jint rs = (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    if (rs != JNI_OK) {
    	__android_log_write(6, "android-spice", "ERROR: Could not attach current thread to jvm.");
    	return;
    }

	// Ask for a new bitmap from the UI.
	(*env)->CallStaticVoidMethod(env, jni_connector_class, jni_settings_changed, 0, width, height, 0);
	*/
	jw = width;
	jh = height;
}

static void primary_destroy(SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = SPICE_DISPLAY(data);
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    //spicex_image_destroy(display);
    d->format = 0;
    d->width  = 0;
    d->height = 0;
    d->stride = 0;
    d->shmid  = 0;
    d->data   = 0;
    d->data_origin = 0;
}

static void invalidate(SpiceChannel *channel,
                       gint x, gint y, gint w, gint h, gpointer data)
{
    SpiceDisplay *display = data;

	if (maintainConnection == FALSE || jbitmap == NULL || x + w > jw || y + h > jh) {
		__android_log_write(6, "android-spice", "Not drawing.");
		return;
	}

    if (!do_color_convert(display, x, y, w, h))
        return;

    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    JNIEnv* env;
    bool attached = false;
    int rs2 = 0;
    int rs1 = (*jvm)->GetEnv(jvm, (void**)&env, JNI_VERSION_1_6);
    switch (rs1) {
    case JNI_OK:
    	break;
    case JNI_EDETACHED:
    	rs2 = (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    	if (rs2 != JNI_OK) {
    		__android_log_write(6, "android-spice", "ERROR: Could not attach current thread to jvm.");
    		return;
    	}
    	attached = true;
    	break;
    }

    // Draw the new data into the Java bitmap object.
    update_bitmap(env, jbitmap, d->data, x, y, w, h, d->width, d->height);

    // Tell the UI that it needs to redraw the bitmap.
	(*env)->CallStaticVoidMethod(env, jni_connector_class, jni_graphics_update, 0, x, y, w, h);

    if (attached) {
    	(*jvm)->DetachCurrentThread(jvm);
    }

    /*
    JNIEnv* env = NULL;
   	jint rs = (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    if (rs != JNI_OK) {
    	__android_log_write(6, "android-spice", "ERROR: Could not attach current thread to jvm.");
    	return;
    }

    // Draw the new data into the Java bitmap object.
    update_bitmap(env, jbitmap, d->data, x, y, w, h, d->width, d->height);

    // Tell the UI that it needs to redraw the bitmap.
	(*env)->CallStaticVoidMethod(env, jni_connector_class, jni_graphics_update, 0, x, y, w, h);
	*/
}

static void mark(SpiceChannel *channel, gint mark, gpointer data) {
	//__android_log_write(6, "android-spice", "mark");
    SpiceDisplay *display = data;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    d->mark = mark;
}


static void disconnect_main(SpiceDisplay *display)
{
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    //gint i;

    if (d->main == NULL)
        return;
    //g_signal_handlers_disconnect_by_func(d->main, G_CALLBACK(mouse_update),
    //                                     display);
    d->main = NULL;
    //for (i = 0; i < CLIPBOARD_LAST; ++i) {
    //    d->clipboard_by_guest[i] = FALSE;
    //    d->clip_grabbed[i] = FALSE;
    //    d->nclip_targets[i] = 0;
    //}
}

static void disconnect_display(SpiceDisplay *display)
{
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->display == NULL)
        return;
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(primary_create),
                                         display);
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(primary_destroy),
                                         display);
    g_signal_handlers_disconnect_by_func(d->display, G_CALLBACK(invalidate),
                                         display);
    d->display = NULL;
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        d->main = SPICE_MAIN_CHANNEL(channel);
        //g_signal_connect(channel, "main-mouse-update",
        //                 G_CALLBACK(mouse_update), display);
        //mouse_update(channel, display);
        //if (id != d->channel_id)
        //    return;
        //g_signal_connect(channel, "main-clipboard-selection-grab",
        //                 G_CALLBACK(clipboard_grab), display);
        //g_signal_connect(channel, "main-clipboard-selection-request",
        //                 G_CALLBACK(clipboard_request), display);
        //g_signal_connect(channel, "main-clipboard-selection-release",
        //                 G_CALLBACK(clipboard_release), display);
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        d->display = channel;
        g_signal_connect(channel, "display-primary-create",
                         G_CALLBACK(primary_create), display);
        g_signal_connect(channel, "display-primary-destroy",
                         G_CALLBACK(primary_destroy), display);
        g_signal_connect(channel, "display-invalidate",
                         G_CALLBACK(invalidate), display);
        g_signal_connect(channel, "display-mark",
                         G_CALLBACK(mark), display);
        spice_channel_connect(channel);
        return;
    }

    //if (SPICE_IS_CURSOR_CHANNEL(channel)) {
    //    if (id != d->channel_id)
    //        return;
    //    d->cursor = SPICE_CURSOR_CHANNEL(channel);
    //    g_signal_connect(channel, "cursor-set",
    //                     G_CALLBACK(cursor_set), display);
    //    g_signal_connect(channel, "cursor-move",
    //                     G_CALLBACK(cursor_move), display);
    //    g_signal_connect(channel, "cursor-hide",
    //                     G_CALLBACK(cursor_hide), display);
    //    g_signal_connect(channel, "cursor-reset",
    //                     G_CALLBACK(cursor_reset), display);
    //    spice_channel_connect(channel);
    //    return;
    //}

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        d->inputs = SPICE_INPUTS_CHANNEL(channel);
        spice_channel_connect(channel);
        //sync_keyboard_lock_modifiers(display);
        return;
    }

    return;
}

static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    SpiceDisplay *display = data;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    SPICE_DEBUG("channel_destroy %d", id);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        disconnect_main(display);
        return;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id != d->channel_id)
            return;
        disconnect_display(display);
        return;
    }

    //if (SPICE_IS_CURSOR_CHANNEL(channel)) {
    //    if (id != d->channel_id)
    //        return;
    //    disconnect_cursor(display);
    //    return;
    //}

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        d->inputs = NULL;
        return;
    }

//#ifdef USE_SMARTCARD
//    if (SPICE_IS_SMARTCARD_CHANNEL(channel)) {
//        d->smartcard = NULL;
//        return;
//    }
//#endif

    return;
}

/**
 * spice_display_new:
 * @session: a #SpiceSession
 * @id: the display channel ID to associate with #SpiceDisplay
 *
 * Returns: a new #SpiceDisplay widget.
 **/
SpiceDisplay *spice_display_new(SpiceSession *session, int id)
{
    SpiceDisplay *display;
    spice_display *d;
    GList *list;
    GList *it;

    display = g_object_new(SPICE_TYPE_DISPLAY, NULL);
    d = SPICE_DISPLAY_GET_PRIVATE(display);
    d->session = g_object_ref(session);
    d->channel_id = id;
    SPICE_DEBUG("channel_id:%d",d->channel_id);

    g_signal_connect(session, "channel-new",
                     G_CALLBACK(channel_new), display);
    g_signal_connect(session, "channel-destroy",
                     G_CALLBACK(channel_destroy), display);
    list = spice_session_get_channels(session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(session, it->data, (gpointer*)display);
    }
    g_list_free(list);

    return display;
}
