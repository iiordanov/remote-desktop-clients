/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib-object.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <stdlib.h>

#include "glib-compat.h"

#ifdef USE_USBREDIR
#ifdef __linux__
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "usbutil.h"
#include "spice-util-priv.h"

#define VENDOR_NAME_LEN (122 - sizeof(void *))
#define PRODUCT_NAME_LEN 126

typedef struct _usb_product_info {
    guint16 product_id;
    char name[PRODUCT_NAME_LEN];
} usb_product_info;

typedef struct _usb_vendor_info {
    usb_product_info *product_info;
    int product_count;
    guint16 vendor_id;
    char name[VENDOR_NAME_LEN];
} usb_vendor_info;

static GStaticMutex usbids_load_mutex = G_STATIC_MUTEX_INIT;
static int usbids_vendor_count = 0; /* < 0: failed, 0: empty, > 0: loaded */
static usb_vendor_info *usbids_vendor_info = NULL;

G_GNUC_INTERNAL
const char *spice_usbutil_libusb_strerror(enum libusb_error error_code)
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
static gchar *spice_usbutil_get_sysfs_attribute(int bus, int address,
                                                const char *attribute)
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

static gboolean spice_usbutil_parse_usbids(gchar *path)
{
    gchar *contents, *line, **lines;
    usb_product_info *product_info;
    int i, j, id, product_count = 0;

    usbids_vendor_count = 0;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        usbids_vendor_count = -1;
        return FALSE;
    }

    lines = g_strsplit(contents, "\n", -1);

    for (i = 0; lines[i]; i++) {
        if (!isxdigit(lines[i][0]) || !isxdigit(lines[i][1]))
            continue;

        for (j = 1; lines[i + j] &&
                   (lines[i + j][0] == '\t' ||
                    lines[i + j][0] == '#'  ||
                    lines[i + j][0] == '\0'); j++) {
            if (lines[i + j][0] == '\t' && isxdigit(lines[i + j][1]))
                product_count++;
        }
        i += j - 1;

        usbids_vendor_count++;
    }

    usbids_vendor_info = g_new(usb_vendor_info, usbids_vendor_count);
    product_info = g_new(usb_product_info, product_count);

    usbids_vendor_count = 0;
    for (i = 0; lines[i]; i++) {
        line = lines[i];

        if (!isxdigit(line[0]) || !isxdigit(line[1]))
            continue;

        id = strtoul(line, &line, 16);
        while (isspace(line[0]))
            line++;

        usbids_vendor_info[usbids_vendor_count].vendor_id = id;
        snprintf(usbids_vendor_info[usbids_vendor_count].name,
                 VENDOR_NAME_LEN, "%s", line);

        product_count = 0;
        for (j = 1; lines[i + j] &&
                   (lines[i + j][0] == '\t' ||
                    lines[i + j][0] == '#'  ||
                    lines[i + j][0] == '\0'); j++) {
            line = lines[i + j];

            if (line[0] != '\t' || !isxdigit(line[1]))
                continue;

            id = strtoul(line + 1, &line, 16);
            while (isspace(line[0]))
                line++;
            product_info[product_count].product_id = id;
            snprintf(product_info[product_count].name,
                     PRODUCT_NAME_LEN, "%s", line);

            product_count++;
        }
        i += j - 1;

        usbids_vendor_info[usbids_vendor_count].product_count = product_count;
        usbids_vendor_info[usbids_vendor_count].product_info  = product_info;
        product_info += product_count;
        usbids_vendor_count++;
    }

    g_strfreev(lines);
    g_free(contents);

#if 0 /* Testing only */
    for (i = 0; i < usbids_vendor_count; i++) {
        printf("%04x  %s\n", usbids_vendor_info[i].vendor_id,
               usbids_vendor_info[i].name);
        product_info = usbids_vendor_info[i].product_info;
        for (j = 0; j < usbids_vendor_info[i].product_count; j++) {
            printf("\t%04x  %s\n", product_info[j].product_id,
                   product_info[j].name);
        }
    }
#endif

    return TRUE;
}

static gboolean spice_usbutil_load_usbids(void)
{
    gboolean success = FALSE;

    g_static_mutex_lock(&usbids_load_mutex);
    if (usbids_vendor_count) {
        success = usbids_vendor_count > 0;
        goto leave;
    }

#ifdef WITH_USBIDS
    success = spice_usbutil_parse_usbids(USB_IDS);
#else
    {
        const gchar * const *dirs = g_get_system_data_dirs();
        gchar *path = NULL;
        int i;

        for (i = 0; dirs[i]; ++i) {
            path = g_build_filename(dirs[i], "hwdata", "usb.ids", NULL);
            success = spice_usbutil_parse_usbids(path);
            SPICE_DEBUG("loading %s success: %s", path, spice_yes_no(success));
            g_free(path);

            if (success)
                goto leave;
        }
    }
#endif

leave:
    g_static_mutex_unlock(&usbids_load_mutex);
    return success;
}

G_GNUC_INTERNAL
void spice_usb_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product)
{
    usb_product_info *product_info;
    int i, j;

    g_return_if_fail(manufacturer != NULL);
    g_return_if_fail(product != NULL);

    *manufacturer = NULL;
    *product = NULL;

#if __linux__
    *manufacturer = spice_usbutil_get_sysfs_attribute(bus, address, "manufacturer");
    *product = spice_usbutil_get_sysfs_attribute(bus, address, "product");
#endif

    if ((!*manufacturer || !*product) &&
        spice_usbutil_load_usbids()) {

        for (i = 0; i < usbids_vendor_count; i++) {
            if ((int)usbids_vendor_info[i].vendor_id != vendor_id)
                continue;

            if (!*manufacturer && usbids_vendor_info[i].name[0])
                *manufacturer = g_strdup(usbids_vendor_info[i].name);

            product_info = usbids_vendor_info[i].product_info;
            for (j = 0; j < usbids_vendor_info[i].product_count; j++) {
                if ((int)product_info[j].product_id != product_id)
                    continue;

                if (!*product && product_info[j].name[0])
                    *product = g_strdup(product_info[j].name);

                break;
            }
            break;
        }
    }

    if (!*manufacturer)
        *manufacturer = g_strdup(_("USB"));
    if (!*product)
        *product = g_strdup(_("Device"));

    /* Some devices have unwanted whitespace in their strings */
    g_strstrip(*manufacturer);
    g_strstrip(*product);

    /* Some devices repeat the manufacturer at the beginning of product */
    if (g_str_has_prefix(*product, *manufacturer) &&
            strlen(*product) > strlen(*manufacturer)) {
        gchar *tmp = g_strdup(*product + strlen(*manufacturer));
        g_free(*product);
        *product = tmp;
        g_strstrip(*product);
    }
}

#endif

#ifdef USBUTIL_TEST
int main()
{
    if (spice_usbutil_load_usbids())
        exit(0);

    exit(1);
}
#endif
