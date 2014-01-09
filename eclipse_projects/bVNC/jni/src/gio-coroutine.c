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

#include "gio-coroutine.h"

typedef struct _GConditionWaitSource
{
    GCoroutine *self;
    GSource src;
    GConditionWaitFunc func;
    gpointer data;
} GConditionWaitSource;

GCoroutine* g_coroutine_self(void)
{
    return (GCoroutine*)coroutine_self();
}

/* Main loop helper functions */
static gboolean g_io_wait_helper(GSocket *sock G_GNUC_UNUSED,
				 GIOCondition cond,
				 gpointer data)
{
    struct coroutine *to = data;
    coroutine_yieldto(to, &cond);
    return FALSE;
}

GIOCondition g_coroutine_socket_wait(GCoroutine *self,
                                     GSocket *sock,
                                     GIOCondition cond)
{
    GIOCondition *ret, val = 0;
    GSource *src;

    g_return_val_if_fail(self != NULL, 0);
    g_return_val_if_fail(self->wait_id == 0, 0);
    g_return_val_if_fail(sock != NULL, 0);

    src = g_socket_create_source(sock, cond | G_IO_HUP | G_IO_ERR | G_IO_NVAL, NULL);
    g_source_set_callback(src, (GSourceFunc)g_io_wait_helper, self, NULL);
    self->wait_id = g_source_attach(src, NULL);
    ret = coroutine_yield(NULL);
    g_source_unref(src);

    if (ret != NULL)
        val = *ret;
    else
        g_source_remove(self->wait_id);

    self->wait_id = 0;
    return val;
}

void g_coroutine_condition_cancel(GCoroutine *coroutine)
{
    g_return_if_fail(coroutine != NULL);

    if (coroutine->condition_id == 0)
        return;

    g_source_remove(coroutine->condition_id);
    coroutine->condition_id = 0;
}

void g_coroutine_wakeup(GCoroutine *coroutine)
{
    g_return_if_fail(coroutine != NULL);
    g_return_if_fail(coroutine != g_coroutine_self());

    if (coroutine->wait_id)
        coroutine_yieldto(&coroutine->coroutine, NULL);
}

/*
 * Call immediately before the main loop does an iteration. Returns
 * true if the condition we're checking is ready for dispatch
 */
static gboolean g_condition_wait_prepare(GSource *src,
					 int *timeout) {
    GConditionWaitSource *vsrc = (GConditionWaitSource *)src;
    *timeout = -1;
    return vsrc->func(vsrc->data);
}

/*
 * Call immediately after the main loop does an iteration. Returns
 * true if the condition we're checking is ready for dispatch
 */
static gboolean g_condition_wait_check(GSource *src)
{
    GConditionWaitSource *vsrc = (GConditionWaitSource *)src;
    return vsrc->func(vsrc->data);
}

static gboolean g_condition_wait_dispatch(GSource *src G_GNUC_UNUSED,
					  GSourceFunc cb,
					  gpointer data) {
    return cb(data);
}

GSourceFuncs waitFuncs = {
    .prepare = g_condition_wait_prepare,
    .check = g_condition_wait_check,
    .dispatch = g_condition_wait_dispatch,
};

static gboolean g_condition_wait_helper(gpointer data)
{
    GCoroutine *self = (GCoroutine *)data;
    coroutine_yieldto(&self->coroutine, NULL);
    return FALSE;
}

/*
 * g_coroutine_condition_wait:
 * @coroutine: the coroutine to wait on
 * @func: the condition callback
 * @data: the user data passed to @func callback
 *
 * This function will wait on caller coroutine until @func returns %TRUE.
 *
 * @func is called when entering the main loop from the main context (coroutine).
 *
 * The condition can be cancelled by calling g_coroutine_wakeup()
 *
 * Returns: %TRUE if condition reached, %FALSE if not and cancelled
 */
gboolean g_coroutine_condition_wait(GCoroutine *self, GConditionWaitFunc func, gpointer data)
{
    GSource *src;
    GConditionWaitSource *vsrc;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(self->condition_id == 0, FALSE);
    g_return_val_if_fail(func != NULL, FALSE);

    /* Short-circuit check in case we've got it ahead of time */
    if (func(data))
        return TRUE;

    /*
     * Don't have it, so yield to the main loop, checking the condition
     * on each iteration of the main loop
     */
    src = g_source_new(&waitFuncs, sizeof(GConditionWaitSource));
    vsrc = (GConditionWaitSource *)src;

    vsrc->func = func;
    vsrc->data = data;
    vsrc->self = self;

    self->condition_id = g_source_attach(src, NULL);
    g_source_set_callback(src, g_condition_wait_helper, self, NULL);
    coroutine_yield(NULL);
    g_source_unref(src);

    /* it got woked up / cancelled? */
    if (self->condition_id == 0)
        return func(data);

    self->condition_id = 0;
    return TRUE;
}

struct signal_data
{
    GObject *object;
    struct coroutine *caller;
    int signum;
    gpointer params;
    GSignalEmitMainFunc func;
    const char *debug_info;
    gboolean notified;
};

static gboolean emit_main_context(gpointer opaque)
{
    struct signal_data *signal = opaque;

    signal->func(signal->object, signal->signum, signal->params);
    signal->notified = TRUE;

    coroutine_yieldto(signal->caller, NULL);

    return FALSE;
}

/* coroutine -> main context */
void g_signal_emit_main_context(GObject *object,
                                GSignalEmitMainFunc emit_main_func,
                                int signum,
                                gpointer params,
                                const char *debug_info)
{
    struct signal_data data;

    g_return_if_fail(coroutine_self()->caller);

    data.object = object;
    data.caller = coroutine_self();
    data.signum = signum;
    data.params = params;
    data.func = emit_main_func;
    data.debug_info = debug_info;
    data.notified = FALSE;
    g_idle_add(emit_main_context, &data);

    /* This switches to the system coroutine context, lets
     * the idle function run to dispatch the signal, and
     * finally returns once complete. ie this is synchronous
     * from the POV of the coroutine despite there being
     * an idle function involved
     */
    coroutine_yield(NULL);
    g_warn_if_fail(data.notified);
}

static gboolean notify_main_context(gpointer opaque)
{
    struct signal_data *signal = opaque;

    g_object_notify(signal->object, signal->params);
    signal->notified = TRUE;

    coroutine_yieldto(signal->caller, NULL);

    return FALSE;
}

/* coroutine -> main context */
void g_object_notify_main_context(GObject *object,
                                  const gchar *property_name)
{
    struct signal_data data;

    if (coroutine_self_is_main()) {
        g_object_notify(object, property_name);
    } else {

        data.object = object;
        data.caller = coroutine_self();
        data.params = (gpointer)property_name;
        data.notified = FALSE;

        g_idle_add(notify_main_context, &data);

        /* This switches to the system coroutine context, lets
         * the idle function run to dispatch the signal, and
         * finally returns once complete. ie this is synchronous
         * from the POV of the coroutine despite there being
         * an idle function involved
         */
        coroutine_yield(NULL);
        g_warn_if_fail(data.notified);
    }
}
