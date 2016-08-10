/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Red Hat, Inc.

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
#ifndef __SPICE_WEBDAV_CHANNEL_H__
#define __SPICE_WEBDAV_CHANNEL_H__

#include <gio/gio.h>
#include "spice-client.h"
#include "channel-port.h"

G_BEGIN_DECLS

#define SPICE_TYPE_WEBDAV_CHANNEL            (spice_webdav_channel_get_type())
#define SPICE_WEBDAV_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_WEBDAV_CHANNEL, SpiceWebdavChannel))
#define SPICE_WEBDAV_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_WEBDAV_CHANNEL, SpiceWebdavChannelClass))
#define SPICE_IS_WEBDAV_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_WEBDAV_CHANNEL))
#define SPICE_IS_WEBDAV_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_WEBDAV_CHANNEL))
#define SPICE_WEBDAV_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_WEBDAV_CHANNEL, SpiceWebdavChannelClass))

typedef struct _SpiceWebdavChannel SpiceWebdavChannel;
typedef struct _SpiceWebdavChannelClass SpiceWebdavChannelClass;
typedef struct _SpiceWebdavChannelPrivate SpiceWebdavChannelPrivate;

/**
 * SpiceWebdavChannel:
 *
 * The #SpiceWebdavChannel struct is opaque and should not be accessed directly.
 */
struct _SpiceWebdavChannel {
    SpicePortChannel parent;

    /*< private >*/
    SpiceWebdavChannelPrivate *priv;
    /* Do not add fields to this struct */
};

/**
 * SpiceWebdavChannelClass:
 * @parent_class: Parent class.
 *
 * Class structure for #SpiceWebdavChannel.
 */
struct _SpiceWebdavChannelClass {
    SpicePortChannelClass parent_class;

    /*< private >*/
    /* Do not add fields to this struct */
};

GType spice_webdav_channel_get_type(void);

G_END_DECLS

#endif /* __SPICE_WEBDAV_CHANNEL_H__ */
