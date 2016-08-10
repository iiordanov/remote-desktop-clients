/*
 * ovirt-resource.h: generic oVirt resource
 *
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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
#ifndef __OVIRT_RESOURCE_H__
#define __OVIRT_RESOURCE_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-types.h>
#include <rest/rest-proxy-call.h>
#include <rest/rest-xml-node.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_RESOURCE            (ovirt_resource_get_type ())
#define OVIRT_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_RESOURCE, OvirtResource))
#define OVIRT_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_RESOURCE, OvirtResourceClass))
#define OVIRT_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_RESOURCE))
#define OVIRT_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_RESOURCE))
#define OVIRT_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_RESOURCE, OvirtResourceClass))

typedef struct _OvirtResource OvirtResource;
typedef struct _OvirtResourcePrivate OvirtResourcePrivate;
typedef struct _OvirtResourceClass OvirtResourceClass;

struct _OvirtResource
{
    GObject parent;

    OvirtResourcePrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtResourceClass
{
    GObjectClass parent_class;

    gboolean (*init_from_xml)(OvirtResource *resource,
                              RestXmlNode *node,
                              GError **error);
    char *(*to_xml)(OvirtResource *resource);
#ifdef GOVIRT_UNSTABLE_API_ABI
    void (*add_rest_params)(OvirtResource *resource,
                            RestProxyCall *call);
#else
    gpointer padding0;
#endif
    gpointer padding[18];
};

GType ovirt_resource_get_type(void);

const char *ovirt_resource_get_sub_collection(OvirtResource *resource,
                                              const char *sub_collection);

gboolean ovirt_resource_update(OvirtResource *resource,
                               OvirtProxy *proxy,
                               GError **error);
void ovirt_resource_update_async(OvirtResource *resource,
                                 OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);
gboolean ovirt_resource_update_finish(OvirtResource *resource,
                                      GAsyncResult *result,
                                      GError **err);

gboolean ovirt_resource_refresh(OvirtResource *resource,
                                OvirtProxy *proxy,
                                GError **error);
void ovirt_resource_refresh_async(OvirtResource *resource,
                                  OvirtProxy *proxy,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean ovirt_resource_refresh_finish(OvirtResource *resource,
                                       GAsyncResult *result,
                                       GError **err);

gboolean ovirt_resource_delete(OvirtResource *resource,
                               OvirtProxy *proxy,
                               GError **error);
void ovirt_resource_delete_async(OvirtResource *resource,
                                 OvirtProxy *proxy,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);
gboolean ovirt_resource_delete_finish(OvirtResource *resource,
                                      GAsyncResult *result,
                                      GError **err);

G_END_DECLS

#endif /* __OVIRT_RESOURCE_H__ */
