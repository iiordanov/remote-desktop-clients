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

#ifndef _H_RECT
#define _H_RECT

#include <spice/macros.h>
#include "draw.h"
#include "log.h"

SPICE_BEGIN_DECLS

static INLINE void rect_sect(SpiceRect* r, const SpiceRect* bounds)
{
    r->left = MAX(r->left, bounds->left);
    r->right = MIN(r->right, bounds->right);
    r->right = MAX(r->left, r->right);

    r->top = MAX(r->top, bounds->top);
    r->bottom = MIN(r->bottom, bounds->bottom);
    r->bottom = MAX(r->top, r->bottom);
}

static INLINE void rect_offset(SpiceRect* r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
}

static INLINE int rect_is_empty(const SpiceRect* r)
{
    return r->top == r->bottom || r->left == r->right;
}

static INLINE int rect_intersects(const SpiceRect* r1, const SpiceRect* r2)
{
    return r1->left < r2->right && r1->right > r2->left &&
           r1->top < r2->bottom && r1->bottom > r2->top;
}

static INLINE int rect_is_equal(const SpiceRect *r1, const SpiceRect *r2)
{
    return r1->top == r2->top && r1->left == r2->left &&
           r1->bottom == r2->bottom && r1->right == r2->right;
}

static INLINE void rect_union(SpiceRect *dest, const SpiceRect *r)
{
    dest->top = MIN(dest->top, r->top);
    dest->left = MIN(dest->left, r->left);
    dest->bottom = MAX(dest->bottom, r->bottom);
    dest->right = MAX(dest->right, r->right);
}

static INLINE int rect_is_same_size(const SpiceRect *r1, const SpiceRect *r2)
{
    return r1->right - r1->left == r2->right - r2->left &&
           r1->bottom - r1->top == r2->bottom - r2->top;
}

static INLINE int rect_contains(const SpiceRect *big, const SpiceRect *small)
{
    return big->left <= small->left && big->right >= small->right &&
           big->top <= small->top && big->bottom >= small->bottom;
}

static INLINE int rect_get_area(const SpiceRect *r)
{
    return (r->right - r->left) * (r->bottom - r->top);
}

static INLINE void rect_debug(const SpiceRect *r)
{
    spice_debug("(%d, %d) (%d, %d)", r->left, r->top, r->right, r->bottom);
}

SPICE_END_DECLS

#ifdef __cplusplus

static inline void rect_sect(SpiceRect& r, const SpiceRect& bounds)
{
    rect_sect(&r, &bounds);
}

static inline void rect_offset(SpiceRect& r, int dx, int dy)
{
    rect_offset(&r, dx, dy);
}

static inline int rect_is_empty(const SpiceRect& r)
{
    return rect_is_empty(&r);
}

static inline int rect_intersects(const SpiceRect& r1, const SpiceRect& r2)
{
    return rect_intersects(&r1, &r2);
}

static inline int rect_is_equal(const SpiceRect& r1, const SpiceRect& r2)
{
    return rect_is_equal(&r1, &r2);
}

static inline void rect_union(SpiceRect& dest, const SpiceRect& r)
{
    rect_union(&dest, &r);
}

static inline int rect_is_same_size(const SpiceRect& r1, const SpiceRect& r2)
{
    return rect_is_same_size(&r1, &r2);
}

static inline int rect_contains(const SpiceRect& big, const SpiceRect& small)
{
    return rect_contains(&big, &small);
}

static inline int rect_get_area(const SpiceRect& r)
{
    return rect_get_area(&r);
}

static inline void rect_debug(const SpiceRect &r)
{
    rect_debug(&r);
}

#endif /* __cplusplus */

#endif
