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

#ifndef _H_FOREIGN_MENU_PROT
#define _H_FOREIGN_MENU_PROT

#include <spice/types.h>
#include <spice/start-packed.h>

#define FOREIGN_MENU_MAGIC      (*(uint32_t*)"FRGM")
#define FOREIGN_MENU_VERSION    1

typedef struct SPICE_ATTR_PACKED FrgMenuInitHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
} FrgMenuInitHeader;

typedef struct SPICE_ATTR_PACKED FrgMenuInit {
    FrgMenuInitHeader base;
    uint64_t credentials;
    uint8_t title[0]; //UTF8
} FrgMenuInit;

typedef struct SPICE_ATTR_PACKED FrgMenuMsg {
    uint32_t id;
    uint32_t size;
} FrgMenuMsg;

enum {
    //extrenal app -> spice client
    FOREIGN_MENU_SET_TITLE = 1,
    FOREIGN_MENU_ADD_ITEM,
    FOREIGN_MENU_MODIFY_ITEM,
    FOREIGN_MENU_REMOVE_ITEM,
    FOREIGN_MENU_CLEAR,

    //spice client -> external app
    FOREIGN_MENU_ITEM_EVENT = 1001,
    FOREIGN_MENU_APP_ACTIVATED,
    FOREIGN_MENU_APP_DEACTIVATED,
};

typedef struct SPICE_ATTR_PACKED FrgMenuSetTitle {
    FrgMenuMsg base;
    uint8_t string[0]; //UTF8
} FrgMenuSetTitle;

enum {
    FOREIGN_MENU_ITEM_TYPE_CHECKED      = 1 << 0,
    FOREIGN_MENU_ITEM_TYPE_DIM          = 1 << 1,
    FOREIGN_MENU_ITEM_TYPE_SEPARATOR    = 1 << 2
};

#define FOREIGN_MENU_INVALID_ID 0

typedef struct SPICE_ATTR_PACKED FrgMenuAddItem {
    FrgMenuMsg base;
    uint32_t id;
    uint32_t type;
    uint32_t position;
    uint8_t string[0]; //UTF8
} FrgMenuAddItem, FrgMenuModItem;

typedef struct SPICE_ATTR_PACKED FrgMenuRmItem {
    FrgMenuMsg base;
    uint32_t id;
} FrgMenuRmItem;

typedef struct FrgMenuMsg FrgMenuRmItems;
typedef struct FrgMenuMsg FrgMenuDelete;

enum {
    FOREIGN_MENU_EVENT_CLICK,
    FOREIGN_MENU_EVENT_CHECKED,
    FOREIGN_MENU_EVENT_UNCHECKED
};

typedef struct SPICE_ATTR_PACKED FrgMenuEvent {
    FrgMenuMsg base;
    uint32_t id;
    uint32_t action; //FOREIGN_MENU_EVENT_?
} FrgMenuEvent;

typedef struct FrgMenuMsg FrgMenuActivate;
typedef struct FrgMenuMsg FrgMenuDeactivate;

#include <spice/end-packed.h>

#endif
