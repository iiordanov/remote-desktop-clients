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

#ifndef _H_MEM
#define _H_MEM

#include <stdlib.h>
#include <spice/macros.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

SPICE_BEGIN_DECLS

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#elif defined __GNUC__
#if !defined alloca
# define alloca __builtin_alloca
#endif
#elif defined _AIX
# define alloca __alloca
#elif defined _MSC_VER
# include <malloc.h>
# define alloca _alloca
#else
# ifndef HAVE_ALLOCA
#  ifdef  __cplusplus
extern "C"
#  endif
void *alloca (size_t);
# endif
#endif

typedef struct SpiceChunk {
    uint8_t *data;
    uint32_t len;
} SpiceChunk;

enum {
    SPICE_CHUNKS_FLAGS_UNSTABLE = (1<<0),
    SPICE_CHUNKS_FLAGS_FREE = (1<<1)
};

typedef struct SpiceChunks {
    uint32_t     data_size;
    uint32_t     num_chunks;
    uint32_t     flags;
    SpiceChunk   chunk[0];
} SpiceChunks;

typedef struct SpiceBuffer
{
    size_t capacity;
    size_t offset;
    uint8_t *buffer;
} SpiceBuffer;

char *spice_strdup(const char *str) SPICE_GNUC_MALLOC;
char *spice_strndup(const char *str, size_t n_bytes) SPICE_GNUC_MALLOC;
void *spice_memdup(const void *mem, size_t n_bytes) SPICE_GNUC_MALLOC;
void *spice_malloc(size_t n_bytes) SPICE_GNUC_MALLOC SPICE_GNUC_ALLOC_SIZE(1);
void *spice_malloc0(size_t n_bytes) SPICE_GNUC_MALLOC SPICE_GNUC_ALLOC_SIZE(1);
void *spice_realloc(void *mem, size_t n_bytes) SPICE_GNUC_WARN_UNUSED_RESULT;
void *spice_malloc_n(size_t n_blocks, size_t n_block_bytes) SPICE_GNUC_MALLOC SPICE_GNUC_ALLOC_SIZE2(1,2);
void *spice_malloc_n_m(size_t n_blocks, size_t n_block_bytes, size_t extra_size) SPICE_GNUC_MALLOC;
void *spice_malloc0_n(size_t n_blocks, size_t n_block_bytes) SPICE_GNUC_MALLOC SPICE_GNUC_ALLOC_SIZE2(1,2);
void *spice_realloc_n(void *mem, size_t n_blocks, size_t n_block_bytes) SPICE_GNUC_WARN_UNUSED_RESULT;
SpiceChunks *spice_chunks_new(uint32_t count) SPICE_GNUC_MALLOC;
SpiceChunks *spice_chunks_new_linear(uint8_t *data, uint32_t len) SPICE_GNUC_MALLOC;
void spice_chunks_destroy(SpiceChunks *chunks);
void spice_chunks_linearize(SpiceChunks *chunks);

size_t spice_strnlen(const char *str, size_t max_len);

/* Optimize: avoid the call to the (slower) _n function if we can
 * determine at compile-time that no overflow happens.
 */
#if defined (__GNUC__) && (__GNUC__ >= 2) && defined (__OPTIMIZE__)
#  define _SPICE_NEW(struct_type, n_structs, func)        \
    (struct_type *) (__extension__ ({                     \
        size_t __n = (size_t) (n_structs);                \
        size_t __s = sizeof (struct_type);                \
        void *__p;                                        \
        if (__s == 1)                                     \
            __p = spice_##func (__n);                     \
        else if (__builtin_constant_p (__n) &&            \
                 __n <= SIZE_MAX / __s)                   \
            __p = spice_##func (__n * __s);               \
        else                                              \
            __p = spice_##func##_n (__n, __s);            \
        __p;                                              \
    }))
#  define _SPICE_RENEW(struct_type, mem, n_structs, func) \
    (struct_type *) (__extension__ ({                             \
        size_t __n = (size_t) (n_structs);                \
        size_t __s = sizeof (struct_type);                \
        void *__p = (void *) (mem);                       \
        if (__s == 1)                                     \
            __p = spice_##func (__p, __n);                \
        else if (__builtin_constant_p (__n) &&            \
                 __n <= SIZE_MAX / __s)                   \
            __p = spice_##func (__p, __n * __s);          \
        else                                              \
            __p = spice_##func##_n (__p, __n, __s);       \
        __p;                                              \
    }))
#else

/* Unoptimized version: always call the _n() function. */

#define _SPICE_NEW(struct_type, n_structs, func)                        \
    ((struct_type *) spice_##func##_n ((n_structs), sizeof (struct_type)))
#define _SPICE_RENEW(struct_type, mem, n_structs, func)                 \
    ((struct_type *) spice_##func##_n (mem, (n_structs), sizeof (struct_type)))

#endif

#define spice_new(struct_type, n_structs) _SPICE_NEW(struct_type, n_structs, malloc)
#define spice_new0(struct_type, n_structs) _SPICE_NEW(struct_type, n_structs, malloc0)
#define spice_renew(struct_type, mem, n_structs) _SPICE_RENEW(struct_type, mem, n_structs, realloc)

/* Buffer management */
void spice_buffer_reserve(SpiceBuffer *buffer, size_t len);
int spice_buffer_empty(SpiceBuffer *buffer);
uint8_t *spice_buffer_end(SpiceBuffer *buffer);
void spice_buffer_reset(SpiceBuffer *buffer);
void spice_buffer_free(SpiceBuffer *buffer);
void spice_buffer_append(SpiceBuffer *buffer, const void *data, size_t len);
size_t spice_buffer_copy(SpiceBuffer *buffer, void *dest, size_t len);
size_t spice_buffer_remove(SpiceBuffer *buffer, size_t len);

SPICE_END_DECLS

#endif
