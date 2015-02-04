/* usbredirproto.h usb redirection protocol definitions

   Copyright 2010-2011 Red Hat, Inc.

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
#ifndef __USBREDIRPROTO_H
#define __USBREDIRPROTO_H

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

#ifdef __cplusplus
extern "C" {
#endif

#define USBREDIR_VERSION 0x000700 /* 0.7 [.0] */

enum {
    usb_redir_success,
    usb_redir_cancelled,    /* The transfer was cancelled */
    usb_redir_inval,        /* Invalid packet type / length / ep, etc. */
    usb_redir_ioerror,      /* IO error */
    usb_redir_stall,        /* Stalled */
    usb_redir_timeout,      /* Request timed out */
    usb_redir_babble,       /* The device has "babbled" (since 0.4.2) */
};

enum {
    /* Note these 4 match the usb spec! */
    usb_redir_type_control,
    usb_redir_type_iso,
    usb_redir_type_bulk,
    usb_redir_type_interrupt,
    usb_redir_type_invalid = 255
};

enum {
    usb_redir_speed_low,
    usb_redir_speed_full,
    usb_redir_speed_high,
    usb_redir_speed_super,
    usb_redir_speed_unknown = 255
};

enum {
    /* Control packets */
    usb_redir_hello,
    usb_redir_device_connect,
    usb_redir_device_disconnect,
    usb_redir_reset,
    usb_redir_interface_info,
    usb_redir_ep_info,
    usb_redir_set_configuration,
    usb_redir_get_configuration,
    usb_redir_configuration_status,
    usb_redir_set_alt_setting,
    usb_redir_get_alt_setting,
    usb_redir_alt_setting_status,
    usb_redir_start_iso_stream,
    usb_redir_stop_iso_stream,
    usb_redir_iso_stream_status,
    usb_redir_start_interrupt_receiving,
    usb_redir_stop_interrupt_receiving,
    usb_redir_interrupt_receiving_status,
    usb_redir_alloc_bulk_streams,
    usb_redir_free_bulk_streams,
    usb_redir_bulk_streams_status,
    usb_redir_cancel_data_packet,
    usb_redir_filter_reject,
    usb_redir_filter_filter,
    usb_redir_device_disconnect_ack,
    usb_redir_start_bulk_receiving,
    usb_redir_stop_bulk_receiving,
    usb_redir_bulk_receiving_status,

    /* Data packets */
    usb_redir_control_packet = 100,
    usb_redir_bulk_packet,
    usb_redir_iso_packet,
    usb_redir_interrupt_packet,
    usb_redir_buffered_bulk_packet,
};

enum {
    /* Supports USB 3 bulk streams */
    usb_redir_cap_bulk_streams, 
    /* The device_connect packet has the device_version_bcd field */
    usb_redir_cap_connect_device_version,
    /* Supports usb_redir_filter_reject and usb_redir_filter_filter pkts */
    usb_redir_cap_filter,
    /* Supports the usb_redir_device_disconnect_ack packet */
    usb_redir_cap_device_disconnect_ack,
    /* The ep_info packet has the max_packet_size field */
    usb_redir_cap_ep_info_max_packet_size,
    /* Supports 64 bits ids in usb_redir_header */
    usb_redir_cap_64bits_ids,
    /* Supports 32 bits length in usb_redir_bulk_packet_header */
    usb_redir_cap_32bits_bulk_length,
    /* Supports bulk receiving / buffered bulk input */
    usb_redir_cap_bulk_receiving,
};
/* Number of uint32_t-s needed to hold all (known) capabilities */
#define USB_REDIR_CAPS_SIZE 1

struct usb_redir_header {
    uint32_t type;
    uint32_t length;
    uint64_t id;  
} ATTR_PACKED;

struct usb_redir_hello_header {
    char     version[64];
    uint32_t capabilities[0];
} ATTR_PACKED;

struct usb_redir_device_connect_header {
    uint8_t speed;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version_bcd;
} ATTR_PACKED;

struct usb_redir_interface_info_header {
    uint32_t interface_count;
    uint8_t interface[32];
    uint8_t interface_class[32];
    uint8_t interface_subclass[32];
    uint8_t interface_protocol[32];
} ATTR_PACKED;

struct usb_redir_ep_info_header {
    uint8_t type[32];
    uint8_t interval[32];
    uint8_t interface[32];
    uint16_t max_packet_size[32];
    uint32_t max_streams[32];
} ATTR_PACKED;

struct usb_redir_set_configuration_header {
    uint8_t configuration;
} ATTR_PACKED;

struct usb_redir_configuration_status_header {
    uint8_t status;
    uint8_t configuration;
} ATTR_PACKED;

struct usb_redir_set_alt_setting_header {
    uint8_t interface;
    uint8_t alt;
} ATTR_PACKED;

struct usb_redir_get_alt_setting_header {
    uint8_t interface;
} ATTR_PACKED;

struct usb_redir_alt_setting_status_header {
    uint8_t status;
    uint8_t interface;
    uint8_t alt;
} ATTR_PACKED;

struct usb_redir_start_iso_stream_header {
    uint8_t endpoint;
    uint8_t pkts_per_urb;
    uint8_t no_urbs;
} ATTR_PACKED;

struct usb_redir_stop_iso_stream_header {
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_iso_stream_status_header {
    uint8_t status;
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_start_interrupt_receiving_header {
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_stop_interrupt_receiving_header {
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_interrupt_receiving_status_header {
    uint8_t status;
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_alloc_bulk_streams_header {
    uint32_t endpoints; /* bitmask indicating on which eps to alloc streams */
    uint32_t no_streams;
} ATTR_PACKED;

struct usb_redir_free_bulk_streams_header {
    uint32_t endpoints; /* bitmask indicating on which eps to free streams */
} ATTR_PACKED;

struct usb_redir_bulk_streams_status_header {
    uint32_t endpoints; /* bitmask indicating eps this status message is for */
    uint32_t no_streams;
    uint8_t status;
} ATTR_PACKED;

struct usb_redir_start_bulk_receiving_header {
    uint32_t stream_id;
    uint32_t bytes_per_transfer;
    uint8_t endpoint;
    uint8_t no_transfers;
} ATTR_PACKED;

struct usb_redir_stop_bulk_receiving_header {
    uint32_t stream_id;
    uint8_t endpoint;
} ATTR_PACKED;

struct usb_redir_bulk_receiving_status_header {
    uint32_t stream_id;
    uint8_t endpoint;
    uint8_t status;
} ATTR_PACKED;

struct usb_redir_control_packet_header {
    uint8_t endpoint;
    uint8_t request;
    uint8_t requesttype;
    uint8_t status;
    uint16_t value;
    uint16_t index;
    uint16_t length; 
} ATTR_PACKED;

struct usb_redir_bulk_packet_header {
    uint8_t endpoint;
    uint8_t status;
    uint16_t length;
    uint32_t stream_id;
    uint16_t length_high; /* High 16 bits of the packet length */
} ATTR_PACKED;

struct usb_redir_iso_packet_header {
    uint8_t endpoint;
    uint8_t status;
    uint16_t length;
} ATTR_PACKED;

struct usb_redir_interrupt_packet_header {
    uint8_t endpoint;
    uint8_t status;
    uint16_t length;
} ATTR_PACKED;

struct usb_redir_buffered_bulk_packet_header {
    uint32_t stream_id;
    uint32_t length;
    uint8_t endpoint;
    uint8_t status;
} ATTR_PACKED;

#undef ATTR_PACKED

#if defined(__MINGW32__) || !defined(__GNUC__)
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif
