/*
 * ovirt-api.h: oVirt API entry point
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
#ifndef __OVIRT_API_H__
#define __OVIRT_API_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_API            (ovirt_api_get_type ())
#define OVIRT_API(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_API, OvirtApi))
#define OVIRT_API_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_API, OvirtApiClass))
#define OVIRT_IS_API(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_API))
#define OVIRT_IS_API_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_API))
#define OVIRT_API_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_API, OvirtApiClass))

typedef struct _OvirtApi OvirtApi;
typedef struct _OvirtApiPrivate OvirtApiPrivate;
typedef struct _OvirtApiClass OvirtApiClass;

struct _OvirtApi
{
    OvirtResource parent;

    OvirtApiPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtApiClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};


GType ovirt_api_get_type(void);
OvirtApi *ovirt_api_new(void);

OvirtCollection *ovirt_api_get_storage_domains(OvirtApi *api);
OvirtCollection *ovirt_api_get_vms(OvirtApi *api);
OvirtCollection *ovirt_api_get_vm_pools(OvirtApi *api);

G_END_DECLS

#endif /* __OVIRT_API_H__ */
