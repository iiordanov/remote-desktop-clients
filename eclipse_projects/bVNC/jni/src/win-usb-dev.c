/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

   Red Hat Authors:
   Arnon Gilboa <agilboa@redhat.com>
   Uri Lublin   <uril@redhat.com>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>
#include <libusb.h>
#include "win-usb-dev.h"
#include "spice-marshal.h"
#include "spice-util.h"
#include "usbutil.h"

#define G_UDEV_CLIENT_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), G_UDEV_TYPE_CLIENT, GUdevClientPrivate))

struct _GUdevClientPrivate {
    libusb_context *ctx;
    gssize udev_list_size;
    GList *udev_list;
    HWND hwnd;
};

#define G_UDEV_CLIENT_WINCLASS_NAME  TEXT("G_UDEV_CLIENT")

static void g_udev_client_initable_iface_init(GInitableIface  *iface);

G_DEFINE_TYPE_WITH_CODE(GUdevClient, g_udev_client, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, g_udev_client_initable_iface_init));


typedef struct _GUdevDeviceInfo GUdevDeviceInfo;

struct _GUdevDeviceInfo {
    guint16 bus;
    guint16 addr;
    guint16 vid;
    guint16 pid;
    guint16 class;
    gchar sclass[4];
    gchar sbus[4];
    gchar saddr[4];
    gchar svid[8];
    gchar spid[8];
};

struct _GUdevDevicePrivate
{
    /* FixMe: move above fields to this structure and access them directly */
    GUdevDeviceInfo *udevinfo;
};

G_DEFINE_TYPE(GUdevDevice, g_udev_device, G_TYPE_OBJECT)


enum
{
    UEVENT_SIGNAL,
    LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0, };
static GUdevClient *singleton = NULL;

static GUdevDevice *g_udev_device_new(GUdevDeviceInfo *udevinfo);
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static gboolean get_usb_dev_info(libusb_device *dev, GUdevDeviceInfo *udevinfo);

//uncomment to debug gudev device lists.
//#define DEBUG_GUDEV_DEVICE_LISTS

#ifdef DEBUG_GUDEV_DEVICE_LISTS
static void g_udev_device_print_list(GList *l, const gchar *msg);
#else
static void g_udev_device_print_list(GList *l, const gchar *msg) {}
#endif
static void g_udev_device_print(GUdevDevice *udev, const gchar *msg);

static gboolean g_udev_skip_search(GUdevDevice *udev);

GQuark g_udev_client_error_quark(void)
{
    return g_quark_from_static_string("win-gudev-client-error-quark");
}

GUdevClient *g_udev_client_new(const gchar* const *subsystems)
{
    if (!singleton) {
        singleton = g_initable_new(G_UDEV_TYPE_CLIENT, NULL, NULL, NULL);
        return singleton;
    } else {
        return g_object_ref(singleton);
    }
}


/*
 * devs [in,out] an empty devs list in, full devs list out
 * Returns: number-of-devices, or a negative value on error.
 */
static ssize_t
g_udev_client_list_devices(GUdevClient *self, GList **devs,
                           GError **err, const gchar *name)
{
    gssize rc;
    libusb_device **lusb_list, **dev;
    GUdevClientPrivate *priv;
    GUdevDeviceInfo *udevinfo;
    GUdevDevice *udevice;
    ssize_t n;

    g_return_val_if_fail(G_UDEV_IS_CLIENT(self), -1);
    g_return_val_if_fail(devs != NULL, -2);

    priv = self->priv;

    g_return_val_if_fail(self->priv->ctx != NULL, -3);

    rc = libusb_get_device_list(priv->ctx, &lusb_list);
    if (rc < 0) {
        const char *errstr = spice_usbutil_libusb_strerror(rc);
        g_warning("%s: libusb_get_device_list failed", name);
        g_set_error(err, G_UDEV_CLIENT_ERROR, G_UDEV_CLIENT_LIBUSB_FAILED,
                    "%s: Error getting device list from libusb: %s [%i]",
                    name, errstr, rc);
        return -4;
    }

    n = 0;
    for (dev = lusb_list; *dev; dev++) {
        udevinfo = g_new0(GUdevDeviceInfo, 1);
        get_usb_dev_info(*dev, udevinfo);
        udevice = g_udev_device_new(udevinfo);
        if (g_udev_skip_search(udevice)) {
            g_object_unref(udevice);
            continue;
        }
        *devs = g_list_prepend(*devs, udevice);
        n++;
    }
    libusb_free_device_list(lusb_list, 1);

    return n;
}

static void g_udev_client_free_device_list(GList **devs)
{
    g_return_if_fail(devs != NULL);
    if (*devs) {
        g_list_free_full(*devs, g_object_unref);
        *devs = NULL;
    }
}


static gboolean
g_udev_client_initable_init(GInitable *initable, GCancellable *cancellable,
                            GError **err)
{
    GUdevClient *self;
    GUdevClientPrivate *priv;
    WNDCLASS wcls;
    int rc;

    g_return_val_if_fail(G_UDEV_IS_CLIENT(initable), FALSE);
    g_return_val_if_fail(cancellable == NULL, FALSE);

    self = G_UDEV_CLIENT(initable);
    priv = self->priv;

    rc = libusb_init(&priv->ctx);
    if (rc < 0) {
        const char *errstr = spice_usbutil_libusb_strerror(rc);
        g_warning("Error initializing USB support: %s [%i]", errstr, rc);
        g_set_error(err, G_UDEV_CLIENT_ERROR, G_UDEV_CLIENT_LIBUSB_FAILED,
                    "Error initializing USB support: %s [%i]", errstr, rc);
        return FALSE;
    }

    /* get initial device list */
    priv->udev_list_size = g_udev_client_list_devices(self, &priv->udev_list,
                                                      err, __FUNCTION__);
    if (priv->udev_list_size < 0) {
        goto g_udev_client_init_failed;
    }

    g_udev_device_print_list(priv->udev_list, "init: first list is: ");

    /* create a hidden window */
    memset(&wcls, 0, sizeof(wcls));
    wcls.lpfnWndProc = wnd_proc;
    wcls.lpszClassName = G_UDEV_CLIENT_WINCLASS_NAME;
    if (!RegisterClass(&wcls)) {
        DWORD e = GetLastError();
        g_warning("RegisterClass failed , %ld", (long)e);
        g_set_error(err, G_UDEV_CLIENT_ERROR, G_UDEV_CLIENT_WINAPI_FAILED,
                    "RegisterClass failed: %ld", (long)e);
        goto g_udev_client_init_failed;
    }
    priv->hwnd = CreateWindow(G_UDEV_CLIENT_WINCLASS_NAME,
                              NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!priv->hwnd) {
        DWORD e = GetLastError();
        g_warning("CreateWindow failed: %ld", (long)e);
        g_set_error(err, G_UDEV_CLIENT_ERROR, G_UDEV_CLIENT_LIBUSB_FAILED,
                    "CreateWindow failed: %ld", (long)e);
        goto g_udev_client_init_failed_unreg;
    }

    return TRUE;

 g_udev_client_init_failed_unreg:
    UnregisterClass(G_UDEV_CLIENT_WINCLASS_NAME, NULL);
 g_udev_client_init_failed:
    libusb_exit(priv->ctx);
    priv->ctx = NULL;

    return FALSE;
}

static void g_udev_client_initable_iface_init(GInitableIface *iface)
{
    iface->init = g_udev_client_initable_init;
}

GList *g_udev_client_query_by_subsystem(GUdevClient *self, const gchar *subsystem)
{
    GList *l = g_list_copy(self->priv->udev_list);
    g_list_foreach(l, (GFunc)g_object_ref, NULL);
    return l;
}

static void g_udev_client_init(GUdevClient *self)
{
    self->priv = G_UDEV_CLIENT_GET_PRIVATE(self);
}

static void g_udev_client_finalize(GObject *gobject)
{
    GUdevClient *self = G_UDEV_CLIENT(gobject);
    GUdevClientPrivate *priv = self->priv;

    singleton = NULL;
    DestroyWindow(priv->hwnd);
    UnregisterClass(G_UDEV_CLIENT_WINCLASS_NAME, NULL);
    g_udev_client_free_device_list(&priv->udev_list);

    /* free libusb context initializing by libusb_init() */
    g_warn_if_fail(priv->ctx != NULL);
    libusb_exit(priv->ctx);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(g_udev_client_parent_class)->finalize)
        G_OBJECT_CLASS(g_udev_client_parent_class)->finalize(gobject);
}

static void g_udev_client_class_init(GUdevClientClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = g_udev_client_finalize;

    signals[UEVENT_SIGNAL] =
        g_signal_new("uevent",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GUdevClientClass, uevent),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__BOXED_BOXED,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_STRING,
                     G_UDEV_TYPE_DEVICE);

    g_type_class_add_private(klass, sizeof(GUdevClientPrivate));
}

static gboolean get_usb_dev_info(libusb_device *dev, GUdevDeviceInfo *udevinfo)
{
    struct libusb_device_descriptor desc;

    g_return_val_if_fail(dev, FALSE);
    g_return_val_if_fail(udevinfo, FALSE);

    if (libusb_get_device_descriptor(dev, &desc) < 0) {
        g_warning("cannot get device descriptor %p", dev);
        return FALSE;
    }

    udevinfo->bus   = libusb_get_bus_number(dev);
    udevinfo->addr  = libusb_get_device_address(dev);
    udevinfo->class = desc.bDeviceClass;
    udevinfo->vid   = desc.idVendor;
    udevinfo->pid   = desc.idProduct;
    snprintf(udevinfo->sclass, sizeof(udevinfo->sclass), "%d", udevinfo->class);
    snprintf(udevinfo->sbus,   sizeof(udevinfo->sbus),   "%d", udevinfo->bus);
    snprintf(udevinfo->saddr,  sizeof(udevinfo->saddr),  "%d", udevinfo->addr);
    snprintf(udevinfo->svid,   sizeof(udevinfo->svid),   "%d", udevinfo->vid);
    snprintf(udevinfo->spid,   sizeof(udevinfo->spid),   "%d", udevinfo->pid);
    return TRUE;
}

/* Only vid:pid are compared */
static gboolean gudev_devices_are_equal(GUdevDevice *a, GUdevDevice *b)
{
    GUdevDeviceInfo *ai, *bi;
    gboolean same_vid;
    gboolean same_pid;

    ai = a->priv->udevinfo;
    bi = b->priv->udevinfo;

    same_vid  = (ai->vid == bi->vid);
    same_pid  = (ai->pid == bi->pid);

    return (same_pid && same_vid);
}


/* Assumes each event stands for a single device change (at most) */
static void handle_dev_change(GUdevClient *self)
{
    GUdevClientPrivate *priv = self->priv;
    GUdevDevice *changed_dev = NULL;
    ssize_t dev_count;
    int is_dev_change;
    GError *err;
    GList *now_devs = NULL;
    GList *llist, *slist; /* long-list and short-list*/
    GList *lit, *sit; /* iterators for long-list and short-list */
    GUdevDevice *ldev, *sdev; /* devices on long-list and short-list */

    dev_count = g_udev_client_list_devices(self, &now_devs, &err,
                                           __FUNCTION__);
    g_return_if_fail(dev_count >= 0);

    SPICE_DEBUG("number of current devices %d, I know about %d devices",
                dev_count, priv->udev_list_size);

    is_dev_change = dev_count - priv->udev_list_size;
    if (is_dev_change == 0) {
        g_udev_client_free_device_list(&now_devs);
        return;
    }

    if (is_dev_change > 0) {
        llist  = now_devs;
        slist = priv->udev_list;
    } else {
        llist = priv->udev_list;
        slist  = now_devs;
    }

    g_udev_device_print_list(llist, "handle_dev_change: long list:");
    g_udev_device_print_list(slist, "handle_dev_change: short list:");

    /* Go over the longer list */
    for (lit = g_list_first(llist); lit != NULL; lit=g_list_next(lit)) {
        ldev = lit->data;
        /* Look for dev in the shorther list */
        for (sit = g_list_first(slist); sit != NULL; sit=g_list_next(sit)) {
            sdev = sit->data;
            if (gudev_devices_are_equal(ldev, sdev))
                break;
        }
        if (sit == NULL) {
            /* Found a device which appears only in the longer list */
            changed_dev = ldev;
            break;
        }
    }

    if (!changed_dev) {
        g_warning("couldn't find any device change");
        goto leave;
    }

    if (is_dev_change > 0) {
        g_udev_device_print(changed_dev, ">>> USB device inserted");
        g_signal_emit(self, signals[UEVENT_SIGNAL], 0, "add", changed_dev);
    } else {
        g_udev_device_print(changed_dev, "<<< USB device removed");
        g_signal_emit(self, signals[UEVENT_SIGNAL], 0, "remove", changed_dev);
    }

leave:
    /* keep most recent info: free previous list, and keep current list */
    g_udev_client_free_device_list(&priv->udev_list);
    priv->udev_list = now_devs;
    priv->udev_list_size = dev_count;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    /* Only DBT_DEVNODES_CHANGED recieved */
    if (message == WM_DEVICECHANGE) {
        handle_dev_change(singleton);
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

/*** GUdevDevice ***/

static void g_udev_device_finalize(GObject *object)
{
    GUdevDevice *device =  G_UDEV_DEVICE(object);

    g_free(device->priv->udevinfo);
    if (G_OBJECT_CLASS(g_udev_device_parent_class)->finalize != NULL)
        (* G_OBJECT_CLASS(g_udev_device_parent_class)->finalize)(object);
}

static void g_udev_device_class_init(GUdevDeviceClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *) klass;

    gobject_class->finalize = g_udev_device_finalize;
    g_type_class_add_private (klass, sizeof(GUdevDevicePrivate));
}

static void g_udev_device_init(GUdevDevice *device)
{
    device->priv = G_TYPE_INSTANCE_GET_PRIVATE(device, G_UDEV_TYPE_DEVICE, GUdevDevicePrivate);
}

static GUdevDevice *g_udev_device_new(GUdevDeviceInfo *udevinfo)
{
    GUdevDevice *device;

    g_return_val_if_fail(udevinfo != NULL, NULL);

    device =  G_UDEV_DEVICE(g_object_new(G_UDEV_TYPE_DEVICE, NULL));
    device->priv->udevinfo = udevinfo;
    return device;
}

const gchar *g_udev_device_get_property(GUdevDevice *udev, const gchar *property)
{
    GUdevDeviceInfo* udevinfo;

    g_return_val_if_fail(G_UDEV_DEVICE(udev), NULL);
    g_return_val_if_fail(property != NULL, NULL);

    udevinfo = udev->priv->udevinfo;
    g_return_val_if_fail(udevinfo != NULL, NULL);

    if (g_strcmp0(property, "BUSNUM") == 0) {
        return udevinfo->sbus;
    } else if (g_strcmp0(property, "DEVNUM") == 0) {
        return udevinfo->saddr;
    } else if (g_strcmp0(property, "DEVTYPE") == 0) {
        return "usb_device";
    } else if (g_strcmp0(property, "VID") == 0) {
        return udevinfo->svid;
    } else if (g_strcmp0(property, "PID") == 0) {
        return udevinfo->spid;
    }

    g_warn_if_reached();
    return NULL;
}

const gchar *g_udev_device_get_sysfs_attr(GUdevDevice *udev, const gchar *attr)
{
    GUdevDeviceInfo* udevinfo;

    g_return_val_if_fail(G_UDEV_DEVICE(udev), NULL);
    g_return_val_if_fail(attr != NULL, NULL);

    udevinfo = udev->priv->udevinfo;
    g_return_val_if_fail(udevinfo != NULL, NULL);


    if (g_strcmp0(attr, "bDeviceClass") == 0) {
        return udevinfo->sclass;
    }
    g_warn_if_reached();
    return NULL;
}

#ifdef DEBUG_GUDEV_DEVICE_LISTS
static void g_udev_device_print_list(GList *l, const gchar *msg)
{
    GList *it;

    for (it = g_list_first(l); it != NULL; it=g_list_next(it)) {
        g_udev_device_print(it->data, msg);
    }
}
#endif

static void g_udev_device_print(GUdevDevice *udev, const gchar *msg)
{
    GUdevDeviceInfo* udevinfo;

    g_return_if_fail(G_UDEV_DEVICE(udev));

    udevinfo = udev->priv->udevinfo;
    g_return_if_fail(udevinfo != NULL);

    SPICE_DEBUG("%s: %d.%d 0x%04x:0x%04x class %d", msg,
                udevinfo->bus, udevinfo->addr,
                udevinfo->vid, udevinfo->pid, udevinfo->class);
}

static gboolean g_udev_skip_search(GUdevDevice *udev)
{
    GUdevDeviceInfo* udevinfo;
    gboolean skip;

    g_return_val_if_fail(G_UDEV_DEVICE(udev), FALSE);

    udevinfo = udev->priv->udevinfo;
    g_return_val_if_fail(udevinfo != NULL, FALSE);

    skip = ((udevinfo->addr == 0xff) ||  /* root hub (HCD) */
            (udevinfo->class == LIBUSB_CLASS_HUB) || /* hub*/
            (udevinfo->addr == 0)); /* bad address */
    return skip;
}
