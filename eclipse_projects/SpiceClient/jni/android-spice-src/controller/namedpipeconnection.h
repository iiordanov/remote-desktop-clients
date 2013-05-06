/*
   Copyright (C) 2011 Red Hat, Inc.

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
#ifndef __NAMED_PIPE_CONNECTION_H__
#define __NAMED_PIPE_CONNECTION_H__

#include <gio/gio.h>
#include "namedpipe.h"

G_BEGIN_DECLS

#define SPICE_TYPE_NAMED_PIPE_CONNECTION                     (spice_named_pipe_connection_get_type ())
#define SPICE_NAMED_PIPE_CONNECTION(inst)                    (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                                              SPICE_TYPE_NAMED_PIPE_CONNECTION, SpiceNamedPipeConnection))
#define SPICE_NAMED_PIPE_CONNECTION_CLASS(class)             (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                              SPICE_TYPE_NAMED_PIPE_CONNECTION, SpiceNamedPipeConnectionClass))
#define SPICE_IS_NAMED_PIPE_CONNECTION(inst)                 (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                              SPICE_TYPE_NAMED_PIPE_CONNECTION))
#define SPICE_IS_NAMED_PIPE_CONNECTION_CLASS(class)          (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                              SPICE_TYPE_NAMED_PIPE_CONNECTION))
#define SPICE_NAMED_PIPE_CONNECTION_GET_CLASS(inst)          (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                              SPICE_TYPE_NAMED_PIPE_CONNECTION, SpiceNamedPipeConnectionClass))

typedef struct _SpiceNamedPipeConnection                     SpiceNamedPipeConnection;
typedef struct _SpiceNamedPipeConnectionPrivate              SpiceNamedPipeConnectionPrivate;
typedef struct _SpiceNamedPipeConnectionClass                SpiceNamedPipeConnectionClass;

struct _SpiceNamedPipeConnectionClass
{
  GIOStreamClass parent_class;
};

struct _SpiceNamedPipeConnection
{
  GIOStream parent_instance;
  SpiceNamedPipeConnectionPrivate *priv;
};

GType    spice_named_pipe_connection_get_type                (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __NAMED_PIPE_CONNECTION_H__ */
