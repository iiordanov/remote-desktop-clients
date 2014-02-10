/*
 * GTK VNC Widget
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef _COROUTINE_H_
#define _COROUTINE_H_

#include "config.h"

#if WITH_UCONTEXT
#include "continuation.h"
#elif WITH_WINFIBER
#include <windows.h>
#else
#include <glib.h>
#endif

struct coroutine
{
	size_t stack_size;
	void *(*entry)(void *);
	int (*release)(struct coroutine *);

	/* read-only */
	int exited;

	/* private */
	struct coroutine *caller;
	void *data;

#if WITH_UCONTEXT
	struct continuation cc;
#elif WITH_WINFIBER
        LPVOID fiber;
        int ret;
#else
	GThread *thread;
	gboolean runnable;
#endif
};

void coroutine_init(struct coroutine *co);

int coroutine_release(struct coroutine *co);

void *coroutine_swap(struct coroutine *from, struct coroutine *to, void *arg);

struct coroutine *coroutine_self(void);

void *coroutine_yieldto(struct coroutine *to, void *arg);

void *coroutine_yield(void *arg);

gboolean coroutine_is_main(struct coroutine *co);

static inline gboolean coroutine_self_is_main(void) {
	return coroutine_self() == NULL || coroutine_is_main(coroutine_self());
}

#endif
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
