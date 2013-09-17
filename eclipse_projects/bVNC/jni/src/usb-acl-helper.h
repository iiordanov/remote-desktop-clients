/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __SPICE_USB_ACL_HELPER_H__
#define __SPICE_USB_ACL_HELPER_H__

#include "spice-client.h"
#include <gio/gio.h>

/* Note the entire usb-acl-helper class is private to spice-client-glib !! */

G_BEGIN_DECLS

#define SPICE_TYPE_USB_ACL_HELPER            (spice_usb_acl_helper_get_type ())
#define SPICE_USB_ACL_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_USB_ACL_HELPER, SpiceUsbAclHelper))
#define SPICE_USB_ACL_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_USB_ACL_HELPER, SpiceUsbAclHelperClass))
#define SPICE_IS_USB_ACL_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_USB_ACL_HELPER))
#define SPICE_IS_USB_ACL_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_USB_ACL_HELPER))
#define SPICE_USB_ACL_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_USB_ACL_HELPER, SpiceUsbAclHelperClass))

typedef struct _SpiceUsbAclHelper SpiceUsbAclHelper;
typedef struct _SpiceUsbAclHelperClass SpiceUsbAclHelperClass;
typedef struct _SpiceUsbAclHelperPrivate SpiceUsbAclHelperPrivate;

struct _SpiceUsbAclHelper
{
    GObject parent;

    /*< private >*/
    SpiceUsbAclHelperPrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpiceUsbAclHelperClass
{
    GObjectClass parent_class;
};

GType spice_usb_acl_helper_get_type(void);

SpiceUsbAclHelper *spice_usb_acl_helper_new(void);

void spice_usb_acl_helper_open_acl(SpiceUsbAclHelper *self,
                                   gint busnum, gint devnum,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data);
gboolean spice_usb_acl_helper_open_acl_finish(
    SpiceUsbAclHelper *self, GAsyncResult *res, GError **err);

void spice_usb_acl_helper_close_acl(SpiceUsbAclHelper *self);

G_END_DECLS

#endif /* __SPICE_USB_ACL_HELPER_H__ */
