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

#ifndef _H_MUTEX
#define _H_MUTEX

#include <spice/macros.h>

SPICE_BEGIN_DECLS

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION mutex_t;
#define MUTEX_INIT(mutex) InitializeCriticalSection(&mutex)
#define MUTEX_LOCK(mutex) EnterCriticalSection(&mutex)
#define MUTEX_UNLOCK(mutex) LeaveCriticalSection(&mutex)
#else
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#define MUTEX_INIT(mutex) pthread_mutex_init(&mutex, NULL);
#define MUTEX_LOCK(mutex) pthread_mutex_lock(&mutex)
#define MUTEX_UNLOCK(mutex) pthread_mutex_unlock(&mutex)
#endif

SPICE_END_DECLS

#endif // _H_MUTEX
