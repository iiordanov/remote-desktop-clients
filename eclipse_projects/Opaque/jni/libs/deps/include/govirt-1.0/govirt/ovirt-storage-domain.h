/*
 * ovirt-storage-domain.h: oVirt storage domain resource
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
#ifndef __OVIRT_STORAGE_DOMAIN_H__
#define __OVIRT_STORAGE_DOMAIN_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-collection.h>
#include <govirt/ovirt-resource.h>
#include <govirt/ovirt-types.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_STORAGE_DOMAIN            (ovirt_storage_domain_get_type ())
#define OVIRT_STORAGE_DOMAIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_STORAGE_DOMAIN, OvirtStorageDomain))
#define OVIRT_STORAGE_DOMAIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_STORAGE_DOMAIN, OvirtStorageDomainClass))
#define OVIRT_IS_STORAGE_DOMAIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_STORAGE_DOMAIN))
#define OVIRT_IS_STORAGE_DOMAIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_STORAGE_DOMAIN))
#define OVIRT_STORAGE_DOMAIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_STORAGE_DOMAIN, OvirtStorageDomainClass))

typedef enum {
    OVIRT_STORAGE_DOMAIN_FORMAT_VERSION_V1,
    OVIRT_STORAGE_DOMAIN_FORMAT_VERSION_V2,
    OVIRT_STORAGE_DOMAIN_FORMAT_VERSION_V3,
} OvirtStorageDomainFormatVersion;

typedef enum {
    OVIRT_STORAGE_DOMAIN_STATE_ACTIVE,
    OVIRT_STORAGE_DOMAIN_STATE_INACTIVE,
    OVIRT_STORAGE_DOMAIN_STATE_LOCKED,
    OVIRT_STORAGE_DOMAIN_STATE_MIXED,
    OVIRT_STORAGE_DOMAIN_STATE_UNATTACHED,
    OVIRT_STORAGE_DOMAIN_STATE_MAINTENANCE,
    OVIRT_STORAGE_DOMAIN_STATE_UNKNOWN,
} OvirtStorageDomainState;

typedef enum {
    OVIRT_STORAGE_DOMAIN_TYPE_DATA,
    OVIRT_STORAGE_DOMAIN_TYPE_ISO,
    OVIRT_STORAGE_DOMAIN_TYPE_EXPORT,
} OvirtStorageDomainType;

typedef struct _OvirtStorageDomain OvirtStorageDomain;
typedef struct _OvirtStorageDomainPrivate OvirtStorageDomainPrivate;
typedef struct _OvirtStorageDomainClass OvirtStorageDomainClass;

struct _OvirtStorageDomain
{
    OvirtResource parent;

    OvirtStorageDomainPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtStorageDomainClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_storage_domain_get_type(void);

OvirtStorageDomain *ovirt_storage_domain_new(void);

OvirtCollection *ovirt_storage_domain_get_files(OvirtStorageDomain *domain);

G_END_DECLS

#endif /* __OVIRT_STORAGE_DOMAIN_H__ */
