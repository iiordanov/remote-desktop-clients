/* usbredirhost.c usb network redirection usb host code header

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
#ifndef __USBREDIRHOST_H
#define __USBREDIRHOST_H

#include <libusb.h>
#include "usbredirparser.h"
#include "usbredirfilter.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbredirhost;

typedef void (*usbredirhost_flush_writes)(void *priv);

/* This function creates an usbredirhost instance, including its embedded
   libusbredirparser instance and sends the initial usb_redir_hello packet to
   the usb-guest.

   If usb_dev_handle is not NULL, usbredirhost_set_device will be called
   with the passed in usb_dev_handle after creating the instance. If
   usbredirhost_set_device fails, the instance will be destroyed and NULL
   will be returned.

   log_func is called by the usbredirhost to log various messages

   read_guest_data_func / write_guest_data_func are called by the
   usbredirhost to read/write data from/to the usb-guest.

   This function returns a pointer to the created usbredirhost object on
   success, or NULL on failure.
   Note:
   1) Both the usbredirtransport_log and the usbredirtransport_write
      callbacks may get called before this function completes.
   2) It is the responsibility of the code instantiating the usbredirhost
      to make sure that libusb_handle_events gets called (using the
      libusb_context from the passed in libusb_device_handle) when there are
      events waiting on the filedescriptors libusb_get_pollfds returns
   3) usbredirhost is partially multi-thread safe, see README.multi-thread
*/

enum {
    usbredirhost_fl_write_cb_owns_buffer = 0x01, /* See usbredirparser.h */
};

struct usbredirhost *usbredirhost_open(
    libusb_context *usb_ctx,
    libusb_device_handle *usb_dev_handle,
    usbredirparser_log log_func,
    usbredirparser_read  read_guest_data_func,
    usbredirparser_write write_guest_data_func,
    void *func_priv, const char *version, int verbose, int flags);

/* See README.multi-thread */
struct usbredirhost *usbredirhost_open_full(
    libusb_context *usb_ctx,
    libusb_device_handle *usb_dev_handle,
    usbredirparser_log log_func,
    usbredirparser_read  read_guest_data_func,
    usbredirparser_write write_guest_data_func,
    usbredirhost_flush_writes flush_writes_func,
    usbredirparser_alloc_lock alloc_lock_func,
    usbredirparser_lock lock_func,
    usbredirparser_unlock unlock_func,
    usbredirparser_free_lock free_lock_func,
    void *func_priv, const char *version, int verbose, int flags);

/* Closes (destroys) the usbredirhost, if the usbredirhost currently
   is redirecting a device this function will first call
   usbredirhost_set_device(host, NULL); See the notes for that function!
*/
void usbredirhost_close(struct usbredirhost *host);

/* Call this function with a valid libusb_device_handle to send the initial
   device info (interface_info, ep_info and device_connect packets) and make
   the device available to the usbredir-guest connected to the usbredir-host.

   Note:
   1) This function *takes ownership of* the passed in libusb_device_handle.
   2) The passed in libusb_device_handle is closed on failure.
   3) If the host already has a device that will get disconnected and closed.

   Call this function with NULL as usb_dev_handle to disconnect a redirected
   device from the usbredir-guest and make the device available to the OS
   on which the usbredir-host is running again.

   Note when disconnecting a redirected device, this function calls
   libusb_handle_events to "reap" cancelled urbs before closing the libusb
   device handle. This means that if you are using the same libusb context
   for other purposes your transfer complete callbacks may get called!

   This function returns a usbredirproto.h status code (ie usb_redir_success)
*/
int usbredirhost_set_device(struct usbredirhost *host,
                            libusb_device_handle *usb_dev_handle);

/* Call this whenever there is data ready for the usbredirhost to read from
   the usb-guest
   returns 0 on success, or an error code from the below enum on error.
   On an usbredirhost_read_io_error this function will continue where it
   left of the last time on the next call. On an usbredirhost_read_parse_error
   it will skip to the next packet (*). On an usbredirhost_read_device_rejected
   error, you are expected to call usbredirhost_set_device(host, NULL).
   An usbredirhost_read_device_lost error means that the host has done the
   equivalent of usbredirhost_set_device(host, NULL) itself because the
   connection to the device was lost.
   *) As determined by the faulty's package headers length field */
enum {
    usbredirhost_read_io_error        = -1,
    usbredirhost_read_parse_error     = -2,
    usbredirhost_read_device_rejected = -3,
    usbredirhost_read_device_lost     = -4,
};
int usbredirhost_read_guest_data(struct usbredirhost *host);

/* This returns the number of usbredir packets queued up for writing */
int usbredirhost_has_data_to_write(struct usbredirhost *host);

/* Call this when usbredirhost_has_data_to_write returns > 0
   returns 0 on success, -1 if a write error happened.
   If a write error happened, this function will retry writing any queued data
   on the next call, and will continue doing so until it has succeeded! */
enum {
    usbredirhost_write_io_error       = -1,
};
int usbredirhost_write_guest_data(struct usbredirhost *host);

/* When passing the usbredirhost_fl_write_cb_owns_buffer flag to
   usbredirhost_open, this function must be called to free the data buffer
   passed to write_guest_data_func when done with this buffer. */
void usbredirhost_free_write_buffer(struct usbredirhost *host, uint8_t *data);

/* Get the *usbredir-guest's* filter, if any. If there is no filter,
   rules is set to NULL and rules_count to 0. */
void usbredirhost_get_guest_filter(struct usbredirhost *host,
    const struct usbredirfilter_rule **rules_ret, int *rules_count_ret);

/* Get device and config descriptors from the USB device dev, and call
   usbredirfilter_check with the passed in filter rules and the needed info
   from the descriptors, flags gets passed to usbredirfilter_check unmodified.
   See the documentation of usbredirfilter_check for more details.

   Return value: -EIO or -ENOMEM when getting the descriptors fails, otherwise
       it returns the return value of the usbredirfilter_check call. */
int usbredirhost_check_device_filter(const struct usbredirfilter_rule *rules,
    int rules_count, libusb_device *dev, int flags);

#ifdef __cplusplus
}
#endif

#endif
