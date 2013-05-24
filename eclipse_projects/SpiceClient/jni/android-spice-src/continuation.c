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
#undef _FORTIFY_SOURCE

#include <config.h>

#include "continuation.h"

/*
 * va_args to makecontext() must be type 'int', so passing
 * the pointer we need may require several int args. This
 * union is a quick hack to let us do that
 */
union cc_arg {
	void *p;
	int i[2];
};

static void continuation_trampoline(int i0, int i1)
{
	union cc_arg arg;
	struct continuation *cc;
	arg.i[0] = i0;
	arg.i[1] = i1;
	cc = arg.p;

	if (_setjmp(cc->jmp) == 0) {
		ucontext_t tmp;
		swapcontext(&tmp, &cc->last);
	}

	cc->entry(cc);
}

int cc_init(struct continuation *cc)
{
	volatile union cc_arg arg;
	arg.p = cc;
	if (getcontext(&cc->uc) == -1)
		return -1;

	cc->uc.uc_link = &cc->last;
	cc->uc.uc_stack.ss_sp = cc->stack;
	cc->uc.uc_stack.ss_size = cc->stack_size;
	cc->uc.uc_stack.ss_flags = 0;

	makecontext(&cc->uc, (void *)continuation_trampoline, 2, arg.i[0], arg.i[1]);
	swapcontext(&cc->last, &cc->uc);

	return 0;
}

int cc_release(struct continuation *cc)
{
	if (cc->release)
		return cc->release(cc);

	return 0;
}

int cc_swap(struct continuation *from, struct continuation *to)
{
	to->exited = 0;
	if (getcontext(&to->last) == -1)
		return -1;
	else if (to->exited == 0)
		to->exited = 1; // so when coroutine finishes
        else if (to->exited == 1)
                return 1; // it ends up here

	if (_setjmp(from->jmp) == 0)
		_longjmp(to->jmp, 1);

	return 0;
}
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
