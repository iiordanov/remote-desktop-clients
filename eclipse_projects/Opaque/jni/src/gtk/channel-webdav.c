/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2013 Red Hat, Inc.

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

#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"
#include "spice-session-priv.h"
#include "spice-marshal.h"
#include "glib-compat.h"
#include "vmcstream.h"
#include "giopipe.h"

/**
 * SECTION:channel-webdav
 * @short_description: exports a directory
 * @title: WebDAV Channel
 * @section_id:
 * @see_also: #SpiceChannel
 * @stability: Stable
 * @include: channel-webdav.h
 *
 * The "webdav" channel exports a directory to the guest for file
 * manipulation (read/write/copy etc). The underlying protocol is
 * implemented using WebDAV (RFC 4918).
 *
 * By default, the shared directory is the one associated with GLib
 * %G_USER_DIRECTORY_PUBLIC_SHARE. You can specify a different
 * directory with #SpiceSession #SpiceSession:shared-dir property.
 *
 * Since: 0.24
 */

#define SPICE_WEBDAV_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_WEBDAV_CHANNEL, SpiceWebdavChannelPrivate))

typedef struct _OutputQueue OutputQueue;

struct _SpiceWebdavChannelPrivate {
    SpiceVmcStream *stream;
    GCancellable *cancellable;
    GHashTable *clients;
    OutputQueue *queue;

    gboolean demuxing;
    struct _demux {
        gint64 client;
        guint16 size;
        guint8 *buf;
    } demux;
};

G_DEFINE_TYPE(SpiceWebdavChannel, spice_webdav_channel, SPICE_TYPE_PORT_CHANNEL)

static void spice_webdav_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg);

struct _OutputQueue {
    GOutputStream *output;
    gboolean flushing;
    guint idle_id;
    GQueue *queue;
};

typedef struct _OutputQueueElem {
    OutputQueue *queue;
    const guint8 *buf;
    gsize size;
    GFunc pushed_cb;
    gpointer user_data;
} OutputQueueElem;

static OutputQueue* output_queue_new(GOutputStream *output)
{
    OutputQueue *queue = g_new0(OutputQueue, 1);

    queue->output = g_object_ref(output);
    queue->queue = g_queue_new();

    return queue;
}

static void output_queue_free(OutputQueue *queue)
{
    g_warn_if_fail(g_queue_get_length(queue->queue) == 0);
    g_warn_if_fail(!queue->flushing);

    g_queue_free_full(queue->queue, g_free);
    g_clear_object(&queue->output);
    if (queue->idle_id)
        g_source_remove(queue->idle_id);
    g_free(queue);
}

static gboolean output_queue_idle(gpointer user_data);

static void output_queue_flush_cb(GObject *source_object,
                                  GAsyncResult *res,
                                  gpointer user_data)
{
    GError *error = NULL;
    OutputQueueElem *e = user_data;
    OutputQueue *q = e->queue;

    q->flushing = FALSE;
    g_output_stream_flush_finish(G_OUTPUT_STREAM(source_object),
                                 res, &error);
    if (error)
        g_warning("error: %s", error->message);

    g_clear_error(&error);

    if (!q->idle_id)
        q->idle_id = g_idle_add(output_queue_idle, q);

    g_free(e);
}

static gboolean output_queue_idle(gpointer user_data)
{
    OutputQueue *q = user_data;
    OutputQueueElem *e;
    GError *error = NULL;

    if (q->flushing) {
        q->idle_id = 0;
        return FALSE;
    }

    e = g_queue_pop_head(q->queue);
    if (!e) {
        q->idle_id = 0;
        return FALSE;
    }

    if (!g_output_stream_write_all(q->output, e->buf, e->size, NULL, NULL, &error))
        goto err;
    else if (e->pushed_cb)
        e->pushed_cb(q, e->user_data);

    q->flushing = TRUE;
    g_output_stream_flush_async(q->output, G_PRIORITY_DEFAULT, NULL, output_queue_flush_cb, e);

    return TRUE;

err:
    g_warning("failed to write to output stream");
    if (error)
        g_warning("error: %s", error->message);
    g_clear_error(&error);

    q->idle_id = 0;
    return FALSE;
}

static void output_queue_push(OutputQueue *q, const guint8 *buf, gsize size,
                              GFunc pushed_cb, gpointer user_data)
{
    OutputQueueElem *e = g_new(OutputQueueElem, 1);

    e->buf = buf;
    e->size = size;
    e->pushed_cb = pushed_cb;
    e->user_data = user_data;
    e->queue = q;
    g_queue_push_tail(q->queue, e);

    if (!q->idle_id && !q->flushing)
        q->idle_id = g_idle_add(output_queue_idle, q);
}

typedef struct Client
{
    guint refs;
    SpiceWebdavChannel *self;
    GIOStream *pipe;
    gint64 id;
    GCancellable *cancellable;

    struct _mux {
        gint64 id;
        guint16 size;
        guint8 *buf;
    } mux;
} Client;

static void
client_unref(Client *client)
{
    if (--client->refs > 0)
        return;

    g_free(client->mux.buf);

    g_object_unref(client->pipe);
    g_object_unref(client->cancellable);

    g_free(client);
}

static Client *
client_ref(Client *client)
{
    client->refs++;
    return client;
}

static void client_start_read(SpiceWebdavChannel *self, Client *client);

static void remove_client(SpiceWebdavChannel *self, Client *client)
{
    SpiceWebdavChannelPrivate *c;

    if (g_cancellable_is_cancelled(client->cancellable))
        return;

    g_cancellable_cancel(client->cancellable);

    c = self->priv;
    g_hash_table_remove(c->clients, &client->id);
}

static void mux_pushed_cb(OutputQueue *q, gpointer user_data)
{
    Client *client = user_data;

    if (client->mux.size == 0) {
        remove_client(client->self, client);
    } else {
        client_start_read(client->self, client);
    }

    client_unref(client);
}

#define MAX_MUX_SIZE G_MAXUINT16

static void server_reply_cb(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
    Client *client = user_data;
    SpiceWebdavChannel *self = client->self;
    SpiceWebdavChannelPrivate *c = self->priv;
    GError *err = NULL;
    gssize size;

    size = g_input_stream_read_finish(G_INPUT_STREAM(source_object), res, &err);
    if (err || g_cancellable_is_cancelled(client->cancellable))
        goto end;

    g_return_if_fail(size <= MAX_MUX_SIZE);
    g_return_if_fail(size >= 0);
    client->mux.size = size;

    output_queue_push(c->queue, (guint8 *)&client->mux.id, sizeof(gint64), NULL, NULL);
    client->mux.size = GUINT16_TO_LE(client->mux.size);
    output_queue_push(c->queue, (guint8 *)&client->mux.size, sizeof(guint16), NULL, NULL);
    output_queue_push(c->queue, (guint8 *)client->mux.buf, size, (GFunc)mux_pushed_cb, client);

    return;

end:
    if (err) {
        if (!g_cancellable_is_cancelled(client->cancellable))
            g_warning("read error: %s", err->message);
        remove_client(self, client);
        g_clear_error(&err);
    }

    client_unref(client);
}

static void client_start_read(SpiceWebdavChannel *self, Client *client)
{
    GInputStream *input;

    input = g_io_stream_get_input_stream(G_IO_STREAM(client->pipe));
    g_input_stream_read_async(input, client->mux.buf, MAX_MUX_SIZE,
                              G_PRIORITY_DEFAULT, client->cancellable, server_reply_cb,
                              client_ref(client));
}

static void start_demux(SpiceWebdavChannel *self);

#ifdef USE_PHODAV
static void demux_to_client_finish(SpiceWebdavChannel *self,
                                   Client *client, gboolean fail)
{
    SpiceWebdavChannelPrivate *c = self->priv;

    if (fail) {
        remove_client(self, client);
    }

    c->demuxing = FALSE;
    start_demux(self);
}

static void demux_to_client_cb(GObject *source, GAsyncResult *result, gpointer user_data)
{
    Client *client = user_data;
    SpiceWebdavChannelPrivate *c = client->self->priv;
    GError *error = NULL;
    gboolean fail;
    gsize size;

    g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result, &size, &error);

    if (error) {
        CHANNEL_DEBUG(client->self, "write failed: %s", error->message);
        g_clear_error(&error);
    }

    fail = (size != c->demux.size);
    g_warn_if_fail(size == c->demux.size);
    demux_to_client_finish(client->self, client, fail);
}
#endif

static void demux_to_client(SpiceWebdavChannel *self,
                            Client *client)
{
#ifdef USE_PHODAV
    SpiceWebdavChannelPrivate *c = self->priv;
    gsize size = c->demux.size;

    CHANNEL_DEBUG(self, "pushing %"G_GSIZE_FORMAT" to client %p", size, client);

    if (size > 0) {
        g_output_stream_write_all_async(g_io_stream_get_output_stream(client->pipe),
                                        c->demux.buf, size, G_PRIORITY_DEFAULT,
                                        c->cancellable, demux_to_client_cb, client);
        return;
    } else {
        /* Nothing to write */
        demux_to_client_finish(self, client, FALSE);
    }
#endif
}

static void start_client(SpiceWebdavChannel *self)
{
#ifdef USE_PHODAV
    SpiceWebdavChannelPrivate *c = self->priv;
    Client *client;
    GIOStream *peer = NULL;
    SpiceSession *session;
    SoupServer *server;
    GSocketAddress *addr;
    GError *error = NULL;

    session = spice_channel_get_session(SPICE_CHANNEL(self));
    server = phodav_server_get_soup_server(spice_session_get_webdav_server(session));

    CHANNEL_DEBUG(self, "starting client %" G_GINT64_FORMAT, c->demux.client);

    client = g_new0(Client, 1);
    client->refs = 1;
    client->id = c->demux.client;
    client->self = self;
    client->mux.id = GINT64_TO_LE(client->id);
    client->mux.buf = g_malloc0(MAX_MUX_SIZE);
    client->cancellable = g_cancellable_new();
    spice_make_pipe(&client->pipe, &peer);

    addr = g_inet_socket_address_new_from_string ("127.0.0.1", 0);
    if (!soup_server_accept_iostream(server, peer, addr, addr, &error))
        goto fail;

    g_hash_table_insert(c->clients, &client->id, client);

    client_start_read(self, client);
    demux_to_client(self, client);

    g_clear_object(&addr);
    return;

fail:
    if (error)
        CHANNEL_DEBUG(self, "failed to start client: %s", error->message);

    g_clear_object(&addr);
    g_clear_object(&peer);
    g_clear_error(&error);
    client_unref(client);
#endif
}

static void data_read_cb(GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    SpiceWebdavChannel *self = user_data;
    SpiceWebdavChannelPrivate *c;
    Client *client;
    GError *error = NULL;
    gssize size;

    size = spice_vmc_input_stream_read_all_finish(G_INPUT_STREAM(source_object), res, &error);
    if (error) {
        g_warning("error: %s", error->message);
        g_clear_error(&error);
        return;
    }

    c = self->priv;
    g_return_if_fail(size == c->demux.size);

    client = g_hash_table_lookup(c->clients, &c->demux.client);

    if (client)
        demux_to_client(self, client);
    else
        start_client(self);
}


static void size_read_cb(GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
    SpiceWebdavChannel *self = user_data;
    SpiceWebdavChannelPrivate *c;
    GInputStream *istream = G_INPUT_STREAM(source_object);
    GError *error = NULL;
    gssize size;

    size = spice_vmc_input_stream_read_all_finish(G_INPUT_STREAM(source_object), res, &error);
    if (error || size != sizeof(guint16))
        goto end;

    c = self->priv;
    c->demux.size = GUINT16_FROM_LE(c->demux.size);
    spice_vmc_input_stream_read_all_async(istream,
        c->demux.buf, c->demux.size,
        G_PRIORITY_DEFAULT, c->cancellable, data_read_cb, self);
    return;

end:
    if (error) {
        g_warning("error: %s", error->message);
        g_clear_error(&error);
    }
}

static void client_read_cb(GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
    SpiceWebdavChannel *self = user_data;
    SpiceWebdavChannelPrivate *c = self->priv;
    GInputStream *istream = G_INPUT_STREAM(source_object);
    GError *error = NULL;
    gssize size;

    size = spice_vmc_input_stream_read_all_finish(G_INPUT_STREAM(source_object), res, &error);
    if (error || size != sizeof(gint64))
        goto end;

    c->demux.client = GINT64_FROM_LE(c->demux.client);
    spice_vmc_input_stream_read_all_async(istream,
        &c->demux.size, sizeof(guint16),
        G_PRIORITY_DEFAULT, c->cancellable, size_read_cb, self);
    return;

end:
    if (error) {
        g_warning("error: %s", error->message);
        g_clear_error(&error);
    }
}

static void start_demux(SpiceWebdavChannel *self)
{
    SpiceWebdavChannelPrivate *c = self->priv;
    GInputStream *istream = g_io_stream_get_input_stream(G_IO_STREAM(c->stream));

    if (c->demuxing)
        return;

    c->demuxing = TRUE;

    CHANNEL_DEBUG(self, "start demux");
    spice_vmc_input_stream_read_all_async(istream, &c->demux.client, sizeof(gint64),
        G_PRIORITY_DEFAULT, c->cancellable, client_read_cb, self);

}

static void port_event(SpiceWebdavChannel *self, gint event)
{
    SpiceWebdavChannelPrivate *c = self->priv;

    CHANNEL_DEBUG(self, "port event:%d", event);
    if (event == SPICE_PORT_EVENT_OPENED) {
        g_cancellable_reset(c->cancellable);
        start_demux(self);
    } else {
        g_cancellable_cancel(c->cancellable);
        c->demuxing = FALSE;
        g_hash_table_remove_all(c->clients);
    }
}

static void client_remove_unref(gpointer data)
{
    Client *client = data;

    g_cancellable_cancel(client->cancellable);
    client_unref(client);
}

static void spice_webdav_channel_init(SpiceWebdavChannel *channel)
{
    SpiceWebdavChannelPrivate *c = SPICE_WEBDAV_CHANNEL_GET_PRIVATE(channel);

    channel->priv = c;
    c->stream = spice_vmc_stream_new(SPICE_CHANNEL(channel));
    c->cancellable = g_cancellable_new();
    c->clients = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                       NULL, client_remove_unref);
    c->demux.buf = g_malloc0(MAX_MUX_SIZE);

    GOutputStream *ostream = g_io_stream_get_output_stream(G_IO_STREAM(c->stream));
    c->queue = output_queue_new(ostream);
}

static void spice_webdav_channel_finalize(GObject *object)
{
    SpiceWebdavChannelPrivate *c = SPICE_WEBDAV_CHANNEL(object)->priv;

    g_free(c->demux.buf);

    G_OBJECT_CLASS(spice_webdav_channel_parent_class)->finalize(object);
}

static void spice_webdav_channel_dispose(GObject *object)
{
    SpiceWebdavChannelPrivate *c = SPICE_WEBDAV_CHANNEL(object)->priv;

    g_cancellable_cancel(c->cancellable);
    g_clear_object(&c->cancellable);
    g_clear_pointer(&c->queue, output_queue_free);
    g_clear_object(&c->stream);
    g_hash_table_unref(c->clients);

    G_OBJECT_CLASS(spice_webdav_channel_parent_class)->dispose(object);
}

static void spice_webdav_channel_up(SpiceChannel *channel)
{
    CHANNEL_DEBUG(channel, "up");
}

static void spice_webdav_channel_class_init(SpiceWebdavChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->dispose      = spice_webdav_channel_dispose;
    gobject_class->finalize     = spice_webdav_channel_finalize;
    channel_class->handle_msg   = spice_webdav_handle_msg;
    channel_class->channel_up   = spice_webdav_channel_up;

    g_signal_override_class_handler("port-event",
                                    SPICE_TYPE_WEBDAV_CHANNEL,
                                    G_CALLBACK(port_event));

    g_type_class_add_private(klass, sizeof(SpiceWebdavChannelPrivate));
}

/* coroutine context */
static void webdav_handle_msg(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceWebdavChannel *self = SPICE_WEBDAV_CHANNEL(channel);
    SpiceWebdavChannelPrivate *c = self->priv;
    int size;
    uint8_t *buf;

    buf = spice_msg_in_raw(in, &size);
    CHANNEL_DEBUG(channel, "len:%d buf:%p", size, buf);

    spice_vmc_input_stream_co_data(
        SPICE_VMC_INPUT_STREAM(g_io_stream_get_input_stream(G_IO_STREAM(c->stream))),
        buf, size);
}


/* coroutine context */
static void spice_webdav_handle_msg(SpiceChannel *channel, SpiceMsgIn *msg)
{
    int type = spice_msg_in_type(msg);
    SpiceChannelClass *parent_class;

    parent_class = SPICE_CHANNEL_CLASS(spice_webdav_channel_parent_class);

    if (type == SPICE_MSG_SPICEVMC_DATA)
        webdav_handle_msg(channel, msg);
    else if (parent_class->handle_msg)
        parent_class->handle_msg(channel, msg);
    else
        g_return_if_reached();
}
