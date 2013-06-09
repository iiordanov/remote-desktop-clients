/*
   Copyright (C) 2009 Red Hat, Inc.

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

#ifndef _H_CONTROLLER_PROT
#define _H_CONTROLLER_PROT

#include <spice/types.h>
#include <spice/start-packed.h>

#define CONTROLLER_MAGIC      (*(uint32_t*)"CTRL")
#define CONTROLLER_VERSION    1


typedef struct SPICE_ATTR_PACKED ControllerInitHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
} ControllerInitHeader;

typedef struct SPICE_ATTR_PACKED ControllerInit {
    ControllerInitHeader base;
    uint64_t credentials;
    uint32_t flags;
} ControllerInit;

enum {
    CONTROLLER_FLAG_EXCLUSIVE = 1 << 0,
};

typedef struct SPICE_ATTR_PACKED ControllerMsg {
    uint32_t id;
    uint32_t size;
} ControllerMsg;

enum {
    //extrenal app -> spice client
    CONTROLLER_HOST = 1,
    CONTROLLER_PORT,
    CONTROLLER_SPORT,
    CONTROLLER_PASSWORD,

    CONTROLLER_SECURE_CHANNELS,
    CONTROLLER_DISABLE_CHANNELS,

    CONTROLLER_TLS_CIPHERS,
    CONTROLLER_CA_FILE,
    CONTROLLER_HOST_SUBJECT,

    CONTROLLER_FULL_SCREEN,
    CONTROLLER_SET_TITLE,

    CONTROLLER_CREATE_MENU,
    CONTROLLER_DELETE_MENU,

    CONTROLLER_HOTKEYS,
    CONTROLLER_SEND_CAD,

    CONTROLLER_CONNECT,
    CONTROLLER_SHOW,
    CONTROLLER_HIDE,

    CONTROLLER_ENABLE_SMARTCARD,

    CONTROLLER_COLOR_DEPTH,
    CONTROLLER_DISABLE_EFFECTS,

    CONTROLLER_ENABLE_USB,
    CONTROLLER_ENABLE_USB_AUTOSHARE,
    CONTROLLER_USB_FILTER,

    CONTROLLER_PROXY,

    //spice client -> external app
    CONTROLLER_MENU_ITEM_CLICK = 1001,
};

#define CONTROLLER_TRUE (1 << 0)

enum {
    CONTROLLER_SET_FULL_SCREEN  = CONTROLLER_TRUE,
    CONTROLLER_AUTO_DISPLAY_RES = 1 << 1,
};

typedef struct SPICE_ATTR_PACKED ControllerValue {
    ControllerMsg base;
    uint32_t value;
} ControllerValue;

typedef struct SPICE_ATTR_PACKED ControllerData {
    ControllerMsg base;
    uint8_t data[0];
} ControllerData;

#define CONTROLLER_MENU_ITEM_DELIMITER "\n"
#define CONTROLLER_MENU_PARAM_DELIMITER "\r"

enum {
    CONTROLLER_MENU_FLAGS_SEPARATOR    = 1 << 0,
    CONTROLLER_MENU_FLAGS_DISABLED     = 1 << 1,
    CONTROLLER_MENU_FLAGS_POPUP        = 1 << 2,
    CONTROLLER_MENU_FLAGS_CHECKED      = 1 << 3,
    CONTROLLER_MENU_FLAGS_GRAYED       = 1 << 4,
};

#define SPICE_MENU_INTERNAL_ID_BASE   0x1300
#define SPICE_MENU_INTERNAL_ID_SHIFT  8

#include <spice/end-packed.h>

#endif
