/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

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
#ifndef __SPICE_USBUTIL_H__
#define __SPICE_USBUTIL_H__

#include <glib.h>

#ifdef USE_USBREDIR
#include <libusb.h>

G_BEGIN_DECLS

const char *spice_usbutil_libusb_strerror(enum libusb_error error_code);
void spice_usb_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product);

G_END_DECLS

#endif /* USE_USBREDIR */
#endif /* __SPICE_USBUTIL_H__ */
