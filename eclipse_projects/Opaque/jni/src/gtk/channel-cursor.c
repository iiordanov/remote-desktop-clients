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
#include "config.h"

#include "glib-compat.h"
#include "spice-client.h"
#include "spice-common.h"

#include "spice-channel-priv.h"
#include "spice-channel-cache.h"
#include "spice-marshal.h"

/**
 * SECTION:channel-cursor
 * @short_description: update cursor shape and position
 * @title: Cursor Channel
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: channel-cursor.h
 *
 * The Spice protocol defines a set of messages for controlling cursor
 * shape and position on the remote display area. The cursor changes
 * that should be reflected on the display are notified by
 * signals. See for example #SpiceCursorChannel::cursor-set
 * #SpiceCursorChannel::cursor-move signals.
 */

#define SPICE_CURSOR_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_CURSOR_CHANNEL, SpiceCursorChannelPrivate))

typedef struct display_cursor display_cursor;

struct display_cursor {
    SpiceCursorHeader           hdr;
    gboolean                    default_cursor;
    int                         refcount;
    guint32                     data[];
};

struct _SpiceCursorChannelPrivate {
    display_cache               *cursors;
    gboolean                    init_done;
};

enum {
    SPICE_CURSOR_SET,
    SPICE_CURSOR_MOVE,
    SPICE_CURSOR_HIDE,
    SPICE_CURSOR_RESET,

    SPICE_CURSOR_LAST_SIGNAL,
};

static guint signals[SPICE_CURSOR_LAST_SIGNAL];

static display_cursor * display_cursor_ref(display_cursor *cursor);
static void display_cursor_unref(display_cursor *cursor);
static void channel_set_handlers(SpiceChannelClass *klass);

G_DEFINE_TYPE(SpiceCursorChannel, spice_cursor_channel, SPICE_TYPE_CHANNEL)

/* ------------------------------------------------------------------ */

static void spice_cursor_channel_init(SpiceCursorChannel *channel)
{
    SpiceCursorChannelPrivate *c;

    c = channel->priv = SPICE_CURSOR_CHANNEL_GET_PRIVATE(channel);

    c->cursors = cache_new((GDestroyNotify)display_cursor_unref);
}

static void spice_cursor_channel_finalize(GObject *obj)
{
    SpiceCursorChannel *channel = SPICE_CURSOR_CHANNEL(obj);
    SpiceCursorChannelPrivate *c = channel->priv;

    g_clear_pointer(&c->cursors, cache_unref);

    if (G_OBJECT_CLASS(spice_cursor_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_cursor_channel_parent_class)->finalize(obj);
}

/* coroutine context */
static void spice_cursor_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    cache_clear(c->cursors);
    c->init_done = FALSE;

    SPICE_CHANNEL_CLASS(spice_cursor_channel_parent_class)->channel_reset(channel, migrating);
}

static void spice_cursor_channel_class_init(SpiceCursorChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_cursor_channel_finalize;
    channel_class->channel_reset = spice_cursor_channel_reset;

    /**
     * SpiceCursorChannel::cursor-set:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     * @width: width of the shape
     * @height: height of the shape
     * @hot_x: horizontal offset of the 'hotspot' of the cursor
     * @hot_y: vertical offset of the 'hotspot' of the cursor
     * @rgba: 32bits shape data, or %NULL if default cursor. It might
     * be freed after the signal is emitted, so make sure to copy it
     * if you need it later!
     *
     * The #SpiceCursorChannel::cursor-set signal is emitted to modify
     * cursor aspect and position on the display area.
     **/
    signals[SPICE_CURSOR_SET] =
        g_signal_new("cursor-set",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_set),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT_INT_INT_POINTER,
                     G_TYPE_NONE,
                     5,
                     G_TYPE_INT, G_TYPE_INT,
                     G_TYPE_INT, G_TYPE_INT,
                     G_TYPE_POINTER);

    /**
     * SpiceCursorChannel::cursor-move:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     * @x: x position
     * @y: y position
     *
     * The #SpiceCursorChannel::cursor-move signal is emitted to update
     * the cursor position on the display area.
     **/
    signals[SPICE_CURSOR_MOVE] =
        g_signal_new("cursor-move",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_move),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_INT, G_TYPE_INT);

    /**
     * SpiceCursorChannel::cursor-hide:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     *
     * The #SpiceCursorChannel::cursor-hide signal is emitted to hide
     * the cursor/pointer on the display area.
     **/
    signals[SPICE_CURSOR_HIDE] =
        g_signal_new("cursor-hide",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_hide),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /**
     * SpiceCursorChannel::cursor-reset:
     * @cursor: the #SpiceCursorChannel that emitted the signal
     *
     * The #SpiceCursorChannel::cursor-reset signal is emitted to
     * reset the cursor to its default context.
     **/
    signals[SPICE_CURSOR_RESET] =
        g_signal_new("cursor-reset",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceCursorChannelClass, cursor_reset),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(SpiceCursorChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
}

/* ------------------------------------------------------------------ */

#ifdef DEBUG_CURSOR
static void print_cursor(display_cursor *cursor, const guint8 *data)
{
    int x, y, bpl;
    const guint8 *xor, *and;

    bpl = (cursor->hdr.width + 7) / 8;
    and = data;
    xor = and + bpl * cursor->hdr.height;

    printf("data (%d x %d):\n", cursor->hdr.width, cursor->hdr.height);
    for (y = 0 ; y < cursor->hdr.height; ++y) {
        for (x = 0 ; x < cursor->hdr.width / 8; x++) {
            printf("%02X", and[x]);
        }
        and += bpl;
        printf("\n");
    }
    printf("xor:\n");
    for (y = 0 ; y < cursor->hdr.height; ++y) {
        for (x = 0 ; x < cursor->hdr.width / 8; ++x) {
            printf("%02X", xor[x]);
        }
        xor += bpl;
        printf("\n");
    }
}
#endif

static void mono_cursor(display_cursor *cursor, const guint8 *data)
{
    int bpl = (cursor->hdr.width + 7) / 8;
    const guint8 *xor, *and;
    guint8 *dest;
    dest = (uint8_t *)cursor->data;

#ifdef DEBUG_CURSOR
    print_cursor(cursor, data);
#endif
    and = data;
    xor = and + bpl * cursor->hdr.height;
    spice_mono_edge_highlight(cursor->hdr.width, cursor->hdr.height,
                              and, xor, dest);
}

static guint8 get_pix_mask(const guint8 *data, gint offset, gint pix_index)
{
    return data[offset + (pix_index >> 3)] & (0x80 >> (pix_index % 8));
}

static guint32 get_pix_hack(gint pix_index, gint width)
{
    return (((pix_index % width) ^ (pix_index / width)) & 1) ? 0xc0303030 : 0x30505050;
}

static display_cursor * display_cursor_ref(display_cursor *cursor)
{
    g_return_val_if_fail(cursor != NULL, NULL);
    g_return_val_if_fail(cursor->refcount > 0, NULL);

    cursor->refcount++;
    return cursor;
}

static void display_cursor_unref(display_cursor *cursor)
{
    g_return_if_fail(cursor != NULL);
    g_return_if_fail(cursor->refcount > 0);

    cursor->refcount--;
    if (cursor->refcount == 0)
        g_free(cursor);
}

static const char *cursor_type_to_string(int type)
{
    switch (type) {
    case SPICE_CURSOR_TYPE_MONO:
        return "mono";
    case SPICE_CURSOR_TYPE_ALPHA:
        return "alpha";
    case SPICE_CURSOR_TYPE_COLOR32:
        return "color32";
    case SPICE_CURSOR_TYPE_COLOR16:
        return "color16";
    case SPICE_CURSOR_TYPE_COLOR4:
        return "color4";
    }
    return "unknown";
}

static display_cursor *set_cursor(SpiceChannel *channel, SpiceCursor *scursor)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    SpiceCursorHeader *hdr = &scursor->header;
    display_cursor *cursor;
    size_t size;
    gint i, pix_mask, pix;
    const guint8* data;
    guint8 *rgba;
    guint8 val;

    CHANNEL_DEBUG(channel, "%s: flags %d, size %d", __FUNCTION__,
                  scursor->flags, scursor->data_size);

    if (scursor->flags & SPICE_CURSOR_FLAGS_NONE)
        return NULL;

    CHANNEL_DEBUG(channel, "%s: type %s(%d), %" PRIx64 ", %dx%d", __FUNCTION__,
                  cursor_type_to_string(hdr->type), hdr->type, hdr->unique,
                  hdr->width, hdr->height);

    if (scursor->flags & SPICE_CURSOR_FLAGS_FROM_CACHE) {
        cursor = cache_find(c->cursors, hdr->unique);
        g_return_val_if_fail(cursor != NULL, NULL);
        return display_cursor_ref(cursor);
    }

    g_return_val_if_fail(scursor->data_size != 0, NULL);

    size = 4u * hdr->width * hdr->height;
    cursor = g_malloc0(sizeof(*cursor) + size);
    cursor->hdr = *hdr;
    cursor->default_cursor = FALSE;
    cursor->refcount = 1;
    data = scursor->data;

    switch (hdr->type) {
    case SPICE_CURSOR_TYPE_MONO:
        mono_cursor(cursor, data);
        break;
    case SPICE_CURSOR_TYPE_ALPHA:
        memcpy(cursor->data, data, size);
        break;
    case SPICE_CURSOR_TYPE_COLOR32:
        memcpy(cursor->data, data, size);
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size, i);
            if (pix_mask && *((guint32*)data + i) == 0xffffff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] |= (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR16:
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size, i);
            pix = *((guint16*)data + i);
            if (pix_mask && pix == 0x7fff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] |= ((pix & 0x1f) << 3) | ((pix & 0x3e0) << 6) |
                    ((pix & 0x7c00) << 9) | (pix_mask ? 0 : 0xff000000);
            }
        }
        break;
    case SPICE_CURSOR_TYPE_COLOR4:
        size = ((unsigned int)(SPICE_ALIGN(hdr->width, 2) / 2)) * hdr->height;
        for (i = 0; i < hdr->width * hdr->height; i++) {
            pix_mask = get_pix_mask(data, size + (sizeof(uint32_t) << 4), i);
            int idx = (i & 1) ? (data[i >> 1] & 0x0f) : ((data[i >> 1] & 0xf0) >> 4);
            pix = *((uint32_t*)(data + size) + idx);
            if (pix_mask && pix == 0xffffff) {
                cursor->data[i] = get_pix_hack(i, hdr->width);
            } else {
                cursor->data[i] = pix | (pix_mask ? 0 : 0xff000000);
            }
        }

        break;
    default:
        g_warning("%s: unimplemented cursor type %d", __FUNCTION__,
                  hdr->type);
        cursor->default_cursor = TRUE;
        goto cache_add;
    }

    rgba = (guint8*)cursor->data;
    for (i = 0; i < hdr->width * hdr->height; i++) {
        val = rgba[0];
        rgba[0] = rgba[2];
        rgba[2] = val;
        rgba += 4;
    }

cache_add:
    if (scursor->flags & SPICE_CURSOR_FLAGS_CACHE_ME) {
        cache_add(c->cursors, hdr->unique, display_cursor_ref(cursor));
    }

    return cursor;
}

/* coroutine context */
static void emit_cursor_set(SpiceChannel *channel, display_cursor *cursor)
{
    g_return_if_fail(cursor != NULL);
    g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_SET], 0,
                            cursor->hdr.width, cursor->hdr.height,
                            cursor->hdr.hot_spot_x, cursor->hdr.hot_spot_y,
                            cursor->default_cursor ? NULL : cursor->data);
}

/* coroutine context */
static void cursor_handle_init(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgCursorInit *init = spice_msg_in_parsed(in);
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    display_cursor *cursor;

    g_return_if_fail(c->init_done == FALSE);

    cache_clear(c->cursors);
    cursor = set_cursor(channel, &init->cursor);
    c->init_done = TRUE;
    if (cursor)
        emit_cursor_set(channel, cursor);
    if (!init->visible || !cursor)
        g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_HIDE], 0);
    if (cursor)
        display_cursor_unref(cursor);
}

/* coroutine context */
static void cursor_handle_reset(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    CHANNEL_DEBUG(channel, "%s, init_done: %d", __FUNCTION__, c->init_done);

    cache_clear(c->cursors);
    g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_RESET], 0);
    c->init_done = FALSE;
}

/* coroutine context */
static void cursor_handle_set(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgCursorSet *set = spice_msg_in_parsed(in);
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    display_cursor *cursor;

    g_return_if_fail(c->init_done == TRUE);

    cursor = set_cursor(channel, &set->cursor);
    if (cursor)
        emit_cursor_set(channel, cursor);
    else
        g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_HIDE], 0);


    if (cursor)
        display_cursor_unref(cursor);
}

/* coroutine context */
static void cursor_handle_move(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgCursorMove *move = spice_msg_in_parsed(in);
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);

    g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_MOVE], 0,
                            move->position.x, move->position.y);
}

/* coroutine context */
static void cursor_handle_hide(SpiceChannel *channel, SpiceMsgIn *in)
{
#ifdef EXTRA_CHECKS
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);
#endif

    g_coroutine_signal_emit(channel, signals[SPICE_CURSOR_HIDE], 0);
}

/* coroutine context */
static void cursor_handle_trail(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    g_return_if_fail(c->init_done == TRUE);

    g_warning("%s: TODO", __FUNCTION__);
}

/* coroutine context */
static void cursor_handle_inval_one(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;
    SpiceMsgDisplayInvalOne *zap = spice_msg_in_parsed(in);

    g_return_if_fail(c->init_done == TRUE);

    cache_remove(c->cursors, zap->id);
}

/* coroutine context */
static void cursor_handle_inval_all(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceCursorChannelPrivate *c = SPICE_CURSOR_CHANNEL(channel)->priv;

    cache_clear(c->cursors);
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_CURSOR_INIT ]              = cursor_handle_init,
        [ SPICE_MSG_CURSOR_RESET ]             = cursor_handle_reset,
        [ SPICE_MSG_CURSOR_SET ]               = cursor_handle_set,
        [ SPICE_MSG_CURSOR_MOVE ]              = cursor_handle_move,
        [ SPICE_MSG_CURSOR_HIDE ]              = cursor_handle_hide,
        [ SPICE_MSG_CURSOR_TRAIL ]             = cursor_handle_trail,
        [ SPICE_MSG_CURSOR_INVAL_ONE ]         = cursor_handle_inval_one,
        [ SPICE_MSG_CURSOR_INVAL_ALL ]         = cursor_handle_inval_all,
    };

    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}
