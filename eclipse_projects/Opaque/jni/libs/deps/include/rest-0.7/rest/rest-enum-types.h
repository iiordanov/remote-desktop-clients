


#ifndef __REST_ENUM_TYPES_H__
#define __REST_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS
/* enumerations from "rest-proxy.h" */
GType rest_proxy_error_get_type (void) G_GNUC_CONST;
#define REST_TYPE_PROXY_ERROR (rest_proxy_error_get_type())
/* enumerations from "rest-proxy-call.h" */
GType rest_proxy_call_error_get_type (void) G_GNUC_CONST;
#define REST_TYPE_PROXY_CALL_ERROR (rest_proxy_call_error_get_type())
G_END_DECLS

#endif /* __REST_ENUM_TYPES_H__ */



