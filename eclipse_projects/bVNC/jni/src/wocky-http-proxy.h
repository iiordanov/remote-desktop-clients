 /* wocky-http-proxy.h: Header for WockyHttpProxy
 *
 * Copyright (C) 2010 Collabora, Ltd.
 * @author Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _WOCKY_HTTP_PROXY_H_
#define _WOCKY_HTTP_PROXY_H_

#include <gio/gio.h>

G_BEGIN_DECLS

#define WOCKY_TYPE_HTTP_PROXY         (_wocky_http_proxy_get_type ())
#define WOCKY_HTTP_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), WOCKY_TYPE_HTTP_PROXY, WockyHttpProxy))
#define WOCKY_HTTP_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), WOCKY_TYPE_HTTP_PROXY, WockyHttpProxyClass))
#define WOCKY_IS_HTTP_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), WOCKY_TYPE_HTTP_PROXY))
#define WOCKY_IS_HTTP_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), WOCKY_TYPE_HTTP_PROXY))
#define WOCKY_HTTP_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), WOCKY_TYPE_HTTP_PROXY, WockyHttpProxyClass))

typedef struct _WockyHttpProxy        WockyHttpProxy;
typedef struct _WockyHttpProxyClass   WockyHttpProxyClass;

GType _wocky_http_proxy_get_type (void);

G_END_DECLS

#endif /* _WOCKY_HTTP_PROXY_H_ */
