/*
 * ovirt-rest-call-error.h: oVirt librest call proxy errors
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
#ifndef __OVIRT_REST_CALL_ERROR_H__
#define __OVIRT_REST_CALL_ERROR_H__

G_BEGIN_DECLS

typedef enum {
    OVIRT_REST_CALL_ERROR_XML,
    OVIRT_REST_CALL_ERROR_CANCELLED,
} OvirtRestCallError;

#define OVIRT_REST_CALL_ERROR ovirt_rest_call_error_quark()

GQuark ovirt_rest_call_error_quark(void);

G_END_DECLS

#endif /* __OVIRT_REST_CALL_ERROR_H__ */
