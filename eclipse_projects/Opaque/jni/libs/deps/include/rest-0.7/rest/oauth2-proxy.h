/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, 2010 Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 *          Ross Burton <ross@linux.intel.com>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
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

#ifndef _OAUTH2_PROXY
#define _OAUTH2_PROXY

#include <rest/rest-proxy.h>

G_BEGIN_DECLS

#define OAUTH2_TYPE_PROXY oauth2_proxy_get_type()

#define OAUTH2_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OAUTH2_TYPE_PROXY, OAuth2Proxy))

#define OAUTH2_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), OAUTH2_TYPE_PROXY, OAuth2ProxyClass))

#define OAUTH2_IS_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OAUTH2_TYPE_PROXY))

#define OAUTH2_IS_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), OAUTH2_TYPE_PROXY))

#define OAUTH2_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), OAUTH2_TYPE_PROXY, OAuth2ProxyClass))

typedef struct _OAuth2ProxyPrivate OAuth2ProxyPrivate;

/**
 * OAuth2Proxy:
 *
 * #OAuth2Proxy has no publicly available members.
 */
typedef struct {
  RestProxy parent;
  OAuth2ProxyPrivate *priv;
} OAuth2Proxy;

typedef struct {
  RestProxyClass parent_class;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
} OAuth2ProxyClass;

GType oauth2_proxy_get_type (void);

RestProxy* oauth2_proxy_new (const char *client_id,
                             const char *auth_endpoint,
                             const gchar *url_format,
                             gboolean binding_required);

RestProxy* oauth2_proxy_new_with_token (const char *client_id,
                                        const char *access_token,
                                        const char *auth_endpoint,
                                        const gchar *url_format,
                                        gboolean binding_required);

char * oauth2_proxy_build_login_url_full (OAuth2Proxy *proxy,
                                          const char* redirect_uri,
                                          GHashTable* extra_params);

char * oauth2_proxy_build_login_url (OAuth2Proxy *proxy,
                                     const char* redirect_uri);

const char * oauth2_proxy_get_access_token (OAuth2Proxy *proxy);

void oauth2_proxy_set_access_token (OAuth2Proxy *proxy, const char *access_token);

char * oauth2_proxy_extract_access_token (const char *url);

G_END_DECLS

#endif /* _OAUTH2_PROXY */
