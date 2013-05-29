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

#ifndef H_SPICE_COMMON
#define H_SPICE_COMMON

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>

#include <spice/macros.h>
#include "backtrace.h"
#include "log.h"

#ifdef SPICE_DISABLE_ABORT
#define spice_abort() do { } while(0)
#else
#define spice_abort() abort()
#endif

#endif
