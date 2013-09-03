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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <spice/macros.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#ifdef WIN32
#include "glext.h"
#include "wglext.h"
#endif

#include "mem.h"
#include "glc.h"
#include "gl_utils.h"
#include "spice_common.h"

#define TESS_VERTEX_ALLOC_BUNCH 20

typedef struct InternaCtx InternaCtx;
typedef struct InternalPat {
    InternaCtx *owner;
    int refs;
    GLuint texture;
    int x_orign;
    int y_orign;
    int width;
    int height;
} InternalPat;

typedef struct Pathpath {
    int start_point;
    int num_segments;
} Path;

enum  {
    GLC_PATH_SEG_LINES,
    GLC_PATH_SEG_BEIZER,
};

//todo: flatten cache
typedef struct PathSegment {
    int type;
    int count;
} PathSegment;

typedef struct PathPoint {
    double x;
    double y;
    double z;
} PathPoint;

typedef GLdouble Vertex[3];

typedef struct InternalPath {
    InternaCtx *owner;

    Path *paths;
    int paths_size;
    int paths_pos;

    PathSegment *segments;
    int segments_size;
    int segments_pos;

    PathPoint *points;
    int points_size;
    int points_pos;

    Path *current_path;
    PathSegment *current_segment;
} InternalPath;

typedef struct TassVertex TassVertex;
struct TassVertex {
    PathPoint point;
    TassVertex *list_link;
    TassVertex *next;
};

typedef struct TassVertexBuf TassVertexBuf;
struct TassVertexBuf {
    TassVertexBuf *next;
    TassVertex vertexs[0];
};

#define USE_LINE_ANTIALIAS 0

typedef struct LineDash {
    double *dashes;
    int num_dashes;
    double offset;
    int cur_dash;
    double dash_pos;
} LineDash;

enum  {
    GLC_STROKE_NONACTIVE,
    GLC_STROKE_FIRST,
    GLC_STROKE_ACTIVE,
};

typedef struct PathStroke {
    double x;
    double y;
    int state;
} PathStroke;

struct InternaCtx {
    int draw_mode;
    int stencil_refs;
    int stencil_mask;
    int width;
    int height;
    GLfloat line_width;
    LineDash line_dash;
    PathStroke path_stroke;
    InternalPat *pat;
    int max_texture_size;
    GLUtesselator* tesselator;
    TassVertex *free_tess_vertex;
    TassVertex *used_tess_vertex;
    TassVertexBuf *vertex_bufs;
    int private_tex_width;
    int private_tex_height;
    GLuint private_tex;
#ifdef WIN32
    PFNGLBLENDEQUATIONPROC glBlendEquation;
#endif
};

#define Y(y) -(y)
#define VERTEX2(x, y) glVertex2d(x, Y(y))

static void fill_rect(InternaCtx *ctx, void *rect);
static void fill_path(InternaCtx *ctx, void *path);
static void fill_mask(InternaCtx *ctx, int x_dest, int y_dest, int width, int height, int stride,
                      const uint8_t *bitmap);
static void set_pat(InternaCtx *ctx, InternalPat *pat);

static inline void set_raster_pos(InternaCtx *ctx, int x, int y)
{
    if (x >= 0 && y >= 0 && x < ctx->width && y < ctx->height) {
        glRasterPos2i(x, Y(y));
        return;
    }
    glRasterPos2i(0, 0);
    glBitmap(0, 0, 0, 0, (GLfloat)x, (GLfloat)Y(y), NULL);
}

static TassVertex *alloc_tess_vertex(InternaCtx *ctx)
{
    TassVertex *vertex;

    if (!ctx->free_tess_vertex) {
        TassVertexBuf *buf;
        int i;

        buf = (TassVertexBuf *)spice_malloc(sizeof(TassVertexBuf) +
                                            sizeof(TassVertex) * TESS_VERTEX_ALLOC_BUNCH);
        buf->next = ctx->vertex_bufs;
        ctx->vertex_bufs = buf;
        for (i = 0; i < TESS_VERTEX_ALLOC_BUNCH; i++) {
            buf->vertexs[i].point.z = 0;
            buf->vertexs[i].next = ctx->free_tess_vertex;
            ctx->free_tess_vertex = &buf->vertexs[i];
        }
    }

    vertex = ctx->free_tess_vertex;
    ctx->free_tess_vertex = vertex->next;
    vertex->next = ctx->used_tess_vertex;
    ctx->used_tess_vertex = vertex;
    return vertex;
}

static void reset_tass_vertex(InternaCtx *ctx)
{
    TassVertex *vertex;
    while ((vertex = ctx->used_tess_vertex)) {
        ctx->used_tess_vertex = vertex->next;
        vertex->next = ctx->free_tess_vertex;
        ctx->free_tess_vertex = vertex;
    }
}

static void free_tass_vertex_bufs(InternaCtx *ctx)
{
    TassVertexBuf *buf;

    ctx->used_tess_vertex = NULL;
    ctx->free_tess_vertex = NULL;
    while ((buf = ctx->vertex_bufs)) {
        ctx->vertex_bufs = buf->next;
        free(buf);
    }
}

//naive bezier flattener
static TassVertex *bezier_flattener(InternaCtx *ctx, PathPoint *points)
{
    double ax, bx, cx;
    double ay, by, cy;
    const int num_points = 30;
    double dt;
    int i;

    TassVertex *vertex_list = NULL;
    TassVertex *curr_vertex;

    for (i = 0; i < num_points - 2; i++) {
        TassVertex *vertex;

        vertex = alloc_tess_vertex(ctx);
        vertex->list_link = vertex_list;
        vertex_list = vertex;
    }

    curr_vertex = vertex_list;

    cx = 3.0 * (points[1].x - points[0].x);
    bx = 3.0 * (points[2].x - points[1].x) - cx;
    ax = points[3].x - points[0].x - cx - bx;

    cy = 3.0 * (points[1].y - points[0].y);
    by = 3.0 * (points[2].y - points[1].y) - cy;
    ay = points[3].y - points[0].y - cy - by;

    dt = 1.0 / (num_points - 1);

    for (i = 1; i < num_points - 1; i++, curr_vertex = curr_vertex->list_link) {
        double tSquared, tCubed;
        double t;
        t = i * dt;

        tSquared = t * t;
        tCubed = tSquared * t;

        curr_vertex->point.x = (ax * tCubed) + (bx * tSquared) + (cx * t) + points[0].x;
        curr_vertex->point.y = (ay * tCubed) + (by * tSquared) + (cy * t) + points[0].y;
    }

    return vertex_list;
}

#define MORE_X(path, Type, name) {                                              \
    Type *name;                                                                 \
                                                                                \
    name = spice_new0(Type, path->name##_size * 2);                             \
    memcpy(name, path->name, sizeof(*name) * path->name##_size);                \
    free(path->name);                                                           \
    path->name = name;                                                          \
    path->name##_size *= 2;                                                     \
}

static void more_points(InternalPath *path)
{
    MORE_X(path, PathPoint, points);
}

static void more_segments(InternalPath *path)
{
    MORE_X(path, PathSegment, segments);
}

static void more_paths(InternalPath *path)
{
    MORE_X(path, Path, paths);
}

static inline void put_point(InternalPath *path, double x, double y)
{
    path->points[path->points_pos].x = x;
    path->points[path->points_pos].y = Y(y + 0.5);
    path->points[path->points_pos++].z = 0;
}

void glc_path_move_to(GLCPath path, double x, double y)
{
    InternalPath *internal = (InternalPath *)path;

    spice_assert(internal);

    if (internal->current_segment) {
        internal->current_segment = NULL;
        internal->current_path = NULL;
        if (internal->points_pos == internal->points_size) {
            more_points(internal);
        }
        internal->points_pos++;
    }
    internal->points[internal->points_pos - 1].x = x;
    internal->points[internal->points_pos - 1].y = Y(y + 0.5);
    internal->points[internal->points_pos - 1].z = 0;
}

static void add_segment_common(InternalPath *internal, int type, int num_points)
{
    if (internal->points_size - internal->points_pos < num_points) {
        more_points(internal);
    }

    if (internal->current_segment) {
        if (internal->current_segment->type == type) {
            internal->current_segment->count++;
            return;
        }
        if (internal->segments_pos == internal->segments_size) {
            more_segments(internal);
        }
        internal->current_segment = &internal->segments[internal->segments_pos++];
        internal->current_segment->type = type;
        internal->current_segment->count = 1;
        internal->current_path->num_segments++;
        return;
    }

    if (internal->paths_pos == internal->paths_size) {
        more_paths(internal);
    }

    if (internal->segments_pos == internal->segments_size) {
        more_segments(internal);
    }

    internal->current_path = &internal->paths[internal->paths_pos++];
    internal->current_path->start_point = internal->points_pos - 1;
    internal->current_path->num_segments = 1;
    internal->current_segment = &internal->segments[internal->segments_pos++];
    internal->current_segment->type = type;
    internal->current_segment->count = 1;
}

void glc_path_line_to(GLCPath path, double x, double y)
{
    InternalPath *internal = (InternalPath *)path;

    spice_assert(internal);

    add_segment_common(internal, GLC_PATH_SEG_LINES, 1);
    put_point(internal, x, y);
}

void glc_path_curve_to(GLCPath path, double p1_x, double p1_y, double p2_x, double p2_y,
                       double p3_x, double p3_y)
{
    InternalPath *internal = (InternalPath *)path;

    spice_assert(internal);

    add_segment_common(internal, GLC_PATH_SEG_BEIZER, 3);
    put_point(internal, p1_x, p1_y);
    put_point(internal, p2_x, p2_y);
    put_point(internal, p3_x, p3_y);
}

void glc_path_close(GLCPath path)
{
    InternalPath *internal = (InternalPath *)path;

    spice_assert(internal);
    if (!internal->current_path) {
        return;
    }
    PathPoint *end_point = &internal->points[internal->current_path->start_point];
    glc_path_line_to(path, end_point->x, Y(end_point->y));
    glc_path_move_to(path, end_point->x, Y(end_point->y));
}

void glc_path_cleare(GLCPath path)
{
    InternalPath *internal = (InternalPath *)path;

    spice_assert(internal);
    internal->paths_pos = internal->segments_pos = 0;
    internal->current_segment = NULL;
    internal->current_path = NULL;

    internal->points[0].x = 0;
    internal->points[0].y = 0;
    internal->points_pos = 1;
}

GLCPath glc_path_create(GLCCtx glc)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    InternalPath *path;

    spice_assert(ctx);
    path = spice_new0(InternalPath, 1);
    path->paths_size = 2;
    path->paths = spice_new(Path, path->paths_size);

    path->segments_size = 4;
    path->segments = spice_new(PathSegment, path->segments_size);

    path->points_size = 20;
    path->points = spice_new(PathPoint, path->points_size);

    path->owner = ctx;
    path->points_pos = 1;
    return path;
}

void glc_path_destroy(GLCPath path)
{
    InternalPath *internal = (InternalPath *)path;

    if (!path) {
        return;
    }

    free(internal->points);
    free(internal->segments);
    free(internal->paths);
    free(internal);
}

static inline void unref_pat(InternalPat *pat)
{
    if (!pat) {
        return;
    }
    spice_assert(pat->refs > 0);
    if (--pat->refs == 0) {
        glFinish();
        glDeleteTextures(1, &pat->texture);
        free(pat);
    }
    GLC_ERROR_TEST_FLUSH;
}

static inline InternalPat *ref_pat(InternalPat *pat)
{
    pat->refs++;
    return pat;
}

static void scale(uint32_t *dest, uint32_t dest_width, uint32_t dest_height,
                  uint32_t *src, uint32_t src_width, uint32_t src_height, int src_stride)
{
    double x_scale = (double)src_width / dest_width;
    double y_scale = (double)src_height / dest_height;
    uint32_t i;
    uint32_t j;
    int prev_row = -1;

    for (i = 0; i < dest_height; i++) {
        int row = (int)(y_scale * i);
        if (row == prev_row) {
            memcpy(dest, dest - dest_width, dest_width * sizeof(uint32_t));
            dest += dest_width;
            continue;
        }
        for (j = 0; j < dest_width; j++) {
            int col = (int)(x_scale * j);
            *(dest++) = *(src + col);
        }
        prev_row = row;
        src = (uint32_t *)((uint8_t *)src + src_stride);
    }
}

static inline void init_pattern(InternalPat *pat, int x_orign, int y_orign, const GLCImage *image)
{
    InternaCtx *ctx = pat->owner;
    uint32_t *tmp_pixmap = NULL;
    int width;
    int height;
    int width2;
    int height2;

    const int pix_bytes = 4;

    spice_assert(image->format == GLC_IMAGE_RGB32); //for now

    width = image->width;
    height = image->height;
    width2 = gl_get_to_power_two(width);
    height2 = gl_get_to_power_two(height);

    spice_assert(width > 0 && height > 0);
    spice_assert(width > 0 && width <= pat->owner->max_texture_size);
    spice_assert(height > 0 && height <= pat->owner->max_texture_size);

    if (width2 != width || height2 != height) {
        tmp_pixmap = (uint32_t *)spice_malloc(width2 * height2 * sizeof(uint32_t));
        scale(tmp_pixmap, width2, height2, (uint32_t *)image->pixels, width, height, image->stride);
    }

    glBindTexture(GL_TEXTURE_2D, pat->texture);

    //glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (tmp_pixmap) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, width2);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, width2, height2, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     tmp_pixmap);
        free(tmp_pixmap);
    } else {
        spice_assert(image->stride % pix_bytes == 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, image->stride / pix_bytes);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     image->pixels);
    }

    GLC_ERROR_TEST_FLUSH;
    pat->x_orign = x_orign % width;
    pat->y_orign = y_orign % height;
    pat->width = width;
    pat->height = height;

    if (ctx->pat == pat) {
        set_pat(pat->owner, pat);
    } else if (ctx->pat) {
        glBindTexture(GL_TEXTURE_2D, ctx->pat->texture);
    }
}

GLCPattern glc_pattern_create(GLCCtx glc, int x_orign, int y_orign, const GLCImage *image)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    InternalPat *pat;

    spice_assert(ctx && image);

    pat = spice_new0(InternalPat, 1);
    pat->refs = 1;
    pat->owner = ctx;
    glGenTextures(1, &pat->texture);
    init_pattern(pat, x_orign, y_orign, image);
    return pat;
}

void glc_pattern_set(GLCPattern pattern, int x_orign, int y_orign, const GLCImage *image)
{
    InternalPat *pat = (InternalPat *)pattern;
    spice_assert(pat && pat->owner);

    glFinish();
    init_pattern(pat, x_orign, y_orign, image);
}

void glc_pattern_destroy(GLCPattern pat)
{
    unref_pat((InternalPat *)pat);
    GLC_ERROR_TEST_FLUSH;
}

static void set_pat(InternaCtx *ctx, InternalPat *pat)
{
    pat = ref_pat(pat);
    unref_pat(ctx->pat);
    ctx->pat = pat;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pat->texture);

    GLfloat s_gen_params[] = { (GLfloat)1.0 / pat->width, 0, 0, 0 };
    GLfloat t_gen_params[] = { 0, (GLfloat)1.0 / (GLfloat)pat->height, 0, 0 };
    glTexGenfv(GL_S, GL_OBJECT_PLANE, s_gen_params);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, t_gen_params);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef((float)pat->x_orign / pat->width, (float)Y(pat->y_orign) / pat->height, 0);
    GLC_ERROR_TEST_FLUSH;
}

void glc_set_pattern(GLCCtx glc, GLCPattern pattern)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    InternalPat *pat = (InternalPat *)pattern;

    spice_assert(ctx && pat && pat->owner == ctx);
    set_pat(ctx, pat);
}

void glc_set_rgb(GLCCtx glc, double red, double green, double blue)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);

    glDisable(GL_TEXTURE_2D);
    unref_pat(ctx->pat);
    ctx->pat = NULL;
    glColor4d(red, green, blue, 1);
    GLC_ERROR_TEST_FLUSH;
}

void glc_set_op(GLCCtx glc, GLCOp op)
{
    if (op == GL_COPY) {
        glDisable(GL_COLOR_LOGIC_OP);
        return;
    }
    glLogicOp(op);
    glEnable(GL_COLOR_LOGIC_OP);
}

void glc_set_line_width(GLCCtx glc, double width)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);
    ctx->line_width = (GLfloat)width;
    if (ctx->line_width > 0) {
        glLineWidth(ctx->line_width);
    } else {
        ctx->line_width = 0;
    }
    GLC_ERROR_TEST_FLUSH;
}

void glc_set_line_dash(GLCCtx glc, const double *dashes, int num_dashes, double offset)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);
    if (dashes && num_dashes >= 0 && offset >= 0) {
        ctx->line_dash.dashes = spice_new(double, num_dashes);
        memcpy(ctx->line_dash.dashes, dashes, sizeof(double) * num_dashes);
        ctx->line_dash.num_dashes = num_dashes;
        ctx->line_dash.offset = offset;
        ctx->line_dash.cur_dash = offset ? -1 : 0;
        ctx->line_dash.dash_pos = 0;
    } else {
        free(ctx->line_dash.dashes);
        memset(&ctx->line_dash, 0, sizeof(ctx->line_dash));
    }
}

void glc_set_fill_mode(GLCCtx glc, GLCFillMode fill_mode)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);
    int mode;
    switch (fill_mode) {
    case GLC_FILL_MODE_WINDING_ODD:
        mode = GLU_TESS_WINDING_ODD;
        break;
    case GLC_FILL_MODE_WINDING_NONZERO:
        mode = GLU_TESS_WINDING_NONZERO;
        break;
    default:
        //warn
        return;
    }
    gluTessProperty(ctx->tesselator, GLU_TESS_WINDING_RULE, mode);
}

static inline void add_stencil_client(InternaCtx *ctx)
{
    if (!ctx->stencil_refs) {
        glEnable(GL_STENCIL_TEST);
    }
    ctx->stencil_refs++;
}

static inline void remove_stencil_client(InternaCtx *ctx)
{
    ctx->stencil_refs--;
    if (!ctx->stencil_refs) {
        glDisable(GL_STENCIL_TEST);
    }
}

void glc_set_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height,
                  int stride, const uint8_t *bitmap, GLCMaskID id)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    uint32_t mask = (id == GLC_MASK_A) ? 0x04 : 0x08;
    spice_assert(ctx && bitmap);
    spice_assert(id == GLC_MASK_A || id == GLC_MASK_B);

    if (ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_BLEND);

    if (!(ctx->stencil_mask & mask)) {
        add_stencil_client(ctx);
        ctx->stencil_mask |= mask;
    }

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    ctx->draw_mode = FALSE;
    glStencilMask(mask);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilFunc(GL_ALWAYS, mask, mask);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    fill_mask(ctx, x_dest, y_dest, width, height, stride, bitmap);
}

void glc_mask_rects(GLCCtx glc, int num_rect, GLCRect *rects, GLCMaskID id)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    uint32_t mask = (id == GLC_MASK_A) ? 0x04 : 0x08;
    GLCRect *end;
    spice_assert(ctx && rects);
    spice_assert(id == GLC_MASK_A || id == GLC_MASK_B);

    if (ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_BLEND);

    if (!(ctx->stencil_mask & mask)) {
        add_stencil_client(ctx);
        ctx->stencil_mask |= mask;
    }

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    ctx->draw_mode = FALSE;
    glStencilMask(mask);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilFunc(GL_ALWAYS, mask, mask);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    end = rects + num_rect;
    for (; rects < end; rects++) {
        fill_rect(ctx, rects);
    }
}

void glc_clear_mask(GLCCtx glc, GLCMaskID id)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    uint32_t mask = (id == GLC_MASK_A) ? 0x04 : 0x08;
    spice_assert(ctx);
    spice_assert(id == GLC_MASK_A || id == GLC_MASK_B);

    if ((ctx->stencil_mask & mask)) {
        ctx->stencil_mask &= ~mask;
        remove_stencil_client(ctx);
    }
}

void glc_clip_reset(GLCCtx glc)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    if (!(ctx->stencil_mask & 0x03)) {
        return;
    }
    remove_stencil_client(ctx);
    ctx->stencil_mask &= ~0x03;
    glStencilMask(0x03);
    glClear(GL_STENCIL_BUFFER_BIT);
    GLC_ERROR_TEST_FLUSH;
}

static void clip_common(InternaCtx *ctx, GLCClipOp op, void (*fill_func)(InternaCtx *, void *),
                        void *data)
{
    int stencil_val;

    if (ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    }
    glDisable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    ctx->draw_mode = FALSE;

    if (op == GLC_CLIP_OP_SET) {
        glc_clip_reset(ctx);
        add_stencil_client(ctx);
        ctx->stencil_mask |= 0x01;
    } else if (!(ctx->stencil_mask & 0x03)) {
        GLCRect area;
        if (op == GLC_CLIP_OP_OR) {
            return;
        }
        area.x = area.y = 0;
        area.width = ctx->width;
        area.height = ctx->height;
        clip_common(ctx, GLC_CLIP_OP_SET, fill_rect, &area);
    }
    glStencilMask(0x03);
    switch (op) {
    case GLC_CLIP_OP_SET:
    case GLC_CLIP_OP_OR:
        stencil_val = ctx->stencil_mask & 0x03;
        glStencilFunc(GL_ALWAYS, stencil_val, stencil_val);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        fill_func(ctx, data);
        break;
    case GLC_CLIP_OP_AND: {
        int clear_mask;
        stencil_val = ctx->stencil_mask & 0x03;
        glStencilFunc(GL_EQUAL, stencil_val, stencil_val);
        if (stencil_val == 0x01) {
            glStencilOp(GL_ZERO, GL_INCR, GL_INCR);
            stencil_val = 0x02;
            clear_mask = 0x01;
        } else {
            glStencilOp(GL_ZERO, GL_DECR, GL_DECR);
            stencil_val = 0x01;
            clear_mask = 0x02;
        }
        fill_func(ctx, data);

        glStencilMask(clear_mask);
        glClear(GL_STENCIL_BUFFER_BIT);
        ctx->stencil_mask = (ctx->stencil_mask & ~clear_mask) | stencil_val;
        break;
    }
    case GLC_CLIP_OP_EXCLUDE:
        stencil_val = ctx->stencil_mask & 0x03;
        glStencilFunc(GL_EQUAL, stencil_val, stencil_val);
        glStencilOp(GL_KEEP, GL_ZERO, GL_ZERO);
        fill_func(ctx, data);
        break;
    }
    GLC_ERROR_TEST_FLUSH;
}

void glc_clip_rect(GLCCtx glc, const GLCRect *rect, GLCClipOp op)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx && rect);
    clip_common(ctx, op, fill_rect, (void *)rect);
}

void glc_clip_path(GLCCtx glc, GLCPath path, GLCClipOp op)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx && path);
    clip_common(ctx, op, fill_path, path);
}

typedef struct FillMaskInfo {
    int x_dest;
    int y_dest;
    int width;
    int height;
    int stride;
    const uint8_t *bitmap;
} FillMaskInfo;

static void __fill_mask(InternaCtx *ctx, void *data)
{
    FillMaskInfo *info = (FillMaskInfo *)data;
    fill_mask(ctx, info->x_dest, info->y_dest, info->width, info->height, info->stride,
              info->bitmap);
}

void glc_clip_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height,
                   int stride, const uint8_t *bitmap, GLCClipOp op)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    FillMaskInfo mask_info;

    spice_assert(ctx && bitmap);
    mask_info.x_dest = x_dest;
    mask_info.y_dest = y_dest;
    mask_info.width = width;
    mask_info.height = height;
    mask_info.stride = stride;
    mask_info.bitmap = bitmap;
    clip_common(ctx, op, __fill_mask, &mask_info);
}

static inline void start_draw(InternaCtx *ctx)
{
    if (ctx->draw_mode) {
        return;
    }
    ctx->draw_mode = TRUE;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, ctx->stencil_mask, ctx->stencil_mask);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    if (ctx->pat) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    GLC_ERROR_TEST_FLUSH;
}

static void fill_rect(InternaCtx *ctx, void *r)
{
    GLCRect *rect = (GLCRect *)r;
    glRectd(rect->x, Y(rect->y), rect->x + rect->width, Y(rect->y + rect->height));
    /*glBegin(GL_POLYGON);
        VERTEX2(rect->x, rect->y);
        VERTEX2 (rect->x + rect->width, rect->y);
        VERTEX2 (rect->x + rect->width, rect->y + rect->height);
        VERTEX2 (rect->x , rect->y + rect->height);
    glEnd();*/
    GLC_ERROR_TEST_FLUSH;
}

void glc_fill_rect(GLCCtx glc, const GLCRect *rect)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    GLCRect *r = (GLCRect *)rect; // to avoid bugs in gcc older than 4.3

    spice_assert(ctx);
    start_draw(ctx);
    fill_rect(ctx, (void *)r);
    GLC_ERROR_TEST_FLUSH;
}

static void fill_path(InternaCtx *ctx, void *p)
{
    InternalPath *path = (InternalPath *)p;

    PathPoint *current_point = path->points;
    PathSegment *current_segment = path->segments;
    Path *current_path = path->paths;
    Path *end_path = current_path + path->paths_pos;
    reset_tass_vertex(ctx);
    gluTessBeginPolygon(ctx->tesselator, ctx);
    for (; current_path < end_path; current_path++) {
        gluTessBeginContour(ctx->tesselator);
        PathSegment *end_segment = current_segment + current_path->num_segments;
        gluTessVertex(ctx->tesselator, (GLdouble *)current_point, current_point);
        current_point++;
        for (; current_segment < end_segment; current_segment++) {
            PathPoint *end_point;
            if (current_segment->type == GLC_PATH_SEG_BEIZER) {
                end_point = current_point + current_segment->count * 3;
                for (; current_point < end_point; current_point += 3) {
                    TassVertex *vertex = bezier_flattener(ctx, current_point - 1);
                    while (vertex) {
                        gluTessVertex(ctx->tesselator, (GLdouble *)&vertex->point,
                                      (GLdouble *)&vertex->point);
                        vertex = vertex->list_link;
                    }
                    gluTessVertex(ctx->tesselator, (GLdouble *)&current_point[2],
                                  (GLdouble *)&current_point[2]);
                }
            } else {
                spice_assert(current_segment->type == GLC_PATH_SEG_LINES);
                end_point = current_point + current_segment->count;
                for (; current_point < end_point; current_point++) {
                    gluTessVertex(ctx->tesselator, (GLdouble *)current_point,
                                  (GLdouble *)current_point);
                }
            }
        }
        gluTessEndContour(ctx->tesselator);
    }
    gluTessEndPolygon(ctx->tesselator);
}

void glc_fill_path(GLCCtx glc, GLCPath path_ref)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx && path_ref);
    start_draw(ctx);
    fill_path(ctx, path_ref);
}

static void fill_mask(InternaCtx *ctx, int x_dest, int y_dest, int width, int height,
                      int stride, const uint8_t *bitmap)
{
    set_raster_pos(ctx, x_dest, y_dest + height);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride * 8);
    glBitmap(width, height, 0, 0, 0, 0, bitmap);
}

void _glc_fill_mask(GLCCtx glc, int x_dest, int y_dest, int width, int height, int stride,
                    const uint8_t *bitmap)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx && bitmap);
    start_draw(ctx);
    if (ctx->pat) {
        spice_critical("unimplemented fill mask with pattern");
    }
    fill_mask(ctx, x_dest, y_dest, width, height, stride, bitmap);
}

void glc_fill_alpha(GLCCtx glc, int x_dest, int y_dest, int width, int height, int stride,
                    const uint8_t *alpha_mask)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    GLCRect r;

    spice_assert(ctx);
    start_draw(ctx);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    set_raster_pos(ctx, x_dest, y_dest + height);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
    glPixelZoom(1, 1);
    glDrawPixels(width, height, GL_ALPHA, GL_UNSIGNED_BYTE, alpha_mask);

    r.x = x_dest;
    r.y = y_dest;
    r.width = width;
    r.height = height;

    //todo: support color/texture alpah vals (GL_MODULATE)
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    fill_rect(ctx, &r);
    glDisable(GL_BLEND);
}

void glc_stroke_rect(GLCCtx glc, const GLCRect *rect)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);
    if (ctx->line_width == 0) {
        return;
    }

    start_draw(ctx);

    glBegin(GL_LINES);
    VERTEX2(rect->x, rect->y + 0.5);
    VERTEX2(rect->x + rect->width, rect->y + 0.5);
    VERTEX2(rect->x + rect->width - 0.5, rect->y);
    VERTEX2(rect->x + rect->width - 0.5, rect->y + rect->height);
    VERTEX2(rect->x + rect->width, rect->y + rect->height - 0.5);
    VERTEX2(rect->x, rect->y + rect->height - 0.5);
    VERTEX2(rect->x + 0.5, rect->y + rect->height);
    VERTEX2(rect->x + 0.5, rect->y);
    glEnd();
    GLC_ERROR_TEST_FLUSH;
}

static void glc_stroke_line(double x1, double y1, double x2, double y2, double width)
{
    double ax, ay, bx, by, cx, cy, dx, dy;
    double norm, tx;

    if (width == 1 || y1 == y2 || x1 == x2) {
        glBegin(GL_LINES);
        glVertex2d(x1, y1);
        glVertex2d(x2, y2);
        glEnd();
        return;
    }
    norm = (x1 - x2) / (y2 - y1);
    tx = width / (2 * sqrt(1 + norm * norm));
    ax = x1 + tx;
    ay = y1 + norm * (ax - x1);
    bx = x2 + tx;
    by = y2 + norm * (bx - x2);
    cx = x2 - tx;
    cy = y2 + norm * (cx - x2);
    dx = x1 - tx;
    dy = y1 + norm * (dx - x1);
    glBegin(GL_POLYGON);
    glVertex2d(ax, ay);
    glVertex2d(bx, by);
    glVertex2d(cx, cy);
    glVertex2d(dx, dy);
    glEnd();
}

static double glc_stroke_line_dash(double x1, double y1, double x2, double y2,
                                   double width, LineDash *dash)
{
    double ax, ay, bx, by;
    double mx, my, len;
    double dash_len, total = 0;

    len = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
    if (!dash->dashes || !dash->num_dashes) {
        glc_stroke_line(x1, y1, x2, y2, width);
        return len;
    }
    mx = (x2 - x1) / len;
    my = (y2 - y1) / len;
    ax = x1;
    ay = y1;
    while (total < len) {
        if (dash->cur_dash >= 0) {
            dash_len = dash->dashes[dash->cur_dash % dash->num_dashes] - dash->dash_pos;
        } else {
            dash_len = dash->offset - dash->dash_pos;
        }
        total += dash_len;
        if (total < len) {
            bx = x1 + mx * total;
            by = y1 + my * total;
            dash->dash_pos = 0;
        } else {
            bx = x2;
            by = y2;
            dash->dash_pos = dash->dashes[dash->cur_dash % dash->num_dashes] - (total - len);
        }
        if (dash->cur_dash % 2 == 0) {
            glc_stroke_line(ax, ay, bx, by, width);
        }
        if (dash->dash_pos == 0) {
            dash->cur_dash = (dash->cur_dash + 1) % (2 * dash->num_dashes);
        }
        ax = bx;
        ay = by;
    }
    return len;
}

static void glc_vertex2d(InternaCtx *ctx, double x, double y)
{
    if (ctx->path_stroke.state == GLC_STROKE_ACTIVE) {
        glc_stroke_line_dash(ctx->path_stroke.x, ctx->path_stroke.y, x, y,
                             ctx->line_width, &ctx->line_dash);
        ctx->path_stroke.x = x;
        ctx->path_stroke.y = y;
    } else if (ctx->path_stroke.state == GLC_STROKE_FIRST) {
        ctx->path_stroke.x = x;
        ctx->path_stroke.y = y;
        ctx->path_stroke.state = GLC_STROKE_ACTIVE;
    } else {
        spice_assert(ctx->path_stroke.state == GLC_STROKE_NONACTIVE);
        //error
    }
}

static void glc_begin_path(InternaCtx *ctx)
{
    ctx->path_stroke.state = GLC_STROKE_FIRST;
    ctx->line_dash.cur_dash = ctx->line_dash.offset ? -1 : 0;
    ctx->line_dash.dash_pos = 0;
}

static void glc_end_path(InternaCtx *ctx)
{
    ctx->path_stroke.state = GLC_STROKE_NONACTIVE;
}

void glc_stroke_path(GLCCtx glc, GLCPath path_ref)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    InternalPath *path = (InternalPath *)path_ref;

    spice_assert(ctx && path);
    if (ctx->line_width == 0) {
        return;
    }
    start_draw(ctx);

    reset_tass_vertex(ctx);
    PathPoint *current_point = path->points;
    PathSegment *current_segment = path->segments;
    Path *current_path = path->paths;
    Path *end_path = current_path + path->paths_pos;
    for (; current_path < end_path; current_path++) {
        glc_begin_path(ctx);
        PathSegment *end_segment = current_segment + current_path->num_segments;
        glc_vertex2d(ctx, current_point->x, current_point->y);
        current_point++;
        for (; current_segment < end_segment; current_segment++) {
            PathPoint *end_point;
            if (current_segment->type == GLC_PATH_SEG_BEIZER) {
                end_point = current_point + current_segment->count * 3;
                for (; current_point < end_point; current_point += 3) {
                    TassVertex *vertex = bezier_flattener(ctx, current_point - 1);
                    while (vertex) {
                        glc_vertex2d(ctx, vertex->point.x, vertex->point.y);
                        vertex = vertex->list_link;
                    }
                    glc_vertex2d(ctx, current_point[2].x, current_point[2].y);
                }
            } else {
                spice_assert(current_segment->type == GLC_PATH_SEG_LINES);
                end_point = current_point + current_segment->count;
                for (; current_point < end_point; current_point++) {
                    glc_vertex2d(ctx, current_point->x, current_point->y);
                }
            }
        }
        glc_end_path(ctx);
    }
}

void glc_draw_image(GLCCtx glc, const GLCRecti *dest, const GLCRecti *src, const GLCImage *image,
                    int scale_mode, double alpha)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    uint8_t *pixels;
    const int pix_bytes = 4;

    spice_assert(ctx && image);
    spice_assert(src->width > 0 && src->height > 0);

    spice_assert(image->format == GLC_IMAGE_RGB32 || image->format == GLC_IMAGE_ARGB32); //for now
    start_draw(ctx);
    if (ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    }
    set_raster_pos(ctx, dest->x, dest->y + dest->height);

    if (dest->width == src->width && src->height == dest->height) {
        glPixelZoom(1, 1);
    } else {
        glPixelZoom((float)dest->width / src->width, (float)dest->height / src->height);
    }

    pixels = image->pixels + src->x * 4 + (image->height - (src->y + src->height)) * image->stride;
    if (image->format == GLC_IMAGE_ARGB32 || alpha != 1) {
        glPixelTransferf(GL_ALPHA_SCALE, (GLfloat)alpha);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
    }
    spice_assert(image->stride % pix_bytes == 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, image->stride / pix_bytes);
    glDrawPixels(src->width, src->height, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

    if (image->format == GLC_IMAGE_ARGB32 || alpha != 1) {
        glDisable(GL_BLEND);
    }

    if (ctx->pat) {
        glEnable(GL_TEXTURE_2D);
    }
    GLC_ERROR_TEST_FLUSH;
}

void glc_copy_pixels(GLCCtx glc, int x_dest, int y_dest, int x_src, int y_src, int width,
                     int height)
{
    InternaCtx *ctx = (InternaCtx *)glc;
    int recreate = 0;

    spice_assert(ctx);
#ifdef USE_COPY_PIXELS
    start_draw(ctx);
    if (ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    }
    set_raster_pos(ctx, x_dest, y_dest + height);
    glPixelZoom(1, 1);
    glCopyPixels(x_src, ctx->height - (y_src + height), width, height, GL_COLOR);
    if (ctx->pat) {
        glEnable(GL_TEXTURE_2D);
    }
#else
    int width2 = gl_get_to_power_two(width);
    int height2 = gl_get_to_power_two(height);

    start_draw(ctx);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (width2 > ctx->private_tex_width) {
        ctx->private_tex_width = width2;
        recreate = 1;
    }
    if (height2 > ctx->private_tex_height) {
        ctx->private_tex_height = height2;
        recreate = 1;
    }
    if (recreate) {
        glDeleteTextures(1, &ctx->private_tex);
        glGenTextures(1, &ctx->private_tex);
        glBindTexture(GL_TEXTURE_2D, ctx->private_tex);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        ctx->private_tex_width = gl_get_to_power_two(width);
        ctx->private_tex_height = gl_get_to_power_two(height);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, ctx->private_tex_width,
                     ctx->private_tex_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }
    spice_assert(ctx->private_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->private_tex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x_src, ctx->height - (y_src + height),
                     width2, height2, 0);

    GLfloat s_gen_params[] = { (GLfloat)1.0 / width2, 0, 0, 0 };
    GLfloat t_gen_params[] = { 0, (GLfloat)1.0 / height2, 0, 0 };
    glTexGenfv(GL_S, GL_OBJECT_PLANE, s_gen_params);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, t_gen_params);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef((float)-x_dest / width2, (float)-Y(y_dest + height) / height2, 0);

    glRecti(x_dest, Y(y_dest), x_dest + width, Y(y_dest + height));
    glFlush();
    if (!ctx->pat) {
        glDisable(GL_TEXTURE_2D);
    } else {
        set_pat(ctx, ctx->pat);
    }
#endif
    GLC_ERROR_TEST_FLUSH;
}

void glc_read_pixels(GLCCtx glc, int x, int y, GLCImage *image)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx && image);
    spice_assert(image->format == GLC_IMAGE_RGB32); //for now
    spice_assert((image->stride % 4) == 0); //for now
    glPixelStorei(GL_PACK_ROW_LENGTH, image->stride / 4);
    glReadPixels(x, ctx->height - (y + image->height), image->width, image->height,
                 GL_BGRA, GL_UNSIGNED_BYTE, image->pixels);
}

void glc_clear(GLCCtx glc)
{
    InternaCtx *ctx = (InternaCtx *)glc;

    spice_assert(ctx);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT);
}

void glc_flush(GLCCtx glc)
{
    glFlush();

    GLC_ERROR_TEST_FLUSH;
}

static void tessellation_combine(GLdouble coords[3], GLdouble *vertex_data[4], GLfloat weight[4],
                                 GLdouble **data_out, void *usr_data)
{
    TassVertex *vertex;

    vertex = alloc_tess_vertex((InternaCtx *)usr_data);
    vertex->point.x = coords[0];
    vertex->point.y = coords[1];
    //vertex->point.z = coords[2];
    *data_out = (GLdouble *)&vertex->point;
}

static void tessellation_error(GLenum errorCode)
{
    printf("%s: %s\n", __FUNCTION__, gluErrorString(errorCode));
}

#ifdef WIN32
#define TESS_CALL_BACK_TYPE void(CALLBACK *)()
#else
#define TESS_CALL_BACK_TYPE void(*)()
#endif

static int init(InternaCtx *ctx, int width, int height)
{
#ifdef WIN32
    if (!(ctx->glBlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation"))) {
        return FALSE;
    }
#endif
    ctx->width = width;
    ctx->height = height;
    ctx->line_width = 1;

    glClearColor(0, 0, 0, 0);
    glClearStencil(0);

    if (!(ctx->tesselator = gluNewTess())) {
        return FALSE;
    }

    glGenTextures(1, &ctx->private_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->private_tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, gl_get_to_power_two(width),
                 gl_get_to_power_two(height), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    ctx->private_tex_width = gl_get_to_power_two(width);
    ctx->private_tex_height = gl_get_to_power_two(height);
    glBindTexture(GL_TEXTURE_2D, 0);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);

    gluTessProperty(ctx->tesselator, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    gluTessCallback(ctx->tesselator, GLU_BEGIN, (TESS_CALL_BACK_TYPE)glBegin);
    gluTessCallback(ctx->tesselator, GLU_VERTEX, (TESS_CALL_BACK_TYPE)glVertex3dv);
    gluTessCallback(ctx->tesselator, GLU_END, (TESS_CALL_BACK_TYPE)glEnd);
    gluTessCallback(ctx->tesselator, GLU_TESS_COMBINE_DATA,
                    (TESS_CALL_BACK_TYPE)tessellation_combine);
    gluTessCallback(ctx->tesselator, GLU_TESS_ERROR, (TESS_CALL_BACK_TYPE)tessellation_error);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, (GLfloat)height, 0);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ctx->max_texture_size);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
    glPixelTransferf(GL_ALPHA_BIAS, 0);
#ifdef WIN32
    ctx->glBlendEquation(GL_FUNC_ADD);
#else
    glBlendEquation(GL_FUNC_ADD);
#endif

    glStencilMask(0xff);
    glClear(GL_STENCIL_BUFFER_BIT);

    glClear(GL_COLOR_BUFFER_BIT);

    return TRUE;
}

GLCCtx glc_create(int width, int height)
{
    InternaCtx *ctx;

    spice_static_assert(sizeof(PathPoint) == sizeof(Vertex));

    ctx = spice_new0(InternaCtx, 1);
    if (!init(ctx, width, height)) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

/*
 * In glx video mode change the textures will be destroyed, therefore
 * if we will try to glDeleteTextures() them we might get seagfault.
 * (this why we use the textures_lost parameter)
 */
void glc_destroy(GLCCtx glc, int textures_lost)
{
    InternaCtx *ctx;

    if (!(ctx = (InternaCtx *)glc)) {
        return;
    }

    if (!textures_lost) {
        unref_pat(ctx->pat);
        ctx->pat = NULL;
        if (ctx->private_tex) {
            glDeleteTextures(1, &ctx->private_tex);
        }
    }

    free_tass_vertex_bufs(ctx);
    free(ctx->line_dash.dashes);
    free(ctx);
    GLC_ERROR_TEST_FINISH;
}

/*
    todo:
        1. test double vs float in gl calls
        2. int vs flat raster position
        3. pixels stride vs bytes stride
        4. improve non power of two.
                glGetString(GL_EXTENSIONS);
                ARB_texture_non_power_of_two
                ARB_texture_rectangle
                GL_TEXTURE_RECTANGLE_ARB
        5. scale
        6. origin
        7. fonts
        8. support more image formats
        9. use GLCImage in mask ops?
*/
