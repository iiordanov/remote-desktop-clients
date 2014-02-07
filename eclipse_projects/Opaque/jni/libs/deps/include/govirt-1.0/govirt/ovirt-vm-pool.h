/*
 * ovirt-vm-pool.h: oVirt VM pool
 *
 * Copyright (C) 2013 Iordan Iordanov <i@iiordanov.com>
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
 */
#ifndef __OVIRT_VM_POOL_H__
#define __OVIRT_VM_POOL_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_VM_POOL            (ovirt_vm_pool_get_type ())
#define OVIRT_VM_POOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_VM_POOL, OvirtVmPool))
#define OVIRT_VM_POOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_VM_POOL, OvirtVmPoolClass))
#define OVIRT_IS_VM_POOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_VM_POOL))
#define OVIRT_IS_VM_POOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_VM_POOL))
#define OVIRT_VM_POOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_VM_POOL, OvirtVmPoolClass))

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

GType ovirt_vm_pool_get_type(void);
OvirtVmPool *ovirt_vm_pool_new(void);

gboolean ovirt_vm_pool_allocate_vm(OvirtVmPool *vm_pool, OvirtProxy *proxy, GError **error);
void ovirt_vm_pool_allocate_vm_async(OvirtVmPool *vm_pool, OvirtProxy *proxy,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
gboolean ovirt_vm_pool_allocate_vm_finish(OvirtVmPool *vm_pool,
                                          GAsyncResult *result, GError **err);

G_END_DECLS

#endif /* __OVIRT_VM_POOL_H__ */
