/*
 * librest - RESTful web services access
 * Copyright (c) 2010 Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 *          Ross Burton <ross@linux.intel.com>
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

#ifndef _LASTFM_PROXY
#define _LASTFM_PROXY

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

G_BEGIN_DECLS

#define LASTFM_TYPE_PROXY lastfm_proxy_get_type()

#define LASTFM_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), LASTFM_TYPE_PROXY, LastfmProxy))

#define LASTFM_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), LASTFM_TYPE_PROXY, LastfmProxyClass))

#define LASTFM_IS_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LASTFM_TYPE_PROXY))

#define LASTFM_IS_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), LASTFM_TYPE_PROXY))

#define LASTFM_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), LASTFM_TYPE_PROXY, LastfmProxyClass))

typedef struct _LastfmProxyPrivate LastfmProxyPrivate;

/**
 * LastfmProxy:
 *
 * #LastfmProxy has no publicly available members.
 */
typedef struct {
  RestProxy parent;
  LastfmProxyPrivate *priv;
} LastfmProxy;

typedef struct {
  RestProxyClass parent_class;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
} LastfmProxyClass;

#define LASTFM_PROXY_ERROR lastfm_proxy_error_quark()

GType lastfm_proxy_get_type (void);

RestProxy* lastfm_proxy_new (const char *api_key,
                             const char *secret);

RestProxy* lastfm_proxy_new_with_session (const char *api_key,
                                          const char *secret,
                                          const char *session_key);

const char * lastfm_proxy_get_api_key (LastfmProxy *proxy);

const char * lastfm_proxy_get_secret (LastfmProxy *proxy);

const char * lastfm_proxy_get_session_key (LastfmProxy *proxy);

void lastfm_proxy_set_session_key (LastfmProxy *proxy, const char *session_key);

char * lastfm_proxy_sign (LastfmProxy *proxy, GHashTable *params);

char * lastfm_proxy_build_login_url (LastfmProxy *proxy, const char *token);

gboolean lastfm_proxy_is_successful (RestXmlNode *root, GError **error);

G_END_DECLS

#endif /* _LASTFM_PROXY */
