/*
 * librest - RESTful web services access
 * Copyright (c) 2010 Intel Corporation.
 *
 * Authors: Ross Burton <ross@linux.intel.com>
 *          Rob Bradford <rob@linux.intel.com>
 *
 * RestParam is inspired by libsoup's SoupBuffer
 * Copyright (C) 2000-2030 Ximian, Inc
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

#ifndef _REST_PARAM
#define _REST_PARAM

#include <glib-object.h>

G_BEGIN_DECLS

#define REST_TYPE_PARAM (rest_param_get_type ())

/**
 * RestMemoryUse:
 * @REST_MEMORY_STATIC: the memory block can be assumed to always exist for the
 * lifetime of the parameter, #RestParam will use it directly.
 * @REST_MEMORY_TAKE: #RestParam will take ownership of the memory block, and
 * g_free() it when it isn't used.
 * @REST_MEMORY_COPY: #RestParam will make a copy of the memory block.
 */
typedef enum {
  REST_MEMORY_STATIC,
  REST_MEMORY_TAKE,
  REST_MEMORY_COPY,
} RestMemoryUse;


typedef struct _RestParam RestParam;

GType rest_param_get_type (void) G_GNUC_CONST;

RestParam *rest_param_new_string (const char    *name,
                                  RestMemoryUse  use,
                                  const char    *string);

RestParam *rest_param_new_full (const char     *name,
                                RestMemoryUse   use,
                                gconstpointer   data,
                                gsize           length,
                                const char     *content_type,
                                const char     *filename);

RestParam *rest_param_new_with_owner (const char     *name,
                                      gconstpointer   data,
                                      gsize           length,
                                      const char     *content_type,
                                      const char     *filename,
                                      gpointer        owner,
                                      GDestroyNotify  owner_dnotify);


gboolean rest_param_is_string (RestParam *param);

const char *rest_param_get_name (RestParam *param);
const char *rest_param_get_content_type (RestParam *param);
const char *rest_param_get_file_name (RestParam *param);
gconstpointer rest_param_get_content (RestParam *param);
gsize rest_param_get_content_length (RestParam *param);

RestParam *rest_param_ref (RestParam *param);
void rest_param_unref (RestParam *param);

G_END_DECLS

#endif /* _REST_PARAM */
