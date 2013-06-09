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
   License along with this library; if not, write to the Free Software

   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef BITOPS_H
#define BITOPS_H

#include <spice/macros.h>

SPICE_BEGIN_DECLS

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
static inline int spice_bit_find_msb(unsigned int val)
{
    int ret;

    asm ("bsrl %1,%0\n\t"
         "jnz 1f\n\t"
         "movl $-1,%0\n"
         "1:"
         : "=r"(ret) : "r"(val));
    return ret + 1;
}

#elif defined(WIN32) && !defined(_WIN64)
static INLINE int spice_bit_find_msb(uint32_t val)
{
    uint32_t r;
    __asm {
        bsr eax, val
        jnz found
        mov eax, -1

found:
        mov r, eax
    }
    return r + 1;
}

#else
static INLINE int spice_bit_find_msb(unsigned int val)
{
    signed char index = 31;

    if(val == 0) {
        return 0;
    }

    do {
        if(val & 0x80000000) {
            break;
        }
        val <<= 1;
    } while(--index >= 0);

    return index+1;
}

#endif

static INLINE int spice_bit_next_pow2(unsigned int val)
{
    if ((val & (val - 1)) == 0) {
        return val;
    }
    return 1 << spice_bit_find_msb(val);
}

SPICE_END_DECLS

#endif
