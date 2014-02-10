/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#include "spice-client.h"
#include "spice-common.h"
#include "glib-compat.h"

#include "spice-channel-priv.h"
#include "spice-session-priv.h"
#include "spice-marshal.h"
#include "bio-gsocket.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h> // TCP_NODELAY
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <ctype.h>

#include "gio-coroutine.h"

static void spice_channel_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg);
static void spice_channel_write_msg(SpiceChannel *channel, SpiceMsgOut *out);
static void spice_channel_send_link(SpiceChannel *channel);
static void channel_disconnect(SpiceChannel *channel);
static void channel_reset(SpiceChannel *channel, gboolean migrating);
static void spice_channel_reset_capabilities(SpiceChannel *channel);
static void spice_channel_send_migration_handshake(SpiceChannel *channel);

/**
 * SECTION:spice-channel
 * @short_description: the base channel class
 * @title: Spice Channel
 * @section_id:
 * @see_also: #SpiceSession, #SpiceMainChannel and other channels
 * @stability: Stable
 * @include: spice-channel.h
 *
 * #SpiceChannel is the base class for the different kind of Spice
 * channel connections, such as #SpiceMainChannel, or
 * #SpiceInputsChannel.
 */

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_CHANNEL, SpiceChannelPrivate))

G_DEFINE_TYPE(SpiceChannel, spice_channel, G_TYPE_OBJECT);

/* Properties */
enum {
    PROP_0,
    PROP_SESSION,
    PROP_CHANNEL_TYPE,
    PROP_CHANNEL_ID,
    PROP_TOTAL_READ_BYTES,
};

/* Signals */
enum {
    SPICE_CHANNEL_EVENT,
    SPICE_CHANNEL_OPEN_FD,

    SPICE_CHANNEL_LAST_SIGNAL,
};

static guint signals[SPICE_CHANNEL_LAST_SIGNAL];

static void spice_channel_iterate_write(SpiceChannel *channel);
static void spice_channel_iterate_read(SpiceChannel *channel);

static void spice_channel_init(SpiceChannel *channel)
{
    SpiceChannelPrivate *c;

    c = channel->priv = SPICE_CHANNEL_GET_PRIVATE(channel);

    c->out_serial = 1;
    c->in_serial = 1;
    c->fd = -1;
    strcpy(c->name, "?");
    c->caps = g_array_new(FALSE, TRUE, sizeof(guint32));
    c->common_caps = g_array_new(FALSE, TRUE, sizeof(guint32));
    c->remote_caps = g_array_new(FALSE, TRUE, sizeof(guint32));
    c->remote_common_caps = g_array_new(FALSE, TRUE, sizeof(guint32));
    spice_channel_set_common_capability(channel, SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION);
    spice_channel_set_common_capability(channel, SPICE_COMMON_CAP_MINI_HEADER);
    g_queue_init(&c->xmit_queue);
    STATIC_MUTEX_INIT(c->xmit_queue_lock);
}

static void spice_channel_constructed(GObject *gobject)
{
    SpiceChannel *channel = SPICE_CHANNEL(gobject);
    SpiceChannelPrivate *c = channel->priv;
    const char *desc = spice_channel_type_to_string(c->channel_type);

    snprintf(c->name, sizeof(c->name), "%s-%d:%d",
             desc, c->channel_type, c->channel_id);
    CHANNEL_DEBUG(channel, "%s", __FUNCTION__);

    const char *disabled  = g_getenv("SPICE_DISABLE_CHANNELS");
    if (disabled && strstr(disabled, desc))
        c->disable_channel_msg = TRUE;

    spice_session_channel_new(c->session, channel);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_channel_parent_class)->constructed)
        G_OBJECT_CLASS(spice_channel_parent_class)->constructed(gobject);
}

static void spice_channel_dispose(GObject *gobject)
{
    SpiceChannel *channel = SPICE_CHANNEL(gobject);
    SpiceChannelPrivate *c = channel->priv;

    CHANNEL_DEBUG(channel, "%s %p", __FUNCTION__, gobject);

    if (c->session)
        spice_session_channel_destroy(c->session, channel);

    spice_channel_disconnect(channel, SPICE_CHANNEL_CLOSED);

    if (c->session) {
         g_object_unref(c->session);
         c->session = NULL;
    }

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_channel_parent_class)->dispose)
        G_OBJECT_CLASS(spice_channel_parent_class)->dispose(gobject);
}

static void spice_channel_finalize(GObject *gobject)
{
    SpiceChannel *channel = SPICE_CHANNEL(gobject);
    SpiceChannelPrivate *c = channel->priv;

    CHANNEL_DEBUG(channel, "%s %p", __FUNCTION__, gobject);

    g_idle_remove_by_data(gobject);

    STATIC_MUTEX_CLEAR(c->xmit_queue_lock);

    if (c->caps)
        g_array_free(c->caps, TRUE);

    if (c->common_caps)
        g_array_free(c->common_caps, TRUE);

    if (c->remote_caps)
        g_array_free(c->remote_caps, TRUE);

    if (c->remote_common_caps)
        g_array_free(c->remote_common_caps, TRUE);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_channel_parent_class)->finalize(gobject);
}

static void spice_channel_get_property(GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    SpiceChannel *channel = SPICE_CHANNEL(gobject);
    SpiceChannelPrivate *c = channel->priv;

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, c->session);
        break;
    case PROP_CHANNEL_TYPE:
        g_value_set_int(value, c->channel_type);
        break;
    case PROP_CHANNEL_ID:
        g_value_set_int(value, c->channel_id);
        break;
    case PROP_TOTAL_READ_BYTES:
        g_value_set_ulong(value, c->total_read_bytes);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

G_GNUC_INTERNAL
gint spice_channel_get_channel_id(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    g_return_val_if_fail(c != NULL, 0);
    return c->channel_id;
}

G_GNUC_INTERNAL
gint spice_channel_get_channel_type(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    g_return_val_if_fail(c != NULL, 0);
    return c->channel_type;
}

static void spice_channel_set_property(GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    SpiceChannel *channel = SPICE_CHANNEL(gobject);
    SpiceChannelPrivate *c = channel->priv;

    switch (prop_id) {
    case PROP_SESSION:
        c->session = g_value_dup_object(value);
        break;
    case PROP_CHANNEL_TYPE:
        c->channel_type = g_value_get_int(value);
        break;
    case PROP_CHANNEL_ID:
        c->channel_id = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_channel_class_init(SpiceChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    klass->iterate_write = spice_channel_iterate_write;
    klass->iterate_read  = spice_channel_iterate_read;
    klass->channel_disconnect = channel_disconnect;
    klass->channel_reset = channel_reset;

    gobject_class->constructed  = spice_channel_constructed;
    gobject_class->dispose      = spice_channel_dispose;
    gobject_class->finalize     = spice_channel_finalize;
    gobject_class->get_property = spice_channel_get_property;
    gobject_class->set_property = spice_channel_set_property;
    klass->handle_msg           = spice_channel_handle_msg;

    g_object_class_install_property
        (gobject_class, PROP_SESSION,
         g_param_spec_object("spice-session",
                             "Spice session",
                             "",
                             SPICE_TYPE_SESSION,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_CHANNEL_TYPE,
         g_param_spec_int("channel-type",
                          "Channel type",
                          "",
                          -1, INT_MAX, -1,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_CHANNEL_ID,
         g_param_spec_int("channel-id",
                          "Channel ID",
                          "",
                          -1, INT_MAX, -1,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

    g_object_class_install_property
        (gobject_class, PROP_TOTAL_READ_BYTES,
         g_param_spec_ulong("total-read-bytes",
                            "Total read bytes",
                            "",
                            0, G_MAXULONG, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_STRINGS));

    /**
     * SpiceChannel::channel-event:
     * @channel: the channel that emitted the signal
     * @event: a #SpiceChannelEvent
     *
     * The #SpiceChannel::channel-event signal is emitted when the
     * state of the connection change.
     **/
    signals[SPICE_CHANNEL_EVENT] =
        g_signal_new("channel-event",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceChannelClass, channel_event),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_CHANNEL_EVENT);

    /**
     * SpiceChannel::open-fd:
     * @channel: the channel that emitted the signal
     * @with_tls: wether TLS connection is requested
     *
     * The #SpiceChannel::open-fd signal is emitted when a new
     * connection is requested. This signal is emitted when the
     * connection is made with spice_session_open_fd().
     **/
    signals[SPICE_CHANNEL_OPEN_FD] =
        g_signal_new("open-fd",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceChannelClass, open_fd),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);

    g_type_class_add_private(klass, sizeof(SpiceChannelPrivate));

    SSL_library_init();
    SSL_load_error_strings();
}

/* ---------------------------------------------------------------- */
/* private header api                                               */

static inline void spice_header_set_msg_type(uint8_t *header, gboolean is_mini_header,
                                             uint16_t type)
{
    if (is_mini_header) {
        ((SpiceMiniDataHeader *)header)->type = type;
    } else {
        ((SpiceDataHeader *)header)->type = type;
    }
}

static inline void spice_header_set_msg_size(uint8_t *header, gboolean is_mini_header,
                                             uint32_t size)
{
    if (is_mini_header) {
        ((SpiceMiniDataHeader *)header)->size = size;
    } else {
        ((SpiceDataHeader *)header)->size = size;
    }
}

G_GNUC_INTERNAL
uint16_t spice_header_get_msg_type(uint8_t *header, gboolean is_mini_header)
{
    if (is_mini_header) {
        return ((SpiceMiniDataHeader *)header)->type;
    } else {
        return ((SpiceDataHeader *)header)->type;
    }
}

G_GNUC_INTERNAL
uint32_t spice_header_get_msg_size(uint8_t *header, gboolean is_mini_header)
{
    if (is_mini_header) {
        return ((SpiceMiniDataHeader *)header)->size;
    } else {
        return ((SpiceDataHeader *)header)->size;
    }
}

static inline int spice_header_get_header_size(gboolean is_mini_header)
{
    return is_mini_header ? sizeof(SpiceMiniDataHeader) : sizeof(SpiceDataHeader);
}

static inline void spice_header_set_msg_serial(uint8_t *header, gboolean is_mini_header,
                                               uint64_t serial)
{
    if (!is_mini_header) {
        ((SpiceDataHeader *)header)->serial = serial;
    }
}

static inline void spice_header_reset_msg_sub_list(uint8_t *header, gboolean is_mini_header)
{
    if (!is_mini_header) {
        ((SpiceDataHeader *)header)->sub_list = 0;
    }
}

static inline uint64_t spice_header_get_in_msg_serial(SpiceMsgIn *in)
{
    SpiceChannelPrivate *c = in->channel->priv;
    uint8_t *header = in->header;

    if (c->use_mini_header) {
        return c->in_serial;
    } else {
        return ((SpiceDataHeader *)header)->serial;
    }
}

static inline uint64_t spice_header_get_out_msg_serial(SpiceMsgOut *out)
{
    SpiceChannelPrivate *c = out->channel->priv;

    if (c->use_mini_header) {
        return c->out_serial;
    } else {
        return ((SpiceDataHeader *)out->header)->serial;
    }
}

static inline uint32_t spice_header_get_msg_sub_list(uint8_t *header, gboolean is_mini_header)
{
    if (is_mini_header) {
        return 0;
    } else {
        return ((SpiceDataHeader *)header)->sub_list;
    }
}

/* ---------------------------------------------------------------- */
/* private msg api                                                  */

G_GNUC_INTERNAL
SpiceMsgIn *spice_msg_in_new(SpiceChannel *channel)
{
    SpiceMsgIn *in;

    g_return_val_if_fail(channel != NULL, NULL);

    in = g_slice_new0(SpiceMsgIn);
    in->refcount = 1;
    in->channel  = channel;

    return in;
}

G_GNUC_INTERNAL
SpiceMsgIn *spice_msg_in_sub_new(SpiceChannel *channel, SpiceMsgIn *parent,
                                   SpiceSubMessage *sub)
{
    SpiceMsgIn *in;

    g_return_val_if_fail(channel != NULL, NULL);

    in = spice_msg_in_new(channel);
    spice_header_set_msg_type(in->header, channel->priv->use_mini_header, sub->type);
    spice_header_set_msg_size(in->header, channel->priv->use_mini_header, sub->size);
    in->data = (uint8_t*)(sub+1);
    in->dpos = sub->size;
    in->parent = parent;
    spice_msg_in_ref(parent);
    return in;
}

G_GNUC_INTERNAL
void spice_msg_in_ref(SpiceMsgIn *in)
{
    g_return_if_fail(in != NULL);

    in->refcount++;
}

G_GNUC_INTERNAL
void spice_msg_in_unref(SpiceMsgIn *in)
{
    g_return_if_fail(in != NULL);

    in->refcount--;
    if (in->refcount > 0)
        return;
    if (in->parsed)
        in->pfree(in->parsed);
    if (in->parent) {
        spice_msg_in_unref(in->parent);
    } else {
        free(in->data);
    }
    g_slice_free(SpiceMsgIn, in);
}

G_GNUC_INTERNAL
int spice_msg_in_type(SpiceMsgIn *in)
{
    g_return_val_if_fail(in != NULL, -1);

    return spice_header_get_msg_type(in->header, in->channel->priv->use_mini_header);
}

G_GNUC_INTERNAL
void *spice_msg_in_parsed(SpiceMsgIn *in)
{
    g_return_val_if_fail(in != NULL, NULL);

    return in->parsed;
}

G_GNUC_INTERNAL
void *spice_msg_in_raw(SpiceMsgIn *in, int *len)
{
    g_return_val_if_fail(in != NULL, NULL);
    g_return_val_if_fail(len != NULL, NULL);

    *len = in->dpos;
    return in->data;
}

static void hexdump(const char *prefix, unsigned char *data, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (i % 16 == 0)
            fprintf(stderr, "%s:", prefix);
        if (i % 4 == 0)
            fprintf(stderr, " ");
        fprintf(stderr, " %02x", data[i]);
        if (i % 16 == 15)
            fprintf(stderr, "\n");
    }
    if (i % 16 != 0)
        fprintf(stderr, "\n");
}

G_GNUC_INTERNAL
void spice_msg_in_hexdump(SpiceMsgIn *in)
{
    SpiceChannelPrivate *c = in->channel->priv;

    fprintf(stderr, "--\n<< hdr: %s serial %" PRIu64 " type %d size %d sub-list %d\n",
            c->name, spice_header_get_in_msg_serial(in),
            spice_header_get_msg_type(in->header, c->use_mini_header),
            spice_header_get_msg_size(in->header, c->use_mini_header),
            spice_header_get_msg_sub_list(in->header, c->use_mini_header));
    hexdump("<< msg", in->data, in->dpos);
}

G_GNUC_INTERNAL
void spice_msg_out_hexdump(SpiceMsgOut *out, unsigned char *data, int len)
{
    SpiceChannelPrivate *c = out->channel->priv;

    fprintf(stderr, "--\n>> hdr: %s serial %" PRIu64 " type %d size %d sub-list %d\n",
            c->name,
            spice_header_get_out_msg_serial(out),
            spice_header_get_msg_type(out->header, c->use_mini_header),
            spice_header_get_msg_size(out->header, c->use_mini_header),
            spice_header_get_msg_sub_list(out->header, c->use_mini_header));
    hexdump(">> msg", data, len);
}

static gboolean msg_check_read_only (int channel_type, int msg_type)
{
    if (msg_type < 100) // those are the common messages
        return FALSE;

    switch (channel_type) {
    /* messages allowed to be sent in read-only mode */
    case SPICE_CHANNEL_MAIN:
        switch (msg_type) {
        case SPICE_MSGC_MAIN_CLIENT_INFO:
        case SPICE_MSGC_MAIN_MIGRATE_CONNECTED:
        case SPICE_MSGC_MAIN_MIGRATE_CONNECT_ERROR:
        case SPICE_MSGC_MAIN_ATTACH_CHANNELS:
        case SPICE_MSGC_MAIN_MIGRATE_END:
            return FALSE;
        }
        break;
    case SPICE_CHANNEL_DISPLAY:
        return FALSE;
    }

    return TRUE;
}

G_GNUC_INTERNAL
SpiceMsgOut *spice_msg_out_new(SpiceChannel *channel, int type)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgOut *out;

    g_return_val_if_fail(c != NULL, NULL);

    out = g_slice_new0(SpiceMsgOut);
    out->refcount = 1;
    out->channel  = channel;
    out->ro_check = msg_check_read_only(c->channel_type, type);

    out->marshallers = c->marshallers;
    out->marshaller = spice_marshaller_new();

    out->header = spice_marshaller_reserve_space(out->marshaller,
                                                 spice_header_get_header_size(c->use_mini_header));
    spice_marshaller_set_base(out->marshaller, spice_header_get_header_size(c->use_mini_header));
    spice_header_set_msg_type(out->header, c->use_mini_header, type);
    spice_header_set_msg_serial(out->header, c->use_mini_header, c->out_serial);
    spice_header_reset_msg_sub_list(out->header, c->use_mini_header);

    c->out_serial++;
    return out;
}

G_GNUC_INTERNAL
void spice_msg_out_ref(SpiceMsgOut *out)
{
    g_return_if_fail(out != NULL);

    out->refcount++;
}

G_GNUC_INTERNAL
void spice_msg_out_unref(SpiceMsgOut *out)
{
    g_return_if_fail(out != NULL);

    out->refcount--;
    if (out->refcount > 0)
        return;
    spice_marshaller_destroy(out->marshaller);
    g_slice_free(SpiceMsgOut, out);
}

/* system context */
static gboolean spice_channel_idle_wakeup(gpointer user_data)
{
    SpiceChannel *channel = SPICE_CHANNEL(user_data);
    SpiceChannelPrivate *c = channel->priv;

    /*
     * Note:
     *
     * - This must be done before the wakeup as that may eventually
     *   call channel_reset() which checks this.
     * - The lock calls are really necessary, this fixes the following race:
     *   1) usb-event-thread calls spice_msg_out_send()
     *   2) spice_msg_out_send calls g_timeout_add_full(...)
     *   3) we run, set xmit_queue_wakeup_id to 0
     *   4) spice_msg_out_send stores the result of g_timeout_add_full() in
     *      xmit_queue_wakeup_id, overwriting the 0 we just stored
     *   5) xmit_queue_wakeup_id now says there is a wakeup pending which is
     *      false
     */
    STATIC_MUTEX_LOCK(c->xmit_queue_lock);
    c->xmit_queue_wakeup_id = 0;
    STATIC_MUTEX_UNLOCK(c->xmit_queue_lock);

    spice_channel_wakeup(channel, FALSE);

    return FALSE;
}

/* any context (system/co-routine/usb-event-thread) */
G_GNUC_INTERNAL
void spice_msg_out_send(SpiceMsgOut *out)
{
    SpiceChannelPrivate *c;
    gboolean was_empty;

    g_return_if_fail(out != NULL);
    g_return_if_fail(out->channel != NULL);
    c = out->channel->priv;

    STATIC_MUTEX_LOCK(c->xmit_queue_lock);
    if (c->xmit_queue_blocked) {
        g_warning("message queue is blocked, dropping message");
        goto end;
    }

    was_empty = g_queue_is_empty(&c->xmit_queue);
    g_queue_push_tail(&c->xmit_queue, out);

    /* One wakeup is enough to empty the entire queue -> only do a wakeup
       if the queue was empty, and there isn't one pending already. */
    if (was_empty && !c->xmit_queue_wakeup_id) {
        c->xmit_queue_wakeup_id =
            /* Use g_timeout_add_full so that can specify the priority */
            g_timeout_add_full(G_PRIORITY_HIGH, 0,
                               spice_channel_idle_wakeup,
                               out->channel, NULL);
    }

end:
    STATIC_MUTEX_UNLOCK(c->xmit_queue_lock);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_msg_out_send_internal(SpiceMsgOut *out)
{
    g_return_if_fail(out != NULL);

    spice_channel_write_msg(out->channel, out);
}

/* ---------------------------------------------------------------- */

struct SPICE_CHANNEL_EVENT {
    SpiceChannelEvent event;
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_CHANNEL_EVENT: {
        struct SPICE_CHANNEL_EVENT *p = params;
        g_signal_emit(object, signals[signum], 0, p->event);
        break;
    }
    case SPICE_CHANNEL_OPEN_FD:
        g_warning("this signal is only sent directly from main context");
        break;
    default:
        g_warn_if_reached();
    }
}

/*
 * Write all 'data' of length 'datalen' bytes out to
 * the wire
 */
/* coroutine context */
static void spice_channel_flush_wire(SpiceChannel *channel,
                                     const void *data,
                                     size_t datalen)
{
    SpiceChannelPrivate *c = channel->priv;
    const char *ptr = data;
    size_t offset = 0;
    GIOCondition cond;

    while (offset < datalen) {
        int ret;

        if (c->has_error) return;

        cond = 0;
        if (c->tls) {
            ret = SSL_write(c->ssl, ptr+offset, datalen-offset);
            if (ret < 0) {
                ret = SSL_get_error(c->ssl, ret);
                if (ret == SSL_ERROR_WANT_READ)
                    cond |= G_IO_IN;
                if (ret == SSL_ERROR_WANT_WRITE)
                    cond |= G_IO_OUT;
                ret = -1;
            }
        } else {
            GError *error = NULL;
            ret = g_socket_send(c->sock, ptr+offset, datalen-offset,
                                NULL, &error);
            if (ret < 0) {
                if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
                    cond = G_IO_OUT;
                } else {
                    CHANNEL_DEBUG(channel, "Send error %s", error->message);
                }
                g_clear_error(&error);
                ret = -1;
            }
        }
        if (ret == -1) {
            if (cond != 0) {
                g_coroutine_socket_wait(&c->coroutine, c->sock, cond);
                continue;
            } else {
                CHANNEL_DEBUG(channel, "Closing the channel: spice_channel_flush %d", errno);
                c->has_error = TRUE;
                return;
            }
        }
        if (ret == 0) {
            CHANNEL_DEBUG(channel, "Closing the connection: spice_channel_flush");
            c->has_error = TRUE;
            return;
        }
        offset += ret;
    }
}

#if HAVE_SASL
/*
 * Encode all buffered data, write all encrypted data out
 * to the wire
 */
static void spice_channel_flush_sasl(SpiceChannel *channel, const void *data, size_t len)
{
    SpiceChannelPrivate *c = channel->priv;
    const char *output;
    unsigned int outputlen;
    int err;

    err = sasl_encode(c->sasl_conn, data, len, &output, &outputlen);
    if (err != SASL_OK) {
        g_warning ("Failed to encode SASL data %s",
                   sasl_errstring(err, NULL, NULL));
        c->has_error = TRUE;
        return;
    }

    //CHANNEL_DEBUG(channel, "Flush SASL %d: %p %d", len, output, outputlen);
    spice_channel_flush_wire(channel, output, outputlen);
}
#endif

/* coroutine context */
static void spice_channel_write(SpiceChannel *channel, const void *data, size_t len)
{
#if HAVE_SASL
    SpiceChannelPrivate *c = channel->priv;

    if (c->sasl_conn)
        spice_channel_flush_sasl(channel, data, len);
    else
#endif
        spice_channel_flush_wire(channel, data, len);
}

/* coroutine context */
static void spice_channel_write_msg(SpiceChannel *channel, SpiceMsgOut *out)
{
    uint8_t *data;
    int free_data;
    size_t len;
    uint32_t msg_size;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(out != NULL);
    g_return_if_fail(channel == out->channel);

    if (out->ro_check &&
        spice_channel_get_read_only(channel)) {
        g_warning("Try to send message while read-only. Please report a bug.");
        return;
    }

    msg_size = spice_marshaller_get_total_size(out->marshaller) -
               spice_header_get_header_size(channel->priv->use_mini_header);
    spice_header_set_msg_size(out->header, channel->priv->use_mini_header, msg_size);
    data = spice_marshaller_linearize(out->marshaller, 0, &len, &free_data);
    /* spice_msg_out_hexdump(out, data, len); */
    spice_channel_write(channel, data, len);

    if (free_data)
        free(data);

    spice_msg_out_unref(out);
}

/*
 * Read at least 1 more byte of data straight off the wire
 * into the requested buffer.
 */
/* coroutine context */
static int spice_channel_read_wire(SpiceChannel *channel, void *data, size_t len)
{
    SpiceChannelPrivate *c = channel->priv;
    int ret;
    GIOCondition cond;

reread:

    if (c->has_error) return 0; /* has_error is set by disconnect(), return no error */

    cond = 0;
    if (c->tls) {
        ret = SSL_read(c->ssl, data, len);
        if (ret < 0) {
            ret = SSL_get_error(c->ssl, ret);
            if (ret == SSL_ERROR_WANT_READ)
                cond |= G_IO_IN;
            if (ret == SSL_ERROR_WANT_WRITE)
                cond |= G_IO_OUT;
            ret = -1;
        }
    } else {
        GError *error = NULL;
        ret = g_socket_receive(c->sock, data, len, NULL, &error);
        if (ret < 0) {
            if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
                cond = G_IO_IN;
            } else {
                CHANNEL_DEBUG(channel, "Read error %s", error->message);
            }
            g_clear_error(&error);
            ret = -1;
        }
    }

    if (ret == -1) {
        if (cond != 0) {
            g_coroutine_socket_wait(&c->coroutine, c->sock, cond);
            goto reread;
        } else {
            c->has_error = TRUE;
            return -errno;
        }
    }
    if (ret == 0) {
        CHANNEL_DEBUG(channel, "Closing the connection: spice_channel_read() - ret=0");
        c->has_error = TRUE;
        return 0;
    }

    return ret;
}

#if HAVE_SASL
/*
 * Read at least 1 more byte of data out of the SASL decrypted
 * data buffer, into the internal read buffer
 */
static int spice_channel_read_sasl(SpiceChannel *channel, void *data, size_t len)
{
    SpiceChannelPrivate *c = channel->priv;

    /* CHANNEL_DEBUG(channel, "Read %lu SASL %p size %d offset %d", len, c->sasl_decoded, */
    /*             c->sasl_decoded_length, c->sasl_decoded_offset); */

    if (c->sasl_decoded == NULL || c->sasl_decoded_length == 0) {
        char encoded[8192]; /* should stay lower than maxbufsize */
        int err, ret;

        g_warn_if_fail(c->sasl_decoded_offset == 0);

        ret = spice_channel_read_wire(channel, encoded, sizeof(encoded));
        if (ret < 0)
            return ret;

        err = sasl_decode(c->sasl_conn, encoded, ret,
                          &c->sasl_decoded, &c->sasl_decoded_length);
        if (err != SASL_OK) {
            g_warning("Failed to decode SASL data %s",
                      sasl_errstring(err, NULL, NULL));
            c->has_error = TRUE;
            return -EINVAL;
        }
        c->sasl_decoded_offset = 0;
    }

    if (c->sasl_decoded_length == 0)
        return 0;

    len = MIN(c->sasl_decoded_length - c->sasl_decoded_offset, len);
    memcpy(data, c->sasl_decoded + c->sasl_decoded_offset, len);
    c->sasl_decoded_offset += len;

    if (c->sasl_decoded_offset == c->sasl_decoded_length) {
        c->sasl_decoded_length = c->sasl_decoded_offset = 0;
        c->sasl_decoded = NULL;
    }

    return len;
}
#endif

/*
 * Fill the 'data' buffer up with exactly 'len' bytes worth of data
 */
/* coroutine context */
static int spice_channel_read(SpiceChannel *channel, void *data, size_t length)
{
    SpiceChannelPrivate *c = channel->priv;
    gsize len = length;
    int ret;

    while (len > 0) {
        if (c->has_error) return 0; /* has_error is set by disconnect(), return no error */

#if HAVE_SASL
        if (c->sasl_conn)
            ret = spice_channel_read_sasl(channel, data, len);
        else
#endif
            ret = spice_channel_read_wire(channel, data, len);
        if (ret < 0)
            return ret;
        g_assert(ret <= len);
        len -= ret;
        data = ((char*)data) + ret;
#if DEBUG
        if (len > 0)
            CHANNEL_DEBUG(channel, "still needs %" G_GSIZE_FORMAT, len);
#endif
    }
    c->total_read_bytes += length;

    return length;
}

/* coroutine context */
static void spice_channel_send_spice_ticket(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    EVP_PKEY *pubkey;
    int nRSASize;
    BIO *bioKey;
    RSA *rsa;
    char *password;
    uint8_t *encrypted;
    int rc;

    bioKey = BIO_new(BIO_s_mem());
    g_return_if_fail(bioKey != NULL);

    BIO_write(bioKey, c->peer_msg->pub_key, SPICE_TICKET_PUBKEY_BYTES);
    pubkey = d2i_PUBKEY_bio(bioKey, NULL);
    g_return_if_fail(pubkey != NULL);

    rsa = pubkey->pkey.rsa;
    nRSASize = RSA_size(rsa);

    encrypted = g_alloca(nRSASize);
    /*
      The use of RSA encryption limit the potential maximum password length.
      for RSA_PKCS1_OAEP_PADDING it is RSA_size(rsa) - 41.
    */
    g_object_get(c->session, "password", &password, NULL);
    if (password == NULL)
        password = g_strdup("");
    rc = RSA_public_encrypt(strlen(password) + 1, (uint8_t*)password,
                            encrypted, rsa, RSA_PKCS1_OAEP_PADDING);
    g_warn_if_fail(rc > 0);

    spice_channel_write(channel, encrypted, nRSASize);
    memset(encrypted, 0, nRSASize);
    EVP_PKEY_free(pubkey);
    BIO_free(bioKey);
    g_free(password);
}

/* coroutine context */
static void spice_channel_recv_auth(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    uint32_t link_res;
    int rc;

    rc = spice_channel_read(channel, &link_res, sizeof(link_res));
    if (rc != sizeof(link_res)) {
        CHANNEL_DEBUG(channel, "incomplete auth reply (%d/%" G_GSIZE_FORMAT ")",
                    rc, sizeof(link_res));
        emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_LINK);
        return;
    }

    if (link_res != SPICE_LINK_ERR_OK) {
        CHANNEL_DEBUG(channel, "link result: reply %d", link_res);
        emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_AUTH);
        return;
    }

    c->state = SPICE_CHANNEL_STATE_READY;

    emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_OPENED);

    if (c->state == SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE) {
        spice_channel_send_migration_handshake(channel);
    }

    if (c->state != SPICE_CHANNEL_STATE_MIGRATING)
        spice_channel_up(channel);
}

G_GNUC_INTERNAL
void spice_channel_up(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;

    CHANNEL_DEBUG(channel, "channel up, state %d", c->state);

    if (SPICE_CHANNEL_GET_CLASS(channel)->channel_up)
        SPICE_CHANNEL_GET_CLASS(channel)->channel_up(channel);
}

/* coroutine context */
static void spice_channel_send_link(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    uint8_t *buffer, *p;
    int protocol, i;

    c->link_hdr.magic = SPICE_MAGIC;
    c->link_hdr.size = sizeof(c->link_msg);

    g_object_get(c->session, "protocol", &protocol, NULL);
    switch (protocol) {
    case 1: /* protocol 1 == major 1, old 0.4 protocol, last active minor */
        c->link_hdr.major_version = 1;
        c->link_hdr.minor_version = 3;
        c->parser = spice_get_server_channel_parser1(c->channel_type, NULL);
        c->marshallers = spice_message_marshallers_get1();
        break;
    case SPICE_VERSION_MAJOR: /* protocol 2 == current */
        c->link_hdr.major_version = SPICE_VERSION_MAJOR;
        c->link_hdr.minor_version = SPICE_VERSION_MINOR;
        c->parser = spice_get_server_channel_parser(c->channel_type, NULL);
        c->marshallers = spice_message_marshallers_get();
        break;
    default:
        g_critical("unknown major %d", protocol);
        return;
    }

    c->link_msg.connection_id = spice_session_get_connection_id(c->session);
    c->link_msg.channel_type  = c->channel_type;
    c->link_msg.channel_id    = c->channel_id;
    c->link_msg.caps_offset   = sizeof(c->link_msg);

    c->link_msg.num_common_caps = c->common_caps->len;
    c->link_msg.num_channel_caps = c->caps->len;
    c->link_hdr.size += (c->link_msg.num_common_caps +
                         c->link_msg.num_channel_caps) * sizeof(uint32_t);

    buffer = spice_malloc(sizeof(c->link_hdr) + c->link_hdr.size);
    p = buffer;

    memcpy(p, &c->link_hdr, sizeof(c->link_hdr)); p += sizeof(c->link_hdr);
    memcpy(p, &c->link_msg, sizeof(c->link_msg)); p += sizeof(c->link_msg);

    for (i = 0; i < c->common_caps->len; i++) {
        *(uint32_t *)p = g_array_index(c->common_caps, uint32_t, i);
        p += sizeof(uint32_t);
    }
    for (i = 0; i < c->caps->len; i++) {
        *(uint32_t *)p = g_array_index(c->caps, uint32_t, i);
        p += sizeof(uint32_t);
    }
    CHANNEL_DEBUG(channel, "channel type %d id %d num common caps %d num caps %d",
                  c->link_msg.channel_type,
                  c->link_msg.channel_id,
                  c->link_msg.num_common_caps,
                  c->link_msg.num_channel_caps);
    spice_channel_write(channel, buffer, p - buffer);
    free(buffer);
}

/* coroutine context */
static gboolean spice_channel_recv_link_hdr(SpiceChannel *channel, gboolean *switch_protocol)
{
    SpiceChannelPrivate *c = channel->priv;
    int rc;

    *switch_protocol = FALSE;
    rc = spice_channel_read(channel, &c->peer_hdr, sizeof(c->peer_hdr));
    if (rc != sizeof(c->peer_hdr)) {
        g_warning("incomplete link header (%d/%" G_GSIZE_FORMAT ")",
                  rc, sizeof(c->peer_hdr));
        goto error;
    }
    if (c->peer_hdr.magic != SPICE_MAGIC) {
        g_warning("invalid SPICE_MAGIC!");
        goto error;
    }

    CHANNEL_DEBUG(channel, "Peer version: %d:%d", c->peer_hdr.major_version, c->peer_hdr.minor_version);
    if (c->peer_hdr.major_version != c->link_hdr.major_version) {
        g_warning("major mismatch (got %d, expected %d)",
                  c->peer_hdr.major_version, c->link_hdr.major_version);
        goto error;
    }

    c->peer_msg = spice_malloc(c->peer_hdr.size);
    if (c->peer_msg == NULL) {
        g_warning("invalid peer header size: %u", c->peer_hdr.size);
        goto error;
    }

    return TRUE;

error:
    /* Windows socket seems to give early CONNRESET errors. The server
       does not linger when closing the socket if the protocol is
       incompatible. Try with the oldest protocol in this case: */
    if (c->link_hdr.major_version != 1) {
        SPICE_DEBUG("%s: error, switching to protocol 1 (spice 0.4)", c->name);
        *switch_protocol = TRUE;
        g_object_set(c->session, "protocol", 1, NULL);
        return FALSE;
    }

    emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_LINK);
    return FALSE;
}

#if HAVE_SASL
/*
 * NB, keep in sync with similar method in spice/server/reds.c
 */
static gchar *addr_to_string(GSocketAddress *addr)
{
    GInetSocketAddress *iaddr = G_INET_SOCKET_ADDRESS(addr);
    guint16 port;
    GInetAddress *host;
    gchar *hoststr;
    gchar *ret;

    host = g_inet_socket_address_get_address(iaddr);
    port = g_inet_socket_address_get_port(iaddr);
    hoststr = g_inet_address_to_string(host);

    ret = g_strdup_printf("%s;%hu", hoststr, port);
    g_free(hoststr);

    return ret;
}

static gboolean
spice_channel_gather_sasl_credentials(SpiceChannel *channel,
				       sasl_interact_t *interact)
{
    SpiceChannelPrivate *c;
    int ninteract;

    g_return_val_if_fail(channel != NULL, FALSE);
    g_return_val_if_fail(channel->priv != NULL, FALSE);

    c = channel->priv;

    /* FIXME: we could keep connection open and ask connection details if missing */

    for (ninteract = 0 ; interact[ninteract].id != 0 ; ninteract++) {
        switch (interact[ninteract].id) {
        case SASL_CB_AUTHNAME:
        case SASL_CB_USER:
            g_warn_if_reached();
            break;

        case SASL_CB_PASS:
            if (spice_session_get_password(c->session) == NULL)
                return FALSE;

            interact[ninteract].result =  spice_session_get_password(c->session);
            interact[ninteract].len = strlen(interact[ninteract].result);
            break;
        }
    }

    CHANNEL_DEBUG(channel, "Filled SASL interact");

    return TRUE;
}

/*
 *
 * Init msg from server
 *
 *  u32 mechlist-length
 *  u8-array mechlist-string
 *
 * Start msg to server
 *
 *  u32 mechname-length
 *  u8-array mechname-string
 *  u32 clientout-length
 *  u8-array clientout-string
 *
 * Start msg from server
 *
 *  u32 serverin-length
 *  u8-array serverin-string
 *  u8 continue
 *
 * Step msg to server
 *
 *  u32 clientout-length
 *  u8-array clientout-string
 *
 * Step msg from server
 *
 *  u32 serverin-length
 *  u8-array serverin-string
 *  u8 continue
 */

#define SASL_MAX_MECHLIST_LEN 300
#define SASL_MAX_MECHNAME_LEN 100
#define SASL_MAX_DATA_LEN (1024 * 1024)

/* Perform the SASL authentication process
 */
static gboolean spice_channel_perform_auth_sasl(SpiceChannel *channel)
{
    SpiceChannelPrivate *c;
    sasl_conn_t *saslconn = NULL;
    sasl_security_properties_t secprops;
    const char *clientout;
    char *serverin = NULL;
    unsigned int clientoutlen;
    int err;
    char *localAddr = NULL, *remoteAddr = NULL;
    const void *val;
    sasl_ssf_t ssf;
    static const sasl_callback_t saslcb[] = {
        { .id = SASL_CB_PASS },
        { .id = 0 },
    };
    sasl_interact_t *interact = NULL;
    guint32 len;
    char *mechlist;
    const char *mechname;
    gboolean ret = FALSE;
    GSocketAddress *addr = NULL;
    guint8 complete;

    g_return_val_if_fail(channel != NULL, FALSE);
    g_return_val_if_fail(channel->priv != NULL, FALSE);

    c = channel->priv;

    /* Sets up the SASL library as a whole */
    err = sasl_client_init(NULL);
    CHANNEL_DEBUG(channel, "Client initialize SASL authentication %d", err);
    if (err != SASL_OK) {
        g_critical("failed to initialize SASL library: %d (%s)",
                   err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    /* Get local address in form  IPADDR:PORT */
    addr = g_socket_get_local_address(c->sock, NULL);
    if (!addr) {
        g_critical("failed to get local address");
        goto error;
    }
    if ((g_socket_address_get_family(addr) == G_SOCKET_FAMILY_IPV4 ||
         g_socket_address_get_family(addr) == G_SOCKET_FAMILY_IPV6) &&
        (localAddr = addr_to_string(addr)) == NULL)
        goto error;
    g_clear_object(&addr);

    /* Get remote address in form  IPADDR:PORT */
    addr = g_socket_get_remote_address(c->sock, NULL);
    if (!addr) {
        g_critical("failed to get peer address");
        goto error;
    }
    if ((g_socket_address_get_family(addr) == G_SOCKET_FAMILY_IPV4 ||
         g_socket_address_get_family(addr) == G_SOCKET_FAMILY_IPV6) &&
        (remoteAddr = addr_to_string(addr)) == NULL)
        goto error;
    g_clear_object(&addr);

    CHANNEL_DEBUG(channel, "Client SASL new host:'%s' local:'%s' remote:'%s'",
                  spice_session_get_host(c->session), localAddr, remoteAddr);

    /* Setup a handle for being a client */
    err = sasl_client_new("spice",
                          spice_session_get_host(c->session),
                          localAddr,
                          remoteAddr,
                          saslcb,
                          SASL_SUCCESS_DATA,
                          &saslconn);
    g_free(localAddr);
    g_free(remoteAddr);

    if (err != SASL_OK) {
        g_critical("Failed to create SASL client context: %d (%s)",
                   err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    if (c->ssl) {
        sasl_ssf_t ssf;

        ssf = SSL_get_cipher_bits(c->ssl, NULL);
        err = sasl_setprop(saslconn, SASL_SSF_EXTERNAL, &ssf);
        if (err != SASL_OK) {
            g_critical("cannot set SASL external SSF %d (%s)",
                       err, sasl_errstring(err, NULL, NULL));
            goto error;
        }
    }

    memset(&secprops, 0, sizeof secprops);
    /* If we've got TLS, we don't care about SSF */
    secprops.min_ssf = c->ssl ? 0 : 56; /* Equiv to DES supported by all Kerberos */
    secprops.max_ssf = c->ssl ? 0 : 100000; /* Very strong ! AES == 256 */
    secprops.maxbufsize = 100000;
    /* If we're not TLS, then forbid any anonymous or trivially crackable auth */
    secprops.security_flags = c->ssl ? 0 :
        SASL_SEC_NOANONYMOUS | SASL_SEC_NOPLAINTEXT;

    err = sasl_setprop(saslconn, SASL_SEC_PROPS, &secprops);
    if (err != SASL_OK) {
        g_critical("cannot set security props %d (%s)",
                   err, sasl_errstring(err, NULL, NULL));
        goto error;
    }

    /* Get the supported mechanisms from the server */
    spice_channel_read(channel, &len, sizeof(len));
    if (c->has_error)
        goto error;
    if (len > SASL_MAX_MECHLIST_LEN) {
        g_critical("mechlistlen %d too long", len);
        goto error;
    }

    mechlist = g_malloc(len + 1);
    spice_channel_read(channel, mechlist, len);
    mechlist[len] = '\0';
    if (c->has_error) {
        g_free(mechlist);
        mechlist = NULL;
        goto error;
    }

restart:
    /* Start the auth negotiation on the client end first */
    CHANNEL_DEBUG(channel, "Client start negotiation mechlist '%s'", mechlist);
    err = sasl_client_start(saslconn,
                            mechlist,
                            &interact,
                            &clientout,
                            &clientoutlen,
                            &mechname);
    if (err != SASL_OK && err != SASL_CONTINUE && err != SASL_INTERACT) {
        g_critical("Failed to start SASL negotiation: %d (%s)",
                   err, sasl_errdetail(saslconn));
        g_free(mechlist);
        mechlist = NULL;
        goto error;
    }

    /* Need to gather some credentials from the client */
    if (err == SASL_INTERACT) {
        if (!spice_channel_gather_sasl_credentials(channel, interact)) {
            CHANNEL_DEBUG(channel, "Failed to collect auth credentials");
            goto error;
        }
        goto restart;
    }

    CHANNEL_DEBUG(channel, "Server start negotiation with mech %s. Data %d bytes %p '%s'",
                  mechname, clientoutlen, clientout, clientout);

    if (clientoutlen > SASL_MAX_DATA_LEN) {
        g_critical("SASL negotiation data too long: %d bytes",
                   clientoutlen);
        goto error;
    }

    /* Send back the chosen mechname */
    len = strlen(mechname);
    spice_channel_write(channel, &len, sizeof(guint32));
    spice_channel_write(channel, mechname, len);

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (clientout) {
        len = clientoutlen + 1;
        spice_channel_write(channel, &len, sizeof(guint32));
        spice_channel_write(channel, clientout, len);
    } else {
        len = 0;
        spice_channel_write(channel, &len, sizeof(guint32));
    }

    if (c->has_error)
        goto error;

    CHANNEL_DEBUG(channel, "Getting sever start negotiation reply");
    /* Read the 'START' message reply from server */
    spice_channel_read(channel, &len, sizeof(len));
    if (c->has_error)
        goto error;
    if (len > SASL_MAX_DATA_LEN) {
        g_critical("SASL negotiation data too long: %d bytes",
                   len);
        goto error;
    }

    /* NB, distinction of NULL vs "" is *critical* in SASL */
    if (len > 0) {
        serverin = g_malloc(len);
        spice_channel_read(channel, serverin, len);
        serverin[len - 1] = '\0';
        len--;
    } else {
        serverin = NULL;
    }
    spice_channel_read(channel, &complete, sizeof(guint8));
    if (c->has_error)
        goto error;

    CHANNEL_DEBUG(channel, "Client start result complete: %d. Data %d bytes %p '%s'",
                complete, len, serverin, serverin);

    /* Loop-the-loop...
     * Even if the server has completed, the client must *always* do at least one step
     * in this loop to verify the server isn't lying about something. Mutual auth */
    for (;;) {
       if (complete && err == SASL_OK)
            break;

    restep:
        err = sasl_client_step(saslconn,
                               serverin,
                               len,
                               &interact,
                               &clientout,
                               &clientoutlen);
        if (err != SASL_OK && err != SASL_CONTINUE && err != SASL_INTERACT) {
            g_critical("Failed SASL step: %d (%s)",
                       err, sasl_errdetail(saslconn));
            goto error;
        }

        /* Need to gather some credentials from the client */
        if (err == SASL_INTERACT) {
            if (!spice_channel_gather_sasl_credentials(channel,
                                                       interact)) {
                CHANNEL_DEBUG(channel, "%s", "Failed to collect auth credentials");
                goto error;
            }
            goto restep;
        }

        if (serverin) {
            g_free(serverin);
            serverin = NULL;
        }

        CHANNEL_DEBUG(channel, "Client step result %d. Data %d bytes %p '%s'", err, clientoutlen, clientout, clientout);

        /* Previous server call showed completion & we're now locally complete too */
        if (complete && err == SASL_OK)
            break;

        /* Not done, prepare to talk with the server for another iteration */

        /* NB, distinction of NULL vs "" is *critical* in SASL */
        if (clientout) {
            len = clientoutlen + 1;
            spice_channel_write(channel, &len, sizeof(guint32));
            spice_channel_write(channel, clientout, len);
        } else {
            len = 0;
            spice_channel_write(channel, &len, sizeof(guint32));
        }

        if (c->has_error)
            goto error;

        CHANNEL_DEBUG(channel, "Server step with %d bytes %p", clientoutlen, clientout);

        spice_channel_read(channel, &len, sizeof(guint32));
        if (c->has_error)
            goto error;
        if (len > SASL_MAX_DATA_LEN) {
            g_critical("SASL negotiation data too long: %d bytes", len);
            goto error;
        }

        /* NB, distinction of NULL vs "" is *critical* in SASL */
        if (len) {
            serverin = g_malloc(len);
            spice_channel_read(channel, serverin, len);
            serverin[len - 1] = '\0';
            len--;
        } else {
            serverin = NULL;
        }

        spice_channel_read(channel, &complete, sizeof(guint8));
        if (c->has_error)
            goto error;

        CHANNEL_DEBUG(channel, "Client step result complete: %d. Data %d bytes %p '%s'",
                    complete, len, serverin, serverin);

        /* This server call shows complete, and earlier client step was OK */
        if (complete) {
            g_free(serverin);
            serverin = NULL;
            if (err == SASL_CONTINUE) /* something went wrong */
                goto complete;
            break;
        }
    }

    /* Check for suitable SSF if non-TLS */
    if (!c->ssl) {
        err = sasl_getprop(saslconn, SASL_SSF, &val);
        if (err != SASL_OK) {
            g_critical("cannot query SASL ssf on connection %d (%s)",
                       err, sasl_errstring(err, NULL, NULL));
            goto error;
        }
        ssf = *(const int *)val;
        CHANNEL_DEBUG(channel, "SASL SSF value %d", ssf);
        if (ssf < 56) { /* 56 == DES level, good for Kerberos */
            g_critical("negotiation SSF %d was not strong enough", ssf);
            goto error;
        }
    }

complete:
    CHANNEL_DEBUG(channel, "%s", "SASL authentication complete");
    spice_channel_read(channel, &len, sizeof(len));
    if (len != SPICE_LINK_ERR_OK)
        emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_AUTH);
    ret = len == SPICE_LINK_ERR_OK;
    /* This must come *after* check-auth-result, because the former
     * is defined to be sent unencrypted, and setting saslconn turns
     * on the SSF layer encryption processing */
    c->sasl_conn = saslconn;
    return ret;

error:
    g_clear_object(&addr);
    if (saslconn)
        sasl_dispose(&saslconn);
    emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_AUTH);
    c->has_error = TRUE; /* force disconnect */
    return FALSE;
}
#endif /* HAVE_SASL */

/* coroutine context */
static void spice_channel_recv_link_msg(SpiceChannel *channel, gboolean *switch_tls)
{
    SpiceChannelPrivate *c;
    int rc, num_caps, i;
    uint32_t *caps;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(channel->priv != NULL);

    c = channel->priv;

    rc = spice_channel_read(channel, (uint8_t*)c->peer_msg + c->peer_pos,
                            c->peer_hdr.size - c->peer_pos);
    c->peer_pos += rc;
    if (c->peer_pos != c->peer_hdr.size) {
        g_critical("%s: %s: incomplete link reply (%d/%d)",
                  c->name, __FUNCTION__, rc, c->peer_hdr.size);
        emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_LINK);
        return;
    }
    switch (c->peer_msg->error) {
    case SPICE_LINK_ERR_OK:
        /* nothing */
        break;
    case SPICE_LINK_ERR_NEED_SECURED:
        *switch_tls = true;
        CHANNEL_DEBUG(channel, "switching to tls");
        return;
    default:
        g_warning("%s: %s: unhandled error %d",
                c->name, __FUNCTION__, c->peer_msg->error);
        goto error;
    }

    num_caps = c->peer_msg->num_channel_caps + c->peer_msg->num_common_caps;
    CHANNEL_DEBUG(channel, "%s: %d caps", __FUNCTION__, num_caps);

    /* see original spice/client code: */
    /* g_return_if_fail(c->peer_msg + c->peer_msg->caps_offset * sizeof(uint32_t) > c->peer_msg + c->peer_hdr.size); */

    caps = (uint32_t *)((uint8_t *)c->peer_msg + c->peer_msg->caps_offset);

    g_array_set_size(c->remote_common_caps, c->peer_msg->num_common_caps);
    for (i = 0; i < c->peer_msg->num_common_caps; i++, caps++) {
        g_array_index(c->remote_common_caps, uint32_t, i) = *caps;
        CHANNEL_DEBUG(channel, "got common caps %u:0x%X", i, *caps);
    }

    g_array_set_size(c->remote_caps, c->peer_msg->num_channel_caps);
    for (i = 0; i < c->peer_msg->num_channel_caps; i++, caps++) {
        g_array_index(c->remote_caps, uint32_t, i) = *caps;
        CHANNEL_DEBUG(channel, "got channel caps %u:0x%X", i, *caps);
    }

    if (!spice_channel_test_common_capability(channel,
            SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION)) {
        CHANNEL_DEBUG(channel, "Server supports spice ticket auth only");
        spice_channel_send_spice_ticket(channel);
    } else {
        SpiceLinkAuthMechanism auth = { 0, };

#if HAVE_SASL
        if (spice_channel_test_common_capability(channel, SPICE_COMMON_CAP_AUTH_SASL)) {
            CHANNEL_DEBUG(channel, "Choosing SASL mechanism");
            auth.auth_mechanism = SPICE_COMMON_CAP_AUTH_SASL;
            spice_channel_write(channel, &auth, sizeof(auth));
            spice_channel_perform_auth_sasl(channel);
        } else
#endif
        if (spice_channel_test_common_capability(channel, SPICE_COMMON_CAP_AUTH_SPICE)) {
            auth.auth_mechanism = SPICE_COMMON_CAP_AUTH_SPICE;
            spice_channel_write(channel, &auth, sizeof(auth));
            spice_channel_send_spice_ticket(channel);
        } else {
            g_warning("No compatible AUTH mechanism");
            goto error;
        }
    }
    c->use_mini_header = spice_channel_test_common_capability(channel,
                                                              SPICE_COMMON_CAP_MINI_HEADER);
    CHANNEL_DEBUG(channel, "use mini header: %d", c->use_mini_header);
    return;

error:
    SPICE_CHANNEL_GET_CLASS(channel)->channel_disconnect(channel);
    emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_LINK);
}

/* system context */
G_GNUC_INTERNAL
void spice_channel_wakeup(SpiceChannel *channel, gboolean cancel)
{
    GCoroutine *c = &channel->priv->coroutine;

    if (cancel)
        g_coroutine_condition_cancel(c);

    g_coroutine_wakeup(c);
}

G_GNUC_INTERNAL
gboolean spice_channel_get_read_only(SpiceChannel *channel)
{
    return spice_session_get_read_only(channel->priv->session);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_recv_msg(SpiceChannel *channel,
                            handler_msg_in msg_handler, gpointer data)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgIn *in;
    int msg_size;
    int msg_type;
    int sub_list_offset = 0;

    in = spice_msg_in_new(channel);

    /* receive message */
    spice_channel_read(channel, in->header,
                       spice_header_get_header_size(c->use_mini_header));
    if (c->has_error)
        goto end;

    msg_size = spice_header_get_msg_size(in->header, c->use_mini_header);
    /* FIXME: do not allow others to take ref on in, and use realloc here?
     * this would avoid malloc/free on each message?
     */
    in->data = spice_malloc(msg_size);
    spice_channel_read(channel, in->data, msg_size);
    if (c->has_error)
        goto end;
    in->dpos = msg_size;

    msg_type = spice_header_get_msg_type(in->header, c->use_mini_header);
    sub_list_offset = spice_header_get_msg_sub_list(in->header, c->use_mini_header);

    if (msg_type == SPICE_MSG_LIST || sub_list_offset) {
        SpiceSubMessageList *sub_list;
        SpiceSubMessage *sub;
        SpiceMsgIn *sub_in;
        int i;

        sub_list = (SpiceSubMessageList *)(in->data + sub_list_offset);
        for (i = 0; i < sub_list->size; i++) {
            sub = (SpiceSubMessage *)(in->data + sub_list->sub_messages[i]);
            sub_in = spice_msg_in_sub_new(channel, in, sub);
            sub_in->parsed = c->parser(sub_in->data, sub_in->data + sub_in->dpos,
                                       spice_header_get_msg_type(sub_in->header,
                                                                 c->use_mini_header),
                                       c->peer_hdr.minor_version,
                                       &sub_in->psize, &sub_in->pfree);
            if (sub_in->parsed == NULL) {
                g_critical("failed to parse sub-message: %s type %d",
                           c->name, spice_header_get_msg_type(sub_in->header, c->use_mini_header));
                goto end;
            }
            msg_handler(channel, sub_in, data);
            spice_msg_in_unref(sub_in);
        }
    }

    /* ack message */
    if (c->message_ack_count) {
        c->message_ack_count--;
        if (!c->message_ack_count) {
            SpiceMsgOut *out = spice_msg_out_new(channel, SPICE_MSGC_ACK);
            spice_msg_out_send_internal(out);
            c->message_ack_count = c->message_ack_window;
        }
    }

    if (msg_type == SPICE_MSG_LIST) {
        goto end;
    }

    /* parse message */
    in->parsed = c->parser(in->data, in->data + msg_size, msg_type,
                           c->peer_hdr.minor_version, &in->psize, &in->pfree);
    if (in->parsed == NULL) {
        g_critical("failed to parse message: %s type %d",
                   c->name, msg_type);
        goto end;
    }

    /* process message */
    /* spice_msg_in_hexdump(in); */
    msg_handler(channel, in, data);

end:
    /* If the server uses full header, the serial is not necessarily equal
     * to c->in_serial (the server can sometimes skip serials) */
    c->last_message_serial = spice_header_get_in_msg_serial(in);
    c->in_serial++;
    spice_msg_in_unref(in);
}

static const char *to_string[] = {
    NULL,
    [ SPICE_CHANNEL_MAIN ] = "main",
    [ SPICE_CHANNEL_DISPLAY ] = "display",
    [ SPICE_CHANNEL_INPUTS ] = "inputs",
    [ SPICE_CHANNEL_CURSOR ] = "cursor",
    [ SPICE_CHANNEL_PLAYBACK ] = "playback",
    [ SPICE_CHANNEL_RECORD ] = "record",
    [ SPICE_CHANNEL_TUNNEL ] = "tunnel",
    [ SPICE_CHANNEL_SMARTCARD ] = "smartcard",
    [ SPICE_CHANNEL_USBREDIR ] = "usbredir",
    [ SPICE_CHANNEL_PORT ] = "port",
};

/**
 * spice_channel_type_to_string:
 * @type: a channel-type property value
 *
 * Convert a channel-type property value to a string.
 *
 * Returns: string representation of @type.
 * Since: 0.20
 **/
const gchar* spice_channel_type_to_string(gint type)
{
    const char *str = NULL;

    if (type >= 0 && type < G_N_ELEMENTS(to_string)) {
        str = to_string[type];
    }

    return str ? str : "unknown channel type";
}

/**
 * spice_channel_string_to_type:
 * @str: a string representation of the channel-type property
 *
 * Convert a channel-type property value to a string.
 *
 * Returns: the channel-type property value for a @str channel
 * Since: 0.21
 **/
gint spice_channel_string_to_type(const gchar *str)
{
    int i;

    g_return_val_if_fail(str != NULL, -1);

    for (i = 0; i < G_N_ELEMENTS(to_string); i++)
        if (g_strcmp0(str, to_string[i]) == 0)
            return i;

    return -1;
}

G_GNUC_INTERNAL
gchar *spice_channel_supported_string(void)
{
    return g_strjoin(", ",
                     spice_channel_type_to_string(SPICE_CHANNEL_MAIN),
                     spice_channel_type_to_string(SPICE_CHANNEL_DISPLAY),
                     spice_channel_type_to_string(SPICE_CHANNEL_INPUTS),
                     spice_channel_type_to_string(SPICE_CHANNEL_CURSOR),
                     spice_channel_type_to_string(SPICE_CHANNEL_PLAYBACK),
                     spice_channel_type_to_string(SPICE_CHANNEL_RECORD),
#ifdef USE_SMARTCARD
                     spice_channel_type_to_string(SPICE_CHANNEL_SMARTCARD),
#endif
#ifdef USE_USBREDIR
                     spice_channel_type_to_string(SPICE_CHANNEL_USBREDIR),
#endif
                     NULL);
}


/**
 * spice_channel_new:
 * @s: the @SpiceSession the channel is linked to
 * @type: the requested SPICECHANNELPRIVATE type
 * @id: the channel-id
 *
 * Create a new #SpiceChannel of type @type, and channel ID @id.
 *
 * Returns: a weak reference to #SpiceChannel, the session owns the reference
 **/
SpiceChannel *spice_channel_new(SpiceSession *s, int type, int id)
{
    SpiceChannel *channel;
    GType gtype = 0;

    g_return_val_if_fail(s != NULL, NULL);

    switch (type) {
    case SPICE_CHANNEL_MAIN:
        gtype = SPICE_TYPE_MAIN_CHANNEL;
        break;
    case SPICE_CHANNEL_DISPLAY:
        gtype = SPICE_TYPE_DISPLAY_CHANNEL;
        break;
    case SPICE_CHANNEL_CURSOR:
        gtype = SPICE_TYPE_CURSOR_CHANNEL;
        break;
    case SPICE_CHANNEL_INPUTS:
        gtype = SPICE_TYPE_INPUTS_CHANNEL;
        break;
    case SPICE_CHANNEL_PLAYBACK:
    case SPICE_CHANNEL_RECORD: {
        if (!s->priv->audio) {
            g_debug("audio channel is disabled, not creating it");
            return NULL;
        }
        gtype = type == SPICE_CHANNEL_RECORD ?
            SPICE_TYPE_RECORD_CHANNEL : SPICE_TYPE_PLAYBACK_CHANNEL;
        break;
    }
#ifdef USE_SMARTCARD
    case SPICE_CHANNEL_SMARTCARD: {
        if (!s->priv->smartcard) {
            g_debug("smartcard channel is disabled, not creating it");
            return NULL;
        }
        gtype = SPICE_TYPE_SMARTCARD_CHANNEL;
        break;
    }
#endif
#ifdef USE_USBREDIR
    case SPICE_CHANNEL_USBREDIR: {
        if (!s->priv->usbredir) {
            g_debug("usbredir channel is disabled, not creating it");
            return NULL;
        }
        gtype = SPICE_TYPE_USBREDIR_CHANNEL;
        break;
    }
#endif
    case SPICE_CHANNEL_PORT:
        gtype = SPICE_TYPE_PORT_CHANNEL;
        break;
    default:
        g_debug("unsupported channel kind: %s: %d",
                spice_channel_type_to_string(type), type);
        return NULL;
    }
    channel = SPICE_CHANNEL(g_object_new(gtype,
                                         "spice-session", s,
                                         "channel-type", type,
                                         "channel-id", id,
                                         NULL));
    return channel;
}

/**
 * spice_channel_destroy:
 * @channel:
 *
 * Disconnect and unref the @channel. Called by @spice_session_disconnect()
 *
 **/
void spice_channel_destroy(SpiceChannel *channel)
{
    g_return_if_fail(channel != NULL);

    CHANNEL_DEBUG(channel, "channel destroy");
    spice_channel_disconnect(channel, SPICE_CHANNEL_NONE);
    g_object_unref(channel);
}

/* any context */
static void spice_channel_flushed(SpiceChannel *channel, gboolean success)
{
    SpiceChannelPrivate *c = channel->priv;
    GSList *l;

    for (l = c->flushing; l != NULL; l = l->next) {
        GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT(l->data);
        g_simple_async_result_set_op_res_gboolean(result, success);
        g_simple_async_result_complete_in_idle(result);
    }

    g_slist_free_full(c->flushing, g_object_unref);
    c->flushing = NULL;
}

/* coroutine context */
static void spice_channel_iterate_write(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgOut *out;

    do {
        STATIC_MUTEX_LOCK(c->xmit_queue_lock);
        out = g_queue_pop_head(&c->xmit_queue);
        STATIC_MUTEX_UNLOCK(c->xmit_queue_lock);
        if (out)
            spice_channel_write_msg(channel, out);
    } while (out);

    spice_channel_flushed(channel, TRUE);
}

/* coroutine context */
static void spice_channel_iterate_read(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;

    g_coroutine_socket_wait(&c->coroutine, c->sock, G_IO_IN);

    /* treat all incoming data (block on message completion) */
    while (!c->has_error &&
           c->state != SPICE_CHANNEL_STATE_MIGRATING &&
           g_socket_condition_check(c->sock, G_IO_IN) & G_IO_IN) {

        do
            spice_channel_recv_msg(channel,
                                   (handler_msg_in)SPICE_CHANNEL_GET_CLASS(channel)->handle_msg, NULL);
#if HAVE_SASL
            /* flush the sasl buffer too */
        while (c->sasl_decoded != NULL);
#else
        while (FALSE);
#endif
    }

}

static gboolean wait_migration(gpointer data)
{
    SpiceChannel *channel = SPICE_CHANNEL(data);
    SpiceChannelPrivate *c = channel->priv;

    if (c->state != SPICE_CHANNEL_STATE_MIGRATING) {
        CHANNEL_DEBUG(channel, "unfreeze channel");
        return TRUE;
    }

    return FALSE;
}

/* coroutine context */
static gboolean spice_channel_iterate(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    GIOCondition ret;

    if (c->state == SPICE_CHANNEL_STATE_MIGRATING &&
        !g_coroutine_condition_wait(&c->coroutine, wait_migration, channel))
        CHANNEL_DEBUG(channel, "migration wait cancelled");

    if (c->has_error) {
        CHANNEL_DEBUG(channel, "channel has error, breaking loop");
        return FALSE;
    }

    /* flush any pending write and read */
    SPICE_CHANNEL_GET_CLASS(channel)->iterate_write(channel);
    SPICE_CHANNEL_GET_CLASS(channel)->iterate_read(channel);

    ret = g_socket_condition_check(c->sock, G_IO_IN | G_IO_ERR | G_IO_HUP);
    if (c->state > SPICE_CHANNEL_STATE_CONNECTING &&
        ret & (G_IO_ERR|G_IO_HUP)) {
        SPICE_DEBUG("got socket error: %d", ret);
        emit_main_context(channel, SPICE_CHANNEL_EVENT,
                          c->state == SPICE_CHANNEL_STATE_READY ?
                          SPICE_CHANNEL_ERROR_IO : SPICE_CHANNEL_ERROR_LINK);
        c->has_error = TRUE;
        return FALSE;
    }

    return TRUE;
}

/* we use an idle function to allow the coroutine to exit before we actually
 * unref the object since the coroutine's state is part of the object */
static gboolean spice_channel_delayed_unref(gpointer data)
{
    SpiceChannel *channel = SPICE_CHANNEL(data);
    SpiceChannelPrivate *c = channel->priv;

    g_return_val_if_fail(channel != NULL, FALSE);
    CHANNEL_DEBUG(channel, "Delayed unref channel %p", channel);

    g_return_val_if_fail(c->coroutine.coroutine.exited == TRUE, FALSE);

    g_object_unref(G_OBJECT(data));

    return FALSE;
}

static X509_LOOKUP_METHOD spice_x509_mem_lookup = {
    "spice_x509_mem_lookup",
    0
};

static int spice_channel_load_ca(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;
    STACK_OF(X509_INFO) *inf;
    X509_INFO *itmp;
    X509_LOOKUP *lookup;
    BIO *in;
    int i, count = 0;
    guint8 *ca;
    guint size;
    const gchar *ca_file;
    int rc;

    g_return_val_if_fail(c->ctx != NULL, 0);

    lookup = X509_STORE_add_lookup(c->ctx->cert_store, &spice_x509_mem_lookup);
    ca_file = spice_session_get_ca_file(c->session);
    spice_session_get_ca(c->session, &ca, &size);

    CHANNEL_DEBUG(channel, "Load CA, file: %s, data: %p", ca_file, ca);
    g_warn_if_fail(ca_file || ca);

    if (ca != NULL) {
        in = BIO_new_mem_buf(ca, size);
        inf = PEM_X509_INFO_read_bio(in, NULL, NULL, NULL);
        BIO_free(in);

        for (i = 0; i < sk_X509_INFO_num(inf); i++) {
            itmp = sk_X509_INFO_value(inf, i);
            if (itmp->x509) {
                X509_STORE_add_cert(lookup->store_ctx, itmp->x509);
                count++;
            }
            if (itmp->crl) {
                X509_STORE_add_crl(lookup->store_ctx, itmp->crl);
                count++;
            }
        }

        sk_X509_INFO_pop_free(inf, X509_INFO_free);
    }

    if (ca_file != NULL) {
        rc = SSL_CTX_load_verify_locations(c->ctx, ca_file, NULL);
        if (rc != 1)
            g_warning("loading ca certs from %s failed", ca_file);
        else
            count++;
    }

    if (count == 0) {
        rc = SSL_CTX_set_default_verify_paths(c->ctx);
        if (rc != 1)
            g_warning("loading ca certs from default location failed");
        else
            count++;
    }

    return count;
}


/* coroutine context */
static void *spice_channel_coroutine(void *data)
{
    SpiceChannel *channel = SPICE_CHANNEL(data);
    SpiceChannelPrivate *c = channel->priv;
    guint verify;
    int rc, delay_val = 1;
    gboolean switch_tls = FALSE;
    gboolean switch_protocol = FALSE;
    /* When some other SSL/TLS version becomes obsolete, add it to this
     * variable. */
    long ssl_options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;

    CHANNEL_DEBUG(channel, "Started background coroutine %p", &c->coroutine);

    if (spice_session_get_client_provided_socket(c->session)) {
        if (c->fd < 0) {
            g_critical("fd not provided!");
            emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_CONNECT);
            goto cleanup;
        }

        if (!(c->sock = g_socket_new_from_fd(c->fd, NULL))) {
                CHANNEL_DEBUG(channel, "Failed to open socket from fd %d", c->fd);
                emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_CONNECT);
                goto cleanup;
        }

        g_socket_set_blocking(c->sock, FALSE);
        g_socket_set_keepalive(c->sock, TRUE);
        goto connected;
    }

reconnect:
    c->conn = spice_session_channel_open_host(c->session, channel, &c->tls);
    if (c->conn == NULL) {
        if (!c->tls) {
            CHANNEL_DEBUG(channel, "trying with TLS port");
            c->tls = true; /* FIXME: does that really work with provided fd */
            goto reconnect;
        } else {
            CHANNEL_DEBUG(channel, "Connect error");
            emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_CONNECT);
            goto cleanup;
        }
    }
    c->sock = g_object_ref(g_socket_connection_get_socket(c->conn));

    c->has_error = FALSE;

    if (c->tls) {
        c->ctx = SSL_CTX_new(SSLv23_method());
        if (c->ctx == NULL) {
            g_critical("SSL_CTX_new failed");
            emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_TLS);
            goto cleanup;
        }

        SSL_CTX_set_options(c->ctx, ssl_options);

        verify = spice_session_get_verify(c->session);
        if (verify &
            (SPICE_SESSION_VERIFY_SUBJECT | SPICE_SESSION_VERIFY_HOSTNAME)) {
            rc = spice_channel_load_ca(channel);
            if (rc == 0) {
                g_warning("no cert loaded");
                if (verify & SPICE_SESSION_VERIFY_PUBKEY) {
                    g_warning("only pubkey active");
                    verify = SPICE_SESSION_VERIFY_PUBKEY;
                } else {
                    emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_TLS);
                    goto cleanup;
                }
            }
        }

        {
            const gchar *ciphers = spice_session_get_ciphers(c->session);
            if (ciphers != NULL) {
                rc = SSL_CTX_set_cipher_list(c->ctx, ciphers);
                if (rc != 1)
                    g_warning("loading cipher list %s failed", ciphers);
            }
        }

        c->ssl = SSL_new(c->ctx);
        if (c->ssl == NULL) {
            g_critical("SSL_new failed");
            emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_TLS);
            goto cleanup;
        }


        BIO *bio = bio_new_gsocket(c->sock);
        SSL_set_bio(c->ssl, bio, bio);

        {
            guint8 *pubkey;
            guint pubkey_len;

            spice_session_get_pubkey(c->session, &pubkey, &pubkey_len);
            c->sslverify = spice_openssl_verify_new(c->ssl, verify,
                spice_session_get_host(c->session),
                (char*)pubkey, pubkey_len,
                spice_session_get_cert_subject(c->session));
        }

ssl_reconnect:
        rc = SSL_connect(c->ssl);
        if (rc <= 0) {
            rc = SSL_get_error(c->ssl, rc);
            if (rc == SSL_ERROR_WANT_READ || rc == SSL_ERROR_WANT_WRITE) {
                g_coroutine_socket_wait(&c->coroutine, c->sock, G_IO_OUT|G_IO_ERR|G_IO_HUP);
                goto ssl_reconnect;
            } else {
                g_warning("%s: SSL_connect: %s",
                          c->name, ERR_error_string(rc, NULL));
                emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_ERROR_TLS);
                goto cleanup;
            }
        }
    }

connected:
    rc = setsockopt(g_socket_get_fd(c->sock), IPPROTO_TCP, TCP_NODELAY,
                    (const char*)&delay_val, sizeof(delay_val));
    if ((rc != 0)
#ifdef ENOTSUP
        && (errno != ENOTSUP)
#endif
        ) {
        g_warning("%s: could not set sockopt TCP_NODELAY: %s", c->name,
                  strerror(errno));
    }

    spice_channel_send_link(channel);
    if (spice_channel_recv_link_hdr(channel, &switch_protocol) == FALSE)
        goto cleanup;
    spice_channel_recv_link_msg(channel, &switch_tls);
    if (switch_tls)
        goto cleanup;
    spice_channel_recv_auth(channel);

    while (spice_channel_iterate(channel))
        ;

cleanup:
    CHANNEL_DEBUG(channel, "Coroutine exit %s", c->name);

    SPICE_CHANNEL_GET_CLASS(channel)->channel_disconnect(channel);

    if (switch_protocol || (switch_tls && !c->tls)) {
        c->tls = switch_tls;
        spice_channel_connect(channel);
        g_object_unref(channel);
    } else
        g_idle_add(spice_channel_delayed_unref, data);

    /* Co-routine exits now - the SpiceChannel object may no longer exist,
       so don't do anything else now unless you like SEGVs */
    return NULL;
}

static gboolean connect_delayed(gpointer data)
{
    SpiceChannel *channel = data;
    SpiceChannelPrivate *c = channel->priv;
    struct coroutine *co;

    CHANNEL_DEBUG(channel, "Open coroutine starting %p", channel);
    c->connect_delayed_id = 0;

    co = &c->coroutine.coroutine;

    co->stack_size = 16 << 20; /* 16Mb */
    co->entry = spice_channel_coroutine;
    co->release = NULL;

    coroutine_init(co);
    coroutine_yieldto(co, channel);

    return FALSE;
}

static gboolean channel_connect(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = channel->priv;

    g_return_val_if_fail(c != NULL, FALSE);

    if (c->session == NULL || c->channel_type == -1 || c->channel_id == -1) {
        /* unset properties or unknown channel type */
        g_warning("%s: channel setup incomplete", __FUNCTION__);
        return false;
    }
    if (c->state != SPICE_CHANNEL_STATE_UNCONNECTED) {
        g_warning("Invalid channel_connect state: %d", c->state);
        return true;
    }

    if (spice_session_get_client_provided_socket(c->session)) {
        if (c->fd == -1) {
            g_signal_emit(channel, signals[SPICE_CHANNEL_OPEN_FD], 0, c->tls);
            return true;
        }
    }
    c->state = SPICE_CHANNEL_STATE_CONNECTING;
    c->xmit_queue_blocked = FALSE;

    g_return_val_if_fail(c->sock == NULL, FALSE);
    g_object_ref(G_OBJECT(channel)); /* Unref'd when co-routine exits */

    /* we connect in idle, to let previous coroutine exit, if present */
    c->connect_delayed_id = g_idle_add(connect_delayed, channel);

    return true;
}

/**
 * spice_channel_connect:
 * @channel:
 *
 * Connect the channel, using #SpiceSession connection informations
 *
 * Returns: %TRUE on success.
 **/
gboolean spice_channel_connect(SpiceChannel *channel)
{
    g_return_val_if_fail(SPICE_IS_CHANNEL(channel), FALSE);
    SpiceChannelPrivate *c = channel->priv;

    if (c->state >= SPICE_CHANNEL_STATE_CONNECTING)
        return TRUE;

    return channel_connect(channel);
}

/**
 * spice_channel_open_fd:
 * @channel:
 * @fd: a file descriptor (socket) or -1.
 * request mechanism
 *
 * Connect the channel using @fd socket.
 *
 * If @fd is -1, a valid fd will be requested later via the
 * SpiceChannel::open-fd signal.
 *
 * Returns: %TRUE on success.
 **/
gboolean spice_channel_open_fd(SpiceChannel *channel, int fd)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    g_return_val_if_fail(c != NULL, FALSE);
    g_return_val_if_fail(fd >= -1, FALSE);

    c->fd = fd;

    return channel_connect(channel);
}

/* system or coroutine context */
static void channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    if (c->connect_delayed_id) {
        g_source_remove(c->connect_delayed_id);
        c->connect_delayed_id = 0;
    }

#if HAVE_SASL
    if (c->sasl_conn) {
        sasl_dispose(&c->sasl_conn);
        c->sasl_conn = NULL;
        c->sasl_decoded_offset = c->sasl_decoded_length = 0;
    }
#endif

    spice_openssl_verify_free(c->sslverify);
    c->sslverify = NULL;

    if (c->ssl) {
        SSL_free(c->ssl);
        c->ssl = NULL;
    }

    if (c->ctx) {
        SSL_CTX_free(c->ctx);
        c->ctx = NULL;
    }

    if (c->conn) {
        g_object_unref(c->conn);
        c->conn = NULL;
    }
    if (c->sock) {
        g_object_unref(c->sock);
        c->sock = NULL;
    }
    c->fd = -1;

    free(c->peer_msg);
    c->peer_msg = NULL;
    c->peer_pos = 0;

    STATIC_MUTEX_LOCK(c->xmit_queue_lock);
    c->xmit_queue_blocked = TRUE; /* Disallow queuing new messages */
    gboolean was_empty = g_queue_is_empty(&c->xmit_queue);
    g_queue_foreach(&c->xmit_queue, (GFunc)spice_msg_out_unref, NULL);
    g_queue_clear(&c->xmit_queue);
    if (c->xmit_queue_wakeup_id) {
        g_source_remove(c->xmit_queue_wakeup_id);
        c->xmit_queue_wakeup_id = 0;
    }
    STATIC_MUTEX_UNLOCK(c->xmit_queue_lock);
    spice_channel_flushed(channel, was_empty);

    g_array_set_size(c->remote_common_caps, 0);
    g_array_set_size(c->remote_caps, 0);
    g_array_set_size(c->common_caps, 0);
    /* Restore our default capabilities in case the channel gets re-used */
    spice_channel_set_common_capability(channel, SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION);
    spice_channel_set_common_capability(channel, SPICE_COMMON_CAP_MINI_HEADER);
    spice_channel_reset_capabilities(channel);
}

/* system or coroutine context */
G_GNUC_INTERNAL
void spice_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SPICE_CHANNEL_GET_CLASS(channel)->channel_reset(channel, migrating);
}

/* system or coroutine context */
static void channel_disconnect(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    g_return_if_fail(c != NULL);

    if (c->state == SPICE_CHANNEL_STATE_UNCONNECTED)
        return;

    c->has_error = TRUE; /* break the loop */

    if (c->state == SPICE_CHANNEL_STATE_READY)
        emit_main_context(channel, SPICE_CHANNEL_EVENT, SPICE_CHANNEL_CLOSED);

    c->state = SPICE_CHANNEL_STATE_UNCONNECTED;
    spice_channel_reset(channel, FALSE);

    g_return_if_fail(SPICE_IS_CHANNEL(channel));
}

/**
 * spice_channel_disconnect:
 * @channel:
 * @reason: a channel event emitted on main context (or #SPICE_CHANNEL_NONE)
 *
 * Close the socket and reset connection specific data. Finally, emit
 * @reason #SpiceChannel::channel-event on main context if not
 * #SPICE_CHANNEL_NONE.
 **/
void spice_channel_disconnect(SpiceChannel *channel, SpiceChannelEvent reason)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    CHANNEL_DEBUG(channel, "channel disconnect %d", reason);
    g_return_if_fail(c != NULL);

    if (c->state == SPICE_CHANNEL_STATE_UNCONNECTED)
        return;

    if (reason == SPICE_CHANNEL_SWITCHING)
        c->state = SPICE_CHANNEL_STATE_SWITCHING;

    c->has_error = TRUE; /* break the loop */

    if (c->state == SPICE_CHANNEL_STATE_MIGRATING) {
        c->state = SPICE_CHANNEL_STATE_READY;
    } else
        spice_channel_wakeup(channel, TRUE);

    if (reason != SPICE_CHANNEL_NONE)
        g_signal_emit(G_OBJECT(channel), signals[SPICE_CHANNEL_EVENT], 0, reason);
}

static gboolean test_capability(GArray *caps, guint32 cap)
{
    guint32 c, word_index = cap / 32;
    gboolean ret;

    if (caps == NULL)
        return FALSE;

    if (caps->len < word_index + 1)
        return FALSE;

    c = g_array_index(caps, guint32, word_index);
    ret = (c & (1 << (cap % 32))) != 0;

    SPICE_DEBUG("test cap %d in 0x%X: %s", cap, c, ret ? "yes" : "no");
    return ret;
}

/**
 * spice_channel_test_capability:
 * @channel:
 * @cap:
 *
 * Test availability of remote "channel kind capability".
 *
 * Returns: %TRUE if @cap (channel kind capability) is available.
 **/
gboolean spice_channel_test_capability(SpiceChannel *self, guint32 cap)
{
    SpiceChannelPrivate *c;

    g_return_val_if_fail(SPICE_IS_CHANNEL(self), FALSE);

    c = self->priv;
    return test_capability(c->remote_caps, cap);
}

/**
 * spice_channel_test_common_capability:
 * @channel:
 * @cap:
 *
 * Test availability of remote "common channel capability".
 *
 * Returns: %TRUE if @cap (common channel capability) is available.
 **/
gboolean spice_channel_test_common_capability(SpiceChannel *self, guint32 cap)
{
    SpiceChannelPrivate *c;

    g_return_val_if_fail(SPICE_IS_CHANNEL(self), FALSE);

    c = self->priv;
    return test_capability(c->remote_common_caps, cap);
}

static void set_capability(GArray *caps, guint32 cap)
{
    guint word_index = cap / 32;

    g_return_if_fail(caps != NULL);

    if (caps->len <= word_index)
        g_array_set_size(caps, word_index + 1);

    g_array_index(caps, guint32, word_index) =
        g_array_index(caps, guint32, word_index) | (1 << (cap % 32));
}

/**
 * spice_channel_set_capability:
 * @channel:
 * @cap: a capability
 *
 * Enable specific channel-kind capability.
 * Deprecated: 0.13: this function has been removed
 **/
#undef spice_channel_set_capability
void spice_channel_set_capability(SpiceChannel *channel, guint32 cap)
{
    SpiceChannelPrivate *c;

    g_return_if_fail(SPICE_IS_CHANNEL(channel));

    c = channel->priv;
    set_capability(c->caps, cap);
}

G_GNUC_INTERNAL
void spice_caps_set(GArray *caps, guint32 cap, const gchar *desc)
{
    g_return_if_fail(caps != NULL);
    g_return_if_fail(desc != NULL);

    if (g_strcmp0(g_getenv(desc), "0") == 0)
        return;

    set_capability(caps, cap);
}

G_GNUC_INTERNAL
SpiceSession* spice_channel_get_session(SpiceChannel *channel)
{
    g_return_val_if_fail(SPICE_IS_CHANNEL(channel), NULL);

    return channel->priv->session;
}

G_GNUC_INTERNAL
enum spice_channel_state spice_channel_get_state(SpiceChannel *channel)
{
    g_return_val_if_fail(SPICE_IS_CHANNEL(channel),
                         SPICE_CHANNEL_STATE_UNCONNECTED);

    return channel->priv->state;
}

G_GNUC_INTERNAL
void spice_channel_swap(SpiceChannel *channel, SpiceChannel *swap, gboolean swap_msgs)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);
    SpiceChannelPrivate *s = SPICE_CHANNEL_GET_PRIVATE(swap);

    g_return_if_fail(c != NULL);
    g_return_if_fail(s != NULL);

    g_return_if_fail(s->session != NULL);
    g_return_if_fail(s->sock != NULL);

#define SWAP(Field) ({                          \
    typeof (c->Field) Field = c->Field;         \
    c->Field = s->Field;                        \
    s->Field = Field;                           \
})

    /* TODO: split channel in 2 objects: a controller and a swappable
       state object */
    SWAP(conn);
    SWAP(sock);
    SWAP(ctx);
    SWAP(ssl);
    SWAP(sslverify);
    SWAP(tls);
    SWAP(use_mini_header);
    if (swap_msgs) {
        SWAP(xmit_queue);
        SWAP(xmit_queue_blocked);
        SWAP(in_serial);
        SWAP(out_serial);
    }
    SWAP(caps);
    SWAP(common_caps);
    SWAP(remote_caps);
    SWAP(remote_common_caps);
#if HAVE_SASL
    SWAP(sasl_conn);
    SWAP(sasl_decoded);
    SWAP(sasl_decoded_length);
    SWAP(sasl_decoded_offset);
#endif
}

/* coroutine context */
static void spice_channel_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg)
{
    SpiceChannelClass *klass = SPICE_CHANNEL_GET_CLASS(channel);
    int type = spice_msg_in_type(msg);
    spice_msg_handler handler;

    g_return_if_fail(type < klass->handlers->len);
    if (type > SPICE_MSG_BASE_LAST && channel->priv->disable_channel_msg)
        return;

    handler = g_array_index(klass->handlers, spice_msg_handler, type);
    g_return_if_fail(handler != NULL);
    handler(channel, msg);
}

static void spice_channel_reset_capabilities(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);
    g_array_set_size(c->caps, 0);

    if (SPICE_CHANNEL_GET_CLASS(channel)->channel_reset_capabilities) {
        SPICE_CHANNEL_GET_CLASS(channel)->channel_reset_capabilities(channel);
    }
}

static void spice_channel_send_migration_handshake(SpiceChannel *channel)
{
    SpiceChannelPrivate *c = SPICE_CHANNEL_GET_PRIVATE(channel);

    if (SPICE_CHANNEL_GET_CLASS(channel)->channel_send_migration_handshake) {
        SPICE_CHANNEL_GET_CLASS(channel)->channel_send_migration_handshake(channel);
    } else {
        c->state = SPICE_CHANNEL_STATE_MIGRATING;
    }
}

/**
 * spice_channel_flush_async:
 * @channel: a #SpiceChannel
 * @cancellable: (allow-none): optional GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Forces an asynchronous write of all user-space buffered data for
 * the given channel.
 *
 * When the operation is finished callback will be called. You can
 * then call spice_channel_flush_finish() to get the result of the
 * operation.
 *
 * Since: 0.15
 **/
void spice_channel_flush_async(SpiceChannel *self, GCancellable *cancellable,
                               GAsyncReadyCallback callback, gpointer user_data)
{
    GSimpleAsyncResult *simple;
    SpiceChannelPrivate *c;
    gboolean was_empty;

    g_return_if_fail(SPICE_IS_CHANNEL(self));
    c = self->priv;

    if (c->state != SPICE_CHANNEL_STATE_READY) {
        g_simple_async_report_error_in_idle(G_OBJECT(self), callback, user_data,
            SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
            "The channel is not ready yet");
        return;
    }

    simple = g_simple_async_result_new(G_OBJECT(self), callback, user_data,
                                       spice_channel_flush_async);

    STATIC_MUTEX_LOCK(c->xmit_queue_lock);
    was_empty = g_queue_is_empty(&c->xmit_queue);
    STATIC_MUTEX_UNLOCK(c->xmit_queue_lock);
    if (was_empty) {
        g_simple_async_result_set_op_res_gboolean(simple, TRUE);
        g_simple_async_result_complete_in_idle(simple);
        g_object_unref(simple);
        return;
    }

    c->flushing = g_slist_append(c->flushing, simple);
}

/**
 * spice_channel_flush_finish:
 * @channel: a #SpiceChannel
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL
 * to ignore.
 *
 * Finishes flushing a channel.
 *
 * Returns: %TRUE if flush operation succeeded, %FALSE otherwise.
 * Since: 0.15
 **/
gboolean spice_channel_flush_finish(SpiceChannel *self, GAsyncResult *result,
                                    GError **error)
{
    GSimpleAsyncResult *simple;

    g_return_val_if_fail(SPICE_IS_CHANNEL(self), FALSE);
    g_return_val_if_fail(result != NULL, FALSE);

    simple = (GSimpleAsyncResult *)result;

    if (g_simple_async_result_propagate_error(simple, error))
        return -1;

    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(self),
                                                        spice_channel_flush_async), FALSE);

    CHANNEL_DEBUG(self, "flushed finished!");
    return g_simple_async_result_get_op_res_gboolean(simple);
}
