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
#ifndef __SPICE_CLIENT_CURSOR_CHANNEL_H__
#define __SPICE_CLIENT_CURSOR_CHANNEL_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_CURSOR_CHANNEL            (spice_cursor_channel_get_type())
#define SPICE_CURSOR_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_CURSOR_CHANNEL, SpiceCursorChannel))
#define SPICE_CURSOR_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_CURSOR_CHANNEL, SpiceCursorChannelClass))
#define SPICE_IS_CURSOR_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_CURSOR_CHANNEL))
#define SPICE_IS_CURSOR_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_CURSOR_CHANNEL))
#define SPICE_CURSOR_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_CURSOR_CHANNEL, SpiceCursorChannelClass))

typedef struct _SpiceCursorChannel SpiceCursorChannel;
typedef struct _SpiceCursorChannelClass SpiceCursorChannelClass;
typedef struct _SpiceCursorChannelPrivate SpiceCursorChannelPrivate;

/**
 * SpiceCursorChannel:
 *
 * The #SpiceCursorChannel struct is opaque and should not be accessed directly.
 */
struct _SpiceCursorChannel {
    SpiceChannel parent;

    /*< private >*/
    SpiceCursorChannelPrivate *priv;
    /* Do not add fields to this struct */
};

/**
 * SpiceCursorChannelClass:
 * @parent_class: Parent class.
 * @cursor_set: Signal class handler for the #SpiceCursorChannel::cursor-set signal.
 * @cursor_move: Signal class handler for the #SpiceCursorChannel::cursor-move signal.
 * @cursor_hide: Signal class handler for the #SpiceCursorChannel::cursor-hide signal.
 * @cursor_reset: Signal class handler for the #SpiceCursorChannel::cursor-reset signal.
 *
 * Class structure for #SpiceCursorChannel.
 */
struct _SpiceCursorChannelClass {
    SpiceChannelClass parent_class;

    /* signals */
    void (*cursor_set)(SpiceCursorChannel *channel, gint width, gint height,
                       gint hot_x, gint hot_y, gpointer rgba);
    void (*cursor_move)(SpiceCursorChannel *channel, gint x, gint y);
    void (*cursor_hide)(SpiceCursorChannel *channel);
    void (*cursor_reset)(SpiceCursorChannel *channel);

    /*< private >*/
    /* Do not add fields to this struct */
};

GType spice_cursor_channel_get_type(void);

G_END_DECLS

#endif /* __SPICE_CLIENT_CURSOR_CHANNEL_H__ */
