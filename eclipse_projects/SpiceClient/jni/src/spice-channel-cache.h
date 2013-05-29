/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef SPICE_CHANNEL_CACHE_H_
# define SPICE_CHANNEL_CACHE_H_

#include <inttypes.h> /* For PRIx64 */
#include "common/mem.h"
#include "common/ring.h"

G_BEGIN_DECLS

typedef struct display_cache_item {
    RingItem                    hash_link;
    RingItem                    lru_link;
    uint64_t                    id;
    uint32_t                    refcount;
    void                        *ptr;
    gboolean                    lossy;
} display_cache_item;

typedef struct display_cache {
    const char                  *name;
    Ring                        hash[64];
    Ring                        lru;
    int                         nitems;
} display_cache;

static inline void cache_init(display_cache *cache, const char *name)
{
    int i;

    cache->name = name;
    ring_init(&cache->lru);
    for (i = 0; i < SPICE_N_ELEMENTS(cache->hash); i++) {
        ring_init(&cache->hash[i]);
    }
}

static inline Ring *cache_head(display_cache *cache, uint64_t id)
{
    return &cache->hash[id % SPICE_N_ELEMENTS(cache->hash)];
}

static inline void cache_used(display_cache *cache, display_cache_item *item)
{
    ring_remove(&item->lru_link);
    ring_add(&cache->lru, &item->lru_link);
}

static inline display_cache_item *cache_get_lru(display_cache *cache)
{
    display_cache_item *item;
    RingItem *ring;

    if (ring_is_empty(&cache->lru))
        return NULL;
    ring = ring_get_tail(&cache->lru);
    item = SPICE_CONTAINEROF(ring, display_cache_item, lru_link);
    return item;
}

static inline display_cache_item *cache_find(display_cache *cache, uint64_t id)
{
    display_cache_item *item;
    RingItem *ring;
    Ring *head;

    head = cache_head(cache, id);
    for (ring = ring_get_head(head); ring != NULL; ring = ring_next(head, ring)) {
        item = SPICE_CONTAINEROF(ring, display_cache_item, hash_link);
        if (item->id == id) {
            return item;
        }
    }

    SPICE_DEBUG("%s: %s %" PRIx64 " [not found]", __FUNCTION__,
            cache->name, id);
    return NULL;
}

static inline display_cache_item *cache_add(display_cache *cache, uint64_t id)
{
    display_cache_item *item;

    item = spice_new0(display_cache_item, 1);
    item->id = id;
    item->refcount = 1;
    ring_add(cache_head(cache, item->id), &item->hash_link);
    ring_add(&cache->lru, &item->lru_link);
    cache->nitems++;

    SPICE_DEBUG("%s: %s %" PRIx64 " (%d)", __FUNCTION__,
            cache->name, id, cache->nitems);
    return item;
}

static inline void cache_del(display_cache *cache, display_cache_item *item)
{
    SPICE_DEBUG("%s: %s %" PRIx64, __FUNCTION__,
            cache->name, item->id);
    ring_remove(&item->hash_link);
    ring_remove(&item->lru_link);
    free(item);
    cache->nitems--;
}

static inline void cache_ref(display_cache_item *item)
{
    item->refcount++;
}

static inline int cache_unref(display_cache_item *item)
{
    item->refcount--;
    return item->refcount == 0;
}

G_END_DECLS

#endif // SPICE_CHANNEL_CACHE_H_
