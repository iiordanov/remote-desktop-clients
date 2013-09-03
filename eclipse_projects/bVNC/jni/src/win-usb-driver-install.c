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

/*
 * Some notes:
 * Each installer (instance) opens a named-pipe to talk with win-usb-clerk.
 * Each installer (instance) requests driver installation for a single device.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>
#include <gio/gio.h>
#include <gio/gwin32inputstream.h>
#include <gio/gwin32outputstream.h>
#include "spice-util.h"
#include "win-usb-clerk.h"
#include "win-usb-driver-install.h"
#include "usb-device-manager-priv.h"

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_WIN_USB_DRIVER_GET_PRIVATE(obj)     \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_WIN_USB_DRIVER, SpiceWinUsbDriverPrivate))

struct _SpiceWinUsbDriverPrivate {
    USBClerkReply         reply;
    GSimpleAsyncResult    *result;
    GCancellable          *cancellable;
    HANDLE                handle;
    SpiceUsbDevice        *device;
};



G_DEFINE_TYPE(SpiceWinUsbDriver, spice_win_usb_driver, G_TYPE_OBJECT);

static void spice_win_usb_driver_init(SpiceWinUsbDriver *self)
{
    self->priv = SPICE_WIN_USB_DRIVER_GET_PRIVATE(self);
}

static void spice_win_usb_driver_close(SpiceWinUsbDriver *self)
{
    if (self->priv->handle) {
        CloseHandle(self->priv->handle);
        self->priv->handle = 0;
    }
}

static void spice_win_usb_driver_finalize(GObject *gobject)
{
    SpiceWinUsbDriver *self = SPICE_WIN_USB_DRIVER(gobject);
    SpiceWinUsbDriverPrivate *priv = self->priv;

    spice_win_usb_driver_close(self);
    g_clear_object(&priv->result);
}

static void spice_win_usb_driver_class_init(SpiceWinUsbDriverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize     = spice_win_usb_driver_finalize;

    g_type_class_add_private(klass, sizeof(SpiceWinUsbDriverPrivate));
}

/* ------------------------------------------------------------------ */
/* callbacks                                                          */

void win_usb_driver_handle_reply_cb(GObject *gobject,
                                    GAsyncResult *read_res,
                                    gpointer user_data)
{
    SpiceWinUsbDriver *self;
    SpiceWinUsbDriverPrivate *priv;

    GInputStream *istream;
    GError *err = NULL;
    gssize bytes;

    g_return_if_fail(SPICE_IS_WIN_USB_DRIVER(user_data));
    self = SPICE_WIN_USB_DRIVER(user_data);
    priv = self->priv;
    istream = G_INPUT_STREAM(gobject);

    bytes = g_input_stream_read_finish(istream, read_res, &err);

    SPICE_DEBUG("Finished reading reply-msg from usbclerk: bytes=%ld "
                "err_exist?=%d", (long)bytes, err!=NULL);

    g_warn_if_fail(g_input_stream_close(istream, NULL, NULL));
    g_clear_object(&istream);

    if (err) {
        g_warning("failed to read reply from usbclerk (%s)", err->message);
        g_simple_async_result_take_error(priv->result, err);
        goto failed_reply;
    }

    if (bytes == 0) {
        g_warning("unexpected EOF from usbclerk");
        g_simple_async_result_set_error(priv->result,
                                        SPICE_WIN_USB_DRIVER_ERROR,
                                        SPICE_WIN_USB_DRIVER_ERROR_FAILED,
                                        "unexpected EOF from usbclerk");
        goto failed_reply;
    }

    if (bytes != sizeof(priv->reply)) {
        g_warning("usbclerk size mismatch: read %d bytes, expected %d (header %d, size in header %d)",
                  bytes, sizeof(priv->reply), sizeof(priv->reply.hdr), priv->reply.hdr.size);
        /* For now just warn, do not fail */
    }

    if (priv->reply.hdr.magic != USB_CLERK_MAGIC) {
        g_warning("usbclerk magic mismatch: mine=0x%04x  server=0x%04x",
                  USB_CLERK_MAGIC, priv->reply.hdr.magic);
        g_simple_async_result_set_error(priv->result,
                                        SPICE_WIN_USB_DRIVER_ERROR,
                                        SPICE_WIN_USB_DRIVER_ERROR_MESSAGE,
                                        "usbclerk magic mismatch");
        goto failed_reply;
    }

    if (priv->reply.hdr.version != USB_CLERK_VERSION) {
        g_warning("usbclerk version mismatch: mine=0x%04x  server=0x%04x",
                  USB_CLERK_VERSION, priv->reply.hdr.version);
        g_simple_async_result_set_error(priv->result,
                                        SPICE_WIN_USB_DRIVER_ERROR,
                                        SPICE_WIN_USB_DRIVER_ERROR_MESSAGE,
                                        "usbclerk version mismatch");
    }

    if (priv->reply.hdr.type != USB_CLERK_REPLY) {
        g_warning("usbclerk message with unexpected type %d",
                  priv->reply.hdr.type);
        g_simple_async_result_set_error(priv->result,
                                        SPICE_WIN_USB_DRIVER_ERROR,
                                        SPICE_WIN_USB_DRIVER_ERROR_MESSAGE,
                                        "usbclerk message with unexpected type");
        goto failed_reply;
    }

    if (priv->reply.hdr.size != bytes) {
        g_warning("usbclerk message size mismatch: read %d bytes  hdr.size=%d",
                  bytes, priv->reply.hdr.size);
        g_simple_async_result_set_error(priv->result,
                                        SPICE_WIN_USB_DRIVER_ERROR,
                                        SPICE_WIN_USB_DRIVER_ERROR_MESSAGE,
                                        "usbclerk message with unexpected size");
        goto failed_reply;
    }

 failed_reply:
    g_simple_async_result_complete_in_idle(priv->result);
    g_clear_object(&priv->result);
}

/* ------------------------------------------------------------------ */
/* helper functions                                                   */

static
gboolean spice_win_usb_driver_send_request(SpiceWinUsbDriver *self, guint16 op,
                                           guint16 vid, guint16 pid, GError **err)
{
    USBClerkDriverOp req;
    GOutputStream *ostream;
    SpiceWinUsbDriverPrivate *priv;
    gsize bytes;
    gboolean ret;

    SPICE_DEBUG("sending a request to usbclerk service (op=%d vid=0x%04x pid=0x%04x",
                op, vid, pid);

    g_return_val_if_fail(SPICE_IS_WIN_USB_DRIVER(self), FALSE);
    priv = self->priv;

    memset(&req, 0, sizeof(req));
    req.hdr.magic   = USB_CLERK_MAGIC;
    req.hdr.version = USB_CLERK_VERSION;
    req.hdr.type    = op;
    req.hdr.size    = sizeof(req);
    req.vid = vid;
    req.pid = pid;

    ostream = g_win32_output_stream_new(priv->handle, FALSE);

    ret = g_output_stream_write_all(ostream, &req, sizeof(req), &bytes, NULL, err);
    g_warn_if_fail(g_output_stream_close(ostream, NULL, NULL));
    g_object_unref(ostream);
    SPICE_DEBUG("write_all request returned %d written bytes %u expecting %u",
                ret, bytes, sizeof(req));
    return ret;
}

static
void spice_win_usb_driver_read_reply_async(SpiceWinUsbDriver *self)
{
    SpiceWinUsbDriverPrivate *priv;
    GInputStream  *istream;

    g_return_if_fail(SPICE_IS_WIN_USB_DRIVER(self));
    priv = self->priv;

    SPICE_DEBUG("waiting for a reply from usbclerk");

    istream = g_win32_input_stream_new(priv->handle, FALSE);

    g_input_stream_read_async(istream, &priv->reply, sizeof(priv->reply),
                              G_PRIORITY_DEFAULT, priv->cancellable,
                              win_usb_driver_handle_reply_cb, self);
}


/* ------------------------------------------------------------------ */
/* private api                                                        */


G_GNUC_INTERNAL
SpiceWinUsbDriver *spice_win_usb_driver_new(void)
{
    GObject *obj;

    obj = g_object_new(SPICE_TYPE_WIN_USB_DRIVER, NULL);

    return SPICE_WIN_USB_DRIVER(obj);
}

static
void spice_win_usb_driver_op(SpiceWinUsbDriver *self,
                             SpiceUsbDevice *device,
                             guint16 op_type,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
    guint16 vid, pid;
    GError *err = NULL;
    GSimpleAsyncResult *result;
    SpiceWinUsbDriverPrivate *priv;

    g_return_if_fail(SPICE_IS_WIN_USB_DRIVER(self));
    g_return_if_fail(device != NULL);

    priv = self->priv;

    result = g_simple_async_result_new(G_OBJECT(self), callback, user_data,
                                       spice_win_usb_driver_op);

    if (priv->result) { /* allow one install/uninstall request at a time */
        g_warning("Another request exists -- try later");
        g_simple_async_result_set_error(result,
                  SPICE_WIN_USB_DRIVER_ERROR, SPICE_WIN_USB_DRIVER_ERROR_FAILED,
                  "Another request exists -- try later");
        goto failed_request;
    }


    vid = spice_usb_device_get_vid(device);
    pid = spice_usb_device_get_pid(device);

    if (! priv->handle ) {
        SPICE_DEBUG("win-usb-driver-install: connecting to usbclerk named pipe");
        priv->handle = CreateFile(USB_CLERK_PIPE_NAME,
                                  GENERIC_READ | GENERIC_WRITE,
                                  0, NULL,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                  NULL);
        if (priv->handle == INVALID_HANDLE_VALUE) {
            DWORD errval  = GetLastError();
            gchar *errstr = g_win32_error_message(errval);
            g_warning("failed to create a named pipe to usbclerk (%ld) %s",
                      errval,errstr);
            g_simple_async_result_set_error(result,
                      G_IO_ERROR, G_IO_ERROR_FAILED,
                      "Failed to create named pipe (%ld) %s", errval, errstr);
            goto failed_request;
        }
    }

    if (!spice_win_usb_driver_send_request(self, op_type,
                                           vid, pid, &err)) {
        g_warning("failed to send a request to usbclerk %s", err->message);
        g_simple_async_result_take_error(result, err);
        goto failed_request;
    }

    /* set up for async read */
    priv->result = result;
    priv->device = device;
    priv->cancellable = cancellable;

    spice_win_usb_driver_read_reply_async(self);

    return;

 failed_request:
    g_simple_async_result_complete_in_idle(result);
    g_clear_object(&result);
}



/**
 * spice_win_usb_driver_install:
 * Start libusb driver installation for @device
 *
 * A new NamedPipe is created for each request.
 *
 * Returns: TRUE if a request was sent to usbclerk
 *          FALSE upon failure to send a request.
 */
G_GNUC_INTERNAL
void spice_win_usb_driver_install(SpiceWinUsbDriver *self,
                                  SpiceUsbDevice *device,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    SPICE_DEBUG("Win usb driver installation started");

    spice_win_usb_driver_op(self, device, USB_CLERK_DRIVER_SESSION_INSTALL,
                            cancellable, callback, user_data);
}

G_GNUC_INTERNAL
void spice_win_usb_driver_uninstall(SpiceWinUsbDriver *self,
                                    SpiceUsbDevice *device,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    SPICE_DEBUG("Win usb driver uninstall operation started");

    spice_win_usb_driver_op(self, device, USB_CLERK_DRIVER_REMOVE, cancellable,
                            callback, user_data);
}


/**
 * Returns: currently returns 0 (failure) and 1 (success)
 * possibly later we'll add error-codes
 */
G_GNUC_INTERNAL
gint spice_win_usb_driver_install_finish(SpiceWinUsbDriver *self,
                                          GAsyncResult *res, GError **err)
{
    GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT(res);

    g_return_val_if_fail(SPICE_IS_WIN_USB_DRIVER(self), 0);
    g_return_val_if_fail(g_simple_async_result_is_valid(res, G_OBJECT(self),
                                                        spice_win_usb_driver_op),
                         FALSE);
    if (g_simple_async_result_propagate_error(result, err))
        return 0;

    return self->priv->reply.status;
}

G_GNUC_INTERNAL
SpiceUsbDevice *spice_win_usb_driver_get_device(SpiceWinUsbDriver *self)
{
    g_return_val_if_fail(SPICE_IS_WIN_USB_DRIVER(self), 0);

    return self->priv->device;
}

GQuark spice_win_usb_driver_error_quark(void)
{
    return g_quark_from_static_string("spice-win-usb-driver-error-quark");
}
