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
#ifndef __SPICE_CLIENT_MAIN_CHANNEL_H__
#define __SPICE_CLIENT_MAIN_CHANNEL_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_MAIN_CHANNEL            (spice_main_channel_get_type())
#define SPICE_MAIN_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_MAIN_CHANNEL, SpiceMainChannel))
#define SPICE_MAIN_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_MAIN_CHANNEL, SpiceMainChannelClass))
#define SPICE_IS_MAIN_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_MAIN_CHANNEL))
#define SPICE_IS_MAIN_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_MAIN_CHANNEL))
#define SPICE_MAIN_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_MAIN_CHANNEL, SpiceMainChannelClass))

typedef struct _SpiceMainChannel SpiceMainChannel;
typedef struct _SpiceMainChannelClass SpiceMainChannelClass;
typedef struct _SpiceMainChannelPrivate SpiceMainChannelPrivate;

/**
 * SpiceMainChannel:
 * @parent: Parent instance.
 *
 * The #SpiceMainChannel struct is opaque and should not be accessed directly.
 */
struct _SpiceMainChannel {
    SpiceChannel parent;

    /*< private >*/
    SpiceMainChannelPrivate *priv;
    /* Do not add fields to this struct */
};

/**
 * SpiceMainChannelClass:
 * @parent_class: Parent class.
 * @mouse_update: Signal class handler for the #SpiceMainChannel::mouse-update signal.
 * @agent_update: Signal class handler for the #SpiceMainChannel::agent-update signal.
 *
 * Class structure for #SpiceMainChannel.
 */
struct _SpiceMainChannelClass {
    SpiceChannelClass parent_class;

    /* signals */
    void (*mouse_update)(SpiceChannel *channel);
    void (*agent_update)(SpiceChannel *channel);

    /*< private >*/
    /* Do not add fields to this struct */
};

GType spice_main_channel_get_type(void);

void spice_main_set_display(SpiceMainChannel *channel, int id,
                            int x, int y, int width, int height);
void spice_main_set_display_enabled(SpiceMainChannel *channel, int id, gboolean enabled);
gboolean spice_main_send_monitor_config(SpiceMainChannel *channel);

void spice_main_clipboard_grab(SpiceMainChannel *channel, guint32 *types, int ntypes);
void spice_main_clipboard_release(SpiceMainChannel *channel);
void spice_main_clipboard_notify(SpiceMainChannel *channel, guint32 type, const guchar *data, size_t size);
void spice_main_clipboard_request(SpiceMainChannel *channel, guint32 type);

void spice_main_clipboard_selection_grab(SpiceMainChannel *channel, guint selection, guint32 *types, int ntypes);
void spice_main_clipboard_selection_release(SpiceMainChannel *channel, guint selection);
void spice_main_clipboard_selection_notify(SpiceMainChannel *channel, guint selection, guint32 type, const guchar *data, size_t size);
void spice_main_clipboard_selection_request(SpiceMainChannel *channel, guint selection, guint32 type);

gboolean spice_main_agent_test_capability(SpiceMainChannel *channel, guint32 cap);

G_END_DECLS

#endif /* __SPICE_CLIENT_MAIN_CHANNEL_H__ */
