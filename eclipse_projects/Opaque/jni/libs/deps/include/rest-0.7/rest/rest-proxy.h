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

#ifndef _REST_PROXY
#define _REST_PROXY

#include <glib-object.h>
#include <rest/rest-proxy-auth.h>
#include <rest/rest-proxy-call.h>

G_BEGIN_DECLS

#define REST_TYPE_PROXY rest_proxy_get_type()

#define REST_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), REST_TYPE_PROXY, RestProxy))

#define REST_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), REST_TYPE_PROXY, RestProxyClass))

#define REST_IS_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REST_TYPE_PROXY))

#define REST_IS_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), REST_TYPE_PROXY))

#define REST_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), REST_TYPE_PROXY, RestProxyClass))

typedef struct _RestProxy RestProxy;
typedef struct _RestProxyClass RestProxyClass;

/**
 * RestProxy:
 *
 * #RestProxy has no publicly available members.
 */
struct _RestProxy {
  GObject parent;
};

/**
 * RestProxyClass:
 * @bind_valist: Virtual function called to bind parameters.
 * @new_call: Virtual function called to construct a new #RestProxyCall.
 * @simple_run_valist: Virtual function called when making a "simple" call.
 *
 * Class structure for #RestProxy for subclasses to implement specialised
 * behaviour.
 *
 * Typically subclasses will override @new_call to construct a subclass of
 * #RestProxyCall.
 */
struct _RestProxyClass {
  /*< private >*/
  GObjectClass parent_class;
  /*< public >*/
  gboolean (*bind_valist)(RestProxy *proxy, va_list params);
  RestProxyCall *(*new_call)(RestProxy *proxy);
  gboolean (*simple_run_valist)(RestProxy *proxy, gchar **payload,
      goffset *len, GError **error, va_list params);
  gboolean (*authenticate)(RestProxy *proxy, RestProxyAuth *auth, gboolean retrying);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[7];
};

#define REST_PROXY_ERROR rest_proxy_error_quark ()

/**
 * RestProxyError:
 *
 * Error domain used when returning errors from a #RestProxy.
 */
typedef enum {
  REST_PROXY_ERROR_CANCELLED = 1,
  REST_PROXY_ERROR_RESOLUTION,
  REST_PROXY_ERROR_CONNECTION,
  REST_PROXY_ERROR_SSL,
  REST_PROXY_ERROR_IO,
  REST_PROXY_ERROR_FAILED,

  REST_PROXY_ERROR_HTTP_MULTIPLE_CHOICES                = 300,
  REST_PROXY_ERROR_HTTP_MOVED_PERMANENTLY               = 301,
  REST_PROXY_ERROR_HTTP_FOUND                           = 302,
  REST_PROXY_ERROR_HTTP_SEE_OTHER                       = 303,
  REST_PROXY_ERROR_HTTP_NOT_MODIFIED                    = 304,
  REST_PROXY_ERROR_HTTP_USE_PROXY                       = 305,
  REST_PROXY_ERROR_HTTP_THREEOHSIX                      = 306,
  REST_PROXY_ERROR_HTTP_TEMPORARY_REDIRECT              = 307,
  REST_PROXY_ERROR_HTTP_BAD_REQUEST                     = 400,
  REST_PROXY_ERROR_HTTP_UNAUTHORIZED                    = 401,
  REST_PROXY_ERROR_HTTP_FOUROHTWO                       = 402,
  REST_PROXY_ERROR_HTTP_FORBIDDEN                       = 403,
  REST_PROXY_ERROR_HTTP_NOT_FOUND                       = 404,
  REST_PROXY_ERROR_HTTP_METHOD_NOT_ALLOWED              = 405,
  REST_PROXY_ERROR_HTTP_NOT_ACCEPTABLE                  = 406,
  REST_PROXY_ERROR_HTTP_PROXY_AUTHENTICATION_REQUIRED   = 407,
  REST_PROXY_ERROR_HTTP_REQUEST_TIMEOUT                 = 408,
  REST_PROXY_ERROR_HTTP_CONFLICT                        = 409,
  REST_PROXY_ERROR_HTTP_GONE                            = 410,
  REST_PROXY_ERROR_HTTP_LENGTH_REQUIRED                 = 411,
  REST_PROXY_ERROR_HTTP_PRECONDITION_FAILED             = 412,
  REST_PROXY_ERROR_HTTP_REQUEST_ENTITY_TOO_LARGE        = 413,
  REST_PROXY_ERROR_HTTP_REQUEST_URI_TOO_LONG            = 414,
  REST_PROXY_ERROR_HTTP_UNSUPPORTED_MEDIA_TYPE          = 415,
  REST_PROXY_ERROR_HTTP_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
  REST_PROXY_ERROR_HTTP_EXPECTATION_FAILED              = 417,
  REST_PROXY_ERROR_HTTP_INTERNAL_SERVER_ERROR           = 500,
  REST_PROXY_ERROR_HTTP_NOT_IMPLEMENTED                 = 501,
  REST_PROXY_ERROR_HTTP_BAD_GATEWAY                     = 502,
  REST_PROXY_ERROR_HTTP_SERVICE_UNAVAILABLE             = 503,
  REST_PROXY_ERROR_HTTP_GATEWAY_TIMEOUT                 = 504,
  REST_PROXY_ERROR_HTTP_HTTP_VERSION_NOT_SUPPORTED      = 505,
} RestProxyError;

GQuark rest_proxy_error_quark (void);

GType rest_proxy_get_type (void);

RestProxy *rest_proxy_new (const gchar *url_format, 
                           gboolean     binding_required);

RestProxy *
rest_proxy_new_with_authentication (const gchar *url_format,
                                    gboolean     binding_required,
                                    const gchar *username,
                                    const gchar *password);

gboolean rest_proxy_bind (RestProxy *proxy,
                          ...);

gboolean rest_proxy_bind_valist (RestProxy *proxy,
                                 va_list    params);

void rest_proxy_set_user_agent (RestProxy *proxy, const char *user_agent);

const gchar *rest_proxy_get_user_agent (RestProxy *proxy);

RestProxyCall *rest_proxy_new_call (RestProxy *proxy);

G_GNUC_NULL_TERMINATED
gboolean rest_proxy_simple_run (RestProxy *proxy, 
                                gchar    **payload, 
                                goffset   *len,
                                GError   **error,
                                ...);
gboolean rest_proxy_simple_run_valist (RestProxy *proxy, 
                                       gchar    **payload, 
                                       goffset   *len,
                                       GError   **error,
                                       va_list    params);
G_END_DECLS

#endif /* _REST_PROXY */

