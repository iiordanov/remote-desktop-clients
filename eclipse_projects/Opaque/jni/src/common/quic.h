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

#ifndef __QUIC_H
#define __QUIC_H

#include <spice/macros.h>
#include "quic_config.h"
#include "macros.h"

SPICE_BEGIN_DECLS

typedef enum {
    QUIC_IMAGE_TYPE_INVALID,
    QUIC_IMAGE_TYPE_GRAY,
    QUIC_IMAGE_TYPE_RGB16,
    QUIC_IMAGE_TYPE_RGB24,
    QUIC_IMAGE_TYPE_RGB32,
    QUIC_IMAGE_TYPE_RGBA
} QuicImageType;

#define QUIC_ERROR -1
#define QUIC_OK 0

typedef void *QuicContext;

typedef struct QuicUsrContext QuicUsrContext;
struct QuicUsrContext {
    SPICE_ATTR_PRINTF(2, 3) void (*error)(QuicUsrContext *usr, const char *fmt, ...);
    SPICE_ATTR_PRINTF(2, 3) void (*warn)(QuicUsrContext *usr, const char *fmt, ...);
    SPICE_ATTR_PRINTF(2, 3) void (*info)(QuicUsrContext *usr, const char *fmt, ...);
    void *(*malloc)(QuicUsrContext *usr, int size);
    void (*free)(QuicUsrContext *usr, void *ptr);
    int (*more_space)(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed);
    int (*more_lines)(QuicUsrContext *usr, uint8_t **lines); // on return the last line of previous
                                                             // lines bunch must still be valid
};

int quic_encode(QuicContext *quic, QuicImageType type, int width, int height,
                uint8_t *lines, unsigned int num_lines, int stride,
                uint32_t *io_ptr, unsigned int num_io_words);

int quic_decode_begin(QuicContext *quic, uint32_t *io_ptr, unsigned int num_io_words,
                      QuicImageType *type, int *width, int *height);
int quic_decode(QuicContext *quic, QuicImageType type, uint8_t *buf, int stride);


QuicContext *quic_create(QuicUsrContext *usr);
void quic_destroy(QuicContext *quic);

void quic_init(void);

SPICE_END_DECLS

#endif
