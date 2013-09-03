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

#ifndef _H_GLCTX
#define _H_GLCTX

#include <spice/macros.h>

SPICE_BEGIN_DECLS

typedef struct OGLCtx OGLCtx;

const char *oglctx_type_str(OGLCtx *ctx);
void oglctx_make_current(OGLCtx *ctx);
OGLCtx *pbuf_create(int width, int heigth);
OGLCtx *pixmap_create(int width, int heigth);
void oglctx_destroy(OGLCtx *ctx);

SPICE_END_DECLS

#endif
