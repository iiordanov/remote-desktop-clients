/* usbredirproto-compat.h usb redirection compatibility protocol definitions

   Copyright 2011 Red Hat, Inc.

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
#ifndef __USBREDIRPROTO_COMPAT_H
#define __USBREDIRPROTO_COMPAT_H

/* PACK macros borrowed from spice-protocol */
#ifdef __GNUC__

#define ATTR_PACKED __attribute__ ((__packed__))

#ifdef __MINGW32__
#pragma pack(push,1)
#endif

#else

#pragma pack(push)
#pragma pack(1)
#define ATTR_PACKED
#pragma warning(disable:4200)
#pragma warning(disable:4103)

#endif

#include <stdint.h>

struct usb_redir_device_connect_header_no_device_version {
    uint8_t speed;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint16_t vendor_id;
    uint16_t product_id;
} ATTR_PACKED;

struct usb_redir_ep_info_header_no_max_pktsz {
    uint8_t type[32];
    uint8_t interval[32];
    uint8_t interface[32];
} ATTR_PACKED;

struct usb_redir_ep_info_header_no_max_streams {
    uint8_t type[32];
    uint8_t interval[32];
    uint8_t interface[32];
    uint16_t max_packet_size[32];
} ATTR_PACKED;

struct usb_redir_header_32bit_id {
    uint32_t type;
    uint32_t length;
    uint32_t id;
} ATTR_PACKED;

struct usb_redir_bulk_packet_header_16bit_length {
    uint8_t endpoint;
    uint8_t status;
    uint16_t length;
    uint32_t stream_id;
} ATTR_PACKED;

#undef ATTR_PACKED

#if defined(__MINGW32__) || !defined(__GNUC__)
#pragma pack(pop)
#endif

#endif
