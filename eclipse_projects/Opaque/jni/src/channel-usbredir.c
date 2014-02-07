/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright 2010-2012 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>
   Richard Hughes <rhughes@redhat.com>

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
#include "config.h"

#ifdef USE_USBREDIR
#include <glib/gi18n.h>
#include <usbredirhost.h>
#if USE_POLKIT
#include "usb-acl-helper.h"
#endif
#include "channel-usbredir-priv.h"
#include "usb-device-manager-priv.h"
#include "usbutil.h"
#endif

#include "spice-client.h"
#include "spice-common.h"

#include "spice-channel-priv.h"
#include "glib-compat.h"

/**
 * SECTION:channel-usbredir
 * @short_description: usb redirection
 * @title: USB Redirection Channel
 * @section_id:
 * @stability: API Stable (channel in development)
 * @include: channel-usbredir.h
 *
 * The Spice protocol defines a set of messages to redirect USB devices
 * from the Spice client to the VM. This channel handles these messages.
 */

#ifdef USE_USBREDIR

#define SPICE_USBREDIR_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_USBREDIR_CHANNEL, SpiceUsbredirChannelPrivate))

enum SpiceUsbredirChannelState {
    STATE_DISCONNECTED,
#if USE_POLKIT
    STATE_WAITING_FOR_ACL_HELPER,
#endif
    STATE_CONNECTED,
    STATE_DISCONNECTING,
};

struct _SpiceUsbredirChannelPrivate {
    libusb_device *device;
    SpiceUsbDevice *spice_device;
    libusb_context *context;
    struct usbredirhost *host;
    /* To catch usbredirhost error messages and report them as a GError */
    GError **catch_error;
    /* Data passed from channel handle msg to the usbredirhost read cb */
    const uint8_t *read_buf;
    int read_buf_size;
    enum SpiceUsbredirChannelState state;
#if USE_POLKIT
    GSimpleAsyncResult *result;
    SpiceUsbAclHelper *acl_helper;
#endif
};

static void channel_set_handlers(SpiceChannelClass *klass);
static void spice_usbredir_channel_up(SpiceChannel *channel);
static void spice_usbredir_channel_dispose(GObject *obj);
static void spice_usbredir_channel_finalize(GObject *obj);
static void usbredir_handle_msg(SpiceChannel *channel, SpiceMsgIn *in);

static void usbredir_log(void *user_data, int level, const char *msg);
static int usbredir_read_callback(void *user_data, uint8_t *data, int count);
static int usbredir_write_callback(void *user_data, uint8_t *data, int count);
static void usbredir_write_flush_callback(void *user_data);

static void *usbredir_alloc_lock(void);
static void usbredir_lock_lock(void *user_data);
static void usbredir_unlock_lock(void *user_data);
static void usbredir_free_lock(void *user_data);

#endif

G_DEFINE_TYPE(SpiceUsbredirChannel, spice_usbredir_channel, SPICE_TYPE_CHANNEL)

/* ------------------------------------------------------------------ */

static void spice_usbredir_channel_init(SpiceUsbredirChannel *channel)
{
#ifdef USE_USBREDIR
    channel->priv = SPICE_USBREDIR_CHANNEL_GET_PRIVATE(channel);
#endif
}

#ifdef USE_USBREDIR
static void spice_usbredir_channel_reset(SpiceChannel *c, gboolean migrating)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(c);
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    if (priv->host) {
        if (priv->state == STATE_CONNECTED)
            spice_usbredir_channel_disconnect_device(channel);
        usbredirhost_close(priv->host);
        priv->host = NULL;
        /* Call set_context to re-create the host */
        spice_usbredir_channel_set_context(channel, priv->context);
    }
    SPICE_CHANNEL_CLASS(spice_usbredir_channel_parent_class)->channel_reset(c, migrating);
}
#endif

static void spice_usbredir_channel_class_init(SpiceUsbredirChannelClass *klass)
{
#ifdef USE_USBREDIR
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->dispose       = spice_usbredir_channel_dispose;
    gobject_class->finalize      = spice_usbredir_channel_finalize;
    channel_class->channel_up    = spice_usbredir_channel_up;
    channel_class->channel_reset = spice_usbredir_channel_reset;

    g_type_class_add_private(klass, sizeof(SpiceUsbredirChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
#endif
}

#ifdef USE_USBREDIR
static void spice_usbredir_channel_dispose(GObject *obj)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(obj);

    spice_usbredir_channel_disconnect_device(channel);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_usbredir_channel_parent_class)->dispose)
        G_OBJECT_CLASS(spice_usbredir_channel_parent_class)->dispose(obj);
}

/*
 * Note we don't unref our device / acl_helper / result references in our
 * finalize. The reason for this is that depending on our state at dispose
 * time they are either:
 * 1) Already unreferenced
 * 2) Will be unreferenced by the disconnect_device call from dispose
 * 3) Will be unreferenced by spice_usbredir_channel_open_acl_cb
 *
 * Now the last one may seem like an issue, since what will happen if
 * spice_usbredir_channel_open_acl_cb will run after finalization?
 *
 * This will never happens since the GSimpleAsyncResult created before we
 * get into the STATE_WAITING_FOR_ACL_HELPER takes a reference to its
 * source object, which is our SpiceUsbredirChannel object, so
 * the finalize won't hapen until spice_usbredir_channel_open_acl_cb runs,
 * and unrefs priv->result which will in turn unref ourselve once the
 * complete_in_idle call it does has completed. And once
 * spice_usbredir_channel_open_acl_cb has run, all references we hold have
 * been released even in the 3th scenario.
 */
static void spice_usbredir_channel_finalize(GObject *obj)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(obj);

    if (channel->priv->host)
        usbredirhost_close(channel->priv->host);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_usbredir_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_usbredir_channel_parent_class)->finalize(obj);
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_SPICEVMC_DATA ] = usbredir_handle_msg,
    };

    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}

/* ------------------------------------------------------------------ */
/* private api                                                        */

G_GNUC_INTERNAL
void spice_usbredir_channel_set_context(SpiceUsbredirChannel *channel,
                                        libusb_context       *context)
{
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    g_return_if_fail(priv->host == NULL);

    priv->context = context;
    priv->host = usbredirhost_open_full(
                                   context, NULL,
                                   usbredir_log,
                                   usbredir_read_callback,
                                   usbredir_write_callback,
                                   usbredir_write_flush_callback,
                                   usbredir_alloc_lock,
                                   usbredir_lock_lock,
                                   usbredir_unlock_lock,
                                   usbredir_free_lock,
                                   channel, PACKAGE_STRING,
                                   spice_util_get_debug() ? usbredirparser_debug : usbredirparser_warning,
                                   usbredirhost_fl_write_cb_owns_buffer);
    if (!priv->host)
        g_error("Out of memory allocating usbredirhost");
}

static gboolean spice_usbredir_channel_open_device(
    SpiceUsbredirChannel *channel, GError **err)
{
    SpiceUsbredirChannelPrivate *priv = channel->priv;
    libusb_device_handle *handle = NULL;
    int rc, status;

    g_return_val_if_fail(priv->state == STATE_DISCONNECTED
#if USE_POLKIT
                         || priv->state == STATE_WAITING_FOR_ACL_HELPER
#endif
                         , FALSE);

    rc = libusb_open(priv->device, &handle);
    if (rc != 0) {
        g_set_error(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                    "Could not open usb device: %s [%i]",
                    spice_usbutil_libusb_strerror(rc), rc);
        return FALSE;
    }

    priv->catch_error = err;
    status = usbredirhost_set_device(priv->host, handle);
    priv->catch_error = NULL;
    if (status != usb_redir_success) {
        g_return_val_if_fail(err == NULL || *err != NULL, FALSE);
        return FALSE;
    }

    if (!spice_usb_device_manager_start_event_listening(
            spice_usb_device_manager_get(
                spice_channel_get_session(SPICE_CHANNEL(channel)), NULL),
            err)) {
        usbredirhost_set_device(priv->host, NULL);
        return FALSE;
    }

    priv->state = STATE_CONNECTED;

    return TRUE;
}

#if USE_POLKIT
static void spice_usbredir_channel_open_acl_cb(
    GObject *gobject, GAsyncResult *acl_res, gpointer user_data)
{
    SpiceUsbAclHelper *acl_helper = SPICE_USB_ACL_HELPER(gobject);
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(user_data);
    SpiceUsbredirChannelPrivate *priv = channel->priv;
    GError *err = NULL;

    g_return_if_fail(acl_helper == priv->acl_helper);
    g_return_if_fail(priv->state == STATE_WAITING_FOR_ACL_HELPER ||
                     priv->state == STATE_DISCONNECTING);

    spice_usb_acl_helper_open_acl_finish(acl_helper, acl_res, &err);
    if (!err && priv->state == STATE_DISCONNECTING) {
        err = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                  "USB redirection channel connect cancelled");
    }
    if (!err) {
        spice_usbredir_channel_open_device(channel, &err);
    }
    if (err) {
        g_simple_async_result_take_error(priv->result, err);
        libusb_unref_device(priv->device);
        priv->device = NULL;
        g_boxed_free(spice_usb_device_get_type(), priv->spice_device);
        priv->spice_device = NULL;
        priv->state  = STATE_DISCONNECTED;
    }

    spice_usb_acl_helper_close_acl(priv->acl_helper);
    g_clear_object(&priv->acl_helper);
    g_object_set(spice_channel_get_session(SPICE_CHANNEL(channel)),
                 "inhibit-keyboard-grab", FALSE, NULL);

    g_simple_async_result_complete_in_idle(priv->result);
    g_clear_object(&priv->result);
}
#endif

G_GNUC_INTERNAL
void spice_usbredir_channel_connect_device_async(
                                          SpiceUsbredirChannel *channel,
                                          libusb_device        *device,
                                          SpiceUsbDevice       *spice_device,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
    SpiceUsbredirChannelPrivate *priv = channel->priv;
    GSimpleAsyncResult *result;
#if ! USE_POLKIT
    GError *err = NULL;
#endif

    g_return_if_fail(SPICE_IS_USBREDIR_CHANNEL(channel));
    g_return_if_fail(device != NULL);

    CHANNEL_DEBUG(channel, "connecting usb channel %p", channel);

    result = g_simple_async_result_new(G_OBJECT(channel), callback, user_data,
                                 spice_usbredir_channel_connect_device_async);

    if (!priv->host) {
        g_simple_async_result_set_error(result,
                            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "Error libusb context not set");
        goto done;
    }

    if (priv->state != STATE_DISCONNECTED) {
        g_simple_async_result_set_error(result,
                            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "Error channel is busy");
        goto done;
    }

    priv->device = libusb_ref_device(device);
    priv->spice_device = g_boxed_copy(spice_usb_device_get_type(),
                                      spice_device);
#if USE_POLKIT
    priv->result = result;
    priv->state  = STATE_WAITING_FOR_ACL_HELPER;
    priv->acl_helper = spice_usb_acl_helper_new();
    g_object_set(spice_channel_get_session(SPICE_CHANNEL(channel)),
                 "inhibit-keyboard-grab", TRUE, NULL);
    spice_usb_acl_helper_open_acl(priv->acl_helper,
                                  libusb_get_bus_number(device),
                                  libusb_get_device_address(device),
                                  cancellable,
                                  spice_usbredir_channel_open_acl_cb,
                                  channel);
    return;
#else
    if (!spice_usbredir_channel_open_device(channel, &err)) {
        g_simple_async_result_take_error(result, err);
        libusb_unref_device(priv->device);
        priv->device = NULL;
        g_boxed_free(spice_usb_device_get_type(), priv->spice_device);
        priv->spice_device = NULL;
    }
#endif

done:
    g_simple_async_result_complete_in_idle(result);
    g_object_unref(result);
}

G_GNUC_INTERNAL
gboolean spice_usbredir_channel_connect_device_finish(
                                               SpiceUsbredirChannel *channel,
                                               GAsyncResult         *res,
                                               GError              **err)
{
    GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT(res);

    g_return_val_if_fail(g_simple_async_result_is_valid(res, G_OBJECT(channel),
                                 spice_usbredir_channel_connect_device_async),
                         FALSE);

    if (g_simple_async_result_propagate_error(result, err))
        return FALSE;

    return TRUE;
}

G_GNUC_INTERNAL
void spice_usbredir_channel_disconnect_device(SpiceUsbredirChannel *channel)
{
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    CHANNEL_DEBUG(channel, "disconnecting device from usb channel %p", channel);

    switch (priv->state) {
    case STATE_DISCONNECTED:
    case STATE_DISCONNECTING:
        break;
#if USE_POLKIT
    case STATE_WAITING_FOR_ACL_HELPER:
        priv->state = STATE_DISCONNECTING;
        /* We're still waiting for the acl helper -> cancel it */
        spice_usb_acl_helper_close_acl(priv->acl_helper);
        break;
#endif
    case STATE_CONNECTED:
        /*
         * This sets the usb event thread run condition to FALSE, therefor
         * it must be done before usbredirhost_set_device NULL, as
         * usbredirhost_set_device NULL will interrupt the
         * libusb_handle_events call in the thread.
         */
        spice_usb_device_manager_stop_event_listening(
            spice_usb_device_manager_get(
                spice_channel_get_session(SPICE_CHANNEL(channel)), NULL));
        /* This also closes the libusb handle we passed from open_device */
        usbredirhost_set_device(priv->host, NULL);
        libusb_unref_device(priv->device);
        priv->device = NULL;
        g_boxed_free(spice_usb_device_get_type(), priv->spice_device);
        priv->spice_device = NULL;
        priv->state  = STATE_DISCONNECTED;
        break;
    }
}

G_GNUC_INTERNAL
libusb_device *spice_usbredir_channel_get_device(SpiceUsbredirChannel *channel)
{
    return channel->priv->device;
}

G_GNUC_INTERNAL
void spice_usbredir_channel_get_guest_filter(
                          SpiceUsbredirChannel               *channel,
                          const struct usbredirfilter_rule  **rules_ret,
                          int                                *rules_count_ret)
{
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    g_return_if_fail(priv->host != NULL);

    usbredirhost_get_guest_filter(priv->host, rules_ret, rules_count_ret);
}

/* ------------------------------------------------------------------ */
/* callbacks (any context)                                            */

/* Note that this function must be re-entrant safe, as it can get called
   from both the main thread as well as from the usb event handling thread */
static void usbredir_write_flush_callback(void *user_data)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(user_data);
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    if (spice_channel_get_state(SPICE_CHANNEL(channel)) !=
            SPICE_CHANNEL_STATE_READY)
        return;

    usbredirhost_write_guest_data(priv->host);
}

static void usbredir_log(void *user_data, int level, const char *msg)
{
    SpiceUsbredirChannel *channel = user_data;
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    if (priv->catch_error && level == usbredirparser_error) {
        CHANNEL_DEBUG(channel, "%s", msg);
        /* Remove "usbredirhost: " prefix from usbredirhost messages */
        if (strncmp(msg, "usbredirhost: ", 14) == 0)
            g_set_error_literal(priv->catch_error, SPICE_CLIENT_ERROR,
                                SPICE_CLIENT_ERROR_FAILED, msg + 14);
        else
            g_set_error_literal(priv->catch_error, SPICE_CLIENT_ERROR,
                                SPICE_CLIENT_ERROR_FAILED, msg);
        return;
    }

    switch (level) {
        case usbredirparser_error:
            g_critical("%s", msg); break;
        case usbredirparser_warning:
            g_warning("%s", msg); break;
        default:
            CHANNEL_DEBUG(channel, "%s", msg); break;
    }
}

static int usbredir_read_callback(void *user_data, uint8_t *data, int count)
{
    SpiceUsbredirChannel *channel = user_data;
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    if (priv->read_buf_size < count) {
        count = priv->read_buf_size;
    }

    memcpy(data, priv->read_buf, count);

    priv->read_buf_size -= count;
    if (priv->read_buf_size) {
        priv->read_buf += count;
    } else {
        priv->read_buf = NULL;
    }

    return count;
}

static void usbredir_free_write_cb_data(uint8_t *data, void *user_data)
{
    SpiceUsbredirChannel *channel = user_data;
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    usbredirhost_free_write_buffer(priv->host, data);
}

static int usbredir_write_callback(void *user_data, uint8_t *data, int count)
{
    SpiceUsbredirChannel *channel = user_data;
    SpiceMsgOut *msg_out;

    msg_out = spice_msg_out_new(SPICE_CHANNEL(channel),
                                SPICE_MSGC_SPICEVMC_DATA);
    spice_marshaller_add_ref_full(msg_out->marshaller, data, count,
                                  usbredir_free_write_cb_data, channel);
    spice_msg_out_send(msg_out);

    return count;
}

static void *usbredir_alloc_lock(void) {
#if GLIB_CHECK_VERSION(2,32,0)
    GMutex *mutex;

    mutex = g_new0(GMutex, 1);
    g_mutex_init(mutex);

    return mutex;
#else
    return g_mutex_new();
#endif
}

static void usbredir_lock_lock(void *user_data) {
    GMutex *mutex = user_data;

    g_mutex_lock(mutex);
}

static void usbredir_unlock_lock(void *user_data) {
    GMutex *mutex = user_data;

    g_mutex_unlock(mutex);
}

static void usbredir_free_lock(void *user_data) {
    GMutex *mutex = user_data;

#if GLIB_CHECK_VERSION(2,32,0)
    g_mutex_clear(mutex);
    g_free(mutex);
#else
    g_mutex_free(mutex);
#endif
}

/* --------------------------------------------------------------------- */

/* Events to be handled in main context */
enum {
    DEVICE_ERROR,
};

struct DEVICE_ERROR {
    SpiceUsbDevice *spice_device;
    GError *error;
};

/* main context */
static void do_emit_main_context(GObject *object, int event, gpointer params)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(object);
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    switch (event) {
    case DEVICE_ERROR: {
        struct DEVICE_ERROR *p = params;
        /* Check that the device has not changed before we manage to run */
        if (p->spice_device == priv->spice_device) {
            spice_usbredir_channel_disconnect_device(channel);
            spice_usb_device_manager_device_error(
                spice_usb_device_manager_get(
                    spice_channel_get_session(SPICE_CHANNEL(channel)), NULL),
                p->spice_device, p->error);
        }
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* --------------------------------------------------------------------- */
/* coroutine context                                                     */
static void spice_usbredir_channel_up(SpiceChannel *c)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(c);
    SpiceUsbredirChannelPrivate *priv = channel->priv;

    /* Flush any pending writes */
    usbredirhost_write_guest_data(priv->host);
}

static void usbredir_handle_msg(SpiceChannel *c, SpiceMsgIn *in)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(c);
    SpiceUsbredirChannelPrivate *priv = channel->priv;
    int r, size;
    uint8_t *buf;

    g_return_if_fail(priv->host != NULL);

    /* No recursion allowed! */
    g_return_if_fail(priv->read_buf == NULL);

    buf = spice_msg_in_raw(in, &size);
    priv->read_buf = buf;
    priv->read_buf_size = size;

    r = usbredirhost_read_guest_data(priv->host);
    if (r != 0) {
        SpiceUsbDevice *spice_device = priv->spice_device;
        gchar *desc;
        GError *err;

        g_return_if_fail(spice_device != NULL);

        desc = spice_usb_device_get_description(spice_device, NULL);
        switch (r) {
        case usbredirhost_read_parse_error:
            err = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                              _("usbredir protocol parse error for %s"), desc);
            break;
        case usbredirhost_read_device_rejected:
            err = g_error_new(SPICE_CLIENT_ERROR,
                              SPICE_CLIENT_USB_DEVICE_REJECTED,
                              _("%s rejected by host"), desc);
            break;
        case usbredirhost_read_device_lost:
            err = g_error_new(SPICE_CLIENT_ERROR,
                              SPICE_CLIENT_USB_DEVICE_LOST,
                              _("%s disconnected (fatal IO error)"), desc);
            break;
        default:
            err = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                              _("Unknown error (%d) for %s"), r, desc);
        }
        g_free(desc);

        CHANNEL_DEBUG(c, "%s", err->message);

        spice_device = g_boxed_copy(spice_usb_device_get_type(), spice_device);
        emit_main_context(channel, DEVICE_ERROR, spice_device, err);
        g_boxed_free(spice_usb_device_get_type(), spice_device);

        g_error_free(err);
    }
}

#endif /* USE_USBREDIR */
