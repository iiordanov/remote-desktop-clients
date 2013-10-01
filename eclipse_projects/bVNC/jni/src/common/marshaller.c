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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "marshaller.h"
#include "mem.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef WORDS_BIGENDIAN
#define write_int8(ptr,v) (*((int8_t *)(ptr)) = v)
#define write_uint8(ptr,v) (*((uint8_t *)(ptr)) = v)
#define write_int16(ptr,v) (*((int16_t *)(ptr)) = SPICE_BYTESWAP16((uint16_t)(v)))
#define write_uint16(ptr,v) (*((uint16_t *)(ptr)) = SPICE_BYTESWAP16((uint16_t)(v)))
#define write_int32(ptr,v) (*((int32_t *)(ptr)) = SPICE_BYTESWAP32((uint32_t)(v)))
#define write_uint32(ptr,v) (*((uint32_t *)(ptr)) = SPICE_BYTESWAP32((uint32_t)(v)))
#define write_int64(ptr,v) (*((int64_t *)(ptr)) = SPICE_BYTESWAP64((uint64_t)(v)))
#define write_uint64(ptr,v) (*((uint64_t *)(ptr)) = SPICE_BYTESWAP64((uint64_t)(v)))
#else
#define write_int8(ptr,v) (*((int8_t *)(ptr)) = v)
#define write_uint8(ptr,v) (*((uint8_t *)(ptr)) = v)
#define write_int16(ptr,v) (*((int16_t *)(ptr)) = v)
#define write_uint16(ptr,v) (*((uint16_t *)(ptr)) = v)
#define write_int32(ptr,v) (*((int32_t *)(ptr)) = v)
#define write_uint32(ptr,v) (*((uint32_t *)(ptr)) = v)
/*
 * Thanks to shohyanglim@gmail.com for saving me time and discovering these
 * hacks to avoid SIGBUS on some ARM processors.
 *
 * TODO: Find out whether something better can be done to avoid the SIGBUS.
 */
#ifdef ANDROID
    #define write_int64(ptr,v) { int64_t val = v; memcpy(ptr, &val, sizeof(int64_t)); }
    #define write_uint64(ptr,v) { uint64_t val = v; memcpy(ptr, &val, sizeof(uint64_t)); }
#else
    #define write_int64(ptr,v) (*((int64_t *)(ptr)) = v)
    #define write_uint64(ptr,v) (*((uint64_t *)(ptr)) = v)
#endif
#endif

typedef struct {
    uint8_t *data;
    size_t len;
    spice_marshaller_item_free_func free_data;
    void *opaque;
} MarshallerItem;

/* Try to fit in 4k page with 2*pointer-size overhead (next ptr and malloc size) */
#define MARSHALLER_BUFFER_SIZE (4096 - sizeof(void *) * 2)

typedef struct MarshallerBuffer MarshallerBuffer;
struct MarshallerBuffer {
    MarshallerBuffer *next;
    uint8_t data[MARSHALLER_BUFFER_SIZE];
};

#define N_STATIC_ITEMS 4

typedef struct SpiceMarshallerData SpiceMarshallerData;

typedef struct {
    SpiceMarshaller *marshaller;
    int item_nr;
    int is_64bit;
    size_t offset;
} MarshallerRef;

struct SpiceMarshaller {
    size_t total_size;
    SpiceMarshallerData *data;
    SpiceMarshaller *next;

    MarshallerRef pointer_ref;

    int n_items;
    int items_size; /* number of items availible in items */
    MarshallerItem *items;

    MarshallerItem static_items[N_STATIC_ITEMS];
};

struct SpiceMarshallerData {
    size_t total_size;
    size_t base;
    SpiceMarshaller *marshallers;
    SpiceMarshaller *last_marshaller;

    size_t current_buffer_position;
    MarshallerBuffer *current_buffer;
    MarshallerItem *current_buffer_item;
    MarshallerBuffer *buffers;

    SpiceMarshaller static_marshaller;
    MarshallerBuffer static_buffer;
};

static void spice_marshaller_init(SpiceMarshaller *m,
                                  SpiceMarshallerData *data)
{
    m->data = data;
    m->next = NULL;
    m->total_size = 0;
    m->pointer_ref.marshaller = NULL;
    m->n_items = 0;
    m->items_size = N_STATIC_ITEMS;
    m->items = m->static_items;
}

SpiceMarshaller *spice_marshaller_new(void)
{
    SpiceMarshallerData *d;
    SpiceMarshaller *m;

    d = spice_new(SpiceMarshallerData, 1);

    d->last_marshaller = d->marshallers = &d->static_marshaller;
    d->total_size = 0;
    d->base = 0;
    d->buffers = &d->static_buffer;
    d->buffers->next = NULL;
    d->current_buffer = d->buffers;
    d->current_buffer_position = 0;
    d->current_buffer_item = NULL;

    m = &d->static_marshaller;
    spice_marshaller_init(m, d);

    return m;
}

static void free_item_data(SpiceMarshaller *m)
{
    MarshallerItem *item;
    int i;

    /* Free all user data */
    for (i = 0; i < m->n_items; i++) {
        item = &m->items[i];
        if (item->free_data != NULL) {
            item->free_data(item->data, item->opaque);
        }
    }
}

static void free_items(SpiceMarshaller *m)
{
    if (m->items != m->static_items) {
        free(m->items);
    }
}

void spice_marshaller_reset(SpiceMarshaller *m)
{
    SpiceMarshaller *m2, *next;
    SpiceMarshallerData *d;

    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    for (m2 = m; m2 != NULL; m2 = next) {
        next = m2->next;
        free_item_data(m2);

        /* Free non-root marshallers */
        if (m2 != m) {
            free_items(m2);
            free(m2);
        }
    }

    m->next = NULL;
    m->n_items = 0;
    m->total_size = 0;

    d = m->data;
    d->last_marshaller = d->marshallers;
    d->total_size = 0;
    d->base = 0;
    d->current_buffer_item = NULL;
    d->current_buffer = d->buffers;
    d->current_buffer_position = 0;
}

void spice_marshaller_destroy(SpiceMarshaller *m)
{
    MarshallerBuffer *buf, *next;
    SpiceMarshallerData *d;

    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    spice_marshaller_reset(m);

    free_items(m);

    d = m->data;

    buf = d->buffers->next;
    while (buf != NULL) {
        next = buf->next;
        free(buf);
        buf = next;
    }

    free(d);
}

static MarshallerItem *spice_marshaller_add_item(SpiceMarshaller *m)
{
    MarshallerItem *item;

    if (m->n_items == m->items_size) {
        int items_size = m->items_size * 2;

        if (m->items == m->static_items) {
            m->items = spice_new(MarshallerItem, items_size);
            memcpy(m->items, m->static_items, sizeof(MarshallerItem) * m->n_items);
        } else {
            m->items = spice_renew(MarshallerItem, m->items, items_size);
        }
        m->items_size = items_size;
    }
    item = &m->items[m->n_items++];
    item->free_data = NULL;

    return item;
}

static size_t remaining_buffer_size(SpiceMarshallerData *d)
{
    return MARSHALLER_BUFFER_SIZE - d->current_buffer_position;
}

uint8_t *spice_marshaller_reserve_space(SpiceMarshaller *m, size_t size)
{
    MarshallerItem *item;
    SpiceMarshallerData *d;
    uint8_t *res;

    if (size == 0) {
        return NULL;
    }

    d = m->data;

    /* Check current item */
    item = &m->items[m->n_items - 1];
    if (item == d->current_buffer_item &&
        remaining_buffer_size(d) >= size) {
        assert(m->n_items >= 1);
        /* We can piggy back on existing item+buffer */
        res = item->data + item->len;
        item->len += size;
        d->current_buffer_position += size;
        d->total_size += size;
        m->total_size += size;
        return res;
    }

    item = spice_marshaller_add_item(m);

    if (remaining_buffer_size(d) >= size) {
        /* Fits in current buffer */
        item->data = d->current_buffer->data + d->current_buffer_position;
        item->len = size;
        d->current_buffer_position += size;
        d->current_buffer_item = item;
    } else if (size > MARSHALLER_BUFFER_SIZE / 2) {
        /* Large item, allocate by itself */
        item->data = (uint8_t *)spice_malloc(size);
        item->len = size;
        item->free_data = (spice_marshaller_item_free_func)free;
        item->opaque = NULL;
    } else {
        /* Use next buffer */
        if (d->current_buffer->next == NULL) {
            d->current_buffer->next = spice_new(MarshallerBuffer, 1);
            d->current_buffer->next->next = NULL;
        }
        d->current_buffer = d->current_buffer->next;
        d->current_buffer_position = size;
        d->current_buffer_item = item;
        item->data = d->current_buffer->data;
        item->len = size;
    }

    d->total_size += size;
    m->total_size += size;
    return item->data;
}

void spice_marshaller_unreserve_space(SpiceMarshaller *m, size_t size)
{
    MarshallerItem *item;

    if (size == 0) {
        return;
    }

    item = &m->items[m->n_items - 1];

    assert(item->len >= size);
    item->len -= size;
}

uint8_t *spice_marshaller_add_ref_full(SpiceMarshaller *m, uint8_t *data, size_t size,
                                       spice_marshaller_item_free_func free_data, void *opaque)
{
    MarshallerItem *item;
    SpiceMarshallerData *d;

    if (data == NULL || size == 0) {
        return NULL;
    }

    item = spice_marshaller_add_item(m);
    item->data = data;
    item->len = size;
    item->free_data = free_data;
    item->opaque = opaque;

    d = m->data;
    m->total_size += size;
    d->total_size += size;

    return data;
}

uint8_t *spice_marshaller_add(SpiceMarshaller *m, const uint8_t *data, size_t size)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, size);
    memcpy(ptr, data, size);
    return ptr;
}

uint8_t *spice_marshaller_add_ref(SpiceMarshaller *m, uint8_t *data, size_t size)
{
    return spice_marshaller_add_ref_full(m, data, size, NULL, NULL);
}

void spice_marshaller_add_ref_chunks(SpiceMarshaller *m, SpiceChunks *chunks)
{
    unsigned int i;

    for (i = 0; i < chunks->num_chunks; i++) {
        spice_marshaller_add_ref(m, chunks->chunk[i].data,
                                 chunks->chunk[i].len);
    }
}

SpiceMarshaller *spice_marshaller_get_submarshaller(SpiceMarshaller *m)
{
    SpiceMarshallerData *d;
    SpiceMarshaller *m2;

    d = m->data;

    m2 = spice_new(SpiceMarshaller, 1);
    spice_marshaller_init(m2, d);

    d->last_marshaller->next = m2;
    d->last_marshaller = m2;

    return m2;
}

SpiceMarshaller *spice_marshaller_get_ptr_submarshaller(SpiceMarshaller *m, int is_64bit)
{
    SpiceMarshaller *m2;
    uint8_t *p;
    int size;

    size = is_64bit ? 8 : 4;

    p = spice_marshaller_reserve_space(m, size);
    memset(p, 0, size);
    m2 = spice_marshaller_get_submarshaller(m);
    m2->pointer_ref.marshaller = m;
    m2->pointer_ref.item_nr = m->n_items - 1;
    m2->pointer_ref.offset = m->items[m->n_items - 1].len - size;
    m2->pointer_ref.is_64bit = is_64bit;

    return m2;
}

static uint8_t *lookup_ref(MarshallerRef *ref)
{
    MarshallerItem *item;

    item = &ref->marshaller->items[ref->item_nr];
    return item->data + ref->offset;
}


void spice_marshaller_set_base(SpiceMarshaller *m, size_t base)
{
    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    m->data->base = base;
}

uint8_t *spice_marshaller_linearize(SpiceMarshaller *m, size_t skip_bytes,
                                    size_t *len, int *free_res)
{
    MarshallerItem *item;
    uint8_t *res, *p;
    int i;

    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    if (m->n_items == 1) {
        *free_res = FALSE;
        if (m->items[0].len <= skip_bytes) {
            *len = 0;
            return NULL;
        }
        *len = m->items[0].len - skip_bytes;
        return m->items[0].data + skip_bytes;
    }

    *free_res = TRUE;
    res = (uint8_t *)spice_malloc(m->data->total_size - skip_bytes);
    *len = m->data->total_size - skip_bytes;
    p = res;

    do {
        for (i = 0; i < m->n_items; i++) {
            item = &m->items[i];

            if (item->len <= skip_bytes) {
                skip_bytes -= item->len;
                continue;
            }
            memcpy(p, item->data + skip_bytes, item->len - skip_bytes);
            p += item->len - skip_bytes;
            skip_bytes = 0;
        }
        m = m->next;
    } while (m != NULL);

    return res;
}

uint8_t *spice_marshaller_get_ptr(SpiceMarshaller *m)
{
    return m->items[0].data;
}

size_t spice_marshaller_get_offset(SpiceMarshaller *m)
{
    SpiceMarshaller *m2;
    size_t offset;

    offset = 0;
    m2 = m->data->marshallers;
    while (m2 != m) {
        offset += m2->total_size;
        m2 = m2->next;
    }
    return offset - m->data->base;
}

size_t spice_marshaller_get_size(SpiceMarshaller *m)
{
    return m->total_size;
}

size_t spice_marshaller_get_total_size(SpiceMarshaller *m)
{
    return m->data->total_size;
}

void spice_marshaller_flush(SpiceMarshaller *m)
{
    SpiceMarshaller *m2;
    uint8_t *ptr_pos;

    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    for (m2 = m; m2 != NULL; m2 = m2->next) {
        if (m2->pointer_ref.marshaller != NULL && m2->total_size > 0) {
            ptr_pos = lookup_ref(&m2->pointer_ref);
            if (m2->pointer_ref.is_64bit) {
                write_uint64(ptr_pos,
                             spice_marshaller_get_offset(m2));
            } else {
                write_uint32(ptr_pos,
                             spice_marshaller_get_offset(m2));
            }
        }
    }
}

#ifndef WIN32
int spice_marshaller_fill_iovec(SpiceMarshaller *m, struct iovec *vec,
                                int n_vec, size_t skip_bytes)
{
    MarshallerItem *item;
    int v, i;

    /* Only supported for root marshaller */
    assert(m->data->marshallers == m);

    v = 0;
    do {
        for (i = 0; i < m->n_items; i++) {
            item = &m->items[i];

            if (item->len <= skip_bytes) {
                skip_bytes -= item->len;
                continue;
            }
            if (v == n_vec) {
                return v; /* Not enough space in vec */
            }
            vec[v].iov_base = (uint8_t *)item->data + skip_bytes;
            vec[v].iov_len = item->len - skip_bytes;
            skip_bytes = 0;
            v++;
        }
        m = m->next;
    } while (m != NULL);

    return v;
}
#endif

void *spice_marshaller_add_uint64(SpiceMarshaller *m, uint64_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(uint64_t));
    write_uint64(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_int64(SpiceMarshaller *m, int64_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(int64_t));
    write_int64(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_uint32(SpiceMarshaller *m, uint32_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(uint32_t));
    write_uint32(ptr, v);
    return (void *)ptr;
}

void spice_marshaller_set_uint32(SpiceMarshaller *m, void *ref, uint32_t v)
{
    write_uint32((uint8_t *)ref, v);
}

void *spice_marshaller_add_int32(SpiceMarshaller *m, int32_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(int32_t));
    write_int32(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_uint16(SpiceMarshaller *m, uint16_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(uint16_t));
    write_uint16(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_int16(SpiceMarshaller *m, int16_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(int16_t));
    write_int16(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_uint8(SpiceMarshaller *m, uint8_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(uint8_t));
    write_uint8(ptr, v);
    return (void *)ptr;
}

void *spice_marshaller_add_int8(SpiceMarshaller *m, int8_t v)
{
    uint8_t *ptr;

    ptr = spice_marshaller_reserve_space(m, sizeof(int8_t));
    write_int8(ptr, v);
    return (void *)ptr;
}
