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

#ifndef _CONTINUATION_H_
#define _CONTINUATION_H_

#include <stddef.h>
#include <ucontext.h>
#include <setjmp.h>

struct continuation
{
	char *stack;
	size_t stack_size;
	void (*entry)(struct continuation *cc);
	int (*release)(struct continuation *cc);

	/* private */
	ucontext_t uc;
	ucontext_t last;
	int exited;
	jmp_buf jmp;
};

void cc_init(struct continuation *cc);

int cc_release(struct continuation *cc);

/* you can use an uninitialized struct continuation for from if you do not have
   the current continuation handy. */
int cc_swap(struct continuation *from, struct continuation *to);

#define offset_of(type, member) ((unsigned long)(&((type *)0)->member))
#define container_of(obj, type, member) \
        (type *)(((char *)obj) - offset_of(type, member))

#endif
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
