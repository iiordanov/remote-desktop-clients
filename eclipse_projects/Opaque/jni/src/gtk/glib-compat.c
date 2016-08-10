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
#include "config.h"

#include <string.h>

#include "glib-compat.h"

#if !GLIB_CHECK_VERSION(2,30,0)
G_DEFINE_BOXED_TYPE (GMainContext, spice_main_context, g_main_context_ref, g_main_context_unref)
#endif


#if !GLIB_CHECK_VERSION(2,32,0)
/**
 * g_queue_free_full:
 * @queue: a pointer to a #GQueue
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GQueue,
 * and calls the specified destroy function on every element's data.
 *
 * Since: 2.32
 */
void
g_queue_free_full (GQueue        *queue,
                  GDestroyNotify  free_func)
{
  g_queue_foreach (queue, (GFunc) free_func, NULL);
  g_queue_free (queue);
}
#endif


#ifndef HAVE_STRTOK_R
G_GNUC_INTERNAL
char *strtok_r(char *s, const char *delim, char **save_ptr)
{
    char *token;

    if (s == NULL)
        s = *save_ptr;

    /* Scan leading delimiters. */
    s += strspn (s, delim);
    if (*s == '\0')
        return NULL;

    /* Find the end of the token. */
    token = s;
    s = strpbrk (token, delim);
    if (s == NULL)
        /* This token finishes the string. */
        *save_ptr = strchr (token, '\0');
    else
    {
        /* Terminate the token and make *SAVE_PTR point past it. */
        *s = '\0';
        *save_ptr = s + 1;
    }
    return token;
}
#endif
