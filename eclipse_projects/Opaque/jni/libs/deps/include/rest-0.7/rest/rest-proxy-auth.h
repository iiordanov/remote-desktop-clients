/*
 * librest - RESTful web services access
 * Copyright (c) 2012, Red Hat, Inc.
 *
 * Authors: Christophe Fergeau <cfergeau@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _REST_PROXY_AUTH
#define _REST_PROXY_AUTH

#include <glib-object.h>

G_BEGIN_DECLS

#define REST_TYPE_PROXY_AUTH rest_proxy_auth_get_type()

#define REST_PROXY_AUTH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), REST_TYPE_PROXY_AUTH, RestProxyAuth))

#define REST_PROXY_AUTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), REST_TYPE_PROXY_AUTH, RestProxyAuthClass))

#define REST_IS_PROXY_AUTH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REST_TYPE_PROXY_AUTH))

#define REST_IS_PROXY_AUTH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), REST_TYPE_PROXY_AUTH))

#define REST_PROXY_AUTH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), REST_TYPE_PROXY_AUTH, RestProxyAuthClass))

typedef struct _RestProxyAuthPrivate RestProxyAuthPrivate;

/**
 * RestProxyAuth:
 *
 * #RestProxyAuth has no publicly available members.
 */
typedef struct {
  GObject parent;
  RestProxyAuthPrivate *priv;
} RestProxyAuth;

typedef struct {
  GObjectClass parent_class;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
} RestProxyAuthClass;

GType rest_proxy_auth_get_type (void);

void rest_proxy_auth_pause (RestProxyAuth *auth);
void rest_proxy_auth_unpause (RestProxyAuth *auth);

G_END_DECLS

#endif /* _REST_PROXY_AUTH */
