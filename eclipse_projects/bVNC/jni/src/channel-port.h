/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

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
#ifndef __SPICE_CLIENT_PORT_CHANNEL_H__
#define __SPICE_CLIENT_PORT_CHANNEL_H__

#include <gio/gio.h>
#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_PORT_CHANNEL            (spice_port_channel_get_type())
#define SPICE_PORT_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_PORT_CHANNEL, SpicePortChannel))
#define SPICE_PORT_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_PORT_CHANNEL, SpicePortChannelClass))
#define SPICE_IS_PORT_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_PORT_CHANNEL))
#define SPICE_IS_PORT_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_PORT_CHANNEL))
#define SPICE_PORT_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_PORT_CHANNEL, SpicePortChannelClass))

typedef struct _SpicePortChannel SpicePortChannel;
typedef struct _SpicePortChannelClass SpicePortChannelClass;
typedef struct _SpicePortChannelPrivate SpicePortChannelPrivate;

/**
 * SpicePortChannel:
 *
 * The #SpicePortChannel struct is opaque and should not be accessed directly.
 */
struct _SpicePortChannel {
    SpiceChannel parent;

    /*< private >*/
    SpicePortChannelPrivate *priv;
    /* Do not add fields to this struct */
};

/**
 * SpicePortChannelClass:
 * @parent_class: Parent class.
 *
 * Class structure for #SpicePortChannel.
 */
struct _SpicePortChannelClass {
    SpiceChannelClass parent_class;

    /*< private >*/
    /* Do not add fields to this struct */
};

GType spice_port_channel_get_type(void);

void spice_port_write_async(SpicePortChannel *port,
                            const void *buffer, gsize count,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);
gssize spice_port_write_finish(SpicePortChannel *port,
                               GAsyncResult *result, GError **error);
void spice_port_event(SpicePortChannel *port, guint8 event);

G_END_DECLS

#endif /* __SPICE_CLIENT_PORT_CHANNEL_H__ */
