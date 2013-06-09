/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011, 2012 Red Hat, Inc.

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

#include "glib-compat.h"

#ifdef USE_USBREDIR
#include <errno.h>
#include <libusb.h>

#if defined(USE_GUDEV)
#include <gudev/gudev.h>
#elif defined(G_OS_WIN32)
#include "win-usb-dev.h"
#include "win-usb-driver-install.h"
#else
#warning "Expecting one of G_OS_WIN32 and USE_GUDEV to be defined"
#endif

#include "channel-usbredir-priv.h"
#include "usbredirhost.h"
#include "usbutil.h"
#endif

#include "spice-session-priv.h"
#include "spice-client.h"
#include "spice-marshal.h"
#include "usb-device-manager-priv.h"

#include <glib/gi18n.h>

#ifndef G_OS_WIN32 /* Linux -- device id is bus.addr */
#define DEV_ID_FMT "at %d.%d"
#else /* Windows -- device id is vid:pid */
#define DEV_ID_FMT "0x%04x:0x%04x"
#endif

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
/* gobject glue                                                       */

#define SPICE_USB_DEVICE_MANAGER_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_USB_DEVICE_MANAGER, SpiceUsbDeviceManagerPrivate))

enum {
    PROP_0,
    PROP_SESSION,
    PROP_AUTO_CONNECT,
    PROP_AUTO_CONNECT_FILTER,
    PROP_REDIRECT_ON_CONNECT,
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    AUTO_CONNECT_FAILED,
    DEVICE_ERROR,
    LAST_SIGNAL,
};

struct _SpiceUsbDeviceManagerPrivate {
    SpiceSession *session;
    gboolean auto_connect;
    gchar *auto_connect_filter;
    gchar *redirect_on_connect;
#ifdef USE_USBREDIR
    libusb_context *context;
    GUdevClient *udev;
    int event_listeners;
    GThread *event_thread;
    gboolean event_thread_run;
    libusb_device **coldplug_list; /* Avoid needless reprobing during init */
    struct usbredirfilter_rule *auto_conn_filter_rules;
    struct usbredirfilter_rule *redirect_on_connect_rules;
    int auto_conn_filter_rules_count;
    int redirect_on_connect_rules_count;
#ifdef G_OS_WIN32
    SpiceWinUsbDriver     *installer;
#endif
#endif
    GPtrArray *devices;
    GPtrArray *channels;
};

enum {
    SPICE_USB_DEVICE_STATE_NONE = 0, /* this is also DISCONNECTED */
    SPICE_USB_DEVICE_STATE_CONNECTING,
    SPICE_USB_DEVICE_STATE_CONNECTED,
    SPICE_USB_DEVICE_STATE_DISCONNECTING,
    SPICE_USB_DEVICE_STATE_INSTALLING,
    SPICE_USB_DEVICE_STATE_UNINSTALLING,
    SPICE_USB_DEVICE_STATE_MAX
};

#ifdef USE_USBREDIR

typedef struct _SpiceUsbDeviceInfo {
    guint8  busnum;
    guint8  devaddr;
    guint16 vid;
    guint16 pid;
    guint8  state;
    guint8  reserved;
    gint    ref;
} SpiceUsbDeviceInfo;


static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data);
static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data);
static void spice_usb_device_manager_uevent_cb(GUdevClient     *client,
                                               const gchar     *action,
                                               GUdevDevice     *udevice,
                                               gpointer         user_data);
static void spice_usb_device_manager_add_dev(SpiceUsbDeviceManager  *self,
                                             GUdevDevice            *udev);
static void spice_usb_device_manager_check_redir_on_connect(
    SpiceUsbDeviceManager *self, SpiceChannel *channel);

static SpiceUsbDeviceInfo *spice_usb_device_new(libusb_device *libdev);
static SpiceUsbDevice *spice_usb_device_ref(SpiceUsbDevice *device);
static void spice_usb_device_unref(SpiceUsbDevice *device);

#ifdef G_OS_WIN32
static guint8 spice_usb_device_get_state(SpiceUsbDevice *device);
static void  spice_usb_device_set_state(SpiceUsbDevice *device, guint8 s);
#endif

static gboolean spice_usb_device_equal_libdev(SpiceUsbDevice *device,
                                              libusb_device *libdev);
static libusb_device *
spice_usb_device_manager_device_to_libdev(SpiceUsbDeviceManager *self,
                                          SpiceUsbDevice *device);

static void
_spice_usb_device_manager_connect_device_async(SpiceUsbDeviceManager *self,
                                               SpiceUsbDevice *device,
                                               GCancellable *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer user_data);

G_DEFINE_BOXED_TYPE(SpiceUsbDevice, spice_usb_device,
                    (GBoxedCopyFunc)spice_usb_device_ref,
                    (GBoxedFreeFunc)spice_usb_device_unref)

#else
G_DEFINE_BOXED_TYPE(SpiceUsbDevice, spice_usb_device, g_object_ref, g_object_unref)
#endif

static void spice_usb_device_manager_initable_iface_init(GInitableIface *iface);

#ifdef USE_USBREDIR
#ifdef G_OS_WIN32
static void spice_usb_device_manager_drv_install_cb(GObject *gobject,
                                                    GAsyncResult *res,
                                                    gpointer user_data);
#endif
#endif

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
                                                    spice_usb_device_unref);
#endif
}

static gboolean spice_usb_device_manager_initable_init(GInitable  *initable,
                                                    GCancellable  *cancellable,
                                                    GError        **err)
{
    SpiceUsbDeviceManager *self;
    SpiceUsbDeviceManagerPrivate *priv;
#ifdef USE_USBREDIR
    GList *list;
    GList *it;
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

#ifdef USE_USBREDIR
    /* Initialize libusb */
    rc = libusb_init(&priv->context);
    if (rc < 0) {
        const char *desc = spice_usbutil_libusb_strerror(rc);
        g_warning("Error initializing USB support: %s [%i]", desc, rc);
        g_set_error(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                    "Error initializing USB support: %s [%i]", desc, rc);
        return FALSE;
    }

    /* Start listening for usb devices plug / unplug */
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

    /* Start listening for usb channels connect/disconnect */
    g_signal_connect(priv->session, "channel-new",
                     G_CALLBACK(channel_new), self);
    g_signal_connect(priv->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), self);
    list = spice_session_get_channels(priv->session);
    for (it = g_list_first(list); it != NULL; it = g_list_next(it)) {
        channel_new(priv->session, it->data, (gpointer*)self);
    }
    g_list_free(list);

    return TRUE;
#else
    g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                        _("USB redirection support not compiled in"));
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
    free(priv->auto_conn_filter_rules);
#ifdef G_OS_WIN32
    if (priv->installer)
        g_object_unref(priv->installer);
#endif
#endif

    g_free(priv->auto_connect_filter);

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
    case PROP_REDIRECT_ON_CONNECT:
        g_value_set_string(value, priv->redirect_on_connect);
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
    case PROP_REDIRECT_ON_CONNECT: {
        const gchar *filter = g_value_get_string(value);
#ifdef USE_USBREDIR
        struct usbredirfilter_rule *rules = NULL;
        int r = 0, count = 0;

        if (filter)
            r = usbredirfilter_string_to_rules(filter, ",", "|",
                                               &rules, &count);
        if (r) {
            if (r == -ENOMEM)
                g_error("Failed to allocate memory for redirect-on-connect");
            g_warning("Error parsing redirect-on-connect string, keeping old filter\n");
            break;
        }

        free(priv->redirect_on_connect_rules);
        priv->redirect_on_connect_rules = rules;
        priv->redirect_on_connect_rules_count = count;
#endif
        g_free(priv->redirect_on_connect);
        priv->redirect_on_connect = g_strdup(filter);
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
     *
     * Set this to TRUE to automatically redirect newly plugged in device.
     *
     * Note when #SpiceGtkSession's auto-usbredir property is TRUE, this
     * property is controlled by #SpiceGtkSession.
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
     * SpiceUsbDeviceManager:redirect-on-connect:
     *
     * Set a string specifying a filter selecting USB devices to automatically
     * redirect after a Spice connection has been established.
     *
     * See SpiceUsbDeviceManager:auto-connect-filter: for the filter string
     * format.
     */
    pspec = g_param_spec_string("redirect-on-connect", "Redirect on connect",
               "Filter selecting USB devices to redirect on connect", NULL,
               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_REDIRECT_ON_CONNECT,
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

    /**
     * SpiceUsbDeviceManager::device-error:
     * @manager: #SpiceUsbDeviceManager that emitted the signal
     * @device:  #SpiceUsbDevice boxed object corresponding to the device which has an error
     * @error:   #GError describing the error
     *
     * The #SpiceUsbDeviceManager::device-error signal is emitted whenever an
     * error happens which causes a device to no longer be available to the
     * guest.
     **/
    signals[DEVICE_ERROR] =
        g_signal_new("device-error",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceUsbDeviceManagerClass, device_error),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__BOXED_BOXED,
                     G_TYPE_NONE,
                     2,
                     SPICE_TYPE_USB_DEVICE,
                     G_TYPE_ERROR);

    g_type_class_add_private(klass, sizeof(SpiceUsbDeviceManagerPrivate));
}

#ifdef USE_USBREDIR

/* ------------------------------------------------------------------ */
/* gudev / libusb Helper functions                                    */

static gboolean spice_usb_device_manager_get_udev_bus_n_address(
    GUdevDevice *udev, int *bus, int *address)
{
    const gchar *bus_str, *address_str;

    *bus = *address = 0;

#ifndef G_OS_WIN32
    bus_str = g_udev_device_get_property(udev, "BUSNUM");
    address_str = g_udev_device_get_property(udev, "DEVNUM");
#else /* Windows -- request vid:pid instead */
    bus_str = g_udev_device_get_property(udev, "VID");
    address_str = g_udev_device_get_property(udev, "PID");
#endif
    if (bus_str)
        *bus = atoi(bus_str);
    if (address_str)
        *address = atoi(address_str);

    return *bus && *address;
}

static gboolean spice_usb_device_manager_get_device_descriptor(
    libusb_device *libdev,
    struct libusb_device_descriptor *desc)
{
    int errcode;
    const gchar *errstr;

    g_return_val_if_fail(libdev != NULL, FALSE);
    g_return_val_if_fail(desc   != NULL, FALSE);

    errcode = libusb_get_device_descriptor(libdev, desc);
    if (errcode < 0) {
        int bus, addr;

        bus = libusb_get_bus_number(libdev);
        addr = libusb_get_device_address(libdev);
        errstr = spice_usbutil_libusb_strerror(errcode);
        g_warning("cannot get device descriptor for (%p) %d.%d -- %s(%d)",
                  libdev, bus, addr, errstr, errcode);
        return FALSE;
    }
    return TRUE;
}

static gboolean spice_usb_device_manager_get_libdev_vid_pid(
    libusb_device *libdev, int *vid, int *pid)
{
    struct libusb_device_descriptor desc;

    g_return_val_if_fail(libdev != NULL, FALSE);
    g_return_val_if_fail(vid != NULL, FALSE);
    g_return_val_if_fail(pid != NULL, FALSE);

    *vid = *pid = 0;

    if (!spice_usb_device_manager_get_device_descriptor(libdev, &desc)) {
        return FALSE;
    }
    *vid = desc.idVendor;
    *pid = desc.idProduct;

    return TRUE;
}

/* ------------------------------------------------------------------ */
/* callbacks                                                          */

static void channel_new(SpiceSession *session, SpiceChannel *channel,
                        gpointer user_data)
{
    SpiceUsbDeviceManager *self = user_data;

    if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
        spice_usbredir_channel_set_context(SPICE_USBREDIR_CHANNEL(channel),
                                           self->priv->context);
        spice_channel_connect(channel);
        g_ptr_array_add(self->priv->channels, channel);

        spice_usb_device_manager_check_redir_on_connect(self, channel);
    }
}

static void channel_destroy(SpiceSession *session, SpiceChannel *channel,
                            gpointer user_data)
{
    SpiceUsbDeviceManager *self = user_data;

    if (SPICE_IS_USBREDIR_CHANNEL(channel))
        g_ptr_array_remove(self->priv->channels, channel);
}

static void spice_usb_device_manager_auto_connect_cb(GObject      *gobject,
                                                     GAsyncResult *res,
                                                     gpointer      user_data)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(gobject);
    SpiceUsbDevice *device = user_data;
    GError *err = NULL;

    spice_usb_device_manager_connect_device_finish(self, res, &err);
    if (err) {
        gchar *desc = spice_usb_device_get_description(device, NULL);
        g_prefix_error(&err, "Could not auto-redirect %s: ", desc);
        g_free(desc);

        SPICE_DEBUG("%s", err->message);
        g_signal_emit(self, signals[AUTO_CONNECT_FAILED], 0, device, err);
        g_error_free(err);
    }
    spice_usb_device_unref(device);
}

#ifndef G_OS_WIN32 /* match functions for Linux -- match by bus.addr */
static gboolean
spice_usb_device_manager_device_match(SpiceUsbDevice *device,
                                      const int bus, const int address)
{
    return (spice_usb_device_get_busnum(device) == bus &&
            spice_usb_device_get_devaddr(device) == address);
}

static gboolean
spice_usb_device_manager_libdev_match(libusb_device *libdev,
                                      const int bus, const int address)
{
    return (libusb_get_bus_number(libdev) == bus &&
            libusb_get_device_address(libdev) == address);
}

#else /* Win32 -- match functions for Windows -- match by vid:pid */
static gboolean
spice_usb_device_manager_device_match(SpiceUsbDevice *device,
                                      const int vid, const int pid)
{
    return (spice_usb_device_get_vid(device) == vid &&
            spice_usb_device_get_pid(device) == pid);
}

static gboolean
spice_usb_device_manager_libdev_match(libusb_device *libdev,
                                      const int vid, const int pid)
{
    int vid2, pid2;

    if (!spice_usb_device_manager_get_libdev_vid_pid(libdev, &vid2, &pid2)) {
        return FALSE;
    }
    return (vid == vid2 && pid == pid2);
}
#endif /* of Win32 -- match functions */

static SpiceUsbDevice*
spice_usb_device_manager_find_device(SpiceUsbDeviceManager *self,
                                     const int bus, const int address)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    SpiceUsbDevice *curr, *device = NULL;
    guint i;

    for (i = 0; i < priv->devices->len; i++) {
        curr = g_ptr_array_index(priv->devices, i);
        if (spice_usb_device_manager_device_match(curr, bus, address)) {
            device = curr;
            break;
        }
    }
    return device;
}

static void spice_usb_device_manager_add_dev(SpiceUsbDeviceManager  *self,
                                             GUdevDevice            *udev)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    libusb_device *libdev = NULL, **dev_list = NULL;
    SpiceUsbDevice *device = NULL;
    const gchar *devtype, *devclass;
    int i, bus, address;
    gboolean auto_ok = FALSE;

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

    device = spice_usb_device_manager_find_device(self, bus, address);
    if (device) {
        SPICE_DEBUG("USB device 0x%04x:0x%04x at %d.%d already exists, ignored",
                    spice_usb_device_get_vid(device),
                    spice_usb_device_get_pid(device),
                    spice_usb_device_get_busnum(device),
                    spice_usb_device_get_devaddr(device));
        return;
    }

    if (priv->coldplug_list)
        dev_list = priv->coldplug_list;
    else
        libusb_get_device_list(priv->context, &dev_list);

    for (i = 0; dev_list && dev_list[i]; i++) {
        if (spice_usb_device_manager_libdev_match(dev_list[i], bus, address)) {
            libdev = dev_list[i];
            break;
        }
    }

    if (libdev)
        device = (SpiceUsbDevice*)spice_usb_device_new(libdev);

    if (device && priv->auto_connect) {
        auto_ok = usbredirhost_check_device_filter(
                            priv->auto_conn_filter_rules,
                            priv->auto_conn_filter_rules_count,
                            libdev, 0) == 0;
    }

    if (!priv->coldplug_list)
        libusb_free_device_list(dev_list, 1);

    if (!device) {
        g_warning("Could not find USB device to add " DEV_ID_FMT,
                  bus, address);
        return;
    }

    g_ptr_array_add(priv->devices, device);

    if (priv->auto_connect) {
        gboolean can_redirect;

        can_redirect = spice_usb_device_manager_can_redirect_device(
                                        self, device, NULL);

        if (can_redirect && auto_ok)
            spice_usb_device_manager_connect_device_async(self,
                                   device, NULL,
                                   spice_usb_device_manager_auto_connect_cb,
                                   spice_usb_device_ref(device));
    }

    SPICE_DEBUG("device added %p", device);
    g_signal_emit(self, signals[DEVICE_ADDED], 0, device);
}

static void spice_usb_device_manager_remove_dev(SpiceUsbDeviceManager  *self,
                                                GUdevDevice            *udev)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    SpiceUsbDevice *device = NULL;
    int bus, address;

    if (!spice_usb_device_manager_get_udev_bus_n_address(udev, &bus, &address))
        return;

    device = spice_usb_device_manager_find_device(self, bus, address);
    if (!device) {
        g_warning("Could not find USB device to remove " DEV_ID_FMT,
                  bus, address);
        return;
    }

#ifdef G_OS_WIN32
    const guint8 state = spice_usb_device_get_state(device);
    if ((state == SPICE_USB_DEVICE_STATE_INSTALLING) ||
        (state == SPICE_USB_DEVICE_STATE_UNINSTALLING)) {
        SPICE_DEBUG("skipping " DEV_ID_FMT ". It is un/installing its driver",
                    bus, address);
        return;
    }
#endif

    spice_usb_device_manager_disconnect_device(self, device);

    SPICE_DEBUG("device removed %p", device);
    spice_usb_device_ref(device);
    g_ptr_array_remove(priv->devices, device);
    g_signal_emit(self, signals[DEVICE_REMOVED], 0, device);
    spice_usb_device_unref(device);
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

    spice_usbredir_channel_connect_device_finish(channel, channel_res, &err);
    if (err) {
        g_simple_async_result_take_error(result, err);
    }
    g_simple_async_result_complete(result);
    g_object_unref(result);
}

#ifdef G_OS_WIN32

typedef struct _UsbInstallCbInfo {
    SpiceUsbDeviceManager *manager;
    SpiceUsbDevice        *device;
    SpiceWinUsbDriver     *installer;
    GCancellable          *cancellable;
    GAsyncReadyCallback   callback;
    gpointer              user_data;
    gboolean              is_install;
} UsbInstallCbInfo;

/**
 * spice_usb_device_manager_drv_install_cb:
 * @gobject: #SpiceWinUsbDriver in charge of installing the driver
 * @res: #GAsyncResult of async win usb driver installation
 * @user_data: #SpiceUsbDeviceManager requested the installation
 *
 * Called when an Windows libusb driver installation completed.
 *
 * If the driver installation was successful, continue with USB
 * device redirection
 *
 * Always call _spice_usb_device_manager_connect_device_async.
 * When installation fails, libusb_open fails too, but cleanup would be better.
 */
static void spice_usb_device_manager_drv_install_cb(GObject *gobject,
                                                    GAsyncResult *res,
                                                    gpointer user_data)
{
    SpiceUsbDeviceManager *self;
    SpiceWinUsbDriver *installer;
    gint status;
    GError *err = NULL;
    SpiceUsbDevice *device;
    UsbInstallCbInfo *cbinfo;
    GCancellable *cancellable;
    GAsyncReadyCallback callback;
    gboolean is_install;
    const gchar *opstr;

    g_return_if_fail(user_data != NULL);

    cbinfo = user_data;
    self        = cbinfo->manager;
    device      = cbinfo->device;
    installer   = cbinfo->installer;
    cancellable = cbinfo->cancellable;
    callback    = cbinfo->callback;
    user_data   = cbinfo->user_data;
    is_install  = cbinfo->is_install;

    g_free(cbinfo);

    g_return_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self));
    g_return_if_fail(SPICE_IS_WIN_USB_DRIVER(installer));
    g_return_if_fail(device!= NULL);

    opstr = is_install ? "install" : "uninstall";
    SPICE_DEBUG("Win USB driver %s finished", opstr);

    status = spice_win_usb_driver_install_finish(installer, res, &err);

    spice_usb_device_unref(device);

    spice_usb_device_set_state(device, SPICE_USB_DEVICE_STATE_NONE);

    if (err) {
        g_warning("win usb driver %s failed -- %s", opstr, err->message);
        g_error_free(err);
    }

    if (!status) {
        g_warning("failed to %s win usb driver (status=0)", opstr);
    }

    if (! is_install) {
        return;
    }

    /* device is already ref'ed */
    _spice_usb_device_manager_connect_device_async(self,
                                                   device,
                                                   cancellable,
                                                   callback,
                                                   user_data);

}
#endif

/* ------------------------------------------------------------------ */
/* private api                                                        */

static gpointer spice_usb_device_manager_usb_ev_thread(gpointer user_data)
{
    SpiceUsbDeviceManager *self = SPICE_USB_DEVICE_MANAGER(user_data);
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    int rc;

    while (priv->event_thread_run) {
        rc = libusb_handle_events(priv->context);
        if (rc && rc != LIBUSB_ERROR_INTERRUPTED) {
            const char *desc = spice_usbutil_libusb_strerror(rc);
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
#if GLIB_CHECK_VERSION(2,31,19)
    priv->event_thread = g_thread_new("usb_ev_thread",
                                      spice_usb_device_manager_usb_ev_thread,
                                      self);
#else
    priv->event_thread = g_thread_create(spice_usb_device_manager_usb_ev_thread,
                                         self, TRUE, err);
#endif
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

static void spice_usb_device_manager_check_redir_on_connect(
    SpiceUsbDeviceManager *self, SpiceChannel *channel)
{
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    GSimpleAsyncResult *result;
    SpiceUsbDevice *device;
    libusb_device *libdev;
    guint i;

    if (priv->redirect_on_connect == NULL)
        return;

    for (i = 0; i < priv->devices->len; i++) {
        device = g_ptr_array_index(priv->devices, i);

        if (spice_usb_device_manager_is_device_connected(self, device))
            continue;

        libdev = spice_usb_device_manager_device_to_libdev(self, device);

        if (usbredirhost_check_device_filter(
                            priv->redirect_on_connect_rules,
                            priv->redirect_on_connect_rules_count,
                            libdev, 0) == 0) {
            /* Note: re-uses spice_usb_device_manager_connect_device_async's
               completion handling code! */
            result = g_simple_async_result_new(G_OBJECT(self),
                               spice_usb_device_manager_auto_connect_cb,
                               spice_usb_device_ref(device),
                               spice_usb_device_manager_connect_device_async);
            spice_usbredir_channel_connect_device_async(
                               SPICE_USBREDIR_CHANNEL(channel),
                               libdev, device, NULL,
                               spice_usb_device_manager_channel_connect_cb,
                               result);
            libusb_unref_device(libdev);
            return; /* We've taken the channel! */
        }

        libusb_unref_device(libdev);
    }
}

void spice_usb_device_manager_device_error(
    SpiceUsbDeviceManager *self, SpiceUsbDevice *device, GError *err)
{
    g_return_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self));
    g_return_if_fail(device != NULL);

    g_signal_emit(self, signals[DEVICE_ERROR], 0, device, err);
}
#endif

static SpiceUsbredirChannel *spice_usb_device_manager_get_channel_for_dev(
    SpiceUsbDeviceManager *manager, SpiceUsbDevice *device)
{
#ifdef USE_USBREDIR
    SpiceUsbDeviceManagerPrivate *priv = manager->priv;
    guint i;

    for (i = 0; i < priv->channels->len; i++) {
        SpiceUsbredirChannel *channel = g_ptr_array_index(priv->channels, i);
        libusb_device *libdev = spice_usbredir_channel_get_device(channel);
        if (spice_usb_device_equal_libdev(device, libdev))
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
                                                  spice_usb_device_unref);
    for (i = 0; i < priv->devices->len; i++) {
        SpiceUsbDevice *device = g_ptr_array_index(priv->devices, i);
        g_ptr_array_add(devices_copy, spice_usb_device_ref(device));
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
static void
_spice_usb_device_manager_connect_device_async(SpiceUsbDeviceManager *self,
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
    libusb_device *libdev;
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

        libdev = spice_usb_device_manager_device_to_libdev(self, device);
        if (libdev == NULL) {
#ifdef G_OS_WIN32
            /* Most likely, the device was plugged out at driver installation
             * time, and its remove-device event was ignored.
             * So remove the device now
             */
            SPICE_DEBUG("libdev does not exist for %p -- removing", device);
            spice_usb_device_ref(device);
            g_ptr_array_remove(priv->devices, device);
            g_signal_emit(self, signals[DEVICE_REMOVED], 0, device);
            spice_usb_device_unref(device);
#endif
            g_simple_async_result_set_error(result,
                                            SPICE_CLIENT_ERROR,
                                            SPICE_CLIENT_ERROR_FAILED,
                                            _("Device was not found"));
            goto done;
        }
        spice_usbredir_channel_connect_device_async(channel,
                                 libdev,
                                 device,
                                 cancellable,
                                 spice_usb_device_manager_channel_connect_cb,
                                 result);
        libusb_unref_device(libdev);
        return;
    }
#endif

    g_simple_async_result_set_error(result,
                            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            _("No free USB channel"));
#ifdef USE_USBREDIR
done:
#endif
    g_simple_async_result_complete_in_idle(result);
    g_object_unref(result);
}


void spice_usb_device_manager_connect_device_async(SpiceUsbDeviceManager *self,
                                             SpiceUsbDevice *device,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data)
{

#if defined(USE_USBREDIR) && defined(G_OS_WIN32)
    SpiceWinUsbDriver *installer;
    UsbInstallCbInfo *cbinfo;

    spice_usb_device_set_state(device, SPICE_USB_DEVICE_STATE_INSTALLING);
    if (! self->priv->installer) {
        self->priv->installer = spice_win_usb_driver_new();
    }
    installer = self->priv->installer;
    cbinfo = g_new0(UsbInstallCbInfo, 1);
    cbinfo->manager     = self;
    cbinfo->device      = spice_usb_device_ref(device);
    cbinfo->installer   = installer;
    cbinfo->cancellable = cancellable;
    cbinfo->callback    = callback;
    cbinfo->user_data   = user_data;
    cbinfo->is_install  = TRUE;

    spice_win_usb_driver_install(installer, device, cancellable,
                                 spice_usb_device_manager_drv_install_cb,
                                 cbinfo);
#else
    _spice_usb_device_manager_connect_device_async(self,
                                                   device,
                                                   cancellable,
                                                   callback,
                                                   user_data);
#endif
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
        spice_usbredir_channel_disconnect_device(channel);

#ifdef G_OS_WIN32
    SpiceWinUsbDriver *installer;
    UsbInstallCbInfo *cbinfo;

    g_warn_if_fail(device != NULL);
    g_warn_if_fail(self->priv->installer != NULL);

    spice_usb_device_set_state(device, SPICE_USB_DEVICE_STATE_UNINSTALLING);
    if (! self->priv->installer) {
        self->priv->installer = spice_win_usb_driver_new();
    }
    installer = self->priv->installer;
    cbinfo = g_new0(UsbInstallCbInfo, 1);
    cbinfo->manager     = self;
    cbinfo->device      = spice_usb_device_ref(device);
    cbinfo->installer   = installer;
    cbinfo->cancellable = NULL;
    cbinfo->callback    = NULL;
    cbinfo->user_data   = NULL;
    cbinfo->is_install  = FALSE;

    spice_win_usb_driver_uninstall(installer, device, NULL,
                                   spice_usb_device_manager_drv_install_cb,
                                   cbinfo);
#endif

#endif
}

gboolean
spice_usb_device_manager_can_redirect_device(SpiceUsbDeviceManager  *self,
                                             SpiceUsbDevice         *device,
                                             GError                **err)
{
#ifdef USE_USBREDIR
    const struct usbredirfilter_rule *guest_filter_rules = NULL;
    SpiceUsbDeviceManagerPrivate *priv = self->priv;
    int i, guest_filter_rules_count;

    g_return_val_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self), FALSE);
    g_return_val_if_fail(device != NULL, FALSE);
    g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

    if (!priv->session->priv->usbredir) {
        g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            _("USB redirection is disabled"));
        return FALSE;
    }

    if (!priv->channels->len) {
        g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            _("The connected VM is not configured for USB redirection"));
        return FALSE;
    }

    /* Skip the other checks for already connected devices */
    if (spice_usb_device_manager_is_device_connected(self, device))
        return TRUE;

    /* We assume all channels have the same filter, so we just take the
       filter from the first channel */
    spice_usbredir_channel_get_guest_filter(
        g_ptr_array_index(priv->channels, 0),
        &guest_filter_rules, &guest_filter_rules_count);

    if (guest_filter_rules) {
        gboolean filter_ok;
        libusb_device *libdev;

        libdev = spice_usb_device_manager_device_to_libdev(self, device);
        g_return_val_if_fail(libdev != NULL, FALSE);
        filter_ok = (usbredirhost_check_device_filter(
                            guest_filter_rules, guest_filter_rules_count,
                            libdev, 0) == 0);
        libusb_unref_device(libdev);
        if (!filter_ok) {
            g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                                _("Some USB devices are blocked by host policy"));
            return FALSE;
        }
    }

    /* Check if there are free channels */
    for (i = 0; i < priv->channels->len; i++) {
        SpiceUsbredirChannel *channel = g_ptr_array_index(priv->channels, i);

        if (!spice_usbredir_channel_get_device(channel))
            break;
    }
    if (i == priv->channels->len) {
        g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                            _("There are no free USB channels"));
        return FALSE;
    }

    return TRUE;
#else
    g_set_error_literal(err, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                        _("USB redirection support not compiled in"));
    return FALSE;
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
gchar *spice_usb_device_get_description(SpiceUsbDevice *device, const gchar *format)
{
#ifdef USE_USBREDIR
    int bus, address, vid, pid;
    gchar *description, *descriptor, *manufacturer = NULL, *product = NULL;

    g_return_val_if_fail(device != NULL, NULL);

    bus     = spice_usb_device_get_busnum(device);
    address = spice_usb_device_get_devaddr(device);
    vid     = spice_usb_device_get_vid(device);
    pid     = spice_usb_device_get_pid(device);

    if ((vid > 0) && (pid > 0)) {
        descriptor = g_strdup_printf("[%04x:%04x]", vid, pid);
    } else {
        descriptor = g_strdup("");
    }

    spice_usb_util_get_device_strings(bus, address, vid, pid,
                                      &manufacturer, &product);

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



#ifdef USE_USBREDIR
/*
 * SpiceUsbDeviceInfo
 */
static SpiceUsbDeviceInfo *spice_usb_device_new(libusb_device *libdev)
{
    SpiceUsbDeviceInfo *info;
    int vid, pid;
    guint8 bus, addr;

    g_return_val_if_fail(libdev != NULL, NULL);

    bus = libusb_get_bus_number(libdev);
    addr = libusb_get_device_address(libdev);

    if (!spice_usb_device_manager_get_libdev_vid_pid(libdev, &vid, &pid)) {
        return NULL;
    }

    info = g_new0(SpiceUsbDeviceInfo, 1);

    info->busnum  = bus;
    info->devaddr = addr;
    info->vid = vid;
    info->pid = pid;
    info->ref = 1;

    return info;
}

guint8 spice_usb_device_get_busnum(const SpiceUsbDevice *device)
{
    const SpiceUsbDeviceInfo *info = (const SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, 0);

    return info->busnum;
}

guint8 spice_usb_device_get_devaddr(const SpiceUsbDevice *device)
{
    const SpiceUsbDeviceInfo *info = (const SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, 0);

    return info->devaddr;
}

guint16 spice_usb_device_get_vid(const SpiceUsbDevice *device)
{
    const SpiceUsbDeviceInfo *info = (const SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, 0);

    return info->vid;
}

guint16 spice_usb_device_get_pid(const SpiceUsbDevice *device)
{
    const SpiceUsbDeviceInfo *info = (const SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, 0);

    return info->pid;
}

#ifdef G_OS_WIN32
void spice_usb_device_set_state(SpiceUsbDevice *device, guint8 state)
{
    SpiceUsbDeviceInfo *info = (SpiceUsbDeviceInfo *)device;

    g_return_if_fail(info != NULL);

    info->state = state;
}

guint8 spice_usb_device_get_state(SpiceUsbDevice *device)
{
    SpiceUsbDeviceInfo *info = (SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, 0);

    return info->state;
}
#endif

static SpiceUsbDevice *spice_usb_device_ref(SpiceUsbDevice *device)
{
    SpiceUsbDeviceInfo *info = (SpiceUsbDeviceInfo *)device;

    g_return_val_if_fail(info != NULL, NULL);
    g_atomic_int_inc(&info->ref);
    return device;
}

static void spice_usb_device_unref(SpiceUsbDevice *device)
{
    gboolean ref_count_is_0;

    SpiceUsbDeviceInfo *info = (SpiceUsbDeviceInfo *)device;

    g_return_if_fail(info != NULL);

    ref_count_is_0 = g_atomic_int_dec_and_test(&info->ref);
    if (ref_count_is_0) {
        g_free(info);
    }
}

#ifndef G_OS_WIN32 /* Linux -- compare bus.addr */
static gboolean
spice_usb_device_equal_libdev(SpiceUsbDevice *device,
                              libusb_device  *libdev)
{
    guint8 addr1, addr2, bus1, bus2;

    if ((device == NULL) || (libdev == NULL))
        return FALSE;

    bus1  = spice_usb_device_get_busnum(device);
    addr1 = spice_usb_device_get_devaddr(device);
    bus2  = libusb_get_bus_number(libdev);
    addr2 = libusb_get_device_address(libdev);

    return ((bus1 == bus2) && (addr1 == addr2));
}
#else /* Windows -- compare vid:pid of device and libdev */
static gboolean
spice_usb_device_equal_libdev(SpiceUsbDevice *device,
                              libusb_device  *libdev)
{
    int vid1, vid2, pid1, pid2;

    if ((device == NULL) || (libdev == NULL))
        return FALSE;

    vid1 = spice_usb_device_get_vid(device);
    pid1 = spice_usb_device_get_pid(device);

    if (!spice_usb_device_manager_get_libdev_vid_pid(libdev, &vid2, &pid2)) {
        return FALSE;
    }

    return ((vid1 == vid2) && (pid1 == pid2));
}
#endif

/*
 * Caller must libusb_unref_device the libusb_device returned by this function.
 * Returns a libusb_device, or NULL upon failure
 */
static libusb_device *
spice_usb_device_manager_device_to_libdev(SpiceUsbDeviceManager *self,
                                          SpiceUsbDevice *device)
{
    libusb_device *d, **devlist;
    int bus, addr;
    int i;

    g_return_val_if_fail(SPICE_IS_USB_DEVICE_MANAGER(self), NULL);
    g_return_val_if_fail(device != NULL, NULL);
    g_return_val_if_fail(self->priv != NULL, NULL);
    g_return_val_if_fail(self->priv->context != NULL, NULL);

#ifndef G_OS_WIN32
    bus  = spice_usb_device_get_busnum(device);
    addr = spice_usb_device_get_devaddr(device);
#else
    bus  = spice_usb_device_get_vid(device);
    addr = spice_usb_device_get_pid(device);
#endif

    libusb_get_device_list(self->priv->context, &devlist);
    if (!devlist)
        return NULL;

    for (i = 0; (d = devlist[i]) != NULL; i++) {
        if (spice_usb_device_manager_libdev_match(d, bus, addr)) {
            libusb_ref_device(d);
            break;
        }
    }

    libusb_free_device_list(devlist, 1);

    return d;
}
#endif /* USE_USBREDIR */
