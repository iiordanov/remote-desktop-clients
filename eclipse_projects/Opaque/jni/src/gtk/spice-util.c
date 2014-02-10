/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.
   Copyright © 2006-2010 Collabora Ltd. <http://www.collabora.co.uk/>

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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "spice-util-priv.h"
#include "spice-util.h"
#include "spice-util-priv.h"

/**
 * SECTION:spice-util
 * @short_description: version and debugging functions
 * @title: Utilities
 * @section_id:
 * @stability: Stable
 * @include: spice-util.h
 *
 * Various functions for debugging and informational purposes.
 */

static GOnce debug_once = G_ONCE_INIT;

static void spice_util_enable_debug_messages(void)
{
#if GLIB_CHECK_VERSION(2, 31, 0)
    const gchar *doms = g_getenv("G_MESSAGES_DEBUG");
    if (!doms) {
        g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, 1);
    } else if (g_str_equal(doms, "all")) {
	return;
    } else if (!strstr(doms, G_LOG_DOMAIN)) {
        gchar *newdoms = g_strdup_printf("%s %s", doms, G_LOG_DOMAIN);
        g_setenv("G_MESSAGES_DEBUG", newdoms, 1);
        g_free(newdoms);
    }
#endif
}

/**
 * spice_util_set_debug:
 * @enabled: %TRUE or %FALSE
 *
 * Enable or disable Spice-GTK debugging messages.
 **/
void spice_util_set_debug(gboolean enabled)
{
    /* Make sure debug_once has been initialised
     * with the value of SPICE_DEBUG already, otherwise
     * spice_util_get_debug() may overwrite the value
     * that was just set using spice_util_set_debug()
     */
    spice_util_get_debug();

    if (enabled) {
        spice_util_enable_debug_messages();
    }

    debug_once.retval = GINT_TO_POINTER(enabled);
}

static gpointer getenv_debug(gpointer data)
{
    gboolean debug;

    debug = (g_getenv("SPICE_DEBUG") != NULL);
    if (debug)
        spice_util_enable_debug_messages();

    return GINT_TO_POINTER(debug);
}

gboolean spice_util_get_debug(void)
{
    g_once(&debug_once, getenv_debug, NULL);

    return GPOINTER_TO_INT(debug_once.retval);
}

/**
 * spice_util_get_version_string:
 *
 * Returns: Spice-GTK version as a const string.
 **/
const gchar *spice_util_get_version_string(void)
{
    return VERSION;
}

G_GNUC_INTERNAL
gboolean spice_strv_contains(const GStrv strv, const gchar *str)
{
    int i;

    if (strv == NULL)
        return FALSE;

    for (i = 0; strv[i] != NULL; i++)
        if (g_str_equal(strv[i], str))
            return TRUE;

    return FALSE;
}

/**
 * spice_uuid_to_string:
 * @uuid: UUID byte array
 *
 * Creates a string representation of @uuid, of the form
 * "06e023d5-86d8-420e-8103-383e4566087a"
 *
 * Returns: A string that should be freed with g_free().
 * Since: 0.22
 **/
gchar* spice_uuid_to_string(const guint8 uuid[16])
{
    return g_strdup_printf(UUID_FMT, uuid[0], uuid[1],
                           uuid[2], uuid[3], uuid[4], uuid[5],
                           uuid[6], uuid[7], uuid[8], uuid[9],
                           uuid[10], uuid[11], uuid[12], uuid[13],
                           uuid[14], uuid[15]);
}

typedef struct {
    GObject *instance;
    GObject *observer;
    GClosure *closure;
    gulong handler_id;
} WeakHandlerCtx;

static WeakHandlerCtx *
whc_new (GObject *instance,
         GObject *observer)
{
    WeakHandlerCtx *ctx = g_slice_new0 (WeakHandlerCtx);

    ctx->instance = instance;
    ctx->observer = observer;

    return ctx;
}

static void
whc_free (WeakHandlerCtx *ctx)
{
    g_slice_free (WeakHandlerCtx, ctx);
}

static void observer_destroyed_cb (gpointer, GObject *);
static void closure_invalidated_cb (gpointer, GClosure *);

/*
 * If signal handlers are removed before the object is destroyed, this
 * callback will never get triggered.
 */
static void
instance_destroyed_cb (gpointer ctx_,
                       GObject *where_the_instance_was)
{
    WeakHandlerCtx *ctx = ctx_;

    /* No need to disconnect the signal here, the instance has gone away. */
    g_object_weak_unref (ctx->observer, observer_destroyed_cb, ctx);
    g_closure_remove_invalidate_notifier (ctx->closure, ctx,
                                          closure_invalidated_cb);
    whc_free (ctx);
}

/* Triggered when the observer is destroyed. */
static void
observer_destroyed_cb (gpointer ctx_,
                       GObject *where_the_observer_was)
{
    WeakHandlerCtx *ctx = ctx_;

    g_closure_remove_invalidate_notifier (ctx->closure, ctx,
                                          closure_invalidated_cb);
    g_signal_handler_disconnect (ctx->instance, ctx->handler_id);
    g_object_weak_unref (ctx->instance, instance_destroyed_cb, ctx);
    whc_free (ctx);
}

/* Triggered when either object is destroyed or the handler is disconnected. */
static void
closure_invalidated_cb (gpointer ctx_,
                        GClosure *where_the_closure_was)
{
    WeakHandlerCtx *ctx = ctx_;

    g_object_weak_unref (ctx->instance, instance_destroyed_cb, ctx);
    g_object_weak_unref (ctx->observer, observer_destroyed_cb, ctx);
    whc_free (ctx);
}

/* Copied from tp_g_signal_connect_object. See documentation. */
/**
  * spice_g_signal_connect_object: (skip)
  * @instance: the instance to connect to.
  * @detailed_signal: a string of the form "signal-name::detail".
  * @c_handler: the #GCallback to connect.
  * @gobject: the object to pass as data to @c_handler.
  * @connect_flags: a combination of #GConnectFlags.
  *
  * Similar to g_signal_connect_object() but will delete connection
  * when any of the objects is destroyed.
  *
  * Returns: the handler id.
  */
gulong spice_g_signal_connect_object (gpointer instance,
                                      const gchar *detailed_signal,
                                      GCallback c_handler,
                                      gpointer gobject,
                                      GConnectFlags connect_flags)
{
    GObject *instance_obj = G_OBJECT (instance);
    WeakHandlerCtx *ctx = whc_new (instance_obj, gobject);

    g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
    g_return_val_if_fail (detailed_signal != NULL, 0);
    g_return_val_if_fail (c_handler != NULL, 0);
    g_return_val_if_fail (G_IS_OBJECT (gobject), 0);
    g_return_val_if_fail (
                          (connect_flags & ~(G_CONNECT_AFTER|G_CONNECT_SWAPPED)) == 0, 0);

    if (connect_flags & G_CONNECT_SWAPPED)
        ctx->closure = g_cclosure_new_object_swap (c_handler, gobject);
    else
        ctx->closure = g_cclosure_new_object (c_handler, gobject);

    ctx->handler_id = g_signal_connect_closure (instance, detailed_signal,
                                                ctx->closure, (connect_flags & G_CONNECT_AFTER) ? TRUE : FALSE);

    g_object_weak_ref (instance_obj, instance_destroyed_cb, ctx);
    g_object_weak_ref (gobject, observer_destroyed_cb, ctx);
    g_closure_add_invalidate_notifier (ctx->closure, ctx,
                                       closure_invalidated_cb);

    return ctx->handler_id;
}

G_GNUC_INTERNAL
const gchar* spice_yes_no(gboolean value)
{
    return value ? "yes" : "no";
}

G_GNUC_INTERNAL
guint16 spice_make_scancode(guint scancode, gboolean release)
{
    SPICE_DEBUG("%s: %s scancode %d",
                __FUNCTION__, release ? "release" : "", scancode);

    if (release) {
        if (scancode < 0x100)
            return scancode | 0x80;
        else
            return 0x80e0 | ((scancode - 0x100) << 8);
    } else {
        if (scancode < 0x100)
            return scancode;
        else
            return 0xe0 | ((scancode - 0x100) << 8);
    }

    g_return_val_if_reached(0);
}

typedef enum {
    NEWLINE_TYPE_LF,
    NEWLINE_TYPE_CR_LF
} NewlineType;

static gssize get_line(const gchar *str, gsize len,
                       NewlineType type, gsize *nl_len,
                       GError **error)
{
    const gchar *p, *endl;
    gsize nl = 0;

    endl = (type == NEWLINE_TYPE_CR_LF) ? "\r\n" : "\n";
    p = g_strstr_len(str, len, endl);
    if (p) {
        len = p - str;
        nl = strlen(endl);
    }

    *nl_len = nl;
    return len;
}


static gchar* spice_convert_newlines(const gchar *str, gssize len,
                                     NewlineType from,
                                     NewlineType to,
                                     GError **error)
{
    GError *err = NULL;
    gssize length;
    gsize nl;
    GString *output;
    gboolean free_segment = FALSE;
    gint i;

    g_return_val_if_fail(str != NULL, NULL);
    g_return_val_if_fail(len >= -1, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    /* only 2 supported combinations */
    g_return_val_if_fail((from == NEWLINE_TYPE_LF &&
                          to == NEWLINE_TYPE_CR_LF) ||
                         (from == NEWLINE_TYPE_CR_LF &&
                          to == NEWLINE_TYPE_LF), NULL);

    if (len == -1)
        len = strlen(str);
    /* sometime we get \0 terminated strings, skip that, or it fails
       to utf8 validate line with \0 end */
    else if (len > 0 && str[len-1] == 0)
        len -= 1;

    /* allocate worst case, if it's small enough, we don't care much,
     * if it's big, malloc will put us in mmap'd region, and we can
     * over allocate.
     */
    output = g_string_sized_new(len * 2 + 1);

    for (i = 0; i < len; i += length + nl) {
        length = get_line(str + i, len - i, from, &nl, &err);
        if (length < 0)
            break;

        g_string_append_len(output, str + i, length);

        if (nl) {
            /* let's not double \r if it's already in the line */
            if (to == NEWLINE_TYPE_CR_LF &&
                output->str[output->len - 1] != '\r')
                g_string_append_c(output, '\r');

            g_string_append_c(output, '\n');
        }
    }

    if (err) {
        g_propagate_error(error, err);
        free_segment = TRUE;
    }

    return g_string_free(output, free_segment);
}

G_GNUC_INTERNAL
gchar* spice_dos2unix(const gchar *str, gssize len, GError **error)
{
    return spice_convert_newlines(str, len,
                                  NEWLINE_TYPE_CR_LF,
                                  NEWLINE_TYPE_LF,
                                  error);
}

G_GNUC_INTERNAL
gchar* spice_unix2dos(const gchar *str, gssize len, GError **error)
{
    return spice_convert_newlines(str, len,
                                  NEWLINE_TYPE_LF,
                                  NEWLINE_TYPE_CR_LF,
                                  error);
}

static bool buf_is_ones(unsigned size, const guint8 *data)
{
    int i;

    for (i = 0 ; i < size; ++i) {
        if (data[i] != 0xff) {
            return false;
        }
    }
    return true;
}

static bool is_edge_helper(const guint8 *xor, int bpl, int x, int y)
{
    return (xor[bpl * y + (x / 8)] & (0x80 >> (x % 8))) > 0;
}

static bool is_edge(unsigned width, unsigned height, const guint8 *xor, int bpl, int x, int y)
{
    if (x == 0 || x == width -1 || y == 0 || y == height - 1) {
        return 0;
    }
#define P(x, y) is_edge_helper(xor, bpl, x, y)
    return !P(x, y) && (P(x - 1, y + 1) || P(x, y + 1) || P(x + 1, y + 1) ||
                        P(x - 1, y)     ||                P(x + 1, y)     ||
                        P(x - 1, y - 1) || P(x, y - 1) || P(x + 1, y - 1));
#undef P
}

/* Mono cursors have two places, "and" and "xor". If a bit is 1 in both, it
 * means invertion of the corresponding pixel in the display. Since X11 (and
 * gdk) doesn't do invertion, instead we do edge detection and turn the
 * sorrounding edge pixels black, and the invert-me pixels white. To
 * illustrate:
 *
 *  and   xor      dest RGB (1=0xffffff, 0=0x000000)
 *
 *                        dest alpha (1=0xff, 0=0x00)
 *
 * 11111 00000     00000  00000
 * 11111 00000     00000  01110
 * 11111 00100 =>  00100  01110
 * 11111 00100     00100  01110
 * 11111 00000     00000  01110
 * 11111 00000     00000  00000
 *
 * See tests/util.c for more tests
 *
 * Notes:
 *  Assumes width >= 8 (i.e. bytes per line is at least 1)
 *  Assumes edges are not on the boundary (first/last line/column) for simplicity
 *
 */
G_GNUC_INTERNAL
void spice_mono_edge_highlight(unsigned width, unsigned height,
                               const guint8 *and, const guint8 *xor, guint8 *dest)
{
    int bpl = (width + 7) / 8;
    bool and_ones = buf_is_ones(height * bpl, and);
    int x, y, bit;
    const guint8 *xor_base = xor;

    for (y = 0; y < height; y++) {
        bit = 0x80;
        for (x = 0; x < width; x++, dest += 4) {
            if (is_edge(width, height, xor_base, bpl, x, y) && and_ones) {
                dest[0] = 0x00;
                dest[1] = 0x00;
                dest[2] = 0x00;
                dest[3] = 0xff;
                goto next_bit;
            }
            if (and[x/8] & bit) {
                if (xor[x/8] & bit) {
                    dest[0] = 0xff;
                    dest[1] = 0xff;
                    dest[2] = 0xff;
                    dest[3] = 0xff;
                } else {
                    /* unchanged -> transparent */
                    dest[0] = 0x00;
                    dest[1] = 0x00;
                    dest[2] = 0x00;
                    dest[3] = 0x00;
                }
            } else {
                if (xor[x/8] & bit) {
                    /* set -> white */
                    dest[0] = 0xff;
                    dest[1] = 0xff;
                    dest[2] = 0xff;
                    dest[3] = 0xff;
                } else {
                    /* clear -> black */
                    dest[0] = 0x00;
                    dest[1] = 0x00;
                    dest[2] = 0x00;
                    dest[3] = 0xff;
                }
            }
        next_bit:
            bit >>= 1;
            if (bit == 0) {
                bit = 0x80;
            }
        }
        and += bpl;
        xor += bpl;
    }
}
