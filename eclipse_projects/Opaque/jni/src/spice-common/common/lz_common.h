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
   License along with this library; if not, write to the Free Software

   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*common header for encoder and decoder*/

#ifndef _LZ_COMMON_H
#define _LZ_COMMON_H

#include <spice/macros.h>
#include "verify.h"

SPICE_BEGIN_DECLS

//#define DEBUG

/* change the max window size will require change in the encoding format*/
#define LZ_MAX_WINDOW_SIZE (1 << 25)
#define MAX_COPY 32

typedef enum {
    LZ_IMAGE_TYPE_INVALID,
    LZ_IMAGE_TYPE_PLT1_LE,
    LZ_IMAGE_TYPE_PLT1_BE,      // PLT stands for palette
    LZ_IMAGE_TYPE_PLT4_LE,
    LZ_IMAGE_TYPE_PLT4_BE,
    LZ_IMAGE_TYPE_PLT8,
    LZ_IMAGE_TYPE_RGB16,
    LZ_IMAGE_TYPE_RGB24,
    LZ_IMAGE_TYPE_RGB32,
    LZ_IMAGE_TYPE_RGBA,
    LZ_IMAGE_TYPE_XXXA,
    LZ_IMAGE_TYPE_A8
} LzImageType;

#define LZ_IMAGE_TYPE_MASK 0x0f
#define LZ_IMAGE_TYPE_LOG 4 // number of bits required for coding the image type

/* access to the arrays is based on the image types */
static const int IS_IMAGE_TYPE_PLT[] = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
static const int IS_IMAGE_TYPE_RGB[] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};
static const int PLT_PIXELS_PER_BYTE[] = {0, 8, 8, 2, 2, 1};
static const int RGB_BYTES_PER_PIXEL[] = {0, 1, 1, 1, 1, 1, 2, 3, 4, 4, 4, 1};

verify(SPICE_N_ELEMENTS(IS_IMAGE_TYPE_PLT) == (LZ_IMAGE_TYPE_A8 + 1));
verify(SPICE_N_ELEMENTS(IS_IMAGE_TYPE_RGB) == (LZ_IMAGE_TYPE_A8 + 1));
verify(SPICE_N_ELEMENTS(PLT_PIXELS_PER_BYTE) == (LZ_IMAGE_TYPE_PLT8 + 1));
verify(SPICE_N_ELEMENTS(RGB_BYTES_PER_PIXEL) == (LZ_IMAGE_TYPE_A8 + 1));

#define LZ_MAGIC (*(uint32_t *)"LZ  ")
#define LZ_VERSION_MAJOR 1U
#define LZ_VERSION_MINOR 1U
#define LZ_VERSION ((LZ_VERSION_MAJOR << 16) | (LZ_VERSION_MINOR & 0xffff))

SPICE_END_DECLS

#endif  // _LZ_COMMON_H
