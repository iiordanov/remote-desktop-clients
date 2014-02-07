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
    guint64                     id;
    gboolean                    lossy;
} display_cache_item;

typedef GHashTable display_cache;

static inline display_cache_item* cache_item_new(guint64 id, gboolean lossy)
{
    display_cache_item *self = g_slice_new(display_cache_item);
    self->id = id;
    self->lossy = lossy;
    return self;
}

static inline void cache_item_free(display_cache_item *self)
{
    g_slice_free(display_cache_item, self);
}

static inline display_cache* cache_new(GDestroyNotify value_destroy)
{
    GHashTable* self;

    self = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                 (GDestroyNotify)cache_item_free,
                                 value_destroy);

    return self;
}

static inline gpointer cache_find(display_cache *cache, uint64_t id)
{
    return g_hash_table_lookup(cache, &id);
}

static inline gpointer cache_find_lossy(display_cache *cache, uint64_t id, gboolean *lossy)
{
    gpointer value;
    display_cache_item *item;

    if (!g_hash_table_lookup_extended(cache, &id, (gpointer*)&item, &value))
        return NULL;

    *lossy = item->lossy;

    return value;
}

static inline void cache_add_lossy(display_cache *cache, uint64_t id,
                                   gpointer value, gboolean lossy)
{
    display_cache_item *item = cache_item_new(id, lossy);

    g_hash_table_replace(cache, item, value);
}

static inline void cache_add(display_cache *cache, uint64_t id, gpointer value)
{
    cache_add_lossy(cache, id, value, FALSE);
}

static inline gboolean cache_remove(display_cache *cache, uint64_t id)
{
    return g_hash_table_remove(cache, &id);
}

static inline void cache_clear(display_cache *cache)
{
    g_hash_table_remove_all(cache);
}

static inline void cache_unref(display_cache *cache)
{
    g_hash_table_unref(cache);
}

G_END_DECLS

#endif // SPICE_CHANNEL_CACHE_H_
