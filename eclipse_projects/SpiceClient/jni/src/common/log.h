/*
   Copyright (C) 2012 Red Hat, Inc.

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

#ifndef H_SPICE_LOG
#define H_SPICE_LOG

#include <spice/macros.h>
#include <stdarg.h>
#include "macros.h"

SPICE_BEGIN_DECLS

#ifndef SPICE_LOG_DOMAIN
#define SPICE_LOG_DOMAIN "Spice"
#endif

#define SPICE_STRINGIFY(x) SPICE_STRINGIFY_ARG (x)
#define SPICE_STRINGIFY_ARG(x) #x

#define SPICE_STRLOC  __FILE__ ":" SPICE_STRINGIFY (__LINE__)

typedef enum {
    SPICE_LOG_LEVEL_ERROR,
    SPICE_LOG_LEVEL_CRITICAL,
    SPICE_LOG_LEVEL_WARNING,
    SPICE_LOG_LEVEL_INFO,
    SPICE_LOG_LEVEL_DEBUG,
} SpiceLogLevel;

void spice_logv(const char *log_domain,
                SpiceLogLevel log_level,
                const char *strloc,
                const char *function,
                const char *format,
                va_list args) SPICE_ATTR_PRINTF(5, 0);

void spice_log(const char *log_domain,
               SpiceLogLevel log_level,
               const char *strloc,
               const char *function,
               const char *format,
               ...) SPICE_ATTR_PRINTF(5, 6);

#ifndef spice_return_if_fail
#define spice_return_if_fail(x) SPICE_STMT_START {                      \
    if SPICE_LIKELY(x) { } else {                                       \
        spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_CRITICAL, SPICE_STRLOC, __FUNCTION__, "condition `%s' failed", #x); \
        return;                                                         \
    }                                                                   \
} SPICE_STMT_END
#endif

#ifndef spice_return_val_if_fail
#define spice_return_val_if_fail(x, val) SPICE_STMT_START {             \
    if SPICE_LIKELY(x) { } else {                                       \
        spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_CRITICAL, SPICE_STRLOC, __FUNCTION__, "condition `%s' failed", #x); \
        return (val);                                                   \
    }                                                                   \
} SPICE_STMT_END
#endif

#ifndef spice_warn_if_reached
#define spice_warn_if_reached() SPICE_STMT_START {                      \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_WARNING, SPICE_STRLOC, __FUNCTION__, "should not be reached"); \
} SPICE_STMT_END
#endif

#ifndef spice_printerr
#define spice_printerr(format, ...) SPICE_STMT_START {                  \
    fprintf(stderr, "%s: " format "\n", __FUNCTION__, ## __VA_ARGS__);  \
} SPICE_STMT_END
#endif

#ifndef spice_info
#define spice_info(format, ...) SPICE_STMT_START {                     \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_INFO, SPICE_STRLOC, __FUNCTION__, format, ## __VA_ARGS__); \
} SPICE_STMT_END
#endif

#ifndef spice_debug
#define spice_debug(format, ...) SPICE_STMT_START {                     \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_DEBUG, SPICE_STRLOC, __FUNCTION__, format, ## __VA_ARGS__); \
} SPICE_STMT_END
#endif

#ifndef spice_warning
#define spice_warning(format, ...) SPICE_STMT_START {                   \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_WARNING, SPICE_STRLOC, __FUNCTION__, format, ## __VA_ARGS__); \
} SPICE_STMT_END
#endif

#ifndef spice_critical
#define spice_critical(format, ...) SPICE_STMT_START {                      \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_CRITICAL, SPICE_STRLOC, __FUNCTION__, format, ## __VA_ARGS__); \
} SPICE_STMT_END
#endif

#ifndef spice_error
#define spice_error(format, ...) SPICE_STMT_START {                     \
    spice_log(SPICE_LOG_DOMAIN, SPICE_LOG_LEVEL_ERROR, SPICE_STRLOC, __FUNCTION__, format, ## __VA_ARGS__); \
} SPICE_STMT_END
#endif

#ifndef spice_warn_if_fail
#define spice_warn_if_fail(x) SPICE_STMT_START {        \
    if SPICE_LIKELY(x) { } else {                       \
        spice_warning("condition `%s' failed", #x);     \
    }                                                   \
} SPICE_STMT_END
#endif

#ifndef spice_warn_if
#define spice_warn_if(x) SPICE_STMT_START {             \
    if SPICE_UNLIKELY(x) {                              \
        spice_warning("condition `%s' reached", #x);    \
    }                                                   \
} SPICE_STMT_END
#endif

#ifndef spice_assert
#define spice_assert(x) SPICE_STMT_START {              \
    if SPICE_LIKELY(x) { } else {                       \
        spice_error("assertion `%s' failed", #x);       \
    }                                                   \
} SPICE_STMT_END
#endif

/* FIXME: improve that some day.. */
#ifndef spice_static_assert
#define spice_static_assert(x) SPICE_STMT_START {       \
    spice_assert(x);                                    \
} SPICE_STMT_END
#endif

SPICE_END_DECLS

#endif /* H_SPICE_LOG */
