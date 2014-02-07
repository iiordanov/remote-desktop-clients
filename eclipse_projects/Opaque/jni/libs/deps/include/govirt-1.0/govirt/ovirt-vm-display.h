/*
 * ovirt-vm-display.h: oVirt virtual machine display information
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
#ifndef __OVIRT_VM_DISPLAY_H__
#define __OVIRT_VM_DISPLAY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_VM_DISPLAY            (ovirt_vm_display_get_type ())
#define OVIRT_VM_DISPLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_VM_DISPLAY, OvirtVmDisplay))
#define OVIRT_VM_DISPLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_VM_DISPLAY, OvirtVmDisplayClass))
#define OVIRT_IS_VM_DISPLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_VM_DISPLAY))
#define OVIRT_IS_VM_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_VM_DISPLAY))
#define OVIRT_VM_DISPLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_VM_DISPLAY, OvirtVmDisplayClass))

typedef struct _OvirtVmDisplay OvirtVmDisplay;
typedef struct _OvirtVmDisplayPrivate OvirtVmDisplayPrivate;
typedef struct _OvirtVmDisplayClass OvirtVmDisplayClass;

struct _OvirtVmDisplay
{
    GObject parent;

    OvirtVmDisplayPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtVmDisplayClass
{
    GObjectClass parent_class;

    gpointer padding[20];
};

typedef enum {
    OVIRT_VM_DISPLAY_SPICE,
    OVIRT_VM_DISPLAY_VNC
} OvirtVmDisplayType;

GType ovirt_vm_display_get_type(void);
OvirtVmDisplay *ovirt_vm_display_new(void);

G_END_DECLS

#endif /* __OVIRT_VM_DISPLAY_H__ */
