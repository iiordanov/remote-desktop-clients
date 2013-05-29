/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

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

#ifndef __MACROS_H
#define __MACROS_H

#if    __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define SPICE_ATTR_PRINTF(a,b)                               \
    __attribute__((format(printf,a,b)))
#else
#define SPICE_ATTR_PRINTF(a,b)
#endif /* __GNUC__ */

#if    __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5)
#define SPICE_ATTR_NORETURN                                  \
    __attribute__((noreturn))
#else
#define SPICE_ATTR_NORETURN
#endif /* __GNUC__ */


#endif /* __MACROS_H */
