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

#ifndef _H_GL_CANVASE
#define _H_GL_CANVASE

#include <stdint.h>
#include <spice/macros.h>

SPICE_BEGIN_DECLS

typedef void * GLCCtx;
typedef void * GLCPattern;
typedef void * GLCPath;

typedef struct GLCRect {
    double x;
    double y;
    double width;
    double height;
} GLCRect;

typedef struct GLCRecti {
    int x;
    int y;
    int width;
    int height;
} GLCRecti;

typedef enum  {
    GLC_IMAGE_RGB32,
    GLC_IMAGE_ARGB32,
} GLCImageFormat;

typedef struct GLCPImage {
    GLCImageFormat format;
    int width;
    int height;
    int stride;
    uint8_t *pixels;
    uint32_t *pallet;
} GLCImage;

GLCPattern glc_pattern_create(GLCCtx glc, int x_orign, int y_orign, const GLCImage *image);
void glc_pattern_set(GLCPattern pattern, int x_orign, int y_orign, const GLCImage *image);
void glc_pattern_destroy(GLCPattern pattern);

void glc_path_move_to(GLCPath path, double x, double y);
void glc_path_line_to(GLCPath path, double x, double y);
void glc_path_curve_to(GLCPath path, double p1_x, double p1_y, double p2_x, double p2_y,
                       double p3_x, double p3_y);
void glc_path_rel_move_to(GLCPath path, double x, double y);
void glc_path_rel_line_to(GLCPath path, double x, double y);
void glc_path_rel_curve_to(GLCPath path, double p1_x, double p1_y, double p2_x, double p2_y,
                           double p3_x, double p3_y);
void glc_path_close(GLCPath path);

void glc_path_cleare(GLCPath);
GLCPath glc_path_create(GLCCtx glc);
void glc_path_destroy(GLCPath path);

void glc_set_rgb(GLCCtx glc, double red, double green, double blue);
void glc_set_rgba(GLCCtx glc, double red, double green, double blue, double alpha);
void glc_set_pattern(GLCCtx glc, GLCPattern pattern);

typedef enum  {
    GLC_OP_CLEAR = 0x1500,
    GLC_OP_SET = 0x150F,
    GLC_OP_COPY = 0x1503,
    GLC_OP_COPY_INVERTED = 0x150C,
    GLC_OP_NOOP = 0x1505,
    GLC_OP_INVERT = 0x150A,
    GLC_OP_AND = 0x1501,
    GLC_OP_NAND = 0x150E,
    GLC_OP_OR = 0x1507,
    GLC_OP_NOR = 0x1508,
    GLC_OP_XOR = 0x1506,
    GLC_OP_EQUIV = 0x1509,
    GLC_OP_AND_REVERSE = 0x1502,
    GLC_OP_AND_INVERTED = 0x1504,
    GLC_OP_OR_REVERSE = 0x150B,
    GLC_OP_OR_INVERTED = 0x150D,
} GLCOp;

void glc_set_op(GLCCtx glc, GLCOp op);
void glc_set_alpha_factor(GLCCtx glc, double alpah);

typedef enum  {
    GLC_FILL_MODE_WINDING_ODD,
    GLC_FILL_MODE_WINDING_NONZERO,
} GLCFillMode;

void glc_set_fill_mode(GLCCtx glc, GLCFillMode mode);
void glc_set_line_width(GLCCtx glc, double width);
void glc_set_line_end_cap(GLCCtx glc, int style);
void glc_set_line_join(GLCCtx glc, int style);
void glc_set_miter_limit(GLCCtx glc, int limit);
void glc_set_line_dash(GLCCtx glc, const double *dashes, int num_dashes, double offset);

typedef enum  {
    GLC_MASK_A,
    GLC_MASK_B,
} GLCMaskID;

void glc_set_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height,
                  int stride, const uint8_t *bitmap, GLCMaskID id);
void glc_mask_rects(GLCCtx glc, int num_rect, GLCRect *rects, GLCMaskID id);
void glc_clear_mask(GLCCtx glc, GLCMaskID id);

typedef enum {
    GLC_CLIP_OP_SET,
    GLC_CLIP_OP_OR,
    GLC_CLIP_OP_AND,
    GLC_CLIP_OP_EXCLUDE,
} GLCClipOp;

void glc_clip_rect(GLCCtx glc, const GLCRect *rect, GLCClipOp op);
void glc_clip_path(GLCCtx glc, GLCPath path, GLCClipOp op);
void glc_clip_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height, int stride,
                   const uint8_t *bitmap, GLCClipOp op);
void glc_clip_reset(GLCCtx glc);

void glc_fill_rect(GLCCtx glc, const GLCRect *rect);
void glc_fill_path(GLCCtx glc, GLCPath path);
void _glc_fill_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height, int stride,
                    const uint8_t *bitmap);
void glc_fill_alpha(GLCCtx glc, int x_dest, int y_dest, int width, int height, int stride,
                    const uint8_t *alpha_mask);

void glc_stroke_rect(GLCCtx glc, const GLCRect *rect);
void glc_stroke_path(GLCCtx glc, GLCPath path);

void glc_draw_image(GLCCtx glc, const GLCRecti *dest, const GLCRecti *src, const GLCImage *image,
                    int scale_mode, double alpha);

void glc_copy_pixels(GLCCtx glc, int x_dest, int y_dest, int x_src, int y_src, int width,
                     int height);
void glc_read_pixels(GLCCtx glc, int x, int y, GLCImage *image);

void glc_flush(GLCCtx glc);
void glc_clear(GLCCtx glc);
GLCCtx glc_create(int width, int height);
void glc_destroy(GLCCtx glc, int textures_lost);

SPICE_END_DECLS

#endif
