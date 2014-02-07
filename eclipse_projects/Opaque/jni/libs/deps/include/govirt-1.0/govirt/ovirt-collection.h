/*
 * ovirt-collection.h: generic oVirt collection
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
#ifndef __OVIRT_COLLECTION_H__
#define __OVIRT_COLLECTION_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-types.h>
#include <govirt/ovirt-resource.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_COLLECTION            (ovirt_collection_get_type ())
#define OVIRT_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_COLLECTION, OvirtCollection))
#define OVIRT_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_COLLECTION, OvirtCollectionClass))
#define OVIRT_IS_COLLECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_COLLECTION))
#define OVIRT_IS_COLLECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_COLLECTION))
#define OVIRT_COLLECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_COLLECTION, OvirtCollectionClass))

typedef struct _OvirtCollection OvirtCollection;
typedef struct _OvirtCollectionPrivate OvirtCollectionPrivate;
typedef struct _OvirtCollectionClass OvirtCollectionClass;

struct _OvirtCollection
{
    GObject parent;

    OvirtCollectionPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtCollectionClass
{
    GObjectClass parent_class;

    gpointer padding[20];
};

GType ovirt_collection_get_type(void);
GHashTable *ovirt_collection_get_resources(OvirtCollection *collection);

OvirtResource *ovirt_collection_lookup_resource(OvirtCollection *collection,
                                                const char *name);
gboolean ovirt_collection_fetch(OvirtCollection *collection,
                                OvirtProxy *proxy,
                                GError **error);
void ovirt_collection_fetch_async(OvirtCollection *collection,
                                  OvirtProxy *proxy,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean ovirt_collection_fetch_finish(OvirtCollection *collection,
                                       GAsyncResult *result,
                                       GError **err);

G_END_DECLS

#endif /* __OVIRT_COLLECTION_H__ */
