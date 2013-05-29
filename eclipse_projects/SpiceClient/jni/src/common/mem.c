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

#include "spice_common.h"
#include "mem.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef MALLOC_ERROR
#define MALLOC_ERROR(format, ...) SPICE_STMT_START {    \
    spice_error(format, ## __VA_ARGS__);                \
    abort();                                            \
} SPICE_STMT_END
#endif

size_t spice_strnlen(const char *str, size_t max_len)
{
    size_t len = 0;

    while (len < max_len && *str != 0) {
        len++;
        str++;
    }

    return len;
}

char *spice_strdup(const char *str)
{
    char *copy;

    if (str == NULL) {
        return NULL;
    }

    copy = (char *)spice_malloc(strlen(str) + 1);
    strcpy(copy, str);
    return copy;
}

char *spice_strndup(const char *str, size_t n_bytes)
{
    char *copy;

    if (str == NULL) {
        return NULL;
    }

    copy = (char *)spice_malloc(n_bytes + 1);
    strncpy(copy, str, n_bytes);
    copy[n_bytes] = 0;
    return copy;
}

void *spice_memdup(const void *mem, size_t n_bytes)
{
    void *copy;

    if (mem == NULL) {
        return NULL;
    }

    copy = spice_malloc(n_bytes);
    memcpy(copy, mem, n_bytes);
    return copy;
}

void *spice_malloc(size_t n_bytes)
{
    void *mem;

    if (SPICE_LIKELY(n_bytes)) {
        mem = malloc(n_bytes);

        if (SPICE_LIKELY(mem != NULL)) {
            return mem;
        }

        MALLOC_ERROR("unable to allocate %lu bytes", (unsigned long)n_bytes);
    }
    return NULL;
}

void *spice_malloc0(size_t n_bytes)
{
    void *mem;

    if (SPICE_LIKELY(n_bytes)) {
        mem = calloc(1, n_bytes);

        if (SPICE_LIKELY(mem != NULL)) {
            return mem;
        }

        MALLOC_ERROR("unable to allocate %lu bytes", (unsigned long)n_bytes);
    }
    return NULL;
}

void *spice_realloc(void *mem, size_t n_bytes)
{
    if (SPICE_LIKELY(n_bytes)) {
        mem = realloc(mem, n_bytes);

        if (SPICE_LIKELY(mem != NULL)) {
            return mem;
        }

        MALLOC_ERROR("unable to allocate %lu bytes", (unsigned long)n_bytes);
    }

    free(mem);

    return NULL;
}

#define SIZE_OVERFLOWS(a,b) (SPICE_UNLIKELY ((a) > SIZE_MAX / (b)))

void *spice_malloc_n(size_t n_blocks, size_t n_block_bytes)
{
    if (SIZE_OVERFLOWS (n_blocks, n_block_bytes)) {
        MALLOC_ERROR("overflow allocating %lu*%lu bytes",
                     (unsigned long)n_blocks, (unsigned long)n_block_bytes);
    }

    return spice_malloc(n_blocks * n_block_bytes);
}

void *spice_malloc_n_m(size_t n_blocks, size_t n_block_bytes, size_t extra_size)
{
    size_t size1, size2;
    if (SIZE_OVERFLOWS (n_blocks, n_block_bytes)) {
        MALLOC_ERROR("spice_malloc_n: overflow allocating %lu*%lu + %lubytes",
                     (unsigned long)n_blocks, (unsigned long)n_block_bytes, (unsigned long)extra_size);
    }
    size1 = n_blocks * n_block_bytes;
    size2 = size1 + extra_size;
    if (size2 < size1) {
        MALLOC_ERROR("spice_malloc_n: overflow allocating %lu*%lu + %lubytes",
                     (unsigned long)n_blocks, (unsigned long)n_block_bytes, (unsigned long)extra_size);
    }
    return spice_malloc(size2);
}


void *spice_malloc0_n(size_t n_blocks, size_t n_block_bytes)
{
    if (SIZE_OVERFLOWS (n_blocks, n_block_bytes)) {
        MALLOC_ERROR("spice_malloc0_n: overflow allocating %lu*%lu bytes",
                     (unsigned long)n_blocks, (unsigned long)n_block_bytes);
    }

    return spice_malloc0 (n_blocks * n_block_bytes);
}

void *spice_realloc_n(void *mem, size_t n_blocks, size_t n_block_bytes)
{
    if (SIZE_OVERFLOWS (n_blocks, n_block_bytes)) {
        MALLOC_ERROR("spice_realloc_n: overflow allocating %lu*%lu bytes",
                     (unsigned long)n_blocks, (unsigned long)n_block_bytes);
    }

    return spice_realloc(mem, n_blocks * n_block_bytes);
}

SpiceChunks *spice_chunks_new(uint32_t count)
{
    SpiceChunks *chunks;

    chunks = (SpiceChunks *)spice_malloc_n_m(count, sizeof(SpiceChunk), sizeof(SpiceChunks));
    chunks->flags = 0;
    chunks->num_chunks = count;

    return chunks;
}

SpiceChunks *spice_chunks_new_linear(uint8_t *data, uint32_t len)
{
    SpiceChunks *chunks;

    chunks = spice_chunks_new(1);
    chunks->data_size = chunks->chunk[0].len = len;
    chunks->chunk[0].data = data;
    return chunks;
}

void spice_chunks_destroy(SpiceChunks *chunks)
{
    unsigned int i;

    if (chunks->flags & SPICE_CHUNKS_FLAGS_FREE) {
        for (i = 0; i < chunks->num_chunks; i++) {
            free(chunks->chunk[i].data);
        }
    }

    free(chunks);
}

void spice_chunks_linearize(SpiceChunks *chunks)
{
    uint8_t *data, *p;
    unsigned int i;

    if (chunks->num_chunks > 1) {
        data = (uint8_t*)spice_malloc(chunks->data_size);
        for (p = data, i = 0; i < chunks->num_chunks; i++) {
            memcpy(p, chunks->chunk[i].data,
                   chunks->chunk[i].len);
            p += chunks->chunk[i].len;
        }
        if (chunks->flags & SPICE_CHUNKS_FLAGS_FREE) {
            for (i = 0; i < chunks->num_chunks; i++) {
                free(chunks->chunk[i].data);
            }
        }
        chunks->num_chunks = 1;
        chunks->flags |= SPICE_CHUNKS_FLAGS_FREE;
        chunks->flags &= ~SPICE_CHUNKS_FLAGS_UNSTABLE;
        chunks->chunk[0].data = data;
        chunks->chunk[0].len = chunks->data_size;
    }
}

void spice_buffer_reserve(SpiceBuffer *buffer, size_t len)
{
    if ((buffer->capacity - buffer->offset) < len) {
        buffer->capacity += (len + 1024);
        buffer->buffer = (uint8_t*)spice_realloc(buffer->buffer, buffer->capacity);
    }
}

int spice_buffer_empty(SpiceBuffer *buffer)
{
    return buffer->offset == 0;
}

uint8_t *spice_buffer_end(SpiceBuffer *buffer)
{
    return buffer->buffer + buffer->offset;
}

void spice_buffer_reset(SpiceBuffer *buffer)
{
    buffer->offset = 0;
}

void spice_buffer_free(SpiceBuffer *buffer)
{
    free(buffer->buffer);
    buffer->offset = 0;
    buffer->capacity = 0;
    buffer->buffer = NULL;
}

void spice_buffer_append(SpiceBuffer *buffer, const void *data, size_t len)
{
    spice_buffer_reserve(buffer, len);
    memcpy(buffer->buffer + buffer->offset, data, len);
    buffer->offset += len;
}

size_t spice_buffer_copy(SpiceBuffer *buffer, void *dest, size_t len)
{
    len = MIN(buffer->offset, len);
    memcpy(dest, buffer->buffer, len);
    return len;
}

size_t spice_buffer_remove(SpiceBuffer *buffer, size_t len)
{
    len = MIN(buffer->offset, len);
    memmove(buffer->buffer, buffer->buffer + len, buffer->offset - len);
    buffer->offset -= len;
    return len;
}
