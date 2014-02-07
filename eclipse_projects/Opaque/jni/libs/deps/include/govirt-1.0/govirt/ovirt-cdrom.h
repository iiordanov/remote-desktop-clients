/*
 * ovirt-cdrom.h: oVirt cdrom resource
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
#ifndef __OVIRT_CDROM_H__
#define __OVIRT_CDROM_H__

#include <gio/gio.h>
#include <glib-object.h>
#include <govirt/ovirt-types.h>
#include <govirt/ovirt-resource.h>

G_BEGIN_DECLS

#define OVIRT_TYPE_CDROM            (ovirt_cdrom_get_type ())
#define OVIRT_CDROM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), OVIRT_TYPE_CDROM, OvirtCdrom))
#define OVIRT_CDROM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), OVIRT_TYPE_CDROM, OvirtCdromClass))
#define OVIRT_IS_CDROM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OVIRT_TYPE_CDROM))
#define OVIRT_IS_CDROM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), OVIRT_TYPE_CDROM))
#define OVIRT_CDROM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OVIRT_TYPE_CDROM, OvirtCdromClass))

typedef struct _OvirtCdrom OvirtCdrom;
typedef struct _OvirtCdromPrivate OvirtCdromPrivate;
typedef struct _OvirtCdromClass OvirtCdromClass;

struct _OvirtCdrom
{
    OvirtResource parent;

    OvirtCdromPrivate *priv;

    /* Do not add fields to this struct */
};

struct _OvirtCdromClass
{
    OvirtResourceClass parent_class;

    gpointer padding[20];
};

GType ovirt_cdrom_get_type(void);

gboolean ovirt_cdrom_update(OvirtCdrom *cdrom, gboolean current,
                            OvirtProxy *proxy, GError **error);

void ovirt_cdrom_update_async(OvirtCdrom *cdrom,
                              gboolean current,
                              OvirtProxy *proxy,
                              GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean ovirt_cdrom_update_finish(OvirtCdrom *cdrom,
                                   GAsyncResult *result,
                                   GError **err);

G_END_DECLS

#endif /* __OVIRT_CDROM_H__ */
