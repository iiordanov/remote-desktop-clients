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

#ifndef _H_RING2
#define _H_RING2

#include <spice/macros.h>
#include "spice_common.h"

SPICE_BEGIN_DECLS

typedef struct Ring RingItem;
typedef struct Ring {
    RingItem *prev;
    RingItem *next;
} Ring;

static inline void ring_init(Ring *ring)
{
    ring->next = ring->prev = ring;
}

static inline void ring_item_init(RingItem *item)
{
    item->next = item->prev = NULL;
}

static inline int ring_item_is_linked(RingItem *item)
{
    return !!item->next;
}

static inline int ring_is_empty(Ring *ring)
{
    spice_assert(ring->next != NULL && ring->prev != NULL);
    return ring == ring->next;
}

static inline void ring_add(Ring *ring, RingItem *item)
{
    spice_assert(ring->next != NULL && ring->prev != NULL);
    spice_assert(item->next == NULL && item->prev == NULL);

    item->next = ring->next;
    item->prev = ring;
    ring->next = item->next->prev = item;
}

static inline void ring_add_after(RingItem *item, RingItem *pos)
{
    ring_add(pos, item);
}

static inline void ring_add_before(RingItem *item, RingItem *pos)
{
    ring_add(pos->prev, item);
}

static inline void __ring_remove(RingItem *item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
    item->prev = item->next = 0;
}

static inline void ring_remove(RingItem *item)
{
    spice_assert(item->next != NULL && item->prev != NULL);
    spice_assert(item->next != item);

    __ring_remove(item);
}

static inline RingItem *ring_get_head(Ring *ring)
{
    RingItem *ret;

    spice_assert(ring->next != NULL && ring->prev != NULL);

    if (ring_is_empty(ring)) {
        return NULL;
    }
    ret = ring->next;
    return ret;
}

static inline RingItem *ring_get_tail(Ring *ring)
{
    RingItem *ret;

    spice_assert(ring->next != NULL && ring->prev != NULL);

    if (ring_is_empty(ring)) {
        return NULL;
    }
    ret = ring->prev;
    return ret;
}

static inline RingItem *ring_next(Ring *ring, RingItem *pos)
{
    RingItem *ret;

    spice_assert(ring->next != NULL && ring->prev != NULL);
    spice_assert(pos);
    spice_assert(pos->next != NULL && pos->prev != NULL);
    ret = pos->next;
    return (ret == ring) ? NULL : ret;
}

static inline RingItem *ring_prev(Ring *ring, RingItem *pos)
{
    RingItem *ret;

    spice_assert(ring->next != NULL && ring->prev != NULL);
    spice_assert(pos);
    spice_assert(pos->next != NULL && pos->prev != NULL);
    ret = pos->prev;
    return (ret == ring) ? NULL : ret;
}

#define RING_FOREACH_SAFE(var, next, ring)                    \
    for ((var) = ring_get_head(ring);                         \
            (var) && ((next) = ring_next(ring, (var)), 1);    \
            (var) = (next))


#define RING_FOREACH(var, ring)                 \
    for ((var) = ring_get_head(ring);           \
            (var);                              \
            (var) = ring_next(ring, var))

#define RING_FOREACH_REVERSED(var, ring)        \
    for ((var) = ring_get_tail(ring);           \
            (var);                              \
            (var) = ring_prev(ring, var))


static inline unsigned int ring_get_length(Ring *ring)
{
    RingItem *i;
    unsigned int ret = 0;

    for (i = ring_get_head(ring);
         i != NULL;
         i = ring_next(ring, i))
        ret++;

    return ret;
}

SPICE_END_DECLS

#endif
