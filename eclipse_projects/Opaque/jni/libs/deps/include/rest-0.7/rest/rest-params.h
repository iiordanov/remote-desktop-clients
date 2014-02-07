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

#ifndef _REST_PARAMS
#define _REST_PARAMS

#include <glib-object.h>
#include "rest-param.h"

G_BEGIN_DECLS

typedef struct _RestParams RestParams;
typedef struct _GHashTableIter RestParamsIter;

RestParams * rest_params_new (void);

void rest_params_free (RestParams *params);

void rest_params_add (RestParams *params, RestParam *param);

RestParam *rest_params_get (RestParams *params, const char *name);

void rest_params_remove (RestParams *params, const char *name);

gboolean rest_params_are_strings (RestParams *params);

GHashTable * rest_params_as_string_hash_table (RestParams *params);

void rest_params_iter_init (RestParamsIter *iter, RestParams *params);
gboolean rest_params_iter_next (RestParamsIter *iter, const char **name, RestParam **param);

G_END_DECLS

#endif /* _REST_PARAMS */
