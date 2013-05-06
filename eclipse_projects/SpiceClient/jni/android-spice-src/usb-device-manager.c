/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

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

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include "glib-compat.h"

#ifdef USE_USBREDIR
#ifdef __linux__
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <errno.h>
#include <libusb.h>
#include <gudev/gudev.h>
#include "channel-usbredir-priv.h"
#include "usbredirhost.h"
#endif

#include "spice-session-priv.h"
#include "spice-client.h"
#include "spice-marshal.h"
#include "usb-device-manager-priv.h"

/**
 * SECTION:usb-device-manager
 * @short_description: USB device management
 * @title: Spice USB Manager
 * @section_id:
 * @see_also:
 * @stability: Stable
 * @include: usb-device-manager.h
 *
 * #SpiceUsbDeviceManager monitors USB redirection channels and USB
 * devices plugging/unplugging. If #SpiceUsbDeviceManager:auto-connect
 * is set to %TRUE, it will automatically connect newly plugged USB
 * devices to available channels.
 *
 * There should always be a 1:1 relation between #SpiceUsbDeviceManager objects
 * and #SpiceSession objects. Therefor there is no
 * spice_usb_device_manager_new, instead there is
 * spice_usb_device_manager_get() which ensures this 1:1 relation.
 */

/* ------------------------------------------------------------------ */
/* Prototypes for private functions */
static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data);
static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data);

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerPrivate))

enum {
    PROP_0,
    PROP_SESSION,
    PROP_AUTO_CONNECT,
    PROP_AUTO_CONNECT_FILTER,
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    AUTO_CONNECT_FAILED,
    LAST_SIGNAL,
};

struct _SpiceUsbDeviceManagerPrivate {
    SpiceSession *session;
    gboolean auto_connect;
    gchar *auto_connect_filter;
#ifdef USE_USBREDIR
    libusb_context *context;
    GUdevClient *udev;
    int event_listeners;
    GThread *event_thread;
    gboolean event_thread_run;
    libusb_device **coldplug_list; /* Avoid needless reprobing during init */
    struct usbredirfilter_rule *auto_conn_filter_rules;
    int auto_conn_filter_rules_count;
#endif
    GPtrArray *devices;
    GPtrArray *channels;
};

#ifdef USE_USBREDIR
static void spice_usb_device_manager_uevent_cb(GUdevClient     *client,
                                               const gchar     *action,
                                               GUdevDevice     *udevice,
                                               gpointer         user_data);
static void spice_usb_device_manager_add_dev(SpiceUsbDeviceManager  *self,
                                             GUdevDevice            *udev);

G_DEFINE_BOXED_TYPE(SpiceUsbDevice, spice_usb_device,
                    (GBoxedCopyFunc)libusb_ref_device,
                    (GBoxedFreeFunc)libusb_unref_device)

#else
G_DEFINE_BOXED_TYPE(SpiceUsbDevice, spice_usb_device, g_object_ref, g_object_unref)
#endif
static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_CODE(SpiceUsbDeviceManager, spice_usb_device_manager, G_TYPE_OBJECT,
     G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, spice_usb_device_manager_initable_iface_init));

static void spice_usb_device_manager_init(SpiceUsbDeviceManager *self)
{
    SpiceUsbDeviceManagerPrivate *priv;

    priv = SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(self);
    self->priv = priv;

    priv->channels = g_ptr_array_new();
#ifdef USE_USBREDIR
    priv->devices  = g_ptr_array_new_with_free_func((GDestroyNotify)
                                                    libusb_unref_device);
#endif
}

static gboolean spice_usb_device_manager_initable_init(GInitable  *initable,
                                                    GCancellable  *cancellable,
                                                    GError        **err)
{
    GList *list;
    GList *it;
    SpiceUsbDeviceManager *self;
    SpiceUsbDeviceManagerPrivate *priv;
#ifdef USE_USBREDIR
    int rc;
    const gchar *const subsystems[] = {"usb", NULL};
#endif

    g_return_val_if_fail(SPICE_IS_USB_DEVICE_MANAGER(initable), FALSE);
    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    if (cancellable != NULL) {
        g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "Cancellable initialization not supported");
        return FALSE;
    }

    self = SPICE_USB_DEVICE_MANAGER(initable);
    priv = self->priv;

    if (!priv->session) {
        g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                "SpiceUsbDeviceManager constructed without a session");
        return FALSE;
    }

    g_signal_connect(priv->session, "channel-new",
                     G_CALLBACK(channel_new), self);
    g_signal_connect(priv->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), self);
    list = spice_session_get_channels(priv->session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(priv->session, it->data, (gpointer*)self);
    }
    g_list_free(list);

#ifdef USE_USBREDIR
    rc = libusb_init(&priv->context);
    if (rc < 0) {
        const char *desc = spice_usb_device_manager_libusb_strerror(rc);
        g_warning("Error initializing USB support: %s [%i]", desc, rc);
        g_set_error(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                    "Error initializing USB support: %s [%i]", desc, rc);
        return FALSE;
    }

    priv->udev = g_udev_client_new(subsystems);
    g_signal_connect(G_OBJECT(priv->udev), "uevent",
                     G_CALLBACK(spice_usb_device_manager_uevent_cb), self);

    /* Do coldplug (detection of already connected devices) */
    libusb_get_device_list(priv->context, &priv->coldplug_list);
    list = g_udev_client_query_by_subsystem(priv->udev, "usb");
    for (it = g_list_first(list); it; it = g_list_next(it)) {
        spice_usb_device_manager_add_dev(self, it->data);
        g_object_unref(it->data);
    }
    g_list_free(list);
    libusb_free_device_list(priv->coldplug_list, 1);
    priv->coldplug_list = NULL;

    return TRUE;
#else
    g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                        "USB redirection support not compiled in");
    return FALSE;
#endif
}

static void spice_usb_device_manager_finalize(GObject *gobject)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    g_ptr_array_unref(priv->channels);
    if (priv->devices)
        g_ptr_array_unref(priv->devices);

#ifdef USE_USBREDIR
    g_clear_object(&priv->udev);
    if (priv->context)
        libusb_exit(priv->context);
    if (priv->event_thread)
        g_thread_join(priv->event_thread);
#endif

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_usb_device_manager_parent_class)->finalize)
        G_OBJECT_CLASS(spice_usb_device_manager_parent_class)->finalize(gobject);
}

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface)
{
    iface->init = spice_usb_device_manager_initable_init;
}

static void spice_usb_device_manager_get_property(GObject     *gobject,
                                                  guint        prop_id,
                                                  GValue      *value,
                                                  GParamSpec  *pspec)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, priv->session);
        break;
    case PROP_AUTO_CONNECT:
        g_value_set_boolean(value, priv->auto_connect);
        break;
    case PROP_AUTO_CONNECT_FILTER:
        g_value_set_string(value, priv->auto_connect_filter);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_usb_device_manager_set_property(GObject       *gobject,
                                                  guint          prop_id,
                                                  const GValue  *value,
                                                  GParamSpec    *pspec)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    switch (prop_id) {
    case PROP_SESSION:
        priv->session = g_value_get_object(value);
        break;
    case PROP_AUTO_CONNECT:
        priv->auto_connect = g_value_get_boolean(value);
        break;
    case PROP_AUTO_CONNECT_FILTER: {
        const gchar *filter = g_value_get_string(value);
#ifdef USE_USBREDIR
        struct usbredirfilter_rule *rules;
        int r, count;

        r = usbredirfilter_string_to_rules(filter, ",", "|", &rules, &count);
        if (r) {
            if (r == -ENOMEM)
                g_error("Failed to allocate memory for auto-connect-filter");
            g_warning("Error parsing auto-connect-filter string, keeping old filter\n");
            break;
        }

        free(priv->auto_conn_filter_rules);
        priv->auto_conn_filter_rules = rules;
        priv->auto_conn_filter_rules_count = count;
#endif
        g_free(priv->auto_connect_filter);
        priv->auto_connect_filter = g_strdup(filter);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_usb_device_manager_class_init(SpiceUsbDeviceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->finalize     = spice_usb_device_manager_finalize;
    gobject_class->get_property = spice_usb_device_manager_get_property;
    gobject_class->set_property = spice_usb_device_manager_set_property;

    /**
     * SpiceUsbDeviceManager:session:
     *
     * #SpiceSession this #SpiceUsbDeviceManager is associated with
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_SESSION,
         g_param_spec_object("session",
                             "Session",
                             "SpiceSession",
                             SPICE_TYPE_SESSION,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceUsbDeviceManager:auto-connect:
     */
    pspec = g_param_spec_boolean("auto-connect", "Auto Connect",
                                 "Auto connect plugged in USB devices",
                                 FALSE,
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_AUTO_CONNECT, pspec);

    /**
     * SpiceUsbDeviceManager:auto-connect-filter:
     *
     * Set a string specifying a filter to use to determine which USB devices
     * to autoconnect when plugged in, a filter consists of one or more rules.
     * Where each rule has the form of:
     *
     * @class,@vendor,@product,@version,@allow
     *
     * Use -1 for @class/@vendor/@product/@version to accept any value.
     *
     * And the rules are themselves are concatonated like this:
     *
     * @rule1|@rule2|@rule3
     *
     * The default setting filters out HID (class 0x03) USB devices from auto
     * connect and auto connects anything else. Note the explicit allow rule at
     * the end, this is necessary since by default all devices without a
     * matching filter rule will not auto-connect.
     *
     * Filter strings in this format can be easily created with the RHEV-M
     * USB filter editor tool.
     */
    pspec = g_param_spec_string("auto-connect-filter", "Auto Connect Filter ",
               "Filter determining which USB devices to auto connect",
               "0x03,-1,-1,-1,0|-1,-1,-1,-1,1",
               G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_AUTO_CONNECT_FILTER,
                                    pspec);

    /**
     * SpiceUsbDeviceManager::device-added:
     * @manager: the #SpiceUsbDeviceManager that emitted the signal
     * @device: #SpiceUsbDevice boxed object corresponding to the added device
     *
     * The #SpiceUsbDeviceManager::device-added signal is emitted whenever
     * a new USB device has been plugged in.
     **/
    signals[DEVICE_ADDED] =
        g_signal_new("device-added",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_added),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_USB_DEVICE);

    /**
     * SpiceUsbDeviceManager::device-removed:
     * @manager: the #SpiceUsbDeviceManager that emitted the signal
     * @device: #SpiceUsbDevice boxed object corresponding to the removed device
     *
     * The #SpiceUsbDeviceManager::device-removed signal is emitted whenever
     * an USB device has been removed.
     **/
    signals[DEVICE_REMOVED] =
        g_signal_new("device-removed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_removed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_USB_DEVICE);

    /**
     * SpiceUsbDeviceManager::auto-connect-failed:
     * @manager: the #SpiceUsbDeviceManager that emitted the signal
     * @device: #SpiceUsbDevice boxed object corresponding to the device which failed to auto connect
     * @error: #GError describing the reason why the autoconnect failed
     *
     * The #SpiceUsbDeviceManager::auto-connect-failed signal is emitted
     * whenever the auto-connect property is true, and a newly plugged in
     * device could not be auto-connected.
     **/
    signals[AUTO_CONNECT_FAILED] =
        g_signal_new("auto-connect-failed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, auto_connect_failed),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__BOXED_BOXED,
                     G_TYPE_NONE,
                     2,
                     SPICE_TYPE_USB_DEVICE,
                     G_TYPE_ERROR);

    g_type_class_add_private(klass, sizeof(SpiceUsbDeviceManagerPrivate));
}

/* ------------------------------------------------------------------ */
/* gudev / libusb Helper functions                                    */

#ifdef USE_USBREDIR
static gboolean spice_usb_device_manager_get_udev_bus_n_address(
    GUdevDevice *udev, int *bus, int *address)
{
    const gchar *bus_str, *address_str;

    *bus = *address = 0;

    bus_str = g_udev_device_get_property(udev, "BUSNUM");
    address_str = g_udev_device_get_property(udev, "DEVNUM");
    if (bus_str)
        *bus = atoi(bus_str);
    if (address_str)
        *address = atoi(address_str);

    return *bus && *address;
}

const char *spice_usb_device_manager_libusb_strerror(enum libusb_error error_code)
{
    switch (error_code) {
    case LIBUSB_SUCCESS:
        return "Success";
    case LIBUSB_ERROR_IO:
        return "Input/output error";
    case LIBUSB_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case LIBUSB_ERROR_ACCESS:
        return "Access denied (insufficient permissions)";
    case LIBUSB_ERROR_NO_DEVICE:
        return "No such device (it may have been disconnected)";
    case LIBUSB_ERROR_NOT_FOUND:
        return "Entity not found";
    case LIBUSB_ERROR_BUSY:
        return "Resource busy";
    case LIBUSB_ERROR_TIMEOUT:
        return "Operation timed out";
    case LIBUSB_ERROR_OVERFLOW:
        return "Overflow";
    case LIBUSB_ERROR_PIPE:
        return "Pipe error";
    case LIBUSB_ERROR_INTERRUPTED:
        return "System call interrupted (perhaps due to signal)";
    case LIBUSB_ERROR_NO_MEM:
        return "Insufficient memory";
    case LIBUSB_ERROR_NOT_SUPPORTED:
        return "Operation not supported or unimplemented on this platform";
    case LIBUSB_ERROR_OTHER:
        return "Other error";
    }
    return "Unknown error";
}

#ifdef __linux__
/* <Sigh> libusb does not allow getting the manufacturer and product strings
   without opening the device, so grab them directly from sysfs */
static gchar *spice_usb_device_manager_get_sysfs_attribute(
    int bus, int address, const char *attribute)
{
    struct stat stat_buf;
    char filename[256];
    gchar *contents;

    snprintf(filename, sizeof(filename), "/dev/bus/usb/%03d/%03d",
             bus, address);
    if (stat(filename, &stat_buf) != 0)
        return NULL;

    snprintf(filename, sizeof(filename), "/sys/dev/char/%d:%d/%s",
             major(stat_buf.st_rdev), minor(stat_buf.st_rdev), attribute);
    if (!g_file_get_contents(filename, &contents, NULL, NULL))
        return NULL;

    /* Remove the newline at the end */
    contents[strlen(contents) - 1] = '\0';

    return contents;
}
#endif
#endif

/* ------------------------------------------------------------------ */
/* callbacks                                                          */

static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data)
{
    SpiceUsbDeviceManager *self = user_data;

    if (SPICE_IS_USBREDIR_CHANNEL(channel))
        g_ptr_array_add(self->priv->channels, channel);
}

static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data)
{
    SpiceUsbDeviceManager *self = user_data;

    if (SPICE_IS_USBREDIR_CHANNEL(channel))
        g_ptr_array_remove(self->priv->channels, channel);
}

#ifdef USE_USBREDIR
static void spice_usb_device_manager_auto_connect_cb(GObject      *gobject,
                                                     GAsyncResult *res,
                                                     gpointer      user_data)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    libusb_device *device = user_data;
    GError *err = NULL;

    spice_usb_device_manager_connect_device_finish(self, res, &err);
    if (err) {
        gchar *desc = spice_usb_device_get_description((SpiceUsbDevice *)device, NULL);
        g_prefix_error(&err, "Could not auto-redirect %s: ", desc);
        g_free(desc);

        SPICE_DEBUG("%s", err->message);
        g_signal_emit(self, signals[AUTO_CONNECT_FAILED], 0, device, err);
        g_error_free(err);
    }
    libusb_unref_device(device);
}

static void spice_usb_device_manager_add_dev(SpiceUsbDeviceManager  *self,
                                             GUdevDevice            *udev)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    libusb_device *device = NULL, **dev_list = NULL;
    const gchar *devtype, *devclass;
    int i, bus, address;

    devtype = g_udev_device_get_property(udev, "DEVTYPE");
    /* Check if this is a usb device (and not an interface) */
    if (!devtype || strcmp(devtype, "usb_device"))
        return;

    /* Skip hubs */
    devclass = g_udev_device_get_sysfs_attr(udev, "bDeviceClass");
    if (!devclass || !strcmp(devclass, "09"))
        return;

    if (!spice_usb_device_manager_get_udev_bus_n_address(udev, &bus, &address)) {
        g_warning("USB device without bus number or device address");
        return;
    }

    if (priv->coldplug_list)
        dev_list = priv->coldplug_list;
    else
        libusb_get_device_list(priv->context, &dev_list);

    for (i = 0; dev_list && dev_list[i]; i++) {
        if (libusb_get_bus_number(dev_list[i]) == bus &&
            libusb_get_device_address(dev_list[i]) == address) {
            device = libusb_ref_device(dev_list[i]);
            break;
        }
    }

    if (!priv->coldplug_list)
        libusb_free_device_list(dev_list, 1);

    if (!device) {
        g_warning("Could not find USB device at busnum %d devaddr %d",
                  bus, address);
        return;
    }

    g_ptr_array_add(priv->devices, device);

    if (priv->auto_connect) {
        if (usbredirhost_check_device_filter(
                priv->auto_conn_filter_rules,
                priv->auto_conn_filter_rules_count,
                device, 0) == 0)
            spice_usb_device_manager_connect_device_async(self,
                                   (SpiceUsbDevice *)device, NULL,
                                   spice_usb_device_manager_auto_connect_cb,
                                   libusb_ref_device(device));
    }

    SPICE_DEBUG("device added %p", device);
    g_signal_emit(self, signals[DEVICE_ADDED], 0, device);
}

static void spice_usb_device_manager_remove_dev(SpiceUsbDeviceManager  *self,
                                                GUdevDevice            *udev)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    libusb_device *curr, *device = NULL;
    int bus, address;
    guint i;

    if (!spice_usb_device_manager_get_udev_bus_n_address(udev, &bus, &address))
        return;

    for (i = 0; i < priv->devices->len; i++) {
        curr = g_ptr_array_index(priv->devices, i);
        if (libusb_get_bus_number(curr) == bus &&
               libusb_get_device_address(curr) == address) {
            device = curr;
            break;
        }
    }
    if (!device)
        return;

    spice_usb_device_manager_disconnect_device(self, (SpiceUsbDevice *)device);

    SPICE_DEBUG("device removed %p", device);
    g_signal_emit(self, signals[DEVICE_REMOVED], 0, device);
    g_ptr_array_remove(priv->devices, device);
}

static void spice_usb_device_manager_uevent_cb(GUdevClient     *client,
                                               const gchar     *action,
                                               GUdevDevice     *udevice,
                                               gpointer         user_data)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(user_data);

    if (g_str_equal(action, "add"))
        spice_usb_device_manager_add_dev(self, udevice);
    else if (g_str_equal (action, "remove"))
        spice_usb_device_manager_remove_dev(self, udevice);
}

static void spice_usb_device_manager_channel_connect_cb(
    GObject *gobject, GAsyncResult *channel_res, gpointer user_data)
{
    SpiceUsbredirChannel *channel = SPICE_USBREDIR_CHANNEL(gobject);
    GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT(user_data);
    GError *err = NULL;

    spice_usbredir_channel_connect_finish(channel, channel_res, &err);
    if (err) {
        g_simple_async_result_take_error(result, err);
    }
    g_simple_async_result_complete(result);
    g_object_unref(result);
}

/* ------------------------------------------------------------------ */
/* private api                                                        */

static gpointer spice_usb_device_manager_usb_ev_thread(gpointer user_data)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(user_data);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    int rc;

    while (priv->event_thread_run) {
        rc = libusb_handle_events(priv->context);
        if (rc) {
            const char *desc = spice_usb_device_manager_libusb_strerror(rc);
            g_warning("Error handling USB events: %s [%i]", desc, rc);
        }
    }

    return NULL;
}

gboolean spice_usb_device_manager_start_event_listening(
    SpiceUsbDeviceManager *self, GError **err)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    priv->event_listeners++;
    if (priv->event_listeners > 1)
        return TRUE;

    /* We don't join the thread when we stop event listening, as the
       libusb_handle_events call in the thread won't exit until the
       libusb_close call for the device is made from usbredirhost_close. */
    if (priv->event_thread) {
         g_thread_join(priv->event_thread);
         priv->event_thread = NULL;
    }
    priv->event_thread_run = TRUE;
    priv->event_thread = g_thread_create(
                             spice_usb_device_manager_usb_ev_thread,
                             self, TRUE, err);
    return priv->event_thread != NULL;
}

void spice_usb_device_manager_stop_event_listening(
    SpiceUsbDeviceManager *self)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;

    g_return_if_fail(priv->event_listeners > 0);

    priv->event_listeners--;
    if (priv->event_listeners == 0)
        priv->event_thread_run = FALSE;
}
#endif

static SpiceUsbredirChannel *spice_usb_device_manager_get_channel_for_dev(
    SpiceUsbDeviceManager *manager, SpiceUsbDevice *_device)
{
#ifdef USE_USBREDIR
    SpiceUsbDeviceManagerPrivate *priv = manager->priv;
    libusb_device *device = (libusb_device *)_device;
    guint i;

    for (i = 0; i < priv->channels->len; i++) {
        SpiceUsbredirChannel *channel = g_ptr_array_index(priv->channels, i);
        if (spice_usbredir_channel_get_device(channel) == device)
            return channel;
    }
#endif
    return NULL;
}

/* ------------------------------------------------------------------ */
/* public api                                                         */

/**
 * spice_usb_device_manager_get:
 * @session: #SpiceSession for which to get the #SpiceUsbDeviceManager
 *
 * Gets the #SpiceUsbDeviceManager associated with the passed in #SpiceSession.
 * A new #SpiceUsbDeviceManager instance will be created the first time this
 * function is called for a certain #SpiceSession.
 *
 * Note that this function returns a weak reference, which should not be used
 * after the #SpiceSession itself has been unref-ed by the caller.
 *
 * Returns: (transfer none): a weak reference to the #SpiceUsbDeviceManager associated with the passed in #SpiceSession
 */
SpiceUsbDeviceManager *spice_usb_device_manager_get(SpiceSession *session,
                                                    GError **err)
{
    SpiceUsbDeviceManager *self;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_val_if_fail(err == NULL || *err == NULL, NULL);

    g_static_mutex_lock(&mutex);
    self = session->priv->usb_manager;
    if (self == NULL) {
        self = g_initable_new(SPICE_TYPE_USB_DEVICE_MANAGER, NULL, err,
                              "session", session, NULL);
        session->priv->usb_manager = self;
    }
    g_static_mutex_unlock(&mutex);

    return self;
}

/**
 * spice_usb_device_manager_get_devices:
 * @manager: the #SpiceUsbDeviceManager manager
 *
 * Returns: (element-type SpiceUsbDevice) (transfer full): a %GPtrArray array of %SpiceUsbDevice
 */
GPtrArray* spice_usb_device_manager_get_devices(SpiceUsbDeviceManager *self)
{
    GPtrArray *devices_copy = NULL;

    g_return_val_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self), NULL);

#ifdef USE_USBREDIR
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    guint i;

    devices_copy = g_ptr_array_new_with_free_func((GDestroyNotify)
                                                  libusb_unref_device);
    for (i = 0; i < priv->devices->len; i++) {
        libusb_device *device = g_ptr_array_index(priv->devices, i);
        g_ptr_array_add(devices_copy, libusb_ref_device(device));
    }
#endif

    return devices_copy;
}

/**
 * spice_usb_device_manager_is_device_connected:
 * @manager: the #SpiceUsbDeviceManager manager
 * @device: a #SpiceUsbDevice
 *
 * Returns: %TRUE if @device has an associated USB redirection channel
 */
gboolean spice_usb_device_manager_is_device_connected(SpiceUsbDeviceManager *self,
                                                      SpiceUsbDevice *device)
{
    g_return_val_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self), FALSE);
    g_return_val_if_fail(device != NULL, FALSE);

    return !!spice_usb_device_manager_get_channel_for_dev(self, device);
}

/**
 * spice_usb_device_manager_connect_device_async:
 * @manager: the #SpiceUsbDeviceManager manager
 * @device: a #SpiceUsbDevice to redirect
 * @cancellable: a #GCancellable or NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to callback
 */
void spice_usb_device_manager_connect_device_async(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self));
    g_return_if_fail(device != NULL);

    SPICE_DEBUG("connecting device %p", device);

    result = g_simple_async_result_new(G_OBJECT(self), callback, user_data,
                               spice_usb_device_manager_connect_device_async);

#ifdef USE_USBREDIR
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    guint i;

    if (spice_usb_device_manager_is_device_connected(self, device)) {
        g_simple_async_result_set_error(result,
                            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "Cannot connect an already connected usb device");
        goto done;
    }

    for (i = 0; i < priv->channels->len; i++) {
        SpiceUsbredirChannel *channel = g_ptr_array_index(priv->channels, i);

        if (spice_usbredir_channel_get_device(channel))
            continue; /* Skip already used channels */

        spice_usbredir_channel_connect_async(channel,
                                 priv->context,
                                 (libusb_device *)device,
                                 cancellable,
                                 spice_usb_device_manager_channel_connect_cb,
                                 result);
        return;
    }
#endif

    g_simple_async_result_set_error(result,
                            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            "No free USB channel");
#ifdef USE_USBREDIR
done:
#endif
    g_simple_async_result_complete_in_idle(result);
    g_object_unref(result);
}

gboolean spice_usb_device_manager_connect_device_finish(
    SpiceUsbDeviceManager *self, GAsyncResult *res, GError **err)
{
    GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT(res);

    g_return_val_if_fail(g_simple_async_result_is_valid(res, G_OBJECT(self),
                               spice_usb_device_manager_connect_device_async),
                         FALSE);

    if (g_simple_async_result_propagate_error(simple, err))
        return FALSE;

    return TRUE;
}

/**
 * spice_usb_device_manager_disconnect_device:
 * @manager: the #SpiceUsbDeviceManager manager
 * @device: a #SpiceUsbDevice to disconnect
 *
 * Returns: %TRUE if @device has an associated USB redirection channel
 */
void spice_usb_device_manager_disconnect_device(SpiceUsbDeviceManager *self,
                                                SpiceUsbDevice *device)
{
    g_return_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self));
    g_return_if_fail(device != NULL);

    SPICE_DEBUG("disconnecting device %p", device);

#ifdef USE_USBREDIR
    SpiceUsbredirChannel *channel;

    channel = spice_usb_device_manager_get_channel_for_dev(self, device);
    if (channel)
        spice_usbredir_channel_disconnect(channel);
#endif
}

/**
 * spice_usb_device_get_description:
 * @device: #SpiceUsbDevice to get the description of
 * @format: an optionnal printf() format string with positional parameters
 *
 * Get a string describing the device which is suitable as a description of
 * the device for the end user. The returned string should be freed with
 * g_free() when no longer needed.
 *
 * The @format positional parameters are the following:
 * - '%%1$s' manufacturer
 * - '%%2$s' product
 * - '%%3$s' descriptor (a [vendor_id:product_id] string)
 * - '%%4$d' bus
 * - '%%5$d' address
 *
 * (the default format string is "%%s %%s %%s at %%d-%%d")
 *
 * Returns: a newly-allocated string holding the description, or %NULL if failed
 */
gchar *spice_usb_device_get_description(SpiceUsbDevice *_device, const gchar *format)
{
#ifdef USE_USBREDIR
    libusb_device *device = (libusb_device *)_device;
    struct libusb_device_descriptor desc;
    int bus, address;
    gchar *description, *descriptor, *manufacturer = NULL, *product = NULL;

    g_return_val_if_fail(device != NULL, NULL);

    bus     = libusb_get_bus_number(device);
    address = libusb_get_device_address(device);

#if __linux__
    manufacturer = spice_usb_device_manager_get_sysfs_attribute(bus, address,
                                                              "manufacturer");
    product = spice_usb_device_manager_get_sysfs_attribute(bus, address,
                                                           "product");
#endif
    if (!manufacturer)
        manufacturer = g_strdup(_("USB"));
    if (!product)
        product = g_strdup(_("Device"));

    if (libusb_get_device_descriptor(device, &desc) == LIBUSB_SUCCESS)
        descriptor = g_strdup_printf("[%04x:%04x]", desc.idVendor, desc.idProduct);
    else
        descriptor = g_strdup("");

    if (!format)
        format = _("%s %s %s at %d-%d");

    description = g_strdup_printf(format, manufacturer, product, descriptor, bus, address);

    g_free(manufacturer);
    g_free(descriptor);
    g_free(product);

    return description;
#else
    return NULL;
#endif
}
