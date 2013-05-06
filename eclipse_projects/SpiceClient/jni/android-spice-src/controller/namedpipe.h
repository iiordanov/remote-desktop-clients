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
#ifndef __NAMED_PIPE_H__
#define __NAMED_PIPE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define SPICE_TYPE_NAMED_PIPE                       (spice_named_pipe_get_type ())
#define SPICE_NAMED_PIPE(inst)                      (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                     SPICE_TYPE_NAMED_PIPE, SpiceNamedPipe))
#define SPICE_NAMED_PIPE_CLASS(class)               (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                     SPICE_TYPE_NAMED_PIPE, SpiceNamedPipeClass))
#define SPICE_IS_NAMED_PIPE(inst)                   (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                     SPICE_TYPE_NAMED_PIPE))
#define SPICE_IS_NAMED_PIPE_CLASS(class)            (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                     SPICE_TYPE_NAMED_PIPE))
#define SPICE_NAMED_PIPE_GET_CLASS(inst)            (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                     SPICE_TYPE_NAMED_PIPE, SpiceNamedPipeClass))

typedef struct _SpiceNamedPipe                       SpiceNamedPipe;
typedef struct _SpiceNamedPipePrivate                SpiceNamedPipePrivate;
typedef struct _SpiceNamedPipeClass                  SpiceNamedPipeClass;

struct _SpiceNamedPipeClass
{
  GObjectClass parent_class;
};

struct _SpiceNamedPipe
{
  GObject parent_instance;
  SpiceNamedPipePrivate *priv;
};

GType            spice_named_pipe_get_type  (void) G_GNUC_CONST;

SpiceNamedPipe * spice_named_pipe_new       (const gchar *name, GError **error);
void *           spice_named_pipe_get_handle(SpiceNamedPipe *namedpipe);
gboolean         spice_named_pipe_close     (SpiceNamedPipe *namedpipe,
                                             GError **error);
G_END_DECLS

#endif /* __NAMED_PIPE_H__ */
