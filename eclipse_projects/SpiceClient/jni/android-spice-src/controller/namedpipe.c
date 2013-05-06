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

#include "namedpipe.h"

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

static void     spice_named_pipe_initable_iface_init (GInitableIface  *iface);
static gboolean spice_named_pipe_initable_init       (GInitable       *initable,
                                                      GCancellable    *cancellable,
                                                      GError         **error);

G_DEFINE_TYPE_WITH_CODE (SpiceNamedPipe, spice_named_pipe, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						spice_named_pipe_initable_iface_init));

enum
{
  PROP_0,
  PROP_NAME,
  PROP_HANDLE,
};

struct _SpiceNamedPipePrivate
{
  gchar *               name;
  GError *              construct_error;
  guint                 inited : 1;
  HANDLE                handle;
};

static void
spice_named_pipe_finalize (GObject *object)
{
  SpiceNamedPipe *np = SPICE_NAMED_PIPE (object);

  g_clear_error (&np->priv->construct_error);

  g_free (np->priv->name);
  np->priv->name = NULL;

  if (np->priv->handle)
    {
      CloseHandle (np->priv->handle);
      np->priv->handle = NULL;
    }

  if (G_OBJECT_CLASS (spice_named_pipe_parent_class)->finalize)
    G_OBJECT_CLASS (spice_named_pipe_parent_class)->finalize (object);
}

#define DEFAULT_PIPE_BUF_SIZE 4096

static void
spice_named_pipe_constructed (GObject *object)
{
  SpiceNamedPipe *np = SPICE_NAMED_PIPE (object);

  if (np->priv->handle)
    /* TODO: find a way to ensure user provided handle is a named
       pipe, in overlapped mode */
    goto end;

  np->priv->handle = CreateNamedPipe (np->priv->name,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES,
      DEFAULT_PIPE_BUF_SIZE, DEFAULT_PIPE_BUF_SIZE,
      0, NULL);

  if (np->priv->handle == INVALID_HANDLE_VALUE)
    {
      int errsv = GetLastError ();
      gchar *emsg = g_win32_error_message (errsv);

      g_set_error (&np->priv->construct_error,
                   G_IO_ERROR,
                   g_io_error_from_win32_error (errsv),
                   "Error CreateNamedPipe(): %s",
                   emsg);

      g_free (emsg);
      return;
    }

  /* TODO: we could have a client backlog by creating many pipes, the
     maximum number of outstanding connections.. or we could just let
     the named_pipe_listener take multiple NamedPipe instances */
end:
  g_assert (np->priv->handle != INVALID_HANDLE_VALUE);
  return;
}

static void
spice_named_pipe_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  SpiceNamedPipe *np = SPICE_NAMED_PIPE (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, np->priv->name);
        break;
      case PROP_HANDLE:
        g_value_set_pointer (value, np->priv->handle);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spice_named_pipe_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  SpiceNamedPipe *np = SPICE_NAMED_PIPE (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_free (np->priv->name);
        np->priv->name = g_value_dup_string (value);
        break;
      case PROP_HANDLE:
        np->priv->handle = g_value_get_pointer (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spice_named_pipe_class_init (SpiceNamedPipeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SpiceNamedPipePrivate));

  gobject_class->set_property = spice_named_pipe_set_property;
  gobject_class->get_property = spice_named_pipe_get_property;
  gobject_class->finalize = spice_named_pipe_finalize;
  gobject_class->constructed = spice_named_pipe_constructed;

  g_object_class_install_property (gobject_class, PROP_NAME,
				   g_param_spec_string ("name",
                                                        "Pipe Name",
                                                        "The NamedPipe name",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HANDLE,
                                   g_param_spec_pointer ("handle",
                                                         "Pipe handle",
                                                         "The pipe handle",
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
spice_named_pipe_init (SpiceNamedPipe *np)
{
  np->priv = G_TYPE_INSTANCE_GET_PRIVATE (np,
                                          SPICE_TYPE_NAMED_PIPE,
                                          SpiceNamedPipePrivate);
}

static gboolean
spice_named_pipe_initable_init (GInitable *initable,
                                GCancellable *cancellable,
                                GError  **error)
{
  SpiceNamedPipe  *np;

  g_return_val_if_fail (SPICE_IS_NAMED_PIPE (initable), FALSE);

  np = SPICE_NAMED_PIPE (initable);

  if (cancellable != NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           "Cancellable initialization not supported");
      return FALSE;
    }

  np->priv->inited = TRUE;

  if (np->priv->construct_error)
    {
      if (error)
	*error = g_error_copy (np->priv->construct_error);
      return FALSE;
    }


  return TRUE;
}

static void
spice_named_pipe_initable_iface_init (GInitableIface *iface)
{
  iface->init = spice_named_pipe_initable_init;
}

SpiceNamedPipe *
spice_named_pipe_new (const gchar *name, GError **error)
{
  return SPICE_NAMED_PIPE (g_initable_new (SPICE_TYPE_NAMED_PIPE,
                                           NULL, error,
                                           "name", name,
                                           NULL));
}

void *
spice_named_pipe_get_handle (SpiceNamedPipe *namedpipe)
{
  g_return_val_if_fail (SPICE_IS_NAMED_PIPE (namedpipe), NULL);

  return namedpipe->priv->handle;
}

gboolean
spice_named_pipe_close (SpiceNamedPipe *np,
                        GError **error)
{
  BOOL res;

  g_return_val_if_fail (SPICE_IS_NAMED_PIPE (np), FALSE);

  res = CloseHandle (np->priv->handle);
  np->priv->handle = NULL;
  if (!res)
    {
      int errsv = GetLastError ();
      gchar *emsg = g_win32_error_message (errsv);

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_win32_error (errsv),
		   "Error closing handle: %s",
		   emsg);
      g_free (emsg);
      return FALSE;
    }

  return TRUE;
}
