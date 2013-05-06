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

G_DEFINE_TYPE(SpiceDisplay, spice_display, SPICE_TYPE_CHANNEL);
static SpiceDisplay* android_display;
void android_show(spice_display* d,gint x,gint y,gint w,gint h);
int android_drop_show;

static void disconnect_main(SpiceDisplay *display);
static void disconnect_display(SpiceDisplay *display);
static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data);
static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data);

/* ---------------------------------------------------------------- */
/*
static int d_width,d_height;
static int write_ppm_32(void* d_data)
{
    FILE* fp;
    uint8_t *p;
    int n;
    char* outf ="ahoo.ppm";

    fp = fopen(outf,"w");
    if (NULL == fp) {
	fprintf(stderr, "snappy: can't open %s\n", outf);
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
*/

/*
static void spice_display_destroy(GtkObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_new),
                                         display);
    g_signal_handlers_disconnect_by_func(d->session, G_CALLBACK(channel_destroy),
                                         display);

    disconnect_main(display);
    disconnect_display(display);
    (spice_display_parent_class)->destroy(obj);
}

static void spice_display_finalize(GObject *obj)
{
    SpiceDisplay *display = SPICE_DISPLAY(obj);
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    SPICE_DEBUG("Finalize SpiceDisplay");

    G_OBJECT_CLASS(spice_display_parent_class)->finalize(obj);
}
*/

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

int androidkey2spice(int keycode)
{
    return keymap_android[keycode];
}
gboolean key_event(AndroidEventKey* key)
{
    SpiceDisplay* display = android_display;
    spice_display* d = SPICE_DISPLAY_GET_PRIVATE(display);
    int scancode;

    SPICE_DEBUG("%s %s: keycode: %d", __FUNCTION__, "Key",key->hardware_keycode);

    if (!d->inputs)
	return true;

    scancode = androidkey2spice(key->hardware_keycode);
    //scancode = key->hardware_keycode;
    switch (key->type) {
	case ANDROID_KEY_PRESS:
	    if(scancode>100){//along with KEY_SHIFT
		send_key(display, 42, 1);
		send_key(display, scancode-100, 1);
	    } else{
		send_key(display, scancode, 1);
	    }
	    break;
	case ANDROID_KEY_RELEASE:
	    if(scancode>100){//along with KEY_SHIFT
		send_key(display, 42, 0);
		send_key(display, scancode-100, 0);
	    } else{
		send_key(display, scancode, 0);
	    }
	    break;

	default:
	    break;
    }

    return true;
}
/*
   static int button_mask_to_spice(int gdk)
   {

   int spice = 0;

   if (gdk & ANDROID_BUTTON1_MASK)
   spice |= SPICE_MOUSE_BUTTON_MASK_LEFT;
   if (gdk & ANDROID_BUTTON2_MASK)
   spice |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
   if (gdk & ANDROID_BUTTON3_MASK)
   spice |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
   return spice;
   }
   */
gboolean button_event(AndroidEventButton *button)
{
    SpiceDisplay* display = android_display;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    SPICE_DEBUG("%s:x:%d,y:%d", 
	    button->type == ANDROID_BUTTON_PRESS?"Button press":"Button release",button->x,button->y);

    if (!d->inputs)
	return true;

    //the motion (x,y) shall be saned by Java
    switch (button->type) {
	case ANDROID_BUTTON_PRESS:
	    if (button->x >= 0 && button->x < d->width && button->y >= 0 && button->y < d->height) {
		spice_inputs_position(d->inputs, button->x, button->y, 0, 0);
		spice_inputs_button_press(d->inputs,SPICE_MOUSE_BUTTON_LEFT, 0);
	    }
	    break;
	case ANDROID_BUTTON_RELEASE:
	    //usleep(50000);
	    if (button->x >= 0 && button->x < d->width && button->y >= 0 && button->y < d->height) {
		spice_inputs_position(d->inputs, button->x, button->y, 0, 1);
		spice_inputs_button_release(d->inputs, SPICE_MOUSE_BUTTON_LEFT, 1);
	    }
	    break;
	default:
	    break;
    }
    return true;
}

void show_event(spice_display* d,gint x,gint y,gint w, gint h)
{
    //drop some tiny but annoying updating caused by QXL to 
    //low the data flow for android.
    if(!(android_drop_show&(w*h < d->width)))
	android_show(d, x, y, w, h);
    android_drop_show = (w*h < d->width);
}

/* ---------------------------------------------------------------- */


static void primary_create(SpiceChannel *channel, gint format,
	gint width, gint height, gint stride,
	gint shmid, gpointer imgdata, gpointer data)
{
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
    if (!do_color_convert(display, x, y, w, h))
	return;

    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    show_event(d, x, y, w, h);
    //fprintf(stderr,"%s:%s:%d:%p\n\t%d:%d:%d:%d\n",__FILE__,
    //__FUNCTION__,__LINE__,(char*)data,w,h,x,y);
    //write_ppm_32(d->data);
}

static void mark(SpiceChannel *channel, gint mark, gpointer data)
{
    SpiceDisplay *display = data;
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);
    d->mark = mark;
    //show_event(d, x, y, w, h);
}


static void disconnect_main(SpiceDisplay *display)
{
    spice_display *d = SPICE_DISPLAY_GET_PRIVATE(display);

    if (d->main == NULL)
	return;
    //g_signal_handlers_disconnect_by_func(d->main, G_CALLBACK(mouse_update),
    //display);
    d->main = NULL;
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
    //return;
    //}

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
	d->inputs = SPICE_INPUTS_CHANNEL(channel);
	spice_channel_connect(channel);
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
    //if (id != d->channel_id)
    //return;
    //disconnect_cursor(display);
    //return;
    //}

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
	d->inputs = NULL;
	return;
    }

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

    display = g_object_new(SPICE_TYPE_DISPLAY, NULL);
    d = SPICE_DISPLAY_GET_PRIVATE(display);
    d->session = session;
    d->channel_id = id;
    SPICE_DEBUG("channel_id:%d",d->channel_id);

    g_signal_connect(session, "channel-new",
	    G_CALLBACK(channel_new), display);
    g_signal_connect(session, "channel-destroy",
	    G_CALLBACK(channel_destroy), display);
    list = spice_session_get_channels(session);
    for (list = g_list_first(list); list != NULL; list = g_list_next(list)) {
	channel_new(session, list->data, (gpointer*)display);
    }
    g_list_free(list);

    return display;
}
