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

#include "spice-session-priv.h"
#include "spice-channel-priv.h"

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_set_ack(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgSetAck* ack = spice_msg_in_parsed(in);
    SpiceMsgOut *out = spice_msg_out_new(channel, SPICE_MSGC_ACK_SYNC);
    SpiceMsgcAckSync sync = {
        .generation = ack->generation,
    };

    c->message_ack_window = c->message_ack_count = ack->window;
    c->marshallers->msgc_ack_sync(out->marshaller, &sync);
    spice_msg_out_send_internal(out);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_ping(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgPing *ping = spice_msg_in_parsed(in);
    SpiceMsgOut *pong = spice_msg_out_new(channel, SPICE_MSGC_PONG);

    c->marshallers->msgc_pong(pong->marshaller, ping);
    spice_msg_out_send_internal(pong);
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_notify(SpiceChannel *channel, SpiceMsgIn *in)
{
    static const char* severity_strings[] = {"info", "warn", "error"};
    static const char* visibility_strings[] = {"!", "!!", "!!!"};

    SpiceMsgNotify *notify = spice_msg_in_parsed(in);
    const char *severity   = "?";
    const char *visibility = "?";
    const char *message_str = NULL;

    if (notify->severity <= SPICE_NOTIFY_SEVERITY_ERROR) {
        severity = severity_strings[notify->severity];
    }
    if (notify->visibilty <= SPICE_NOTIFY_VISIBILITY_HIGH) {
        visibility = visibility_strings[notify->visibilty];
    }

    if (notify->message_len &&
        notify->message_len <= in->dpos - sizeof(*notify)) {
        message_str = (char*)notify->message;
    }

    CHANNEL_DEBUG(channel, "%s -- %s%s #%u%s%.*s", __FUNCTION__,
            severity, visibility, notify->what,
            message_str ? ": " : "", notify->message_len,
            message_str ? message_str : "");
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_disconnect(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgDisconnect *disconnect = spice_msg_in_parsed(in);

    CHANNEL_DEBUG(channel, "%s: ts: %" PRIu64", reason: %u", __FUNCTION__,
                  disconnect->time_stamp, disconnect->reason);
}

typedef struct WaitForChannelData
{
    SpiceWaitForChannel *wait;
    SpiceChannel *channel;
} WaitForChannelData;

/* coroutine and main context */
static gboolean wait_for_channel(gpointer data)
{
    WaitForChannelData *wfc = data;
    SpiceChannelPrivate *c = wfc->channel->priv;
    SpiceChannel *wait_channel;

    wait_channel = spice_session_lookup_channel(c->session, wfc->wait->channel_id, wfc->wait->channel_type);
    g_return_val_if_fail(wait_channel != NULL, TRUE);

    if (wait_channel->priv->last_message_serial >= wfc->wait->message_serial)
        return TRUE;

    return FALSE;
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_wait_for_channels(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceChannelPrivate *c = channel->priv;
    SpiceMsgWaitForChannels *wfc = spice_msg_in_parsed(in);
    int i;

    for (i = 0; i < wfc->wait_count; ++i) {
        WaitForChannelData data = {
            .wait = wfc->wait_list + i,
            .channel = channel
        };

        CHANNEL_DEBUG(channel, "waiting for serial %" PRIu64 " (%d/%d)", data.wait->message_serial, i + 1, wfc->wait_count);
        if (g_coroutine_condition_wait(&c->coroutine, wait_for_channel, &data))
            CHANNEL_DEBUG(channel, "waiting for serial %"  PRIu64 ", done", data.wait->message_serial);
        else
            CHANNEL_DEBUG(channel, "waiting for serial %" PRIu64 ", cancelled", data.wait->message_serial);
    }
}

static void
get_msg_handler(SpiceChannel *channel, SpiceMsgIn *in, gpointer data)
{
    SpiceMsgIn **msg = data;

    g_return_if_fail(msg != NULL);
    g_return_if_fail(*msg == NULL);

    spice_msg_in_ref(in);
    *msg = in;
}

/* coroutine context */
G_GNUC_INTERNAL
void spice_channel_handle_migrate(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceMsgOut *out;
    SpiceMsgIn *data = NULL;
    SpiceMsgMigrate *mig = spice_msg_in_parsed(in);
    SpiceChannelPrivate *c = channel->priv;

    CHANNEL_DEBUG(channel, "%s: flags %u", __FUNCTION__, mig->flags);
    if (mig->flags & SPICE_MIGRATE_NEED_FLUSH) {
        /* if peer version > 1: pushing the mark msg before all other messgages and sending it,
         * and only it */
        if (c->peer_hdr.major_version == 1) {
            /* iterate_write is blocking and flushing all pending write */
            SPICE_CHANNEL_GET_CLASS(channel)->iterate_write(channel);
        }
        out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MIGRATE_FLUSH_MARK);
        spice_msg_out_send_internal(out);
    }
    if (mig->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) {
        spice_channel_recv_msg(channel, get_msg_handler, &data);
        if (!data) {
            g_critical("expected SPICE_MSG_MIGRATE_DATA, got empty message");
            goto end;
        } else if (spice_header_get_msg_type(data->header, c->use_mini_header) !=
                   SPICE_MSG_MIGRATE_DATA) {
            g_critical("expected SPICE_MSG_MIGRATE_DATA, got %d",
                      spice_header_get_msg_type(data->header, c->use_mini_header));
            goto end;
        }
    }

    /* swapping channels sockets */
    spice_session_channel_migrate(c->session, channel);

    /* pushing the MIGRATE_DATA before all other pending messages */
    if ((mig->flags & SPICE_MIGRATE_NEED_DATA_TRANSFER) && (data != NULL)) {
        out = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_MIGRATE_DATA);
        spice_marshaller_add(out->marshaller, data->data,
                             spice_header_get_msg_size(data->header, c->use_mini_header));
        spice_msg_out_send_internal(out);
    }

end:
    if (data)
        spice_msg_in_unref(data);
}


static void set_handlers(SpiceChannelClass *klass,
                         const spice_msg_handler* handlers, const int n)
{
    int i;

    g_array_set_size(klass->handlers, MAX(klass->handlers->len, n));
    for (i = 0; i < n; i++) {
        if (handlers[i])
            g_array_index(klass->handlers, spice_msg_handler, i) = handlers[i];
    }
}

static void spice_channel_add_base_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_SET_ACK ]                  = spice_channel_handle_set_ack,
        [ SPICE_MSG_PING ]                     = spice_channel_handle_ping,
        [ SPICE_MSG_NOTIFY ]                   = spice_channel_handle_notify,
        [ SPICE_MSG_DISCONNECTING ]            = spice_channel_handle_disconnect,
        [ SPICE_MSG_WAIT_FOR_CHANNELS ]        = spice_channel_handle_wait_for_channels,
        [ SPICE_MSG_MIGRATE ]                  = spice_channel_handle_migrate,
    };

    set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}

G_GNUC_INTERNAL
void spice_channel_set_handlers(SpiceChannelClass *klass,
                                const spice_msg_handler* handlers, const int n)
{
    /* FIXME: use class private (requires glib 2.24) */
    g_return_if_fail(klass->handlers == NULL);
    klass->handlers = g_array_sized_new(FALSE, TRUE, sizeof(spice_msg_handler), n);

    spice_channel_add_base_handlers(klass);
    set_handlers(klass, handlers, n);
}
