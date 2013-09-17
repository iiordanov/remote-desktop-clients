/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011,2012 Red Hat, Inc.

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
#ifndef __SPICE_USB_DEVICE_MANAGER_PRIV_H__
#define __SPICE_USB_DEVICE_MANAGER_PRIV_H__

#include "usb-device-manager.h"

G_BEGIN_DECLS

gboolean spice_usb_device_manager_start_event_listening(
    SpiceUsbDeviceManager *manager, GError **err);

void spice_usb_device_manager_stop_event_listening(
    SpiceUsbDeviceManager *manager);

#ifdef USE_USBREDIR
#include <libusb.h>
void spice_usb_device_manager_device_error(
    SpiceUsbDeviceManager *manager, SpiceUsbDevice *device, GError *err);

guint8 spice_usb_device_get_busnum(const SpiceUsbDevice *device);
guint8 spice_usb_device_get_devaddr(const SpiceUsbDevice *device);
guint16 spice_usb_device_get_vid(const SpiceUsbDevice *device);
guint16 spice_usb_device_get_pid(const SpiceUsbDevice *device);

#endif

G_END_DECLS

#endif /* __SPICE_USB_DEVICE_MANAGER_PRIV_H__ */
