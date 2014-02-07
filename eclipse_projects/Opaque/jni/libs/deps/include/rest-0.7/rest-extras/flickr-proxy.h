/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, Intel Corporation.
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

#ifndef _FLICKR_PROXY
#define _FLICKR_PROXY

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

G_BEGIN_DECLS

#define FLICKR_TYPE_PROXY flickr_proxy_get_type()

#define FLICKR_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLICKR_TYPE_PROXY, FlickrProxy))

#define FLICKR_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), FLICKR_TYPE_PROXY, FlickrProxyClass))

#define FLICKR_IS_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLICKR_TYPE_PROXY))

#define FLICKR_IS_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), FLICKR_TYPE_PROXY))

#define FLICKR_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLICKR_TYPE_PROXY, FlickrProxyClass))

typedef struct _FlickrProxyPrivate FlickrProxyPrivate;

/**
 * FlickrProxy:
 *
 * #FlickrProxy has no publicly available members.
 */
typedef struct {
  RestProxy parent;
  FlickrProxyPrivate *priv;
} FlickrProxy;

typedef struct {
  RestProxyClass parent_class;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
} FlickrProxyClass;

#define FLICKR_PROXY_ERROR flickr_proxy_error_quark()

GType flickr_proxy_get_type (void);

RestProxy* flickr_proxy_new (const char *api_key,
                             const char *shared_secret);

RestProxy* flickr_proxy_new_with_token (const char *api_key,
                                        const char *shared_secret,
                                        const char *token);

const char * flickr_proxy_get_api_key (FlickrProxy *proxy);

const char * flickr_proxy_get_shared_secret (FlickrProxy *proxy);

const char * flickr_proxy_get_token (FlickrProxy *proxy);

void flickr_proxy_set_token (FlickrProxy *proxy, const char *token);

char * flickr_proxy_sign (FlickrProxy *proxy, GHashTable *params);

char * flickr_proxy_build_login_url (FlickrProxy *proxy,
                                     const char  *frob,
                                     const char  *perms);

gboolean flickr_proxy_is_successful (RestXmlNode *root, GError **error);

RestProxyCall * flickr_proxy_new_upload (FlickrProxy *proxy);

RestProxyCall * flickr_proxy_new_upload_for_file (FlickrProxy *proxy, const char *filename, GError **error);

G_END_DECLS

#endif /* _FLICKR_PROXY */
