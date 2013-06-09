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

#ifndef _H_ROP3
#define _H_ROP3

#include <stdint.h>
#include <spice/macros.h>

#include "draw.h"
#include "pixman_utils.h"

SPICE_BEGIN_DECLS

void do_rop3_with_pattern(uint8_t rop3, pixman_image_t *d, pixman_image_t *s, SpicePoint *src_pos,
                          pixman_image_t *p, SpicePoint *pat_pos);
void do_rop3_with_color(uint8_t rop3, pixman_image_t *d, pixman_image_t *s, SpicePoint *src_pos,
                        uint32_t rgb);

void rop3_init(void);

SPICE_END_DECLS

#endif
