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
#ifdef HAVE_CONFIG_H
#ifdef __MINGW32__
#undef HAVE_STDLIB_H
#endif
#include <config.h>
#endif

#include <math.h>
#include "sw_canvas.h"
#define CANVAS_USE_PIXMAN
#define CANVAS_SINGLE_INSTANCE
#include "canvas_base.c"
#include "rect.h"
#include "region.h"
#include "pixman_utils.h"

typedef struct SwCanvas SwCanvas;

struct SwCanvas {
    CanvasBase base;
    uint32_t *private_data;
    int private_data_size;
    pixman_image_t *image;
};

static pixman_image_t *canvas_get_pixman_brush(SwCanvas *canvas,
                                               SpiceBrush *brush)
{
    switch (brush->type) {
    case SPICE_BRUSH_TYPE_SOLID: {
        uint32_t color = brush->u.color;
        pixman_color_t c;

        c.blue = ((color & canvas->base.color_mask) * 0xffff) / canvas->base.color_mask;
        color >>= canvas->base.color_shift;
        c.green = ((color & canvas->base.color_mask) * 0xffff) / canvas->base.color_mask;
        color >>= canvas->base.color_shift;
        c.red = ((color & canvas->base.color_mask) * 0xffff) / canvas->base.color_mask;
        c.alpha = 0xffff;

        return pixman_image_create_solid_fill(&c);
    }
    case SPICE_BRUSH_TYPE_PATTERN: {
        SwCanvas *surface_canvas;
        pixman_image_t* surface;
        pixman_transform_t t;

        surface_canvas = (SwCanvas *)canvas_get_surface(&canvas->base, brush->u.pattern.pat);
        if (surface_canvas) {
            surface = surface_canvas->image;
            surface = pixman_image_ref(surface);
        } else {
            surface = canvas_get_image(&canvas->base, brush->u.pattern.pat, FALSE);
        }
        pixman_transform_init_translate(&t,
                                        pixman_int_to_fixed(-brush->u.pattern.pos.x),
                                        pixman_int_to_fixed(-brush->u.pattern.pos.y));
        pixman_image_set_transform(surface, &t);
        pixman_image_set_repeat(surface, PIXMAN_REPEAT_NORMAL);
        return surface;
    }
    case SPICE_BRUSH_TYPE_NONE:
        return NULL;
    default:
        spice_warn_if_reached();
        return NULL;
    }
    return NULL;
}

static pixman_image_t *get_image(SpiceCanvas *canvas, int force_opaque)
{
    SwCanvas *sw_canvas = (SwCanvas *)canvas;
    pixman_format_code_t format;

    spice_pixman_image_get_format (sw_canvas->image, &format);
    if (force_opaque && PIXMAN_FORMAT_A (format) != 0) {
        uint32_t *data;
        int stride;
        int width, height;
        
        /* Remove alpha bits from format */
        format = (pixman_format_code_t)(((uint32_t)format) & ~(0xf << 12));
        data = pixman_image_get_data (sw_canvas->image);
        stride = pixman_image_get_stride (sw_canvas->image);
        width = pixman_image_get_width (sw_canvas->image);
        height = pixman_image_get_height (sw_canvas->image);
        return pixman_image_create_bits (format, width, height, data, stride);
    } else {
        pixman_image_ref(sw_canvas->image);
    }

    return sw_canvas->image;
}

static void copy_region(SpiceCanvas *spice_canvas,
                        pixman_region32_t *dest_region,
                        int dx, int dy)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_box32_t *dest_rects;
    int n_rects;
    int i, j, end_line;

    dest_rects = pixman_region32_rectangles(dest_region, &n_rects);

    if (dy > 0) {
        if (dx >= 0) {
            /* south-east: copy x and y in reverse order */
            for (i = n_rects - 1; i >= 0; i--) {
                spice_pixman_copy_rect(canvas->image,
                                       dest_rects[i].x1 - dx, dest_rects[i].y1 - dy,
                                       dest_rects[i].x2 - dest_rects[i].x1,
                                       dest_rects[i].y2 - dest_rects[i].y1,
                                       dest_rects[i].x1, dest_rects[i].y1);
            }
        } else {
            /* south-west: Copy y in reverse order, but x in forward order */
            i = n_rects - 1;

            while (i >= 0) {
                /* Copy all rects with same y in forward order */
                for (end_line = i - 1;
                     end_line >= 0 && dest_rects[end_line].y1 == dest_rects[i].y1;
                     end_line--) {
                }
                for (j = end_line + 1; j <= i; j++) {
                    spice_pixman_copy_rect(canvas->image,
                                           dest_rects[j].x1 - dx, dest_rects[j].y1 - dy,
                                           dest_rects[j].x2 - dest_rects[j].x1,
                                           dest_rects[j].y2 - dest_rects[j].y1,
                                           dest_rects[j].x1, dest_rects[j].y1);
                }
                i = end_line;
            }
        }
    } else {
        if (dx > 0) {
            /* north-east: copy y in forward order, but x in reverse order */
            i = 0;

            while (i < n_rects) {
                /* Copy all rects with same y in reverse order */
                for (end_line = i;
                     end_line < n_rects && dest_rects[end_line].y1 == dest_rects[i].y1;
                     end_line++) {
                }
                for (j = end_line - 1; j >= i; j--) {
                    spice_pixman_copy_rect(canvas->image,
                                           dest_rects[j].x1 - dx, dest_rects[j].y1 - dy,
                                           dest_rects[j].x2 - dest_rects[j].x1,
                                           dest_rects[j].y2 - dest_rects[j].y1,
                                           dest_rects[j].x1, dest_rects[j].y1);
                }
                i = end_line;
            }
        } else {
            /* north-west: Copy x and y in forward order */
            for (i = 0; i < n_rects; i++) {
                spice_pixman_copy_rect(canvas->image,
                                       dest_rects[i].x1 - dx, dest_rects[i].y1 - dy,
                                       dest_rects[i].x2 - dest_rects[i].x1,
                                       dest_rects[i].y2 - dest_rects[i].y1,
                                       dest_rects[i].x1, dest_rects[i].y1);
            }
        }
    }
}

static void fill_solid_spans(SpiceCanvas *spice_canvas,
                             SpicePoint *points,
                             int *widths,
                             int n_spans,
                             uint32_t color)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    int i;

   for (i = 0; i < n_spans; i++) {
        spice_pixman_fill_rect(canvas->image,
                               points[i].x, points[i].y,
                               widths[i],
                               1,
                               color);
    }
}

static void fill_solid_rects(SpiceCanvas *spice_canvas,
                             pixman_box32_t *rects,
                             int n_rects,
                             uint32_t color)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    int i;

   for (i = 0; i < n_rects; i++) {
        spice_pixman_fill_rect(canvas->image,
                               rects[i].x1, rects[i].y1,
                               rects[i].x2 - rects[i].x1,
                               rects[i].y2 - rects[i].y1,
                               color);
    }
}

static void fill_solid_rects_rop(SpiceCanvas *spice_canvas,
                                 pixman_box32_t *rects,
                                 int n_rects,
                                 uint32_t color,
                                 SpiceROP rop)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    int i;

   for (i = 0; i < n_rects; i++) {
        spice_pixman_fill_rect_rop(canvas->image,
                                   rects[i].x1, rects[i].y1,
                                   rects[i].x2 - rects[i].x1,
                                   rects[i].y2 - rects[i].y1,
                                   color, rop);
    }
}

static void __fill_tiled_rects(SpiceCanvas *spice_canvas,
                               pixman_box32_t *rects,
                               int n_rects,
                               pixman_image_t *tile,
                               int offset_x, int offset_y)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    int i;

    for (i = 0; i < n_rects; i++) {
        spice_pixman_tile_rect(canvas->image,
                               rects[i].x1, rects[i].y1,
                               rects[i].x2 - rects[i].x1,
                               rects[i].y2 - rects[i].y1,
                               tile, offset_x, offset_y);
    }
}

static void fill_tiled_rects(SpiceCanvas *spice_canvas,
                               pixman_box32_t *rects,
                               int n_rects,
                               pixman_image_t *tile,
                               int offset_x, int offset_y)
{
    __fill_tiled_rects(spice_canvas, rects, n_rects, tile, offset_x, offset_y);
}

static void fill_tiled_rects_from_surface(SpiceCanvas *spice_canvas,
                                          pixman_box32_t *rects,
                                          int n_rects,
                                          SpiceCanvas *surface_canvas,
                                          int offset_x, int offset_y)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __fill_tiled_rects(spice_canvas, rects, n_rects, sw_surface_canvas->image, offset_x,
                       offset_y);
}

static void __fill_tiled_rects_rop(SpiceCanvas *spice_canvas,
                                   pixman_box32_t *rects,
                                   int n_rects,
                                   pixman_image_t *tile,
                                   int offset_x, int offset_y,
                                   SpiceROP rop)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    int i;

    for (i = 0; i < n_rects; i++) {
        spice_pixman_tile_rect_rop(canvas->image,
                                   rects[i].x1, rects[i].y1,
                                   rects[i].x2 - rects[i].x1,
                                   rects[i].y2 - rects[i].y1,
                                   tile, offset_x, offset_y,
                                   rop);
    }
}
static void fill_tiled_rects_rop(SpiceCanvas *spice_canvas,
                                 pixman_box32_t *rects,
                                 int n_rects,
                                 pixman_image_t *tile,
                                 int offset_x, int offset_y,
                                 SpiceROP rop)
{
    __fill_tiled_rects_rop(spice_canvas, rects, n_rects, tile, offset_x, offset_y, rop);
}

static void fill_tiled_rects_rop_from_surface(SpiceCanvas *spice_canvas,
                                              pixman_box32_t *rects,
                                              int n_rects,
                                              SpiceCanvas *surface_canvas,
                                              int offset_x, int offset_y,
                                              SpiceROP rop)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __fill_tiled_rects_rop(spice_canvas, rects, n_rects, sw_surface_canvas->image, offset_x,
                           offset_y, rop);
}

/* Some pixman implementations of OP_OVER on xRGB32 sets
   the high bit to 0xff (which is the right value if the
   destination was ARGB32, and it should be ignored for
   xRGB32. However, this fills our alpha bits with
   data that is not wanted or expected by windows, and its
   causing us to send rgba images rather than rgb images to
   the client. So, we manually clear these bytes. */
static void clear_dest_alpha(pixman_image_t *dest,
                             int x, int y,
                             int width, int height)
{
    uint32_t *data;
    int stride;
    int w, h;

    w = pixman_image_get_width(dest);
    h = pixman_image_get_height(dest);

    if (x + width <= 0 || x >= w ||
        y + height <= 0 || y >= h ||
        width == 0 || height == 0) {
        return;
    }

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (x + width > w) {
        width = w - x;
    }

    if (y < 0) {
        height += y;
        y = 0;
    }
    if (y + height > h) {
        height = h - y;
    }

    stride = pixman_image_get_stride(dest);
    data = (uint32_t *) (
        (uint8_t *)pixman_image_get_data(dest) + y * stride + 4 * x);

    if ((*data & 0xff000000U) == 0xff000000U) {
        spice_pixman_fill_rect_rop(dest,
                                   x, y, width, height,
                                   0x00ffffff, SPICE_ROP_AND);
    }
}

static void __blit_image(SpiceCanvas *spice_canvas,
                         pixman_region32_t *region,
                         pixman_image_t *src_image,
                         int offset_x, int offset_y)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_box32_t *rects;
    int n_rects, i;

    rects = pixman_region32_rectangles(region, &n_rects);

    for (i = 0; i < n_rects; i++) {
        int src_x, src_y, dest_x, dest_y, width, height;

        dest_x = rects[i].x1;
        dest_y = rects[i].y1;
        width = rects[i].x2 - rects[i].x1;
        height = rects[i].y2 - rects[i].y1;

        src_x = rects[i].x1 - offset_x;
        src_y = rects[i].y1 - offset_y;

        spice_pixman_blit(canvas->image,
                          src_image,
                          src_x, src_y,
                          dest_x, dest_y,
                          width, height);
    }
}

static void blit_image(SpiceCanvas *spice_canvas,
                       pixman_region32_t *region,
                       pixman_image_t *src_image,
                       int offset_x, int offset_y)
{
    __blit_image(spice_canvas, region, src_image, offset_x, offset_y);
}

static void blit_image_from_surface(SpiceCanvas *spice_canvas,
                                    pixman_region32_t *region,
                                    SpiceCanvas *surface_canvas,
                                    int offset_x, int offset_y)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __blit_image(spice_canvas, region, sw_surface_canvas->image, offset_x, offset_y);
}

static void __blit_image_rop(SpiceCanvas *spice_canvas,
                             pixman_region32_t *region,
                             pixman_image_t *src_image,
                             int offset_x, int offset_y,
                             SpiceROP rop)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_box32_t *rects;
    int n_rects, i;

    rects = pixman_region32_rectangles(region, &n_rects);

    for (i = 0; i < n_rects; i++) {
        int src_x, src_y, dest_x, dest_y, width, height;

        dest_x = rects[i].x1;
        dest_y = rects[i].y1;
        width = rects[i].x2 - rects[i].x1;
        height = rects[i].y2 - rects[i].y1;

        src_x = rects[i].x1 - offset_x;
        src_y = rects[i].y1 - offset_y;

        spice_pixman_blit_rop(canvas->image,
                              src_image,
                              src_x, src_y,
                              dest_x, dest_y,
                              width, height, rop);
    }
}

static void blit_image_rop(SpiceCanvas *spice_canvas,
                           pixman_region32_t *region,
                           pixman_image_t *src_image,
                           int offset_x, int offset_y,
                           SpiceROP rop)
{
    __blit_image_rop(spice_canvas, region, src_image, offset_x, offset_y, rop);
}

static void blit_image_rop_from_surface(SpiceCanvas *spice_canvas,
                                        pixman_region32_t *region,
                                        SpiceCanvas *surface_canvas,
                                        int offset_x, int offset_y,
                                        SpiceROP rop)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __blit_image_rop(spice_canvas, region, sw_surface_canvas->image, offset_x, offset_y, rop);
}



static void __scale_image(SpiceCanvas *spice_canvas,
                          pixman_region32_t *region,
                          pixman_image_t *src,
                          int src_x, int src_y,
                          int src_width, int src_height,
                          int dest_x, int dest_y,
                          int dest_width, int dest_height,
                          int scale_mode)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_transform_t transform;
    pixman_fixed_t fsx, fsy;

    fsx = ((pixman_fixed_48_16_t) src_width * 65536) / dest_width;
    fsy = ((pixman_fixed_48_16_t) src_height * 65536) / dest_height;

    pixman_image_set_clip_region32(canvas->image, region);

    pixman_transform_init_scale(&transform, fsx, fsy);
    pixman_transform_translate(&transform, NULL,
                               pixman_int_to_fixed (src_x),
                               pixman_int_to_fixed (src_y));

    pixman_image_set_transform(src, &transform);
    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);
    spice_return_if_fail(scale_mode == SPICE_IMAGE_SCALE_MODE_INTERPOLATE ||
                         scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST);
    pixman_image_set_filter(src,
                            (scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST) ?
                            PIXMAN_FILTER_NEAREST : PIXMAN_FILTER_GOOD,
                            NULL, 0);

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src, NULL, canvas->image,
                             0, 0, /* src */
                             0, 0, /* mask */
                             dest_x, dest_y, /* dst */
                             dest_width, dest_height);

    pixman_transform_init_identity(&transform);
    pixman_image_set_transform(src, &transform);

    pixman_image_set_clip_region32(canvas->image, NULL);
}

static void scale_image(SpiceCanvas *spice_canvas,
                        pixman_region32_t *region,
                        pixman_image_t *src,
                        int src_x, int src_y,
                        int src_width, int src_height,
                        int dest_x, int dest_y,
                        int dest_width, int dest_height,
                        int scale_mode)
{
    __scale_image(spice_canvas, region, src, src_x, src_y, src_width, src_height, dest_x, dest_y,
                  dest_width,dest_height,scale_mode);
}

static void scale_image_from_surface(SpiceCanvas *spice_canvas,
                                     pixman_region32_t *region,
                                     SpiceCanvas *surface_canvas,
                                     int src_x, int src_y,
                                     int src_width, int src_height,
                                     int dest_x, int dest_y,
                                     int dest_width, int dest_height,
                                     int scale_mode)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __scale_image(spice_canvas, region, sw_surface_canvas->image, src_x, src_y, src_width,
                  src_height, dest_x, dest_y, dest_width,dest_height,scale_mode);
}

static void __scale_image_rop(SpiceCanvas *spice_canvas,
                              pixman_region32_t *region,
                              pixman_image_t *src,
                              int src_x, int src_y,
                              int src_width, int src_height,
                              int dest_x, int dest_y,
                              int dest_width, int dest_height,
                              int scale_mode, SpiceROP rop)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_transform_t transform;
    pixman_image_t *scaled;
    pixman_box32_t *rects;
    int n_rects, i;
    pixman_fixed_t fsx, fsy;
    pixman_format_code_t format;

    fsx = ((pixman_fixed_48_16_t) src_width * 65536) / dest_width;
    fsy = ((pixman_fixed_48_16_t) src_height * 65536) / dest_height;

    spice_return_if_fail(spice_pixman_image_get_format(src, &format));
    scaled = pixman_image_create_bits(format,
                                      dest_width,
                                      dest_height,
                                      NULL, 0);

    pixman_region32_translate(region, -dest_x, -dest_y);
    pixman_image_set_clip_region32(scaled, region);

    pixman_transform_init_scale(&transform, fsx, fsy);
    pixman_transform_translate(&transform, NULL,
                               pixman_int_to_fixed (src_x),
                               pixman_int_to_fixed (src_y));

    pixman_image_set_transform(src, &transform);
    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);
    spice_return_if_fail(scale_mode == SPICE_IMAGE_SCALE_MODE_INTERPOLATE ||
                         scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST);
    pixman_image_set_filter(src,
                            (scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST) ?
                            PIXMAN_FILTER_NEAREST : PIXMAN_FILTER_GOOD,
                            NULL, 0);

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src, NULL, scaled,
                             0, 0, /* src */
                             0, 0, /* mask */
                             0, 0, /* dst */
                             dest_width,
                             dest_height);

    pixman_transform_init_identity(&transform);
    pixman_image_set_transform(src, &transform);

    /* Translate back */
    pixman_region32_translate(region, dest_x, dest_y);

    rects = pixman_region32_rectangles(region, &n_rects);

    for (i = 0; i < n_rects; i++) {
        spice_pixman_blit_rop(canvas->image,
                              scaled,
                              rects[i].x1 - dest_x,
                              rects[i].y1 - dest_y,
                              rects[i].x1, rects[i].y1,
                              rects[i].x2 - rects[i].x1,
                              rects[i].y2 - rects[i].y1,
                              rop);
    }

    pixman_image_unref(scaled);
}

static void scale_image_rop(SpiceCanvas *spice_canvas,
                            pixman_region32_t *region,
                            pixman_image_t *src,
                            int src_x, int src_y,
                            int src_width, int src_height,
                            int dest_x, int dest_y,
                            int dest_width, int dest_height,
                            int scale_mode, SpiceROP rop)
{
    __scale_image_rop(spice_canvas, region, src, src_x, src_y, src_width, src_height, dest_x,
                      dest_y, dest_width, dest_height, scale_mode, rop);
}

static void scale_image_rop_from_surface(SpiceCanvas *spice_canvas,
                                         pixman_region32_t *region,
                                         SpiceCanvas *surface_canvas,
                                         int src_x, int src_y,
                                         int src_width, int src_height,
                                         int dest_x, int dest_y,
                                         int dest_width, int dest_height,
                                         int scale_mode, SpiceROP rop)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __scale_image_rop(spice_canvas, region, sw_surface_canvas->image, src_x, src_y, src_width,
                      src_height, dest_x, dest_y, dest_width, dest_height, scale_mode, rop);
}

static pixman_image_t *canvas_get_as_surface(SwCanvas *canvas,
                                          int with_alpha)
{
    pixman_image_t *target;

    if (with_alpha &&
        canvas->base.format == SPICE_SURFACE_FMT_32_xRGB) {
        target = pixman_image_create_bits(PIXMAN_a8r8g8b8,
                                          pixman_image_get_width(canvas->image),
                                          pixman_image_get_height(canvas->image),
                                          pixman_image_get_data(canvas->image),
                                          pixman_image_get_stride(canvas->image));
    } else {
        target = pixman_image_ref(canvas->image);
    }

    return target;
}

static void __blend_image(SpiceCanvas *spice_canvas,
                          pixman_region32_t *region,
                          int dest_has_alpha,
                          pixman_image_t *src,
                          int src_x, int src_y,
                          int dest_x, int dest_y,
                          int width, int height,
                          int overall_alpha)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_image_t *mask, *dest;

    dest = canvas_get_as_surface(canvas, dest_has_alpha);

    pixman_image_set_clip_region32(dest, region);

    mask = NULL;
    if (overall_alpha != 0xff) {
        pixman_color_t color = { 0, 0, 0, 0 };
        color.alpha = overall_alpha * 0x101;
        mask = pixman_image_create_solid_fill(&color);
    }

    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);

    pixman_image_composite32(PIXMAN_OP_OVER,
                             src, mask, dest,
                             src_x, src_y, /* src */
                             0, 0, /* mask */
                             dest_x, dest_y, /* dst */
                             width,
                             height);

    if (canvas->base.format == SPICE_SURFACE_FMT_32_xRGB &&
        !dest_has_alpha) {
        clear_dest_alpha(dest, dest_x, dest_y, width, height);
    }

    if (mask) {
        pixman_image_unref(mask);
    }

    pixman_image_set_clip_region32(dest, NULL);
    pixman_image_unref(dest);
}

static void blend_image(SpiceCanvas *spice_canvas,
                        pixman_region32_t *region,
                        int dest_has_alpha,
                        pixman_image_t *src,
                        int src_x, int src_y,
                        int dest_x, int dest_y,
                        int width, int height,
                        int overall_alpha)
{
    __blend_image(spice_canvas, region, dest_has_alpha, src, src_x, src_y,
                  dest_x, dest_y, width, height,
                  overall_alpha);
}

static void blend_image_from_surface(SpiceCanvas *spice_canvas,
                                     pixman_region32_t *region,
                                     int dest_has_alpha,
                                     SpiceCanvas *surface_canvas,
                                     int src_has_alpha,
                                     int src_x, int src_y,
                                     int dest_x, int dest_y,
                                     int width, int height,
                                     int overall_alpha)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    pixman_image_t *src;

    src = canvas_get_as_surface(sw_surface_canvas, src_has_alpha);
    __blend_image(spice_canvas, region, dest_has_alpha,
                  src, src_x, src_y,
                  dest_x, dest_y,
                  width, height, overall_alpha);
    pixman_image_unref(src);
}

static void __blend_scale_image(SpiceCanvas *spice_canvas,
                                pixman_region32_t *region,
                                int dest_has_alpha,
                                pixman_image_t *src,
                                int src_x, int src_y,
                                int src_width, int src_height,
                                int dest_x, int dest_y,
                                int dest_width, int dest_height,
                                int scale_mode,
                                int overall_alpha)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_transform_t transform;
    pixman_image_t *mask, *dest;
    pixman_fixed_t fsx, fsy;

    fsx = ((pixman_fixed_48_16_t) src_width * 65536) / dest_width;
    fsy = ((pixman_fixed_48_16_t) src_height * 65536) / dest_height;

    dest = canvas_get_as_surface(canvas, dest_has_alpha);

    pixman_image_set_clip_region32(dest, region);

    pixman_transform_init_scale(&transform, fsx, fsy);
    pixman_transform_translate(&transform, NULL,
                               pixman_int_to_fixed (src_x),
                               pixman_int_to_fixed (src_y));

    mask = NULL;
    if (overall_alpha != 0xff) {
        pixman_color_t color = { 0, 0, 0, 0 };
        color.alpha = overall_alpha * 0x101;
        mask = pixman_image_create_solid_fill(&color);
    }

    pixman_image_set_transform(src, &transform);
    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);
    spice_return_if_fail(scale_mode == SPICE_IMAGE_SCALE_MODE_INTERPOLATE ||
                         scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST);
    pixman_image_set_filter(src,
                            (scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST) ?
                            PIXMAN_FILTER_NEAREST : PIXMAN_FILTER_GOOD,
                            NULL, 0);

    pixman_image_composite32(PIXMAN_OP_OVER,
                             src, mask, dest,
                             0, 0, /* src */
                             0, 0, /* mask */
                             dest_x, dest_y, /* dst */
                             dest_width, dest_height);

    if (canvas->base.format == SPICE_SURFACE_FMT_32_xRGB &&
        !dest_has_alpha) {
        clear_dest_alpha(dest, dest_x, dest_y, dest_width, dest_height);
    }

    pixman_transform_init_identity(&transform);
    pixman_image_set_transform(src, &transform);

    if (mask) {
        pixman_image_unref(mask);
    }

    pixman_image_set_clip_region32(dest, NULL);
    pixman_image_unref(dest);
}

static void blend_scale_image(SpiceCanvas *spice_canvas,
                              pixman_region32_t *region,
                              int dest_has_alpha,
                              pixman_image_t *src,
                              int src_x, int src_y,
                              int src_width, int src_height,
                              int dest_x, int dest_y,
                              int dest_width, int dest_height,
                              int scale_mode,
                              int overall_alpha)
{
    __blend_scale_image(spice_canvas, region, dest_has_alpha,
                        src, src_x, src_y, src_width, src_height,
                        dest_x, dest_y, dest_width, dest_height,
                        scale_mode, overall_alpha);
}

static void blend_scale_image_from_surface(SpiceCanvas *spice_canvas,
                                           pixman_region32_t *region,
                                           int dest_has_alpha,
                                           SpiceCanvas *surface_canvas,
                                           int src_has_alpha,
                                           int src_x, int src_y,
                                           int src_width, int src_height,
                                           int dest_x, int dest_y,
                                           int dest_width, int dest_height,
                                           int scale_mode,
                                           int overall_alpha)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    pixman_image_t *src;

    src = canvas_get_as_surface(sw_surface_canvas, src_has_alpha);
    __blend_scale_image(spice_canvas, region, dest_has_alpha, src, src_x, src_y, src_width,
                        src_height, dest_x, dest_y, dest_width, dest_height, scale_mode,
                        overall_alpha);
    pixman_image_unref(src);
}

static void __colorkey_image(SpiceCanvas *spice_canvas,
                             pixman_region32_t *region,
                             pixman_image_t *src_image,
                             int offset_x, int offset_y,
                             uint32_t transparent_color)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_box32_t *rects;
    int n_rects, i;

    rects = pixman_region32_rectangles(region, &n_rects);

    for (i = 0; i < n_rects; i++) {
        int src_x, src_y, dest_x, dest_y, width, height;

        dest_x = rects[i].x1;
        dest_y = rects[i].y1;
        width = rects[i].x2 - rects[i].x1;
        height = rects[i].y2 - rects[i].y1;

        src_x = rects[i].x1 - offset_x;
        src_y = rects[i].y1 - offset_y;

        spice_pixman_blit_colorkey(canvas->image,
                                   src_image,
                                   src_x, src_y,
                                   dest_x, dest_y,
                                   width, height,
                                   transparent_color);
    }
}

static void colorkey_image(SpiceCanvas *spice_canvas,
                           pixman_region32_t *region,
                           pixman_image_t *src_image,
                           int offset_x, int offset_y,
                           uint32_t transparent_color)
{
    __colorkey_image(spice_canvas, region, src_image, offset_x, offset_y, transparent_color);
}

static void colorkey_image_from_surface(SpiceCanvas *spice_canvas,
                                        pixman_region32_t *region,
                                        SpiceCanvas *surface_canvas,
                                        int offset_x, int offset_y,
                                        uint32_t transparent_color)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __colorkey_image(spice_canvas, region, sw_surface_canvas->image, offset_x, offset_y,
                     transparent_color);
}

static void __colorkey_scale_image(SpiceCanvas *spice_canvas,
                                   pixman_region32_t *region,
                                   pixman_image_t *src,
                                   int src_x, int src_y,
                                   int src_width, int src_height,
                                   int dest_x, int dest_y,
                                   int dest_width, int dest_height,
                                   uint32_t transparent_color)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_transform_t transform;
    pixman_image_t *scaled;
    pixman_box32_t *rects;
    int n_rects, i;
    pixman_fixed_t fsx, fsy;
    pixman_format_code_t format;

    fsx = ((pixman_fixed_48_16_t) src_width * 65536) / dest_width;
    fsy = ((pixman_fixed_48_16_t) src_height * 65536) / dest_height;

    spice_return_if_fail(spice_pixman_image_get_format(src, &format));
    scaled = pixman_image_create_bits(format,
                                      dest_width,
                                      dest_height,
                                      NULL, 0);

    pixman_region32_translate(region, -dest_x, -dest_y);
    pixman_image_set_clip_region32(scaled, region);

    pixman_transform_init_scale(&transform, fsx, fsy);
    pixman_transform_translate(&transform, NULL,
                               pixman_int_to_fixed (src_x),
                               pixman_int_to_fixed (src_y));

    pixman_image_set_transform(src, &transform);
    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);
    pixman_image_set_filter(src,
                            PIXMAN_FILTER_NEAREST,
                            NULL, 0);

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src, NULL, scaled,
                             0, 0, /* src */
                             0, 0, /* mask */
                             0, 0, /* dst */
                             dest_width,
                             dest_height);

    pixman_transform_init_identity(&transform);
    pixman_image_set_transform(src, &transform);

    /* Translate back */
    pixman_region32_translate(region, dest_x, dest_y);

    rects = pixman_region32_rectangles(region, &n_rects);

    for (i = 0; i < n_rects; i++) {
        spice_pixman_blit_colorkey(canvas->image,
                                   scaled,
                                   rects[i].x1 - dest_x,
                                   rects[i].y1 - dest_y,
                                   rects[i].x1, rects[i].y1,
                                   rects[i].x2 - rects[i].x1,
                                   rects[i].y2 - rects[i].y1,
                                   transparent_color);
    }

    pixman_image_unref(scaled);
}

static void colorkey_scale_image(SpiceCanvas *spice_canvas,
                                 pixman_region32_t *region,
                                 pixman_image_t *src,
                                 int src_x, int src_y,
                                 int src_width, int src_height,
                                 int dest_x, int dest_y,
                                 int dest_width, int dest_height,
                                 uint32_t transparent_color)
{
    __colorkey_scale_image(spice_canvas, region, src, src_x, src_y, src_width, src_height, dest_x,
                           dest_y, dest_width, dest_height, transparent_color);
}

static void colorkey_scale_image_from_surface(SpiceCanvas *spice_canvas,
                                              pixman_region32_t *region,
                                              SpiceCanvas *surface_canvas,
                                              int src_x, int src_y,
                                              int src_width, int src_height,
                                              int dest_x, int dest_y,
                                              int dest_width, int dest_height,
                                              uint32_t transparent_color)
{
    SwCanvas *sw_surface_canvas = (SwCanvas *)surface_canvas;
    __colorkey_scale_image(spice_canvas, region, sw_surface_canvas->image, src_x, src_y,
                           src_width, src_height, dest_x, dest_y, dest_width, dest_height,
                           transparent_color);
}

static void canvas_put_image(SpiceCanvas *spice_canvas,
#ifdef WIN32
                             HDC dc,
#endif
                             const SpiceRect *dest, const uint8_t *src_data,
                             uint32_t src_width, uint32_t src_height, int src_stride,
                             const QRegion *clip)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_image_t *src;
    uint32_t dest_width;
    uint32_t dest_height;
    double sx, sy;
    pixman_transform_t transform;

    src = pixman_image_create_bits(PIXMAN_x8r8g8b8,
                                   src_width,
                                   src_height,
                                   (uint32_t*)src_data,
                                   src_stride);


    if (clip) {
        pixman_image_set_clip_region32 (canvas->image, (pixman_region32_t *)clip);
    }

    dest_width = dest->right - dest->left;
    dest_height = dest->bottom - dest->top;

    if (dest_width != src_width || dest_height != src_height) {
        sx = (double)(src_width) / (dest_width);
        sy = (double)(src_height) / (dest_height);

        pixman_transform_init_scale(&transform,
                                    pixman_double_to_fixed(sx),
                                    pixman_double_to_fixed(sy));
        pixman_image_set_transform(src, &transform);
        pixman_image_set_filter(src,
                                PIXMAN_FILTER_NEAREST,
                                NULL, 0);
    }

    pixman_image_set_repeat(src, PIXMAN_REPEAT_NONE);

    pixman_image_composite32(PIXMAN_OP_SRC,
                             src, NULL, canvas->image,
                             0, 0, /* src */
                             0, 0, /* mask */
                             dest->left, dest->top, /* dst */
                             dest_width, dest_height);


    if (clip) {
        pixman_image_set_clip_region32(canvas->image, NULL);
    }
    pixman_image_unref(src);
}


static void canvas_draw_text(SpiceCanvas *spice_canvas, SpiceRect *bbox,
                             SpiceClip *clip, SpiceText *text)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_region32_t dest_region;
    pixman_image_t *str_mask, *brush;
    SpiceString *str;
    SpicePoint pos = { 0, 0 };
    int depth;

    pixman_region32_init_rect(&dest_region,
                              bbox->left, bbox->top,
                              bbox->right - bbox->left,
                              bbox->bottom - bbox->top);

    canvas_clip_pixman(&canvas->base, &dest_region, clip);

    if (!pixman_region32_not_empty(&dest_region)) {
        touch_brush(&canvas->base, &text->fore_brush);
        touch_brush(&canvas->base, &text->back_brush);
        pixman_region32_fini(&dest_region);
        return;
    }

    if (!rect_is_empty(&text->back_area)) {
        pixman_region32_t back_region;

        /* Nothing else makes sense for text and we should deprecate it
         * and actually it means OVER really */
        spice_return_if_fail(text->fore_mode == SPICE_ROPD_OP_PUT);

        pixman_region32_init_rect(&back_region,
                                  text->back_area.left,
                                  text->back_area.top,
                                  text->back_area.right - text->back_area.left,
                                  text->back_area.bottom - text->back_area.top);

        pixman_region32_intersect(&back_region, &back_region, &dest_region);

        if (pixman_region32_not_empty(&back_region)) {
            draw_brush(spice_canvas, &back_region, &text->back_brush, SPICE_ROP_COPY);
        }

        pixman_region32_fini(&back_region);
    }
    str = (SpiceString *)SPICE_GET_ADDRESS(text->str);

    if (str->flags & SPICE_STRING_FLAGS_RASTER_A1) {
        depth = 1;
    } else if (str->flags & SPICE_STRING_FLAGS_RASTER_A4) {
        depth = 4;
    } else if (str->flags & SPICE_STRING_FLAGS_RASTER_A8) {
        spice_warning("untested path A8 glyphs");
        depth = 8;
    } else {
        spice_warning("unsupported path vector glyphs");
        pixman_region32_fini (&dest_region);
        return;
    }

    brush = canvas_get_pixman_brush(canvas, &text->fore_brush);

    str_mask = canvas_get_str_mask(&canvas->base, str, depth, &pos);
    if (brush) {
        pixman_image_set_clip_region32(canvas->image, &dest_region);

        pixman_image_composite32(PIXMAN_OP_OVER,
                                 brush,
                                 str_mask,
                                 canvas->image,
                                 0, 0,
                                 0, 0,
                                 pos.x, pos.y,
                                 pixman_image_get_width(str_mask),
                                 pixman_image_get_height(str_mask));
        if (canvas->base.format == SPICE_SURFACE_FMT_32_xRGB) {
            clear_dest_alpha(canvas->image, pos.x, pos.y,
                             pixman_image_get_width(str_mask),
                             pixman_image_get_height(str_mask));
        }
        pixman_image_unref(brush);

        pixman_image_set_clip_region32(canvas->image, NULL);
    }
    pixman_image_unref(str_mask);
    pixman_region32_fini(&dest_region);
}

static void canvas_read_bits(SpiceCanvas *spice_canvas, uint8_t *dest,
                             int dest_stride, const SpiceRect *area)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    pixman_image_t* surface;
    uint8_t *src;
    int src_stride;
    uint8_t *dest_end;
    int bpp;

    spice_return_if_fail(canvas && area);

    surface = canvas->image;

    bpp = spice_pixman_image_get_bpp(surface) / 8;

    src_stride = pixman_image_get_stride(surface);
    src = (uint8_t *)pixman_image_get_data(surface) +
        area->top * src_stride + area->left * bpp;
    dest_end = dest + (area->bottom - area->top) * dest_stride;
    for (; dest != dest_end; dest += dest_stride, src += src_stride) {
        memcpy(dest, src, (area->right - area->left) * bpp);
    }
}

static void canvas_clear(SpiceCanvas *spice_canvas)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    spice_pixman_fill_rect(canvas->image,
                           0, 0,
                           pixman_image_get_width(canvas->image),
                           pixman_image_get_height(canvas->image),
                           0);
}

static void canvas_destroy(SpiceCanvas *spice_canvas)
{
    SwCanvas *canvas = (SwCanvas *)spice_canvas;
    if (!canvas) {
        return;
    }
    pixman_image_unref(canvas->image);
    canvas_base_destroy(&canvas->base);
    free(canvas->private_data);
    free(canvas);
}

static int need_init = 1;
static SpiceCanvasOps sw_canvas_ops;

static SpiceCanvas *canvas_create_common(pixman_image_t *image,
                                         uint32_t format
#ifdef SW_CANVAS_CACHE
                           , SpiceImageCache *bits_cache
                           , SpicePaletteCache *palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                           , SpiceImageCache *bits_cache
#endif
                           , SpiceImageSurfaces *surfaces
                           , SpiceGlzDecoder *glz_decoder
                           , SpiceJpegDecoder *jpeg_decoder
                           , SpiceZlibDecoder *zlib_decoder
                           )
{
    SwCanvas *canvas;

    if (need_init) {
        return NULL;
    }
    spice_pixman_image_set_format(image,
                                  spice_surface_format_to_pixman (format));

    canvas = spice_new0(SwCanvas, 1);
    canvas_base_init(&canvas->base, &sw_canvas_ops,
                               pixman_image_get_width (image),
                               pixman_image_get_height (image),
                               format
#ifdef SW_CANVAS_CACHE
                               , bits_cache
                               , palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                               , bits_cache
#endif
                               , surfaces
                               , glz_decoder
                               , jpeg_decoder
                               , zlib_decoder
                               );
    canvas->private_data = NULL;
    canvas->private_data_size = 0;

    canvas->image = image;

    return (SpiceCanvas *)canvas;
}

SpiceCanvas *canvas_create(int width, int height, uint32_t format
#ifdef SW_CANVAS_CACHE
                           , SpiceImageCache *bits_cache
                           , SpicePaletteCache *palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                           , SpiceImageCache *bits_cache
#endif
                           , SpiceImageSurfaces *surfaces
                           , SpiceGlzDecoder *glz_decoder
                           , SpiceJpegDecoder *jpeg_decoder
                           , SpiceZlibDecoder *zlib_decoder
                           )
{
    pixman_image_t *image;

    image = pixman_image_create_bits(spice_surface_format_to_pixman (format),
                                     width, height, NULL, 0);

    return canvas_create_common(image, format
#ifdef SW_CANVAS_CACHE
                                , bits_cache
                                , palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                                , bits_cache
#endif
                                , surfaces
                                , glz_decoder
                                , jpeg_decoder
                                , zlib_decoder
                                );
}

SpiceCanvas *canvas_create_for_data(int width, int height, uint32_t format,
                                    uint8_t *data, int stride
#ifdef SW_CANVAS_CACHE
                           , SpiceImageCache *bits_cache
                           , SpicePaletteCache *palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                           , SpiceImageCache *bits_cache
#endif
                           , SpiceImageSurfaces *surfaces
                           , SpiceGlzDecoder *glz_decoder
                           , SpiceJpegDecoder *jpeg_decoder
                           , SpiceZlibDecoder *zlib_decoder
                           )
{
    pixman_image_t *image;

    image = pixman_image_create_bits(spice_surface_format_to_pixman (format),
                                     width, height, (uint32_t *)data, stride);

    return canvas_create_common(image, format
#ifdef SW_CANVAS_CACHE
                                , bits_cache
                                , palette_cache
#elif defined(SW_CANVAS_IMAGE_CACHE)
                                , bits_cache
#endif
                                , surfaces
                                , glz_decoder
                                , jpeg_decoder
                                , zlib_decoder
                                );
}

void sw_canvas_init(void) //unsafe global function
{
    if (!need_init) {
        return;
    }
    need_init = 0;

    canvas_base_init_ops(&sw_canvas_ops);
    sw_canvas_ops.draw_text = canvas_draw_text;
    sw_canvas_ops.put_image = canvas_put_image;
    sw_canvas_ops.clear = canvas_clear;
    sw_canvas_ops.read_bits = canvas_read_bits;
    sw_canvas_ops.destroy = canvas_destroy;

    sw_canvas_ops.fill_solid_spans = fill_solid_spans;
    sw_canvas_ops.fill_solid_rects = fill_solid_rects;
    sw_canvas_ops.fill_solid_rects_rop = fill_solid_rects_rop;
    sw_canvas_ops.fill_tiled_rects = fill_tiled_rects;
    sw_canvas_ops.fill_tiled_rects_from_surface = fill_tiled_rects_from_surface;
    sw_canvas_ops.fill_tiled_rects_rop = fill_tiled_rects_rop;
    sw_canvas_ops.fill_tiled_rects_rop_from_surface = fill_tiled_rects_rop_from_surface;
    sw_canvas_ops.blit_image = blit_image;
    sw_canvas_ops.blit_image_from_surface = blit_image_from_surface;
    sw_canvas_ops.blit_image_rop = blit_image_rop;
    sw_canvas_ops.blit_image_rop_from_surface = blit_image_rop_from_surface;
    sw_canvas_ops.scale_image = scale_image;
    sw_canvas_ops.scale_image_from_surface = scale_image_from_surface;
    sw_canvas_ops.scale_image_rop = scale_image_rop;
    sw_canvas_ops.scale_image_rop_from_surface = scale_image_rop_from_surface;
    sw_canvas_ops.blend_image = blend_image;
    sw_canvas_ops.blend_image_from_surface = blend_image_from_surface;
    sw_canvas_ops.blend_scale_image = blend_scale_image;
    sw_canvas_ops.blend_scale_image_from_surface = blend_scale_image_from_surface;
    sw_canvas_ops.colorkey_image = colorkey_image;
    sw_canvas_ops.colorkey_image_from_surface = colorkey_image_from_surface;
    sw_canvas_ops.colorkey_scale_image = colorkey_scale_image;
    sw_canvas_ops.colorkey_scale_image_from_surface = colorkey_scale_image_from_surface;
    sw_canvas_ops.copy_region = copy_region;
    sw_canvas_ops.get_image = get_image;
    rop3_init();
}
