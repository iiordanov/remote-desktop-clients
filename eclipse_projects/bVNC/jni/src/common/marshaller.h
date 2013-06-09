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

#ifndef _H_MARSHALLER
#define _H_MARSHALLER

#include <spice/macros.h>
#include <spice/types.h>
#include "mem.h"
#ifndef WIN32
#include <sys/uio.h>
#endif

SPICE_BEGIN_DECLS

typedef struct SpiceMarshaller SpiceMarshaller;
typedef void (*spice_marshaller_item_free_func)(uint8_t *data, void *opaque);

SpiceMarshaller *spice_marshaller_new(void);
void spice_marshaller_reset(SpiceMarshaller *m);
void spice_marshaller_destroy(SpiceMarshaller *m);
uint8_t *spice_marshaller_reserve_space(SpiceMarshaller *m, size_t size);
void spice_marshaller_unreserve_space(SpiceMarshaller *m, size_t size);
uint8_t *spice_marshaller_add(SpiceMarshaller *m, const uint8_t *data, size_t size);
uint8_t *spice_marshaller_add_ref(SpiceMarshaller *m, uint8_t *data, size_t size);
uint8_t *spice_marshaller_add_ref_full(SpiceMarshaller *m, uint8_t *data, size_t size,
                                       spice_marshaller_item_free_func free_data, void *opaque);
void     spice_marshaller_add_ref_chunks(SpiceMarshaller *m, SpiceChunks *chunks);
void spice_marshaller_flush(SpiceMarshaller *m);
void spice_marshaller_set_base(SpiceMarshaller *m, size_t base);
uint8_t *spice_marshaller_linearize(SpiceMarshaller *m, size_t skip,
                                    size_t *len, int *free_res);
uint8_t *spice_marshaller_get_ptr(SpiceMarshaller *m);
size_t spice_marshaller_get_offset(SpiceMarshaller *m);
size_t spice_marshaller_get_size(SpiceMarshaller *m);
size_t spice_marshaller_get_total_size(SpiceMarshaller *m);
SpiceMarshaller *spice_marshaller_get_submarshaller(SpiceMarshaller *m);
SpiceMarshaller *spice_marshaller_get_ptr_submarshaller(SpiceMarshaller *m, int is_64bit);
#ifndef WIN32
int spice_marshaller_fill_iovec(SpiceMarshaller *m, struct iovec *vec,
                                int n_vec, size_t skip_bytes);
#endif
void *spice_marshaller_add_uint64(SpiceMarshaller *m, uint64_t v);
void *spice_marshaller_add_int64(SpiceMarshaller *m, int64_t v);
void *spice_marshaller_add_uint32(SpiceMarshaller *m, uint32_t v);
void *spice_marshaller_add_int32(SpiceMarshaller *m, int32_t v);
void *spice_marshaller_add_uint16(SpiceMarshaller *m, uint16_t v);
void *spice_marshaller_add_int16(SpiceMarshaller *m, int16_t v);
void *spice_marshaller_add_uint8(SpiceMarshaller *m, uint8_t v);
void *spice_marshaller_add_int8(SpiceMarshaller *m, int8_t v);

void  spice_marshaller_set_uint32(SpiceMarshaller *m, void *ref, uint32_t v);

SPICE_END_DECLS

#endif
