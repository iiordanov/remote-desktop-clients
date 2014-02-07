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
#ifndef __SPICE_CLIENT_CHANNEL_PRIV_H__
#define __SPICE_CLIENT_CHANNEL_PRIV_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <openssl/ssl.h>
#include <gio/gio.h>

#if HAVE_SASL
#include <sasl/sasl.h>
#endif

#include "spice-channel.h"
#include "spice-util-priv.h"
#include "coroutine.h"
#include "gio-coroutine.h"

#include "common/client_marshallers.h"
#include "common/client_demarshallers.h"
#include "common/ssl_verify.h"

G_BEGIN_DECLS

#define MAX_SPICE_DATA_HEADER_SIZE sizeof(SpiceDataHeader)

#define CHANNEL_DEBUG(channel, fmt, ...) \
    SPICE_DEBUG("%s: " fmt, SPICE_CHANNEL(channel)->priv->name, ## __VA_ARGS__)

struct _SpiceMsgOut {
    int                   refcount;
    SpiceChannel          *channel;
    SpiceMessageMarshallers *marshallers;
    SpiceMarshaller       *marshaller;
    uint8_t               *header;
    gboolean              ro_check;
};

struct _SpiceMsgIn {
    int                   refcount;
    SpiceChannel          *channel;
    uint8_t               header[MAX_SPICE_DATA_HEADER_SIZE];
    uint8_t               *data;
    int                   dpos;
    uint8_t               *parsed;
    size_t                psize;
    message_destructor_t  pfree;
    SpiceMsgIn            *parent;
};

enum spice_channel_state {
    SPICE_CHANNEL_STATE_UNCONNECTED = 0,
    SPICE_CHANNEL_STATE_CONNECTING,
    SPICE_CHANNEL_STATE_READY,
    SPICE_CHANNEL_STATE_SWITCHING,
    SPICE_CHANNEL_STATE_MIGRATING,
    SPICE_CHANNEL_STATE_MIGRATION_HANDSHAKE,
};

struct _SpiceChannelPrivate {
    /* swapped on migration */
    SSL_CTX                     *ctx;
    SSL                         *ssl;
    SpiceOpenSSLVerify          *sslverify;
    GSocket                     *sock;
    GSocketConnection           *conn;

#if HAVE_SASL
    sasl_conn_t                 *sasl_conn;
    const char                  *sasl_decoded;
    unsigned int                sasl_decoded_length;
    unsigned int                sasl_decoded_offset;
#endif

    gboolean                    use_mini_header;
    uint64_t                    out_serial;
    uint64_t                    in_serial;

    /* not swapped */
    SpiceSession                *session;
    GCoroutine                  coroutine;
    int                         fd;
    gboolean                    has_error;
    guint                       connect_delayed_id;

    GQueue                      xmit_queue;
    gboolean                    xmit_queue_blocked;
    STATIC_MUTEX                xmit_queue_lock;
    guint                       xmit_queue_wakeup_id;

    char                        name[16];
    enum spice_channel_state    state;
    spice_parse_channel_func_t  parser;
    SpiceMessageMarshallers     *marshallers;
    guint                       channel_watch;
    int                         tls;

    int                         channel_id;
    int                         channel_type;
    SpiceLinkHeader             link_hdr;
    SpiceLinkMess               link_msg;
    SpiceLinkHeader             peer_hdr;
    SpiceLinkReply*             peer_msg;
    int                         peer_pos;

    int                         message_ack_window;
    int                         message_ack_count;

    GArray                      *caps;
    GArray                      *common_caps;
    GArray                      *remote_caps;
    GArray                      *remote_common_caps;

    gsize                       total_read_bytes;
    uint64_t                    last_message_serial;
    GSList                      *flushing;

    gboolean                    disable_channel_msg;
};

SpiceMsgIn *spice_msg_in_new(SpiceChannel *channel);
SpiceMsgIn *spice_msg_in_sub_new(SpiceChannel *channel, SpiceMsgIn *parent,
                                   SpiceSubMessage *sub);
void spice_msg_in_ref(SpiceMsgIn *in);
void spice_msg_in_unref(SpiceMsgIn *in);
int spice_msg_in_type(SpiceMsgIn *in);
void *spice_msg_in_parsed(SpiceMsgIn *in);
void *spice_msg_in_raw(SpiceMsgIn *in, int *len);
void spice_msg_in_hexdump(SpiceMsgIn *in);

SpiceMsgOut *spice_msg_out_new(SpiceChannel *channel, int type);
void spice_msg_out_ref(SpiceMsgOut *out);
void spice_msg_out_unref(SpiceMsgOut *out);
void spice_msg_out_send(SpiceMsgOut *out);
void spice_msg_out_send_internal(SpiceMsgOut *out);
void spice_msg_out_hexdump(SpiceMsgOut *out, unsigned char *data, int len);

uint16_t spice_header_get_msg_type(uint8_t *header, gboolean is_mini_header);
uint32_t spice_header_get_msg_size(uint8_t *header, gboolean is_mini_header);

void spice_channel_up(SpiceChannel *channel);
void spice_channel_wakeup(SpiceChannel *channel, gboolean cancel);

SpiceSession* spice_channel_get_session(SpiceChannel *channel);
enum spice_channel_state spice_channel_get_state(SpiceChannel *channel);

/* coroutine context */
typedef void (*handler_msg_in)(SpiceChannel *channel, SpiceMsgIn *msg, gpointer data);
void spice_channel_recv_msg(SpiceChannel *channel, handler_msg_in handler, gpointer data);

/* channel-base.c */
void spice_channel_set_handlers(SpiceChannelClass *klass,
                                const spice_msg_handler* handlers, const int n);
void spice_channel_handle_wait_for_channels(SpiceChannel *channel, SpiceMsgIn *in);

gint spice_channel_get_channel_id(SpiceChannel *channel);
gint spice_channel_get_channel_type(SpiceChannel *channel);
void spice_channel_swap(SpiceChannel *channel, SpiceChannel *swap, gboolean swap_msgs);
gboolean spice_channel_get_read_only(SpiceChannel *channel);
void spice_channel_reset(SpiceChannel *channel, gboolean migrating);

void spice_caps_set(GArray *caps, guint32 cap, const gchar *desc);
#define spice_channel_set_common_capability(channel, cap)               \
    spice_caps_set(SPICE_CHANNEL(channel)->priv->common_caps, cap, #cap)
#define spice_channel_set_capability(channel, cap)                      \
    spice_caps_set(SPICE_CHANNEL(channel)->priv->caps, cap, #cap)

/* coroutine context */
#define emit_main_context(object, event, args...)                                      \
    G_STMT_START {                                                                     \
        if (coroutine_self_is_main()) {                                 \
            do_emit_main_context(G_OBJECT(object), event, &((struct event) { args })); \
        } else {                                                                       \
            g_signal_emit_main_context(G_OBJECT(object), do_emit_main_context,         \
                                       event, &((struct event) { args }), G_STRLOC);   \
        }                                                                              \
    } G_STMT_END

gchar *spice_channel_supported_string(void);

G_END_DECLS

#endif /* __SPICE_CLIENT_CHANNEL_PRIV_H__ */
