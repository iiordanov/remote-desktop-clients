/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012-2014 Red Hat, Inc.
   Copyright © 1998-2009 VLC authors and VideoLAN

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
#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>


#if !GLIB_CHECK_VERSION(2,30,0)
#define G_TYPE_MAIN_CONTEXT (spice_main_context_get_type ())
GType spice_main_context_get_type (void) G_GNUC_CONST;
#endif

#if !GLIB_CHECK_VERSION(2,32,0)
# define G_SIGNAL_DEPRECATED (1 << 9)

#define G_SOURCE_CONTINUE   TRUE
#define G_SOURCE_REMOVE     FALSE

void
g_queue_free_full (GQueue        *queue,
                   GDestroyNotify  free_func);
#endif

#ifndef g_clear_pointer
#define g_clear_pointer(pp, destroy) \
  G_STMT_START {                                                               \
    G_STATIC_ASSERT (sizeof *(pp) == sizeof (gpointer));                       \
    /* Only one access, please */                                              \
    gpointer *_pp = (gpointer *) (pp);                                         \
    gpointer _p;                                                               \
    /* This assignment is needed to avoid a gcc warning */                     \
    GDestroyNotify _destroy = (GDestroyNotify) (destroy);                      \
                                                                               \
    (void) (0 ? (gpointer) *(pp) : 0);                                         \
    do                                                                         \
      _p = g_atomic_pointer_get (_pp);                                         \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_pp, _p, NULL)); \
                                                                               \
    if (_p)                                                                    \
      _destroy (_p);                                                           \
  } G_STMT_END
#endif

#ifndef HAVE_STRTOK_R
char* strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#endif /* GLIB_COMPAT_H */
