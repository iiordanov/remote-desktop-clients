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

#ifndef GL_UTILS_H
#define GL_UTILS_H

#include <spice/macros.h>
#include "spice_common.h"

SPICE_BEGIN_DECLS

#ifdef RED_DEBUG
#define GLC_ERROR_TEST_FLUSH {                                          \
    GLenum gl_err;  glFlush();                                          \
    if ((gl_err = glGetError()) != GL_NO_ERROR) {                       \
        printf("%s[%d]: opengl error: %s\n",  __FUNCTION__, __LINE__,   \
        gluErrorString(gl_err));                                        \
        spice_abort();                                                  \
    }                                                                   \
}

#define GLC_ERROR_TEST_FINISH {                                         \
    GLenum gl_err;  glFinish();                                         \
    if ((gl_err = glGetError()) != GL_NO_ERROR) {                       \
        printf("%s[%d]: opengl error: %s\n",  __FUNCTION__, __LINE__,   \
        gluErrorString(gl_err));                                        \
        spice_abort();                                                  \
    }                                                                   \
}
#else
#define GLC_ERROR_TEST_FLUSH ;

#define GLC_ERROR_TEST_FINISH ;
#endif

#include "bitops.h"

#define find_msb spice_bit_find_msb
#define gl_get_to_power_two spice_bit_next_pow2

SPICE_END_DECLS

#endif
