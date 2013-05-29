/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef __SPICE_CLIENT_CLIENT_H__
#define __SPICE_CLIENT_CLIENT_H__

/* glib */
#include <glib.h>
#include <glib-object.h>

/* spice-protocol */
#include <spice/enums.h>
#include <spice/protocol.h>

/* spice/gtk */
#include "spice-types.h"
#include "spice-session.h"
#include "spice-channel.h"
#include "spice-option.h"

#include "channel-main.h"
#include "channel-display.h"
#include "channel-cursor.h"
#include "channel-inputs.h"
#include "channel-playback.h"
#include "channel-record.h"
#include "channel-smartcard.h"
#include "channel-usbredir.h"
#include "channel-port.h"

#include "smartcard-manager.h"
#include "usb-device-manager.h"
#include "spice-audio.h"

G_BEGIN_DECLS

#define SPICE_CLIENT_ERROR spice_client_error_quark()

/**
 * SpiceClientError:
 * @SPICE_CLIENT_ERROR_FAILED: generic error code
 *
 * Error codes returned by spice-client API.
 */
typedef enum
{
    SPICE_CLIENT_ERROR_FAILED,
    SPICE_CLIENT_USB_DEVICE_REJECTED,
    SPICE_CLIENT_USB_DEVICE_LOST,
} SpiceClientError;

GQuark spice_client_error_quark(void);

G_END_DECLS

#endif /* __SPICE_CLIENT_CLIENT_H__ */
