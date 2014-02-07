/*
 * ovirt-proxy.h: oVirt proxy using the oVirt REST API
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */
#ifndef __OVIRT_PROXY_H__
#define __OVIRT_PROXY_H__

#include <rest/rest-proxy.h>
#include <govirt/ovirt-api.h>
#include <govirt/ovirt-types.h>
#include <govirt/ovirt-vm.h>

G_BEGIN_DECLS

#if !GLIB_CHECK_VERSION(2,32,0)
#define G_DEPRECATED_FOR(name) G_GNUC_DEPRECATED_FOR(name)
#endif

#define OVIRT_TYPE_PROXY ovirt_proxy_get_type()

#define OVIRT_PROXY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_PROXY, OvirtProxy))

#define OVIRT_PROXY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_PROXY, OvirtProxyClass))

#define OVIRT_IS_PROXY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_PROXY))

#define OVIRT_IS_PROXY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_PROXY))

#define OVIRT_PROXY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_PROXY, OvirtProxyClass))

typedef struct _OvirtProxyClass OvirtProxyClass;
typedef struct _OvirtProxyPrivate OvirtProxyPrivate;

struct _OvirtProxy {
    RestProxy parent;

    OvirtProxyPrivate *priv;
};

struct _OvirtProxyClass {
    RestProxyClass parent_class;
};

GType ovirt_proxy_get_type(void);

OvirtProxy *ovirt_proxy_new(const char *uri);

G_DEPRECATED_FOR(ovirt_collection_lookup_resource)
OvirtVm *ovirt_proxy_lookup_vm(OvirtProxy *proxy, const char *vm_name);

G_DEPRECATED_FOR(ovirt_collection_get_resources)
GList *ovirt_proxy_get_vms(OvirtProxy *proxy);

G_DEPRECATED_FOR(ovirt_collection_fetch)
gboolean ovirt_proxy_fetch_vms(OvirtProxy *proxy, GError **error);

G_DEPRECATED_FOR(ovirt_collection_fetch_async)
void ovirt_proxy_fetch_vms_async(OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);

G_DEPRECATED_FOR(ovirt_collection_fetch_finish)
GList *ovirt_proxy_fetch_vms_finish(OvirtProxy *proxy,
                                    GAsyncResult *result,
                                    GError **err);

gboolean ovirt_proxy_fetch_ca_certificate(OvirtProxy *proxy, GError **error);
void ovirt_proxy_fetch_ca_certificate_async(OvirtProxy *proxy,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data);
GByteArray *ovirt_proxy_fetch_ca_certificate_finish(OvirtProxy *proxy,
                                                    GAsyncResult *result,
                                                    GError **err);

OvirtApi  *ovirt_proxy_fetch_api(OvirtProxy *proxy, GError **error);
void ovirt_proxy_fetch_api_async(OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);
OvirtApi *ovirt_proxy_fetch_api_finish(OvirtProxy *proxy,
                                       GAsyncResult *result,
                                       GError **err);

#endif
