/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.
   Copyright (C) 2006 Anthony Liguori <anthony@codemonkey.ws>
   Copyright (C) 2009-2010 Daniel P. Berrange <dan@berrange.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.0 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/
#ifndef __GIO_COROUTINE_H__
#define __GIO_COROUTINE_H__

#include <gio/gio.h>
#include "coroutine.h"

G_BEGIN_DECLS

typedef struct _GCoroutine GCoroutine;

struct _GCoroutine
{
    struct coroutine coroutine;
    guint wait_id;
    guint condition_id;
};

/*
 * A special GSource impl which allows us to wait on a certain
 * condition to be satisfied. This is effectively a boolean test
 * run on each iteration of the main loop. So whenever a file has
 * new I/O, or a timer occurs, etc we'll do the check. This is
 * pretty efficient compared to a normal GLib Idle func which has
 * to busy wait on a timeout, since our condition is only checked
 * when some other source's state changes
 */
typedef gboolean (*GConditionWaitFunc)(gpointer);

typedef void (*GSignalEmitMainFunc)(GObject *object, int signum, gpointer params);

GCoroutine*  g_coroutine_self           (void);
void         g_coroutine_wakeup         (GCoroutine *coroutine);
GIOCondition g_coroutine_socket_wait    (GCoroutine *coroutine,
                                         GSocket *sock, GIOCondition cond);
gboolean     g_coroutine_condition_wait (GCoroutine *coroutine,
                                         GConditionWaitFunc func, gpointer data);
void         g_coroutine_condition_cancel(GCoroutine *coroutine);

void         g_coroutine_signal_emit (gpointer instance, guint signal_id,
                                      GQuark detail, ...);

void         g_coroutine_object_notify(GObject *object, const gchar *property_name);

G_END_DECLS

#endif /* __GIO_COROUTINE_H__ */
