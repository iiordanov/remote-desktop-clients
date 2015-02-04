/* usbredirparser.h usb redirection protocol parser header

   Copyright 2010-2012 Red Hat, Inc.

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
#ifndef __USBREDIRPARSER_H
#define __USBREDIRPARSER_H

#include "usbredirproto.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbredirparser;
struct usbredirfilter_rule;

/* Called by a usbredirparser to log various messages */
enum { usbredirparser_none, usbredirparser_error, usbredirparser_warning,
       usbredirparser_info, usbredirparser_debug, usbredirparser_debug_data };
typedef void (*usbredirparser_log)(void *priv, int level, const char *msg);

/* Called by a usbredirparser to read/write data to its peer.
   Must return the amount of bytes read/written, 0 when the read/write would
   block (and this is undesirable) and -1 on error.

   If the usbredirparser_fl_write_cb_owns_buffer flag is passed to
   usbredirparser_init, then the usbredirparser_write callback becomes
   the owner of the buffer pointed to by data and should call
   usbredirparser_free_write_buffer() when it is done with the buffer.
   In this case the callback is not allowed to return any amount of bytes
   written, it must either accept the entire buffer (return count),
   or signal blocking (return 0) or error (return -1). Returning any other
   value will result in a call to abort().
*/
typedef int (*usbredirparser_read)(void *priv, uint8_t *data, int count);
typedef int (*usbredirparser_write)(void *priv, uint8_t *data, int count);

/* Locking functions for use by multithread apps */
typedef void *(*usbredirparser_alloc_lock)(void);
typedef void (*usbredirparser_lock)(void *lock);
typedef void (*usbredirparser_unlock)(void *lock);
typedef void (*usbredirparser_free_lock)(void *lock);

/* The below callbacks are called when a complete packet of the relevant
   type has been received.

   Note that the passed in packet-type-specific-header's lifetime is only
   guarenteed to be that of the callback.

   Control packets: */
typedef void (*usbredirparser_hello)(void *priv,
    struct usb_redir_hello_header *hello);
typedef void (*usbredirparser_device_connect)(void *priv,
    struct usb_redir_device_connect_header *device_connect);
typedef void (*usbredirparser_device_disconnect)(void *priv);
typedef void (*usbredirparser_reset)(void *priv);
typedef void (*usbredirparser_interface_info)(void *priv,
    struct usb_redir_interface_info_header *interface_info);
typedef void (*usbredirparser_ep_info)(void *priv,
    struct usb_redir_ep_info_header *ep_info);
typedef void (*usbredirparser_set_configuration)(void *priv,
    uint64_t id, struct usb_redir_set_configuration_header *set_configuration);
typedef void (*usbredirparser_get_configuration)(void *priv, uint64_t id);
typedef void (*usbredirparser_configuration_status)(void *priv,
    uint64_t id, struct usb_redir_configuration_status_header *configuration_status);
typedef void (*usbredirparser_set_alt_setting)(void *priv,
    uint64_t id, struct usb_redir_set_alt_setting_header *set_alt_setting);
typedef void (*usbredirparser_get_alt_setting)(void *priv,
    uint64_t id, struct usb_redir_get_alt_setting_header *get_alt_setting);
typedef void (*usbredirparser_alt_setting_status)(void *priv,
    uint64_t id, struct usb_redir_alt_setting_status_header *alt_setting_status);
typedef void (*usbredirparser_start_iso_stream)(void *priv,
    uint64_t id, struct usb_redir_start_iso_stream_header *start_iso_stream);
typedef void (*usbredirparser_stop_iso_stream)(void *priv,
    uint64_t id, struct usb_redir_stop_iso_stream_header *stop_iso_stream);
typedef void (*usbredirparser_iso_stream_status)(void *priv,
    uint64_t id, struct usb_redir_iso_stream_status_header *iso_stream_status);
typedef void (*usbredirparser_start_interrupt_receiving)(void *priv,
    uint64_t id, struct usb_redir_start_interrupt_receiving_header *start_interrupt_receiving);
typedef void (*usbredirparser_stop_interrupt_receiving)(void *priv,
    uint64_t id, struct usb_redir_stop_interrupt_receiving_header *stop_interrupt_receiving);
typedef void (*usbredirparser_interrupt_receiving_status)(void *priv,
    uint64_t id, struct usb_redir_interrupt_receiving_status_header *interrupt_receiving_status);
typedef void (*usbredirparser_alloc_bulk_streams)(void *priv,
    uint64_t id, struct usb_redir_alloc_bulk_streams_header *alloc_bulk_streams);
typedef void (*usbredirparser_free_bulk_streams)(void *priv,
    uint64_t id, struct usb_redir_free_bulk_streams_header *free_bulk_streams);
typedef void (*usbredirparser_bulk_streams_status)(void *priv,
    uint64_t id, struct usb_redir_bulk_streams_status_header *bulk_streams_status);
typedef void (*usbredirparser_cancel_data_packet)(void *priv, uint64_t id);
typedef void (*usbredirparser_filter_reject)(void *priv);
/* Note that the ownership of the rules array is passed on to the callback. */
typedef void (*usbredirparser_filter_filter)(void *priv,
    struct usbredirfilter_rule *rules, int rules_count);
typedef void (*usbredirparser_device_disconnect_ack)(void *priv);
typedef void (*usbredirparser_start_bulk_receiving)(void *priv,
    uint64_t id, struct usb_redir_start_bulk_receiving_header *start_bulk_receiving);
typedef void (*usbredirparser_stop_bulk_receiving)(void *priv,
    uint64_t id, struct usb_redir_stop_bulk_receiving_header *stop_bulk_receiving);
typedef void (*usbredirparser_bulk_receiving_status)(void *priv,
    uint64_t id, struct usb_redir_bulk_receiving_status_header *bulk_receiving_status);

/* Data packets:

   Note that ownership of the the data buffer (if not NULL) is passed on to
   the callback. The callback should free it by calling
   usbredirparser_free_packet_data when it is done with it. */
typedef void (*usbredirparser_control_packet)(void *priv,
    uint64_t id, struct usb_redir_control_packet_header *control_header,
    uint8_t *data, int data_len);
typedef void (*usbredirparser_bulk_packet)(void *priv,
    uint64_t id, struct usb_redir_bulk_packet_header *bulk_header,
    uint8_t *data, int data_len);
typedef void (*usbredirparser_iso_packet)(void *priv,
    uint64_t id, struct usb_redir_iso_packet_header *iso_header,
    uint8_t *data, int data_len);
typedef void (*usbredirparser_interrupt_packet)(void *priv,
    uint64_t id, struct usb_redir_interrupt_packet_header *interrupt_header,
    uint8_t *data, int data_len);
typedef void (*usbredirparser_buffered_bulk_packet)(void *priv, uint64_t id,
    struct usb_redir_buffered_bulk_packet_header *buffered_bulk_header,
    uint8_t *data, int data_len);


/* Public part of the data allocated by usbredirparser_alloc, *never* allocate
   a usbredirparser struct yourself, it may be extended in the future to add
   callbacks for new packet types (which will then get added at the end),
   *and* usbredirparser_alloc will also alloc some space behind it for
   private data */
struct usbredirparser {
    /* app private data passed into all callbacks as the priv argument */
    void *priv;
    /* non packet callbacks */
    usbredirparser_log log_func;
    usbredirparser_read read_func;
    usbredirparser_write write_func;
    /* usb-redir-protocol v0.3 control packet complete callbacks */
    usbredirparser_device_connect device_connect_func;
    usbredirparser_device_disconnect device_disconnect_func;
    usbredirparser_reset reset_func;
    usbredirparser_interface_info interface_info_func;
    usbredirparser_ep_info ep_info_func;
    usbredirparser_set_configuration set_configuration_func;
    usbredirparser_get_configuration get_configuration_func;
    usbredirparser_configuration_status configuration_status_func;
    usbredirparser_set_alt_setting set_alt_setting_func;
    usbredirparser_get_alt_setting get_alt_setting_func;
    usbredirparser_alt_setting_status alt_setting_status_func;
    usbredirparser_start_iso_stream start_iso_stream_func;
    usbredirparser_stop_iso_stream stop_iso_stream_func;
    usbredirparser_iso_stream_status iso_stream_status_func;
    usbredirparser_start_interrupt_receiving start_interrupt_receiving_func;
    usbredirparser_stop_interrupt_receiving stop_interrupt_receiving_func;
    usbredirparser_interrupt_receiving_status interrupt_receiving_status_func;
    usbredirparser_alloc_bulk_streams alloc_bulk_streams_func;
    usbredirparser_free_bulk_streams free_bulk_streams_func;
    usbredirparser_bulk_streams_status bulk_streams_status_func;
    usbredirparser_cancel_data_packet cancel_data_packet_func;
    /* usb-redir-protocol v0.3 data packet complete callbacks */
    usbredirparser_control_packet control_packet_func;
    usbredirparser_bulk_packet bulk_packet_func;
    usbredirparser_iso_packet iso_packet_func;
    usbredirparser_interrupt_packet interrupt_packet_func;
    /* usbredir 0.3.2 new non packet callbacks (for multi-thread locking) */
    usbredirparser_alloc_lock alloc_lock_func;
    usbredirparser_lock lock_func;
    usbredirparser_unlock unlock_func;
    usbredirparser_free_lock free_lock_func;
    /* usbredir 0.3.2 new control packet complete callbacks */
    usbredirparser_hello hello_func;
    /* usbredir 0.4 new control packet complete callbacks */
    usbredirparser_filter_reject filter_reject_func;
    usbredirparser_filter_filter filter_filter_func;
    usbredirparser_device_disconnect_ack device_disconnect_ack_func;
    /* usbredir 0.6 new control packet complete callbacks */
    usbredirparser_start_bulk_receiving start_bulk_receiving_func;
    usbredirparser_stop_bulk_receiving stop_bulk_receiving_func;
    usbredirparser_bulk_receiving_status bulk_receiving_status_func;
    /* usbredir 0.6 new data packet complete callbacks */
    usbredirparser_buffered_bulk_packet buffered_bulk_packet_func;
};

/* Allocate a usbredirparser, after this the app should set the callback app
   private data and all the callbacks it needs, before calling
   usbredirparser_init */
struct usbredirparser *usbredirparser_create(void);

/* Set capability cap in the USB_REDIR_CAPS_SIZE sized caps array,
   this is a helper function to set capabilities in the caps array
   passed to usbredirparser_init(). */
void usbredirparser_caps_set_cap(uint32_t *caps, int cap);

/* Init the parser, this will queue an initial usb_redir_hello packet,
   sending the version and caps to the peer, as well as configure the parsing
   according to the passed in flags. */
enum {
    usbredirparser_fl_usb_host = 0x01,
    usbredirparser_fl_write_cb_owns_buffer = 0x02,
    usbredirparser_fl_no_hello = 0x04,
};

void usbredirparser_init(struct usbredirparser *parser,
    const char *version, uint32_t *caps, int caps_len, int flags);

void usbredirparser_destroy(struct usbredirparser *parser);

/* See if our side has a certain cap (checks the caps passed into _init) */
int usbredirparser_have_cap(struct usbredirparser *parser, int cap);

/* Check if we've received the caps from the peer */
int usbredirparser_have_peer_caps(struct usbredirparser *parser);

/* Check if our peer has a certain capability. Note this function
   should not be used before the hello_func callback has been called. */
int usbredirparser_peer_has_cap(struct usbredirparser *parser, int cap);

/* Call this whenever there is data ready from the otherside to parse
   On an usbredirparser_read_io_error this function will continue where it
   left of the last time on the next call. On an
   usbredirparser_read_parse_error it will skip to the next packet (*).
   *) As determined by the faulty's package headers length field */
enum {
    usbredirparser_read_io_error    = -1,
    usbredirparser_read_parse_error = -2,
};
int usbredirparser_do_read(struct usbredirparser *parser);

/* This returns the number of usbredir packets queued up for writing */
int usbredirparser_has_data_to_write(struct usbredirparser *parser);

/* Call this when usbredirparser_has_data_to_write returns > 0
   returns 0 on success, -1 if a write error happened.
   If a write error happened, this function will retry writing any queued data
   on the next call, and will continue doing so until it has succeeded! */
enum {
    usbredirparser_write_io_error   = -1,
};
int usbredirparser_do_write(struct usbredirparser *parser);

/* See usbredirparser_write documentation */
void usbredirparser_free_write_buffer(struct usbredirparser *parser,
    uint8_t *data);

/* See the data packet callbacks documentation */
void usbredirparser_free_packet_data(struct usbredirparser *parser,
    uint8_t *data);

/* Functions to marshall and queue a packet for sending to its peer. Note:
   1) it will not be actually send until usbredirparser_do_write is called
   2) if their is not enough memory for buffers the packet will be dropped
      (and an error message will be logged */
/* Control packets: */

/* Note this function should not be used before the hello_func callback has
   been called (as it checks the usb_redir_cap_connect_device_version cap). */
void usbredirparser_send_device_connect(struct usbredirparser *parser,
    struct usb_redir_device_connect_header *device_connect);
void usbredirparser_send_device_disconnect(struct usbredirparser *parser);
void usbredirparser_send_reset(struct usbredirparser *parser);
void usbredirparser_send_interface_info(struct usbredirparser *parser,
    struct usb_redir_interface_info_header *interface_info);
void usbredirparser_send_ep_info(struct usbredirparser *parser,
    struct usb_redir_ep_info_header *ep_info);
void usbredirparser_send_set_configuration(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_set_configuration_header *set_configuration);
void usbredirparser_send_get_configuration(struct usbredirparser *parser,
    uint64_t id);
void usbredirparser_send_configuration_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_configuration_status_header *configuration_status);
void usbredirparser_send_set_alt_setting(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_set_alt_setting_header *set_alt_setting);
void usbredirparser_send_get_alt_setting(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_get_alt_setting_header *get_alt_setting);
void usbredirparser_send_alt_setting_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_alt_setting_status_header *alt_setting_status);
void usbredirparser_send_start_iso_stream(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_start_iso_stream_header *start_iso_stream);
void usbredirparser_send_stop_iso_stream(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_stop_iso_stream_header *stop_iso_stream);
void usbredirparser_send_iso_stream_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_iso_stream_status_header *iso_stream_status);
void usbredirparser_send_start_interrupt_receiving(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_start_interrupt_receiving_header *start_interrupt_receiving);
void usbredirparser_send_stop_interrupt_receiving(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_stop_interrupt_receiving_header *stop_interrupt_receiving);
void usbredirparser_send_interrupt_receiving_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_interrupt_receiving_status_header *interrupt_receiving_status);
void usbredirparser_send_alloc_bulk_streams(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_alloc_bulk_streams_header *alloc_bulk_streams);
void usbredirparser_send_free_bulk_streams(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_free_bulk_streams_header *free_bulk_streams);
void usbredirparser_send_bulk_streams_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_bulk_streams_status_header *bulk_streams_status);
void usbredirparser_send_cancel_data_packet(struct usbredirparser *parser,
    uint64_t id);
void usbredirparser_send_filter_reject(struct usbredirparser *parser);
void usbredirparser_send_filter_filter(struct usbredirparser *parser,
    const struct usbredirfilter_rule *rules, int rules_count);
void usbredirparser_send_start_bulk_receiving(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_start_bulk_receiving_header *start_bulk_receiving);
void usbredirparser_send_stop_bulk_receiving(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_stop_bulk_receiving_header *stop_bulk_receiving);
void usbredirparser_send_bulk_receiving_status(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_bulk_receiving_status_header *bulk_receiving_status);
/* Data packets: */
void usbredirparser_send_control_packet(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_control_packet_header *control_header,
    uint8_t *data, int data_len);
void usbredirparser_send_bulk_packet(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_bulk_packet_header *bulk_header,
    uint8_t *data, int data_len);
void usbredirparser_send_iso_packet(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_iso_packet_header *iso_header,
    uint8_t *data, int data_len);
void usbredirparser_send_interrupt_packet(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_interrupt_packet_header *interrupt_header,
    uint8_t *data, int data_len);
void usbredirparser_send_buffered_bulk_packet(struct usbredirparser *parser,
    uint64_t id,
    struct usb_redir_buffered_bulk_packet_header *buffered_bulk_header,
    uint8_t *data, int data_len);


/* Serialization */

/* This function serializes the current usbredirparser state. It will allocate
   a large enough buffer for this itself and store this in state_dest, it will
   store the size of this buffer in state_len.

   Return value: 0 on success, -1 on error (out of memory).

   The buffer should be free-ed by the caller using free(). */
int usbredirparser_serialize(struct usbredirparser *parser,
                             uint8_t **state_dest, int *state_len);

/* This function sets the current usbredirparser state from a serialized state.
   This function assumes that the parser has just been initialized with the
   usbredirparser_fl_no_hello flag.

   Return value: 0 on success, -1 on error (out of memory, or
                                            invalid state data). */
int usbredirparser_unserialize(struct usbredirparser *parser_pub,
                               uint8_t *state, int len);

#ifdef __cplusplus
}
#endif

#endif
