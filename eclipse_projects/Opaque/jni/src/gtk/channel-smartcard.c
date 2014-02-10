/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

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

#ifdef USE_SMARTCARD
#include <vreader.h>
#endif

#include "spice-client.h"
#include "spice-common.h"

#include "spice-channel-priv.h"
#include "smartcard-manager.h"
#include "smartcard-manager-priv.h"
#include "spice-session-priv.h"

/**
 * SECTION:channel-smartcard
 * @short_description: smartcard authentication
 * @title: Smartcard Channel
 * @section_id:
 * @see_also: #SpiceSmartcardManager, #SpiceSession
 * @stability: API Stable (channel in development)
 * @include: channel-smartcard.h
 *
 * The Spice protocol defines a set of messages to forward smartcard
 * information from the Spice client to the VM. This channel handles
 * these messages. While it's mainly focus on smartcard readers and
 * smartcards, it's also possible to use it with a software smartcard
 * (ie a set of 3 certificates from the client machine).
 * This class doesn't provide useful methods, see #SpiceSession properties
 * for a way to enable/disable this channel, and #SpiceSmartcardManager
 * if you want to detect smartcard reader hotplug/unplug, and smartcard
 * insertion/removal.
 */

#define SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_SMARTCARD_CHANNEL, SpiceSmartcardChannelPrivate))

struct _SpiceSmartcardChannelMessage {
#ifdef USE_SMARTCARD
    VSCMsgType message_type;
#endif
    SpiceMsgOut *message;
};
typedef struct _SpiceSmartcardChannelMessage SpiceSmartcardChannelMessage;


struct _SpiceSmartcardChannelPrivate {
    /* track readers that have been added but for which we didn't receive
     * an ack from the spice server yet. We rely on the fact that the
     * readers in this list are ordered by the time we sent the request to
     * the server. When we get an ack from the server for a reader addition,
     * we can pop the 1st entry to get the reader the ack corresponds to. */
    GList *pending_reader_additions;

    /* used to removals of readers that were not ack'ed yet by the spice
     * server */
    GHashTable *pending_reader_removals;

    /* used to track card insertions on readers that were not ack'ed yet
     * by the spice server */
    GHashTable *pending_card_insertions;

    /* next commands to be sent to the spice server. This is needed since
     * we have to wait for a command answer before sending the next one
     */
    GQueue *message_queue;

    /* message that is currently being processed by the spice server (ie last
     * message that was sent to the server)
     */
    SpiceSmartcardChannelMessage *in_flight_message;
};

G_DEFINE_TYPE(SpiceSmartcardChannel, spice_smartcard_channel, SPICE_TYPE_CHANNEL)

enum {

    SPICE_SMARTCARD_LAST_SIGNAL,
};

static void spice_smartcard_channel_up(SpiceChannel *channel);
static void handle_smartcard_msg(SpiceChannel *channel, SpiceMsgIn *in);
static void smartcard_message_free(SpiceSmartcardChannelMessage *message);

/* ------------------------------------------------------------------ */
#ifdef USE_SMARTCARD
static void reader_added_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data);
static void reader_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                              gpointer user_data);
static void card_inserted_cb(SpiceSmartcardManager *manager, VReader *reader,
                             gpointer user_data);
static void card_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data);
#endif

static void spice_smartcard_channel_init(SpiceSmartcardChannel *channel)
{
    SpiceSmartcardChannelPrivate *priv;

    channel->priv = SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(channel);
    priv = channel->priv;
    priv->message_queue = g_queue_new();

#ifdef USE_SMARTCARD
    priv->pending_card_insertions =
        g_hash_table_new_full(g_direct_hash, g_direct_equal,
                              (GDestroyNotify)vreader_free, NULL);
    priv->pending_reader_removals =
         g_hash_table_new_full(g_direct_hash, g_direct_equal,
                               (GDestroyNotify)vreader_free, NULL);
#endif
}

static void spice_smartcard_channel_constructed(GObject *object)
{
    SpiceSession *s = spice_channel_get_session(SPICE_CHANNEL(object));

    g_return_if_fail(s != NULL);

#ifdef USE_SMARTCARD
    if (!s->priv->migration_copy) {
        SpiceSmartcardChannel *channel = SPICE_SMARTCARD_CHANNEL(object);
        SpiceSmartcardManager *manager = spice_smartcard_manager_get();

        g_signal_connect(G_OBJECT(manager), "reader-added",
                         (GCallback)reader_added_cb, channel);
        g_signal_connect(G_OBJECT(manager), "reader-removed",
                         (GCallback)reader_removed_cb, channel);
        g_signal_connect(G_OBJECT(manager), "card-inserted",
                         (GCallback)card_inserted_cb, channel);
        g_signal_connect(G_OBJECT(manager), "card-removed",
                         (GCallback)card_removed_cb, channel);
    }
#endif

    if (G_OBJECT_CLASS(spice_smartcard_channel_parent_class)->constructed)
        G_OBJECT_CLASS(spice_smartcard_channel_parent_class)->constructed(object);

}

static void spice_smartcard_channel_finalize(GObject *obj)
{
    SpiceSmartcardChannelPrivate *c = SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(obj);

    if (c->pending_card_insertions != NULL) {
        g_hash_table_destroy(c->pending_card_insertions);
        c->pending_card_insertions = NULL;
    }
    if (c->pending_reader_removals != NULL) {
        g_hash_table_destroy(c->pending_reader_removals);
        c->pending_reader_removals = NULL;
    }
    if (c->message_queue != NULL) {
        g_queue_foreach(c->message_queue, (GFunc)smartcard_message_free, NULL);
        g_queue_free(c->message_queue);
        c->message_queue = NULL;
    }
    if (c->in_flight_message != NULL) {
        smartcard_message_free(c->in_flight_message);
        c->in_flight_message = NULL;
    }

    g_list_free(c->pending_reader_additions);
    c->pending_reader_additions = NULL;

    if (G_OBJECT_CLASS(spice_smartcard_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_smartcard_channel_parent_class)->finalize(obj);
}

static void spice_smartcard_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceSmartcardChannelPrivate *c = SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(channel);

    g_hash_table_remove_all(c->pending_card_insertions);
    g_hash_table_remove_all(c->pending_reader_removals);

    if (c->message_queue != NULL) {
        g_queue_foreach(c->message_queue, (GFunc)smartcard_message_free, NULL);
        g_queue_clear(c->message_queue);
    }

    if (c->in_flight_message != NULL) {
        smartcard_message_free(c->in_flight_message);
        c->in_flight_message = NULL;
    }

    g_list_free(c->pending_reader_additions);
    c->pending_reader_additions = NULL;

    SPICE_CHANNEL_CLASS(spice_smartcard_channel_parent_class)->channel_reset(channel, migrating);
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_SMARTCARD_DATA ] = handle_smartcard_msg,
    };
    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}

static void spice_smartcard_channel_class_init(SpiceSmartcardChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_smartcard_channel_finalize;
    gobject_class->constructed  = spice_smartcard_channel_constructed;

    channel_class->channel_up   = spice_smartcard_channel_up;
    channel_class->channel_reset = spice_smartcard_channel_reset;

    g_type_class_add_private(klass, sizeof(SpiceSmartcardChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
}

/* ------------------------------------------------------------------ */
/* private api                                                        */

static void
smartcard_message_free(SpiceSmartcardChannelMessage *message)
{
    if (message->message)
        spice_msg_out_unref(message->message);
    g_slice_free(SpiceSmartcardChannelMessage, message);
}

#if USE_SMARTCARD
static gboolean is_attached_to_server(VReader *reader)
{
    return (vreader_get_id(reader) != (vreader_id_t)-1);
}

static gboolean
spice_channel_has_pending_card_insertion(SpiceSmartcardChannel *channel,
                                         VReader *reader)
{
    return (g_hash_table_lookup(channel->priv->pending_card_insertions, reader) != NULL);
}

static void
spice_channel_queue_card_insertion(SpiceSmartcardChannel *channel,
                                   VReader *reader)
{
    vreader_reference(reader);
    g_hash_table_insert(channel->priv->pending_card_insertions,
                        reader, reader);
}

static void
spice_channel_drop_pending_card_insertion(SpiceSmartcardChannel *channel,
                                          VReader *reader)
{
    g_hash_table_remove(channel->priv->pending_card_insertions, reader);
}

static gboolean
spice_channel_has_pending_reader_removal(SpiceSmartcardChannel *channel,
                                         VReader *reader)
{
    return (g_hash_table_lookup(channel->priv->pending_reader_removals, reader) != NULL);
}

static void
spice_channel_queue_reader_removal(SpiceSmartcardChannel *channel,
                                   VReader *reader)
{
    vreader_reference(reader);
    g_hash_table_insert(channel->priv->pending_reader_removals,
                        reader, reader);
}

static void
spice_channel_drop_pending_reader_removal(SpiceSmartcardChannel *channel,
                                          VReader *reader)
{
    g_hash_table_remove(channel->priv->pending_reader_removals, reader);
}

static SpiceSmartcardChannelMessage *
smartcard_message_new(VSCMsgType msg_type, SpiceMsgOut *msg_out)
{
    SpiceSmartcardChannelMessage *message;

    message = g_slice_new0(SpiceSmartcardChannelMessage);
    message->message = msg_out;
    message->message_type = msg_type;

    return message;
}

/* Indicates that handling of the message that is currently in flight has
 * been completed. If needed, sends the next queued command to the server. */
static void
smartcard_message_complete_in_flight(SpiceSmartcardChannel *channel)
{
    g_return_if_fail(channel->priv->in_flight_message != NULL);

    smartcard_message_free(channel->priv->in_flight_message);
    channel->priv->in_flight_message = g_queue_pop_head(channel->priv->message_queue);
    if (channel->priv->in_flight_message != NULL) {
        spice_msg_out_send(channel->priv->in_flight_message->message);
        channel->priv->in_flight_message->message = NULL;
    }
}

static void smartcard_message_send(SpiceSmartcardChannel *channel,
                                   VSCMsgType msg_type,
                                   SpiceMsgOut *msg_out, gboolean queue)
{
    SpiceSmartcardChannelMessage *message;

    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    CHANNEL_DEBUG(channel, "send message %d, %s",
                  msg_type, queue ? "queued" : "now");
    if (!queue) {
        spice_msg_out_send(msg_out);
        return;
    }

    message = smartcard_message_new(msg_type, msg_out);
    if (channel->priv->in_flight_message == NULL) {
        g_return_if_fail(g_queue_is_empty(channel->priv->message_queue));
        channel->priv->in_flight_message = message;
        spice_msg_out_send(channel->priv->in_flight_message->message);
        channel->priv->in_flight_message->message = NULL;
    } else {
        g_queue_push_tail(channel->priv->message_queue, message);
    }
}

static void
send_msg_generic_with_data(SpiceSmartcardChannel *channel, VReader *reader,
                           VSCMsgType msg_type,
                           const uint8_t *data, gsize data_len,
                           gboolean serialize_msg)
{
    SpiceMsgOut *msg_out;
    VSCMsgHeader header = {
        .type = msg_type,
        .length = data_len
    };

    if(vreader_get_id(reader) == -1)
        header.reader_id = VSCARD_UNDEFINED_READER_ID;
    else
        header.reader_id = vreader_get_id(reader);

    msg_out = spice_msg_out_new(SPICE_CHANNEL(channel),
                                SPICE_MSGC_SMARTCARD_DATA);
    msg_out->marshallers->msgc_smartcard_header(msg_out->marshaller, &header);
    if ((data != NULL) && (data_len != 0)) {
        spice_marshaller_add(msg_out->marshaller, data, data_len);
    }

    smartcard_message_send(channel, msg_type, msg_out, serialize_msg);
}

static void send_msg_generic(SpiceSmartcardChannel *channel, VReader *reader,
                             VSCMsgType msg_type)
{
    send_msg_generic_with_data(channel, reader, msg_type, NULL, 0, TRUE);
}

static void send_msg_atr(SpiceSmartcardChannel *channel, VReader *reader)
{
#define MAX_ATR_LEN 40 //this should be defined in libcacard
    uint8_t atr[MAX_ATR_LEN];
    int atr_len = MAX_ATR_LEN;

    g_return_if_fail(vreader_get_id(reader) != VSCARD_UNDEFINED_READER_ID);
    vreader_power_on(reader, atr, &atr_len);
    send_msg_generic_with_data(channel, reader, VSC_ATR, atr, atr_len, TRUE);
}

static void reader_added_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data)
{
    SpiceSmartcardChannel *channel = SPICE_SMARTCARD_CHANNEL(user_data);
    const char *reader_name = vreader_get_name(reader);

    channel->priv->pending_reader_additions =
        g_list_append(channel->priv->pending_reader_additions, reader);

    send_msg_generic_with_data(channel, reader, VSC_ReaderAdd,
                               (uint8_t*)reader_name, strlen(reader_name), TRUE);
}

static void reader_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                              gpointer user_data)
{
    SpiceSmartcardChannel *channel = SPICE_SMARTCARD_CHANNEL(user_data);

    if (is_attached_to_server(reader)) {
        send_msg_generic(channel, reader, VSC_ReaderRemove);
    } else {
        spice_channel_queue_reader_removal(channel, reader);
    }
}

/* ------------------------------------------------------------------ */
/* callbacks                                                          */
static void card_inserted_cb(SpiceSmartcardManager *manager, VReader *reader,
                             gpointer user_data)
{
    SpiceSmartcardChannel *channel = SPICE_SMARTCARD_CHANNEL(user_data);

    if (is_attached_to_server(reader)) {
        send_msg_atr(channel, reader);
    } else {
        spice_channel_queue_card_insertion(channel, reader);
    }
}

static void card_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data)
{
    SpiceSmartcardChannel *channel = SPICE_SMARTCARD_CHANNEL(user_data);

    if (is_attached_to_server(reader)) {
        send_msg_generic(channel, reader, VSC_CardRemove);
    } else {
        /* this does nothing when reader has no card insertion pending */
        spice_channel_drop_pending_card_insertion(channel, reader);
    }
}
#endif /* USE_SMARTCARD */

static void spice_smartcard_channel_up_cb(GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data)
{
    SpiceChannel *channel = SPICE_CHANNEL(user_data);

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_IS_SESSION(source_object));

    if (!spice_channel_get_session(SPICE_CHANNEL(channel))->priv->migration_copy) {
        GError *error = NULL;

        spice_smartcard_manager_init_finish(SPICE_SESSION(source_object),
                                            res, &error);
        if (error)
            g_warning("%s", error->message);
        g_clear_error(&error);
    }
}

static void spice_smartcard_channel_up(SpiceChannel *channel)
{
    spice_smartcard_manager_init_async(spice_channel_get_session(channel),
                                       g_cancellable_new(),
                                       spice_smartcard_channel_up_cb,
                                       channel);
}

static void handle_smartcard_msg(SpiceChannel *channel, SpiceMsgIn *in)
{
#ifdef USE_SMARTCARD
    SpiceSmartcardChannelPrivate *priv = SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(channel);
    SpiceMsgSmartcard *msg = spice_msg_in_parsed(in);
    VReader *reader;

    priv = SPICE_SMARTCARD_CHANNEL_GET_PRIVATE(channel);
    CHANNEL_DEBUG(channel, "handle msg %d", msg->type);
    switch (msg->type) {
        case VSC_Error:
            g_return_if_fail(priv->in_flight_message != NULL);
            CHANNEL_DEBUG(channel, "in flight %d", priv->in_flight_message->message_type);
            switch (priv->in_flight_message->message_type) {
                case VSC_ReaderAdd:
                    g_return_if_fail(priv->pending_reader_additions != NULL);
                    reader = priv->pending_reader_additions->data;
                    g_return_if_fail(reader != NULL);
                    g_return_if_fail(vreader_get_id(reader) == -1);
                    priv->pending_reader_additions =
                        g_list_delete_link(priv->pending_reader_additions,
                                           priv->pending_reader_additions);
                    vreader_set_id(reader, msg->reader_id);

                    if (spice_channel_has_pending_card_insertion(SPICE_SMARTCARD_CHANNEL(channel), reader)) {
                        send_msg_atr(SPICE_SMARTCARD_CHANNEL(channel), reader);
                        spice_channel_drop_pending_card_insertion(SPICE_SMARTCARD_CHANNEL(channel), reader);
                    }

                    if (spice_channel_has_pending_reader_removal(SPICE_SMARTCARD_CHANNEL(channel), reader)) {
                        send_msg_generic(SPICE_SMARTCARD_CHANNEL(channel),
                                         reader, VSC_CardRemove);
                        spice_channel_drop_pending_reader_removal(SPICE_SMARTCARD_CHANNEL(channel), reader);
                    }
                    break;
                case VSC_APDU:
                case VSC_ATR:
                case VSC_CardRemove:
                case VSC_Error:
                case VSC_ReaderRemove:
                    break;
                default:
                    g_warning("Unexpected message: %d", priv->in_flight_message->message_type);
                    break;
            }
            smartcard_message_complete_in_flight(SPICE_SMARTCARD_CHANNEL(channel));

            break;

        case VSC_APDU:
        case VSC_Init: {
            const unsigned int APDU_BUFFER_SIZE = 270;
            VReaderStatus reader_status;
            uint8_t data_out[APDU_BUFFER_SIZE + sizeof(uint32_t)];
            int data_out_len = sizeof(data_out);

            g_return_if_fail(msg->reader_id != VSCARD_UNDEFINED_READER_ID);
            reader = vreader_get_reader_by_id(msg->reader_id);
            g_return_if_fail(reader != NULL); //FIXME: add log message

            reader_status = vreader_xfr_bytes(reader,
                                              msg->data, msg->length,
                                              data_out, &data_out_len);
            if (reader_status == VREADER_OK) {
                send_msg_generic_with_data(SPICE_SMARTCARD_CHANNEL(channel),
                                           reader, VSC_APDU,
                                           data_out, data_out_len, FALSE);
            } else {
                uint32_t error_code;
                error_code = GUINT32_TO_LE(reader_status);
                send_msg_generic_with_data(SPICE_SMARTCARD_CHANNEL(channel),
                                           reader, VSC_Error,
                                           (uint8_t*)&error_code,
                                           sizeof (error_code), FALSE);
            }
            break;
        }
        default:
            g_return_if_reached();
    }
#endif
}
