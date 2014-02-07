/*
 * ovirt-vm.h: oVirt virtual machine
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
#ifndef __OVIRT_VM_H__
#define __OVIRT_VM_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_VM            (ovirt_vm_get_type ())
#define OVIRT_VM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_VM, OvirtVm))
#define OVIRT_VM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_VM, OvirtVmClass))
#define OVIRT_IS_VM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_VM))
#define OVIRT_IS_VM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_VM))
#define OVIRT_VM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_VM, OvirtVmClass))

typedef struct _OvirtVm OvirtVm;
typedef struct _OvirtVmPrivate OvirtVmPrivate;
typedef struct _OvirtVmClass OvirtVmClass;

struct _OvirtVm
{
    OvirtResource parent;

    OvirtVmPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtVmClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};


typedef enum {
    OVIRT_VM_STATE_DOWN,
    OVIRT_VM_STATE_UP,
    OVIRT_VM_STATE_REBOOTING,
    OVIRT_VM_STATE_POWERING_UP, /*< nick=powering_up >*/
    OVIRT_VM_STATE_POWERED_DOWN, /*< nick=powered_down >*/
    OVIRT_VM_STATE_PAUSED,
    OVIRT_VM_STATE_MIGRATING,
    OVIRT_VM_STATE_UNKNOWN,
    OVIRT_VM_STATE_NOT_RESPONDING, /*< nick=not_responding >*/
    OVIRT_VM_STATE_WAIT_FOR_LAUNCH, /*< nick=wait_for_launch >*/
    OVIRT_VM_STATE_REBOOT_IN_PROGRESS, /*< nick=reboot_in_progress >*/
    OVIRT_VM_STATE_SAVING_STATE, /*< nick=saving_state >*/
    OVIRT_VM_STATE_RESTORING_STATE, /*< nick=restoring_state >*/
    OVIRT_VM_STATE_SUSPENDED,
    OVIRT_VM_STATE_IMAGE_LOCKED, /*< nick=image_locked >*/
    OVIRT_VM_STATE_POWERING_DOWN, /*< nick=powering_down >*/
} OvirtVmState;

GType ovirt_vm_get_type(void);
OvirtVm *ovirt_vm_new(void);

gboolean ovirt_vm_get_ticket(OvirtVm *vm, OvirtProxy *proxy, GError **error);
gboolean ovirt_vm_start(OvirtVm *vm, OvirtProxy *proxy, GError **error);
gboolean ovirt_vm_stop(OvirtVm *vm, OvirtProxy *proxy, GError **error);

void ovirt_vm_get_ticket_async(OvirtVm *vm, OvirtProxy *proxy,
                               GCancellable *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer user_data);
gboolean ovirt_vm_get_ticket_finish(OvirtVm *vm,
                                    GAsyncResult *result,
                                    GError **err);
void ovirt_vm_start_async(OvirtVm *vm, OvirtProxy *proxy,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data);
gboolean ovirt_vm_start_finish(OvirtVm *vm,
                               GAsyncResult *result,
                               GError **err);
void ovirt_vm_stop_async(OvirtVm *vm, OvirtProxy *proxy,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data);
gboolean ovirt_vm_stop_finish(OvirtVm *vm,
                              GAsyncResult *result,
                              GError **err);

void ovirt_vm_refresh_async(OvirtVm *vm, OvirtProxy *proxy,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);
gboolean ovirt_vm_refresh_finish(OvirtVm *vm,
                                 GAsyncResult *result,
                                 GError **err);

OvirtCollection *ovirt_vm_get_cdroms(OvirtVm *vm);

G_END_DECLS

#endif /* __OVIRT_VM_H__ */
