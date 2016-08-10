/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
  Copyright (C) 2015 Red Hat, Inc.

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

#include <string.h>
#include <errno.h>

#include "giopipe.h"

#define TYPE_PIPE_INPUT_STREAM         (pipe_input_stream_get_type ())
#define PIPE_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_PIPE_INPUT_STREAM, PipeInputStream))
#define PIPE_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_PIPE_INPUT_STREAM, PipeInputStreamClass))
#define IS_PIPE_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_PIPE_INPUT_STREAM))
#define IS_PIPE_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_PIPE_INPUT_STREAM))
#define PIPE_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_PIPE_INPUT_STREAM, PipeInputStreamClass))

typedef struct _PipeInputStreamClass                              PipeInputStreamClass;
typedef struct _PipeInputStream                                   PipeInputStream;
typedef struct _PipeOutputStream                                  PipeOutputStream;

struct _PipeInputStream
{
    GInputStream parent_instance;

    PipeOutputStream *peer;
    gssize read;

    /* GIOstream:closed is protected against pending operations, so we
     * use an additional close flag to cancel those when the peer is
     * closing.
     */
    gboolean peer_closed;
    GList *sources;
};

struct _PipeInputStreamClass
{
    GInputStreamClass parent_class;
};

#define TYPE_PIPE_OUTPUT_STREAM         (pipe_output_stream_get_type ())
#define PIPE_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_PIPE_OUTPUT_STREAM, PipeOutputStream))
#define PIPE_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_PIPE_OUTPUT_STREAM, PipeOutputStreamClass))
#define IS_PIPE_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_PIPE_OUTPUT_STREAM))
#define IS_PIPE_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_PIPE_OUTPUT_STREAM))
#define PIPE_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_PIPE_OUTPUT_STREAM, PipeOutputStreamClass))

typedef struct _PipeOutputStreamClass                             PipeOutputStreamClass;

struct _PipeOutputStream
{
    GOutputStream parent_instance;

    PipeInputStream *peer;
    const gchar *buffer;
    gsize count;
    gboolean peer_closed;
    GList *sources;
};

struct _PipeOutputStreamClass
{
    GOutputStreamClass parent_class;
};

static void pipe_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface);
static void pipe_input_stream_check_source (PipeInputStream *self);
static void pipe_output_stream_check_source (PipeOutputStream *self);

G_DEFINE_TYPE_WITH_CODE (PipeInputStream, pipe_input_stream, G_TYPE_INPUT_STREAM,
                         G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM,
                                                pipe_input_stream_pollable_iface_init))

static gssize
pipe_input_stream_read (GInputStream  *stream,
                        void          *buffer,
                        gsize          count,
                        GCancellable  *cancellable,
                        GError       **error)
{
    PipeInputStream *self = PIPE_INPUT_STREAM (stream);

    g_return_val_if_fail(count > 0, -1);

    if (g_input_stream_is_closed (stream) || self->peer_closed) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                             "Stream is already closed");
        return -1;
    }

    if (!self->peer->buffer) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
                             g_strerror(EAGAIN));
        return -1;
    }

    count = MIN(self->peer->count, count);
    memcpy(buffer, self->peer->buffer, count);
    self->read = count;
    self->peer->buffer = NULL;

    //g_debug("read %p :%"G_GSIZE_FORMAT, self->peer, count);
    /* schedule peer source */
    pipe_output_stream_check_source(self->peer);

    return count;
}

static GList *
set_all_sources_ready (GList *sources)
{
    GList *it = sources;
    while (it != NULL) {
        GSource *s = it->data;
        GList *next = it->next;

        if (s == NULL || g_source_is_destroyed(s)) {
            /* remove */
            sources = g_list_delete_link(sources, it);
            g_source_unref(s);
        } else {
            /* dispatch */
            g_source_set_ready_time(s, 0);
        }
        it = next;
    }
    return sources;
}

static void
pipe_input_stream_check_source (PipeInputStream *self)
{
    if (g_pollable_input_stream_is_readable(G_POLLABLE_INPUT_STREAM(self)))
        self->sources = set_all_sources_ready(self->sources);
}

static gboolean
pipe_input_stream_close (GInputStream  *stream,
                         GCancellable   *cancellable,
                         GError        **error)
{
    PipeInputStream *self;

    self = PIPE_INPUT_STREAM(stream);

    if (self->peer) {
        /* ignore any pending errors */
        self->peer->peer_closed = TRUE;
        g_output_stream_close(G_OUTPUT_STREAM(self->peer), cancellable, NULL);
        pipe_output_stream_check_source(self->peer);
    }

    return TRUE;
}

static void
pipe_input_stream_close_async (GInputStream       *stream,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             data)
{
    GTask *task;

    task = g_task_new (stream, cancellable, callback, data);

    /* will always return TRUE */
    pipe_input_stream_close (stream, cancellable, NULL);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
pipe_input_stream_close_finish (GInputStream  *stream,
                                GAsyncResult   *result,
                                GError        **error)
{
    g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
pipe_input_stream_init (PipeInputStream *self)
{
    self->read = -1;
}

static void
pipe_input_stream_dispose(GObject *object)
{
    PipeInputStream *self;

    self = PIPE_INPUT_STREAM(object);

    if (self->peer) {
        g_object_remove_weak_pointer(G_OBJECT(self->peer), (gpointer*)&self->peer);
        self->peer = NULL;
    }

    g_list_free_full (self->sources, (GDestroyNotify) g_source_unref);
    self->sources = NULL;

    G_OBJECT_CLASS(pipe_input_stream_parent_class)->dispose (object);
}

static void
pipe_input_stream_class_init (PipeInputStreamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GInputStreamClass *istream_class = G_INPUT_STREAM_CLASS (klass);

    istream_class->read_fn  = pipe_input_stream_read;
    istream_class->close_fn = pipe_input_stream_close;
    istream_class->close_async  = pipe_input_stream_close_async;
    istream_class->close_finish = pipe_input_stream_close_finish;

    gobject_class->dispose = pipe_input_stream_dispose;
}

static gboolean
pipe_input_stream_is_readable (GPollableInputStream *stream)
{
    PipeInputStream *self = PIPE_INPUT_STREAM (stream);
    gboolean readable;

    readable = (self->peer && self->peer->buffer && self->read == -1) || self->peer_closed;
    //g_debug("readable %p %d", self->peer, readable);

    return readable;
}

static GSource *
pipe_input_stream_create_source (GPollableInputStream *stream,
                                 GCancellable         *cancellable)
{
    PipeInputStream *self = PIPE_INPUT_STREAM(stream);
    GSource *pollable_source;

    pollable_source = g_pollable_source_new_full (self, NULL, cancellable);
    self->sources = g_list_prepend (self->sources, g_source_ref (pollable_source));

    return pollable_source;
}

static void
pipe_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
    iface->is_readable   = pipe_input_stream_is_readable;
    iface->create_source = pipe_input_stream_create_source;
}

static void pipe_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PipeOutputStream, pipe_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM,
                                                pipe_output_stream_pollable_iface_init))

static gssize
pipe_output_stream_write (GOutputStream  *stream,
                          const void     *buffer,
                          gsize           count,
                          GCancellable   *cancellable,
                          GError        **error)
{
    PipeOutputStream *self = PIPE_OUTPUT_STREAM(stream);
    PipeInputStream *peer = self->peer;

    //g_debug("write %p :%"G_GSIZE_FORMAT, stream, count);
    if (g_output_stream_is_closed (stream) || self->peer_closed) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                             "Stream is already closed");
        return -1;
    }

    /* this abuses pollable stream, writing sync would likely lead to
       crashes, since the buffer pointer would become invalid, a
       generic solution would need a copy..
    */
    g_return_val_if_fail(self->buffer == buffer || self->buffer == NULL, -1);
    self->buffer = buffer;
    self->count = count;

    pipe_input_stream_check_source(self->peer);

    if (peer->read < 0) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
                             g_strerror (EAGAIN));
        return -1;
    }

    g_assert(peer->read <= self->count);
    count = peer->read;

    self->buffer = NULL;
    self->count = 0;
    peer->read = -1;

    return count;
}

static void
pipe_output_stream_init (PipeOutputStream *stream)
{
}

static void
pipe_output_stream_dispose(GObject *object)
{
    PipeOutputStream *self;

    self = PIPE_OUTPUT_STREAM(object);

    if (self->peer) {
        g_object_remove_weak_pointer(G_OBJECT(self->peer), (gpointer*)&self->peer);
        self->peer = NULL;
    }

    g_list_free_full (self->sources, (GDestroyNotify) g_source_unref);
    self->sources = NULL;

    G_OBJECT_CLASS(pipe_output_stream_parent_class)->dispose (object);
}

static void
pipe_output_stream_check_source (PipeOutputStream *self)
{
    if (g_pollable_output_stream_is_writable(G_POLLABLE_OUTPUT_STREAM(self)))
        self->sources = set_all_sources_ready(self->sources);
}

static gboolean
pipe_output_stream_close (GOutputStream  *stream,
                          GCancellable   *cancellable,
                          GError        **error)
{
    PipeOutputStream *self;

    self = PIPE_OUTPUT_STREAM(stream);

    if (self->peer) {
        /* ignore any pending errors */
        self->peer->peer_closed = TRUE;
        g_input_stream_close(G_INPUT_STREAM(self->peer), cancellable, NULL);
        pipe_input_stream_check_source(self->peer);
    }

    return TRUE;
}

static void
pipe_output_stream_close_async (GOutputStream       *stream,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             data)
{
    GTask *task;

    task = g_task_new (stream, cancellable, callback, data);

    /* will always return TRUE */
    pipe_output_stream_close (stream, cancellable, NULL);

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

static gboolean
pipe_output_stream_close_finish (GOutputStream  *stream,
                                 GAsyncResult   *result,
                                 GError        **error)
{
    g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}


static void
pipe_output_stream_class_init (PipeOutputStreamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GOutputStreamClass *ostream_class = G_OUTPUT_STREAM_CLASS (klass);

    ostream_class->write_fn = pipe_output_stream_write;
    ostream_class->close_fn = pipe_output_stream_close;
    ostream_class->close_async  = pipe_output_stream_close_async;
    ostream_class->close_finish = pipe_output_stream_close_finish;

    gobject_class->dispose = pipe_output_stream_dispose;
}

static gboolean
pipe_output_stream_is_writable (GPollableOutputStream *stream)
{
    PipeOutputStream *self = PIPE_OUTPUT_STREAM(stream);
    gboolean writable;

    writable = self->buffer == NULL || self->peer->read >= 0;
    //g_debug("writable %p %d", self, writable);

    return writable;
}

static GSource *
pipe_output_stream_create_source (GPollableOutputStream *stream,
                                  GCancellable          *cancellable)
{
    PipeOutputStream *self = PIPE_OUTPUT_STREAM(stream);
    GSource *pollable_source;

    pollable_source = g_pollable_source_new_full (self, NULL, cancellable);
    self->sources = g_list_prepend (self->sources, g_source_ref (pollable_source));

    return pollable_source;
}

static void
pipe_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface)
{
    iface->is_writable = pipe_output_stream_is_writable;
    iface->create_source = pipe_output_stream_create_source;
}

G_GNUC_INTERNAL void
make_gio_pipe(GInputStream **input, GOutputStream **output)
{
    PipeInputStream *in;
    PipeOutputStream *out;

    g_return_if_fail(input != NULL && *input == NULL);
    g_return_if_fail(output != NULL && *output == NULL);

    in = g_object_new(TYPE_PIPE_INPUT_STREAM, NULL);
    out = g_object_new(TYPE_PIPE_OUTPUT_STREAM, NULL);

    out->peer = in;
    g_object_add_weak_pointer(G_OBJECT(in), (gpointer*)&out->peer);

    in->peer = out;
    g_object_add_weak_pointer(G_OBJECT(out), (gpointer*)&in->peer);

    *input = G_INPUT_STREAM(in);
    *output = G_OUTPUT_STREAM(out);
}

G_GNUC_INTERNAL void
spice_make_pipe(GIOStream **p1, GIOStream **p2)
{
    GInputStream *in1 = NULL, *in2 = NULL;
    GOutputStream *out1 = NULL, *out2 = NULL;

    g_return_if_fail(p1 != NULL);
    g_return_if_fail(p2 != NULL);
    g_return_if_fail(*p1 == NULL);
    g_return_if_fail(*p2 == NULL);

    make_gio_pipe(&in1, &out2);
    make_gio_pipe(&in2, &out1);

    *p1 = g_simple_io_stream_new(in1, out1);
    *p2 = g_simple_io_stream_new(in2, out2);

    g_object_unref(in1);
    g_object_unref(in2);
    g_object_unref(out1);
    g_object_unref(out2);
}
