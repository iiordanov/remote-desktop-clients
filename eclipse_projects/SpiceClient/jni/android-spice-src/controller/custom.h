#ifndef CUSTOM_H_
#define CUSTOM_H_

#include <glib.h>

static inline gboolean g_warn_if_expr (gboolean condition,
                                       const char *pretty_func,
                                       const char *expression) {
  if G_UNLIKELY(condition) {
      g_log (G_LOG_DOMAIN,
             G_LOG_LEVEL_CRITICAL,
             "%s: `%s' condition reached",
             pretty_func,
             expression);
    }

  return condition;
}

#define g_warn_if(expr) g_warn_if_expr((expr), __PRETTY_FUNCTION__, #expr)

#endif
