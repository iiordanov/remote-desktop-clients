/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

   Red Hat Authors:
   Uri Lublin <uril@redhat.com>

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

#ifndef SPICE_WIN_USB_DRIVER_H
#define SPICE_WIN_USB_DRIVER_H

#include "usb-device-manager.h"

G_BEGIN_DECLS

GQuark win_usb_driver_error_quark(void);


#define SPICE_TYPE_WIN_USB_DRIVER      (spice_win_usb_driver_get_type ())
#define SPICE_WIN_USB_DRIVER(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),    \
            SPICE_TYPE_WIN_USB_DRIVER, SpiceWinUsbDriver))
#define SPICE_IS_WIN_USB_DRIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),    \
            SPICE_TYPE_WIN_USB_DRIVER))
#define SPICE_WIN_USB_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),  \
            SPICE_TYPE_WIN_USB_DRIVER, SpiceWinUsbDriverClass))
#define SPICE_IS_WIN_USB_DRIVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
            SPICE_TYPE_WIN_USB_DRIVER))
#define SPICE_WIN_USB_DRIVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
            SPICE_TYPE_WIN_USB_DRIVER, SpiceWinUsbDriverClass))

typedef struct _SpiceWinUsbDriver          SpiceWinUsbDriver;
typedef struct _SpiceWinUsbDriverClass     SpiceWinUsbDriverClass;
typedef struct _SpiceWinUsbDriverPrivate   SpiceWinUsbDriverPrivate;

struct _SpiceWinUsbDriver
{
    GObject parent;

    /*< private >*/
    SpiceWinUsbDriverPrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpiceWinUsbDriverClass
{
    GObjectClass parent_class;
};

GType spice_win_usb_driver_get_type(void);

SpiceWinUsbDriver *spice_win_usb_driver_new(void);


void spice_win_usb_driver_install(SpiceWinUsbDriver *self,
                                  SpiceUsbDevice *device,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);

void spice_win_usb_driver_uninstall(SpiceWinUsbDriver *self,
                                    SpiceUsbDevice *device,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);

gint spice_win_usb_driver_install_finish(SpiceWinUsbDriver *self,
                                         GAsyncResult *res, GError **err);


SpiceUsbDevice *spice_win_usb_driver_get_device(SpiceWinUsbDriver *self);

#define SPICE_WIN_USB_DRIVER_ERROR spice_win_usb_driver_error_quark()

/**
 * SpiceWinUsbDriverError:
 * @SPICE_WIN_USB_DRIVER_ERROR_FAILED: generic error code
 * @SPICE_WIN_USB_DRIVER_ERROR_MESSAGE: bad message read from clerk
 *
 * Error codes returned by spice-client API.
 */
typedef enum
{
    SPICE_WIN_USB_DRIVER_ERROR_FAILED,
    SPICE_WIN_USB_DRIVER_ERROR_MESSAGE,
} SpiceWinUsbDriverError;

GQuark spice_win_usb_driver_error_quark(void);

G_END_DECLS

#endif /* SPICE_WIN_USB_DRIVER_H */
