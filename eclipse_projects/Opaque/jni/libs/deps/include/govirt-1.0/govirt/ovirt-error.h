/*
 * ovirt-error.h: govirt error handling
 *
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Christophe Fergeau <cfergeau@redhat.com>
 */
#ifndef __OVIRT_ERROR_H__
#define __OVIRT_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    OVIRT_ERROR_FAILED,
    OVIRT_ERROR_PARSING_FAILED,
    OVIRT_ERROR_NOT_SUPPORTED,
    OVIRT_ERROR_ACTION_FAILED,
    OVIRT_ERROR_BAD_URI,
} OvirtError;

GQuark ovirt_error_quark(void);
#define OVIRT_ERROR ovirt_error_quark()

#endif
