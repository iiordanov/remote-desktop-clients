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

#ifndef _H_REGION
#define _H_REGION

#include <stdint.h>
#include <spice/macros.h>

#include "draw.h"
#include "pixman_utils.h"

SPICE_BEGIN_DECLS

typedef pixman_region32_t QRegion;

#define REGION_TEST_LEFT_EXCLUSIVE (1 << 0)
#define REGION_TEST_RIGHT_EXCLUSIVE (1 << 1)
#define REGION_TEST_SHARED (1 << 2)
#define REGION_TEST_ALL \
    (REGION_TEST_LEFT_EXCLUSIVE | REGION_TEST_RIGHT_EXCLUSIVE | REGION_TEST_SHARED)

void region_init(QRegion *rgn);
void region_clear(QRegion *rgn);
void region_destroy(QRegion *rgn);
void region_clone(QRegion *dest, const QRegion *src);
SpiceRect *region_dup_rects(const QRegion *rgn, uint32_t *num_rects);
void region_ret_rects(const QRegion *rgn, SpiceRect *rects, uint32_t num_rects);
void region_extents(const QRegion *rgn, SpiceRect *r);

int region_test(const QRegion *rgn, const QRegion *other_rgn, int query);
int region_is_valid(const QRegion *rgn);
int region_is_empty(const QRegion *rgn);
int region_is_equal(const QRegion *rgn1, const QRegion *rgn2);
int region_intersects(const QRegion *rgn1, const QRegion *rgn2);
int region_bounds_intersects(const QRegion *rgn1, const QRegion *rgn2);
int region_contains(const QRegion *rgn, const QRegion *other);
int region_contains_point(const QRegion *rgn, int32_t x, int32_t y);

void region_or(QRegion *rgn, const QRegion *other_rgn);
void region_and(QRegion *rgn, const QRegion *other_rgn);
void region_xor(QRegion *rgn, const QRegion *other_rgn);
void region_exclude(QRegion *rgn, const QRegion *other_rgn);

void region_add(QRegion *rgn, const SpiceRect *r);
void region_remove(QRegion *rgn, const SpiceRect *r);

void region_offset(QRegion *rgn, int32_t dx, int32_t dy);


void region_dump(const QRegion *rgn, const char *prefix);

SPICE_END_DECLS

#endif
