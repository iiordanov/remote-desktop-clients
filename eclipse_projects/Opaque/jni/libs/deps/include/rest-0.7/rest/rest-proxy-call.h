/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 *          Ross Burton <ross@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _REST_PROXY_CALL
#define _REST_PROXY_CALL

#include <glib-object.h>
#include <gio/gio.h>
#include <rest/rest-params.h>

G_BEGIN_DECLS

#define REST_TYPE_PROXY_CALL rest_proxy_call_get_type()

#define REST_PROXY_CALL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), REST_TYPE_PROXY_CALL, RestProxyCall))

#define REST_PROXY_CALL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), REST_TYPE_PROXY_CALL, RestProxyCallClass))

#define REST_IS_PROXY_CALL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REST_TYPE_PROXY_CALL))

#define REST_IS_PROXY_CALL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), REST_TYPE_PROXY_CALL))

#define REST_PROXY_CALL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), REST_TYPE_PROXY_CALL, RestProxyCallClass))

typedef struct _RestProxyCallPrivate RestProxyCallPrivate;

/**
 * RestProxyCall:
 *
 * #RestProxyCall has no publicly available members.
 */
typedef struct {
  GObject parent;
  RestProxyCallPrivate *priv;
} RestProxyCall;

/**
 * RestProxyCallClass:
 * @prepare: Virtual function called before making the request, This allows the
 * call to be modified, for example to add a signature.
 * @serialize_params: Virtual function allowing custom serialization of the
 * parameters, for example when the API doesn't expect standard form content.
 *
 * Class structure for #RestProxyCall for subclasses to implement specialised
 * behaviour.
 */
typedef struct {
  /*< private >*/
  GObjectClass parent_class;
  /*< public >*/
  gboolean (*prepare)(RestProxyCall *call, GError **error);
  gboolean (*serialize_params) (RestProxyCall *call,
                                gchar **content_type,
                                gchar **content,
                                gsize *content_len,
                                GError **error);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[7];
} RestProxyCallClass;

#define REST_PROXY_CALL_ERROR rest_proxy_call_error_quark ()

/**
 * RestProxyCallError:
 * @REST_PROXY_CALL_FAILED: the method call failed
 *
 * Error domain used when returning errors from #RestProxyCall.
 */
typedef enum {
  REST_PROXY_CALL_FAILED
} RestProxyCallError;

GQuark rest_proxy_call_error_quark (void);

GType rest_proxy_call_get_type (void);

/* Functions for dealing with request */
void rest_proxy_call_set_method (RestProxyCall *call,
                                 const gchar   *method);

const char * rest_proxy_call_get_method (RestProxyCall *call);

void rest_proxy_call_set_function (RestProxyCall *call,
                                   const gchar   *function);

void rest_proxy_call_add_header (RestProxyCall *call,
                                 const gchar   *header,
                                 const gchar   *value);

G_GNUC_NULL_TERMINATED
void rest_proxy_call_add_headers (RestProxyCall *call,
                                  ...);

void rest_proxy_call_add_headers_from_valist (RestProxyCall *call,
                                              va_list        headers);

const gchar *rest_proxy_call_lookup_header (RestProxyCall *call,
                                            const gchar   *header);

void rest_proxy_call_remove_header (RestProxyCall *call,
                                    const gchar   *header);

void rest_proxy_call_add_param (RestProxyCall *call,
                                const gchar   *name,
                                const gchar   *value);

void rest_proxy_call_add_param_full (RestProxyCall            *call,
                                     RestParam                *param);

G_GNUC_NULL_TERMINATED
void rest_proxy_call_add_params (RestProxyCall *call,
                                 ...);

void rest_proxy_call_add_params_from_valist (RestProxyCall *call,
                                             va_list        params);

RestParam *rest_proxy_call_lookup_param (RestProxyCall *call,
                                           const gchar *name);

void rest_proxy_call_remove_param (RestProxyCall *call,
                                   const gchar   *name);

RestParams *rest_proxy_call_get_params (RestProxyCall *call);

gboolean rest_proxy_call_run (RestProxyCall *call,
                              GMainLoop    **loop,
                              GError       **error);

typedef void (*RestProxyCallAsyncCallback)(RestProxyCall *call,
                                           const GError  *error,
                                           GObject       *weak_object,
                                           gpointer       userdata);

gboolean rest_proxy_call_async (RestProxyCall                *call,
                                RestProxyCallAsyncCallback    callback,
                                GObject                      *weak_object,
                                gpointer                      userdata,
                                GError                      **error);

void rest_proxy_call_invoke_async (RestProxyCall       *call,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);
gboolean rest_proxy_call_invoke_finish (RestProxyCall *call,
                                        GAsyncResult  *result,
                                        GError       **error);

typedef void (*RestProxyCallContinuousCallback) (RestProxyCall *call,
                                                 const gchar   *buf,
                                                 gsize          len,
                                                 const GError  *error,
                                                 GObject       *weak_object,
                                                 gpointer       userdata);

gboolean rest_proxy_call_continuous (RestProxyCall                    *call,
                                     RestProxyCallContinuousCallback   callback,
                                     GObject                          *weak_object,
                                     gpointer                          userdata,
                                     GError                          **error);

typedef void (*RestProxyCallUploadCallback) (RestProxyCall *call,
                                             gsize          total,
                                             gsize          uploaded,
                                             const GError  *error,
                                             GObject       *weak_object,
                                             gpointer       userdata);

gboolean rest_proxy_call_upload (RestProxyCall                *call,
                                 RestProxyCallUploadCallback   callback,
                                 GObject                      *weak_object,
                                 gpointer                      userdata,
                                 GError                      **error);

gboolean rest_proxy_call_cancel (RestProxyCall *call);

gboolean rest_proxy_call_sync (RestProxyCall *call, GError **error_out);

/* Functions for dealing with responses */

const gchar *rest_proxy_call_lookup_response_header (RestProxyCall *call,
                                                     const gchar   *header);

GHashTable *rest_proxy_call_get_response_headers (RestProxyCall *call);

goffset rest_proxy_call_get_payload_length (RestProxyCall *call);
const gchar *rest_proxy_call_get_payload (RestProxyCall *call);
guint rest_proxy_call_get_status_code (RestProxyCall *call);
const gchar *rest_proxy_call_get_status_message (RestProxyCall *call);
gboolean rest_proxy_call_serialize_params (RestProxyCall *call,
                                           gchar        **content_type,
                                           gchar        **content,
                                           gsize         *content_len,
                                           GError       **error);


G_END_DECLS

#endif /* _REST_PROXY_CALL */

