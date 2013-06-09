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
#ifndef __SPICE_PROXY_H__
#define __SPICE_PROXY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SPICE_TYPE_PROXY (spice_proxy_get_type ())
#define SPICE_PROXY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_PROXY, SpiceProxy))
#define SPICE_PROXY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_PROXY, SpiceProxyClass))
#define SPICE_IS_PROXY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_PROXY))
#define SPICE_IS_PROXY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_PROXY))
#define SPICE_PROXY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_PROXY, SpiceProxyClass))

typedef struct _SpiceProxy SpiceProxy;
typedef struct _SpiceProxyClass SpiceProxyClass;
typedef struct _SpiceProxyPrivate SpiceProxyPrivate;

struct _SpiceProxy {
    GObject parent_instance;
    SpiceProxyPrivate * priv;
};

struct _SpiceProxyClass {
    GObjectClass parent_class;
};


GType spice_proxy_get_type(void) G_GNUC_CONST;

SpiceProxy* spice_proxy_new(void);
gboolean spice_proxy_parse(SpiceProxy* self, const gchar* uri, GError** error);
const gchar* spice_proxy_get_protocol(SpiceProxy* self);
void spice_proxy_set_protocol(SpiceProxy* self, const gchar* value);
const gchar* spice_proxy_get_hostname(SpiceProxy* self);
void spice_proxy_set_hostname(SpiceProxy* self, const gchar* value);
guint spice_proxy_get_port(SpiceProxy* self);
void spice_proxy_set_port(SpiceProxy* self, guint port);
gchar *spice_proxy_to_string(SpiceProxy* self);

G_END_DECLS

#endif /* __SPICE_PROXY_H__ */
