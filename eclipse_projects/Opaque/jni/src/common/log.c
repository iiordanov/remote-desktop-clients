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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "log.h"
#include "backtrace.h"

static int debug_level = -1;
static int abort_level = -1;

static const char * spice_log_level_to_string(SpiceLogLevel level)
{
#ifdef _MSC_VER
    /* MSVC++ does not implement C99 */
    static const char *to_string[] = {
         "ERROR",
         "CRITICAL",
         "Warning",
         "Info",
         "Debug"};
#else
    static const char *to_string[] = {
        [ SPICE_LOG_LEVEL_ERROR ] = "ERROR",
        [ SPICE_LOG_LEVEL_CRITICAL ] = "CRITICAL",
        [ SPICE_LOG_LEVEL_WARNING ] = "Warning",
        [ SPICE_LOG_LEVEL_INFO ] = "Info",
        [ SPICE_LOG_LEVEL_DEBUG ] = "Debug",
    };
#endif
    const char *str = NULL;
 
    if (level < SPICE_N_ELEMENTS(to_string)) {
        str = to_string[level];
    }

    return str;
}

#ifndef SPICE_ABORT_LEVEL_DEFAULT
#ifdef SPICE_DISABLE_ABORT
#define SPICE_ABORT_LEVEL_DEFAULT -1
#else
#define SPICE_ABORT_LEVEL_DEFAULT SPICE_LOG_LEVEL_CRITICAL
#endif
#endif

void spice_logv(const char *log_domain,
                SpiceLogLevel log_level,
                const char *strloc,
                const char *function,
                const char *format,
                va_list args)
{
    const char *level = spice_log_level_to_string(log_level);
    
    if (debug_level == -1) {
        debug_level = getenv("SPICE_DEBUG_LEVEL") ? atoi(getenv("SPICE_DEBUG_LEVEL")) : SPICE_LOG_LEVEL_WARNING;
    }
    if (abort_level == -1) {
        abort_level = getenv("SPICE_ABORT_LEVEL") ? atoi(getenv("SPICE_ABORT_LEVEL")) : SPICE_ABORT_LEVEL_DEFAULT;
    }

    if (debug_level < log_level)
        return;

    fprintf(stderr, "(%s:%d): ", getenv("_"), getpid());

    if (log_domain) {
        fprintf(stderr, "%s-", log_domain);
    }
    if (level) {
        fprintf(stderr, "%s **: ", level);
    }
    if (strloc && function) {
        fprintf(stderr, "%s:%s: ", strloc, function);
    }
    if (format) {
        vfprintf(stderr, format, args);
    }

    fprintf(stderr, "\n");

    if (abort_level != -1 && abort_level >= log_level) {
        spice_backtrace();
        abort();
    }
}

void spice_log(const char *log_domain,
               SpiceLogLevel log_level,
               const char *strloc,
               const char *function,
               const char *format,
               ...)
{
    va_list args;

    va_start (args, format);
    spice_logv (log_domain, log_level, strloc, function, format, args);
    va_end (args);
}
