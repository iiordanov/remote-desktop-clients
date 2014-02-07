/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, Intel Corporation.
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

#ifndef _OAUTH2_PROXY_CALL
#define _OAUTH2_PROXY_CALL

#include <rest/rest-proxy-call.h>

G_BEGIN_DECLS

#define OAUTH2_TYPE_PROXY_CALL oauth2_proxy_call_get_type()

#define OAUTH2_PROXY_CALL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), OAUTH2_TYPE_PROXY_CALL, OAuth2ProxyCall))

#define OAUTH2_PROXY_CALL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), OAUTH2_TYPE_PROXY_CALL, OAuth2ProxyCallClass))

#define OAUTH2_IS_PROXY_CALL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OAUTH2_TYPE_PROXY_CALL))

#define OAUTH2_IS_PROXY_CALL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), OAUTH2_TYPE_PROXY_CALL))

#define OAUTH2_PROXY_CALL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), OAUTH2_TYPE_PROXY_CALL, OAuth2ProxyCallClass))

/**
 * OAuth2ProxyCall:
 *
 * #OAuth2ProxyCall has no publicly available members.
 */
typedef struct {
  RestProxyCall parent;
} OAuth2ProxyCall;

typedef struct {
  RestProxyCallClass parent_class;
  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
} OAuth2ProxyCallClass;

GType oauth2_proxy_call_get_type (void);

G_END_DECLS

#endif /* _OAUTH2_PROXY_CALL */
