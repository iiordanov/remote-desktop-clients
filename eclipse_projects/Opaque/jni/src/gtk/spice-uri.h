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
#ifndef __SPICE_URI_H__
#define __SPICE_URI_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SPICE_TYPE_URI (spice_uri_get_type ())
#define SPICE_URI(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_URI, SpiceURI))
#define SPICE_URI_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_URI, SpiceURIClass))
#define SPICE_IS_URI(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_URI))
#define SPICE_IS_URI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_URI))
#define SPICE_URI_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_URI, SpiceURIClass))

typedef struct _SpiceURI SpiceURI;
typedef struct _SpiceURIClass SpiceURIClass;
typedef struct _SpiceURIPrivate SpiceURIPrivate;

GType spice_uri_get_type(void) G_GNUC_CONST;

const gchar* spice_uri_get_scheme(SpiceURI* uri);
void spice_uri_set_scheme(SpiceURI* uri, const gchar* scheme);
const gchar* spice_uri_get_hostname(SpiceURI* uri);
void spice_uri_set_hostname(SpiceURI* uri, const gchar* hostname);
guint spice_uri_get_port(SpiceURI* uri);
void spice_uri_set_port(SpiceURI* uri, guint port);
gchar *spice_uri_to_string(SpiceURI* uri);
const gchar* spice_uri_get_user(SpiceURI* uri);
void spice_uri_set_user(SpiceURI* uri, const gchar* user);
const gchar* spice_uri_get_password(SpiceURI* uri);
void spice_uri_set_password(SpiceURI* uri, const gchar* password);

G_END_DECLS

#endif /* __SPICE_URI_H__ */
