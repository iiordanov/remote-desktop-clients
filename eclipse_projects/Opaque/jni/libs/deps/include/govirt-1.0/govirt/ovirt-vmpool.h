/*
 * ovirt-vmpool.h: oVirt VM pool
 *
 * Copyright (C) 2013 Red Hat, Inc.
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
 * Author: Iordan Iordanov <i@iiordanov.com>
 */
#ifndef __OVIRT_VMPOOL_H__
#define __OVIRT_VMPOOL_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_VMPOOL            (ovirt_vmpool_get_type ())
#define OVIRT_VMPOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_VMPOOL, OvirtVmPool))
#define OVIRT_VMPOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_VMPOOL, OvirtVmPoolClass))
#define OVIRT_IS_VMPOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_VMPOOL))
#define OVIRT_IS_VMPOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_VMPOOL))
#define OVIRT_VMPOOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_VMPOOL, OvirtVmPoolClass))

typedef struct _OvirtVmPool OvirtVmPool;
typedef struct _OvirtVmPoolPrivate OvirtVmPoolPrivate;
typedef struct _OvirtVmPoolClass OvirtVmPoolClass;

struct _OvirtVmPool
{
    OvirtResource parent;

    OvirtVmPoolPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtVmPoolClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_vmpool_get_type(void);
OvirtVmPool *ovirt_vmpool_new(void);

gboolean ovirt_vmpool_allocatevm(OvirtVmPool *vmpool, OvirtProxy *proxy, GError **error);

G_END_DECLS

#endif /* __OVIRT_VMPOOL_H__ */
