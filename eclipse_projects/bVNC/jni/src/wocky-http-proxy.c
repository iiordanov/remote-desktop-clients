 /* wocky-http-proxy.c: Source for WockyHttpProxy
 *
 * Copyright (C) 2010 Collabora, Ltd.
 * @author Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "wocky-http-proxy.h"

#include <string.h>
#include <stdlib.h>


struct _WockyHttpProxy
{
  GObject parent;
};

struct _WockyHttpProxyClass
{
  GObjectClass parent_class;
};

static void wocky_http_proxy_iface_init (GProxyInterface *proxy_iface);

#define wocky_http_proxy_get_type _wocky_http_proxy_get_type
G_DEFINE_TYPE_WITH_CODE (WockyHttpProxy, wocky_http_proxy, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_PROXY,
      wocky_http_proxy_iface_init)
    g_io_extension_point_set_required_type (
      g_io_extension_point_register (G_PROXY_EXTENSION_POINT_NAME),
      G_TYPE_PROXY);
    g_io_extension_point_implement (G_PROXY_EXTENSION_POINT_NAME,
      g_define_type_id, "http", 0))

static void
wocky_http_proxy_init (WockyHttpProxy *proxy)
{
}

#define HTTP_END_MARKER "\r\n\r\n"

static gchar *
create_request (GProxyAddress *proxy_address, gboolean *has_cred)
{
  const gchar *hostname;
  gint port;
  const gchar *username;
  const gchar *password;
  GString *request;
  gchar *ascii_hostname;

  if (has_cred)
    *has_cred = FALSE;

  hostname = g_proxy_address_get_destination_hostname (proxy_address);
  port = g_proxy_address_get_destination_port (proxy_address);
  username = g_proxy_address_get_username (proxy_address);
  password = g_proxy_address_get_password (proxy_address);

  request = g_string_new (NULL);

  ascii_hostname = g_hostname_to_ascii (hostname);
  g_string_append_printf (request,
      "CONNECT %s:%i HTTP/1.0\r\n"
        "Host: %s:%i\r\n"
        "Proxy-Connection: keep-alive\r\n"
        "User-Agent: GLib/%i.%i\r\n",
      ascii_hostname, port,
      ascii_hostname, port,
      GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION);
  g_free (ascii_hostname);

  if (username != NULL && password != NULL)
    {
      gchar *cred;
      gchar *base64_cred;

      if (has_cred)
        *has_cred = TRUE;

      cred = g_strdup_printf ("%s:%s", username, password);
      base64_cred = g_base64_encode ((guchar *) cred, strlen (cred));
      g_free (cred);
      g_string_append_printf (request,
          "Proxy-Authorization: %s\r\n",
          base64_cred);
      g_free (base64_cred);
    }

  g_string_append (request, "\r\n");

  return g_string_free (request, FALSE);
}

static gboolean
check_reply (const gchar *buffer, gboolean has_cred, GError **error)
{
  gint err_code;
  const gchar *ptr = buffer + 7;

  if (strncmp (buffer, "HTTP/1.", 7) != 0
      || (*ptr != '0' && *ptr != '1'))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
          "Bad HTTP proxy reply");
      return FALSE;
    }

  ptr++;
  while (*ptr == ' ') ptr++;

  err_code = atoi (ptr);

  if (err_code < 200 || err_code >= 300)
    {
      const gchar *msg_start;
      gchar *msg;

      while (g_ascii_isdigit (*ptr))
        ptr++;

      while (*ptr == ' ')
        ptr++;

      msg_start = ptr;

      ptr = strchr (msg_start, '\r');

      if (ptr == NULL)
        ptr = strchr (msg_start, '\0');

      msg = g_strndup (msg_start, ptr - msg_start);

      if (err_code == 407)
        {
          if (has_cred)
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_PROXY_AUTH_FAILED,
                "HTTP proxy authentication failed");
          else
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_PROXY_NEED_AUTH,
                "HTTP proxy authentication required");
        }
      else if (msg[0] == '\0')
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
            "Connection failed due to broken HTTP reply");
      else
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
            "HTTP proxy connection failed: %i %s",
            err_code, msg);

      g_free (msg);
      return FALSE;
    }

  return TRUE;
}

static GIOStream *
wocky_http_proxy_connect (GProxy *proxy,
    GIOStream *io_stream,
    GProxyAddress *proxy_address,
    GCancellable *cancellable,
    GError **error)
{
  GInputStream *in;
  GOutputStream *out;
  GDataInputStream *data_in;
  gchar *buffer;
  gboolean has_cred;

  in = g_io_stream_get_input_stream (io_stream);
  out = g_io_stream_get_output_stream (io_stream);

  data_in = g_data_input_stream_new (in);
  g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (data_in),
      FALSE);

  buffer = create_request (proxy_address, &has_cred);
  if (!g_output_stream_write_all (out, buffer, strlen (buffer), NULL,
        cancellable, error))
      goto error;

  g_free (buffer);
  buffer = g_data_input_stream_read_until (data_in, HTTP_END_MARKER, NULL,
      cancellable, error);
  g_object_unref (data_in);
  data_in = NULL;

  if (buffer == NULL)
    {
      if (error && (*error == NULL))
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
            "HTTP proxy server closed connection unexpectedly.");
      goto error;
    }

  if (!check_reply (buffer, has_cred, error))
    goto error;

  g_free (buffer);

  return g_object_ref (io_stream);

error:
  if (data_in != NULL)
    g_object_unref (data_in);

  g_free (buffer);
  return NULL;
}


typedef struct
{
  GSimpleAsyncResult *simple;
  GIOStream *io_stream;
  gchar *buffer;
  gssize length;
  gssize offset;
  GDataInputStream *data_in;
  gboolean has_cred;
  GCancellable *cancellable;
} ConnectAsyncData;

static void request_write_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);
static void reply_read_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data);

static void
free_connect_data (ConnectAsyncData *data)
{
  if (data->io_stream != NULL)
    g_object_unref (data->io_stream);

  g_free (data->buffer);

  if (data->data_in != NULL)
    g_object_unref (data->data_in);

  if (data->cancellable != NULL)
    g_object_unref (data->cancellable);

  g_slice_free (ConnectAsyncData, data);
}

static void
complete_async_from_error (ConnectAsyncData *data, GError *error)
{
  GSimpleAsyncResult *simple = data->simple;

  if (error == NULL)
    g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
        "HTTP proxy server closed connection unexpectedly.");

  g_simple_async_result_set_from_error (data->simple, error);
  g_error_free (error);
  g_simple_async_result_set_op_res_gpointer (simple, NULL, NULL);
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
do_write (GAsyncReadyCallback callback, ConnectAsyncData *data)
{
  GOutputStream *out;
  out = g_io_stream_get_output_stream (data->io_stream);
  g_output_stream_write_async (out,
      data->buffer + data->offset,
      data->length - data->offset,
      G_PRIORITY_DEFAULT, data->cancellable,
      callback, data);
}

static void
wocky_http_proxy_connect_async (GProxy *proxy,
    GIOStream *io_stream,
    GProxyAddress *proxy_address,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *simple;
  ConnectAsyncData *data;
  GInputStream *in;

  simple = g_simple_async_result_new (G_OBJECT (proxy),
      callback, user_data,
      wocky_http_proxy_connect_async);

  data = g_slice_new0 (ConnectAsyncData);

  data->simple = simple;
  data->io_stream = g_object_ref (io_stream);

  if (cancellable != NULL)
    data->cancellable = g_object_ref (cancellable);

  in = g_io_stream_get_input_stream (io_stream);

  data->data_in = g_data_input_stream_new (in);
  g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (data->data_in),
      FALSE);

  g_simple_async_result_set_op_res_gpointer (simple, data,
      (GDestroyNotify) free_connect_data);

  data->buffer = create_request (proxy_address, &data->has_cred);
  data->length = strlen (data->buffer);
  data->offset = 0;

  do_write (request_write_cb, data);
}

static void
request_write_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;
  gssize written;

  written = g_output_stream_write_finish (G_OUTPUT_STREAM (source),
      res, &error);
  if (written < 0)
    {
      complete_async_from_error (data, error);
      return;
    }

  data->offset += written;

   if (data->offset == data->length)
    {
      g_free (data->buffer);
      data->buffer = NULL;

      g_data_input_stream_read_until_async (data->data_in,
          HTTP_END_MARKER,
          G_PRIORITY_DEFAULT,
          data->cancellable,
          reply_read_cb, data);

    }
  else
    {
      do_write (request_write_cb, data);
    }
}

static void
reply_read_cb (GObject *source,
    GAsyncResult *res,
    gpointer user_data)
{
  GError *error = NULL;
  ConnectAsyncData *data = user_data;

  data->buffer = g_data_input_stream_read_until_finish (data->data_in,
      res, NULL, &error);

  if (data->buffer == NULL)
    {
      complete_async_from_error (data, error);
      return;
    }

  if (!check_reply (data->buffer, data->has_cred, &error))
    {
      complete_async_from_error (data, error);
      return;
    }

  g_simple_async_result_complete (data->simple);
  g_object_unref (data->simple);
}

static GIOStream *
wocky_http_proxy_connect_finish (GProxy *proxy,
    GAsyncResult *result,
    GError **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  ConnectAsyncData *data = g_simple_async_result_get_op_res_gpointer (simple);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  return g_object_ref (data->io_stream);
}

static gboolean
wocky_http_proxy_supports_hostname (GProxy *proxy)
{
  return TRUE;
}

static void
wocky_http_proxy_class_init (WockyHttpProxyClass *class)
{
}

static void
wocky_http_proxy_iface_init (GProxyInterface *proxy_iface)
{
  proxy_iface->connect  = wocky_http_proxy_connect;
  proxy_iface->connect_async = wocky_http_proxy_connect_async;
  proxy_iface->connect_finish = wocky_http_proxy_connect_finish;
  proxy_iface->supports_hostname = wocky_http_proxy_supports_hostname;
}
