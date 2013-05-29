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

#ifndef _H_CANVAS_BASE
#define _H_CANVAS_BASE

#include <spice/macros.h>

#include "pixman_utils.h"
#include "lz.h"
#include "region.h"
#include "draw.h"
#ifdef WIN32
#include <windows.h>
#endif

SPICE_BEGIN_DECLS

typedef void (*spice_destroy_fn_t)(void *data);

typedef struct _SpiceImageCache SpiceImageCache;
typedef struct _SpiceImageSurfaces SpiceImageSurfaces;
typedef struct _SpicePaletteCache SpicePaletteCache;
typedef struct _SpiceGlzDecoder SpiceGlzDecoder;
typedef struct _SpiceJpegDecoder SpiceJpegDecoder;
typedef struct _SpiceZlibDecoder SpiceZlibDecoder;
typedef struct _SpiceCanvas SpiceCanvas;

typedef struct {
    void (*put)(SpiceImageCache *cache,
                uint64_t id,
                pixman_image_t *surface);
    pixman_image_t *(*get)(SpiceImageCache *cache,
                           uint64_t id);
#ifdef SW_CANVAS_CACHE
    void (*put_lossy)(SpiceImageCache *cache,
                      uint64_t id,
                      pixman_image_t *surface);
    void (*replace_lossy)(SpiceImageCache *cache,
                          uint64_t id,
                          pixman_image_t *surface);
    pixman_image_t *(*get_lossless)(SpiceImageCache *cache,
                                    uint64_t id);
#endif
} SpiceImageCacheOps;

struct _SpiceImageCache {
  SpiceImageCacheOps *ops;
};

typedef struct {
 SpiceCanvas *(*get)(SpiceImageSurfaces *surfaces,
                     uint32_t surface_id);
} SpiceImageSurfacesOps;

struct _SpiceImageSurfaces {
 SpiceImageSurfacesOps *ops;
};

typedef struct {
    void (*put)(SpicePaletteCache *cache,
                SpicePalette *palette);
    SpicePalette *(*get)(SpicePaletteCache *cache,
                         uint64_t id);
    void (*release)(SpicePaletteCache *cache,
                    SpicePalette *palette);
} SpicePaletteCacheOps;

struct _SpicePaletteCache {
  SpicePaletteCacheOps *ops;
};

typedef struct {
    void (*decode)(SpiceGlzDecoder *decoder,
                   uint8_t *data,
                   SpicePalette *plt,
                   void *usr_data);
} SpiceGlzDecoderOps;

struct _SpiceGlzDecoder {
  SpiceGlzDecoderOps *ops;
};


typedef struct SpiceJpegDecoderOps {
    void (*begin_decode)(SpiceJpegDecoder *decoder,
                         uint8_t* data,
                         int data_size,
                         int* out_width,
                         int* out_height);
    void (*decode)(SpiceJpegDecoder *decoder,
                   uint8_t* dest,
                   int stride,
                   int format);
} SpiceJpegDecoderOps;

struct _SpiceJpegDecoder {
    SpiceJpegDecoderOps *ops;
};

typedef struct {
    void (*decode)(SpiceZlibDecoder *decoder,
                   uint8_t *data,
                   int data_size,
                   uint8_t *dest,
                   int dest_size);
} SpiceZlibDecoderOps;

struct _SpiceZlibDecoder {
  SpiceZlibDecoderOps *ops;
};

typedef struct {
    void (*draw_fill)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceFill *fill);
    void (*draw_copy)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceCopy *copy);
    void (*draw_opaque)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceOpaque *opaque);
    void (*copy_bits)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpicePoint *src_pos);
    void (*draw_text)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceText *text);
    void (*draw_stroke)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceStroke *stroke);
    void (*draw_rop3)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceRop3 *rop3);
    void (*draw_composite)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceComposite *composite);
    void (*draw_blend)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlend *blend);
    void (*draw_blackness)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlackness *blackness);
    void (*draw_whiteness)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceWhiteness *whiteness);
    void (*draw_invers)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceInvers *invers);
    void (*draw_transparent)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceTransparent* transparent);
    void (*draw_alpha_blend)(SpiceCanvas *canvas, SpiceRect *bbox, SpiceClip *clip, SpiceAlphaBlend* alpha_blend);
    void (*put_image)(SpiceCanvas *canvas,
#ifdef WIN32
                      HDC dc,
#endif
                      const SpiceRect *dest, const uint8_t *src_data,
                      uint32_t src_width, uint32_t src_height, int src_stride,
                      const QRegion *clip);
    void (*clear)(SpiceCanvas *canvas);
    void (*read_bits)(SpiceCanvas *canvas, uint8_t *dest, int dest_stride, const SpiceRect *area);
    void (*group_start)(SpiceCanvas *canvas, QRegion *region);
    void (*group_end)(SpiceCanvas *canvas);
    void (*destroy)(SpiceCanvas *canvas);

    /* Implementation vfuncs */
    void (*fill_solid_spans)(SpiceCanvas *canvas,
                             SpicePoint *points,
                             int *widths,
                             int n_spans,
                             uint32_t color);
    void (*fill_solid_rects)(SpiceCanvas *canvas,
                             pixman_box32_t *rects,
                             int n_rects,
                             uint32_t color);
    void (*fill_solid_rects_rop)(SpiceCanvas *canvas,
                                 pixman_box32_t *rects,
                                 int n_rects,
                                 uint32_t color,
                                 SpiceROP rop);
    void (*fill_tiled_rects)(SpiceCanvas *canvas,
                             pixman_box32_t *rects,
                             int n_rects,
                             pixman_image_t *tile,
                             int offset_x, int offset_y);
    void (*fill_tiled_rects_from_surface)(SpiceCanvas *canvas,
                                          pixman_box32_t *rects,
                                          int n_rects,
                                          SpiceCanvas *tile,
                                          int offset_x, int offset_y);
    void (*fill_tiled_rects_rop)(SpiceCanvas *canvas,
                                 pixman_box32_t *rects,
                                 int n_rects,
                                 pixman_image_t *tile,
                                 int offset_x, int offset_y,
                                 SpiceROP rop);
    void (*fill_tiled_rects_rop_from_surface)(SpiceCanvas *canvas,
                                              pixman_box32_t *rects,
                                              int n_rects,
                                              SpiceCanvas *tile,
                                              int offset_x, int offset_y,
                                              SpiceROP rop);
    void (*blit_image)(SpiceCanvas *canvas,
                       pixman_region32_t *region,
                       pixman_image_t *src_image,
                       int offset_x, int offset_y);
    void (*blit_image_from_surface)(SpiceCanvas *canvas,
                                    pixman_region32_t *region,
                                    SpiceCanvas *src_image,
                                    int offset_x, int offset_y);
    void (*blit_image_rop)(SpiceCanvas *canvas,
                           pixman_region32_t *region,
                           pixman_image_t *src_image,
                           int offset_x, int offset_y,
                           SpiceROP rop);
    void (*blit_image_rop_from_surface)(SpiceCanvas *canvas,
                                        pixman_region32_t *region,
                                        SpiceCanvas *src_image,
                                        int offset_x, int offset_y,
                                        SpiceROP rop);
    void (*scale_image)(SpiceCanvas *canvas,
                        pixman_region32_t *region,
                        pixman_image_t *src_image,
                        int src_x, int src_y,
                        int src_width, int src_height,
                        int dest_x, int dest_y,
                        int dest_width, int dest_height,
                        int scale_mode);
    void (*scale_image_from_surface)(SpiceCanvas *canvas,
                                     pixman_region32_t *region,
                                     SpiceCanvas *src_image,
                                     int src_x, int src_y,
                                     int src_width, int src_height,
                                     int dest_x, int dest_y,
                                     int dest_width, int dest_height,
                                     int scale_mode);
    void (*scale_image_rop)(SpiceCanvas *canvas,
                            pixman_region32_t *region,
                            pixman_image_t *src_image,
                            int src_x, int src_y,
                            int src_width, int src_height,
                            int dest_x, int dest_y,
                            int dest_width, int dest_height,
                            int scale_mode, SpiceROP rop);
    void (*scale_image_rop_from_surface)(SpiceCanvas *canvas,
                                         pixman_region32_t *region,
                                         SpiceCanvas *src_image,
                                         int src_x, int src_y,
                                         int src_width, int src_height,
                                         int dest_x, int dest_y,
                                         int dest_width, int dest_height,
                                         int scale_mode, SpiceROP rop);
    void (*blend_image)(SpiceCanvas *canvas,
                        pixman_region32_t *region,
                        int dest_has_alpha,
                        pixman_image_t *src_image,
                        int src_x, int src_y,
                        int dest_x, int dest_y,
                        int width, int height,
                        int overall_alpha);
    void (*blend_image_from_surface)(SpiceCanvas *canvas,
                                     pixman_region32_t *region,
                                     int dest_has_alpha,
                                     SpiceCanvas *src_image,
                                     int src_has_alpha,
                                     int src_x, int src_y,
                                     int dest_x, int dest_y,
                                     int width, int height,
                                     int overall_alpha);
    void (*blend_scale_image)(SpiceCanvas *canvas,
                              pixman_region32_t *region,
                              int dest_has_alpha,
                              pixman_image_t *src_image,
                              int src_x, int src_y,
                              int src_width, int src_height,
                              int dest_x, int dest_y,
                              int dest_width, int dest_height,
                              int scale_mode,
                              int overall_alpha);
    void (*blend_scale_image_from_surface)(SpiceCanvas *canvas,
                                           pixman_region32_t *region,
                                           int dest_has_alpha,
                                           SpiceCanvas *src_image,
                                           int src_has_alpha,
                                           int src_x, int src_y,
                                           int src_width, int src_height,
                                           int dest_x, int dest_y,
                                           int dest_width, int dest_height,
                                           int scale_mode,
                                           int overall_alpha);
    void (*colorkey_image)(SpiceCanvas *canvas,
                           pixman_region32_t *region,
                           pixman_image_t *src_image,
                           int offset_x, int offset_y,
                           uint32_t transparent_color);
    void (*colorkey_image_from_surface)(SpiceCanvas *canvas,
                                        pixman_region32_t *region,
                                        SpiceCanvas *src_image,
                                        int offset_x, int offset_y,
                                        uint32_t transparent_color);
    void (*colorkey_scale_image)(SpiceCanvas *canvas,
                                 pixman_region32_t *region,
                                 pixman_image_t *src_image,
                                 int src_x, int src_y,
                                 int src_width, int src_height,
                                 int dest_x, int dest_y,
                                 int dest_width, int dest_height,
                                 uint32_t transparent_color);
    void (*colorkey_scale_image_from_surface)(SpiceCanvas *canvas,
                                              pixman_region32_t *region,
                                              SpiceCanvas *src_image,
                                              int src_x, int src_y,
                                              int src_width, int src_height,
                                              int dest_x, int dest_y,
                                              int dest_width, int dest_height,
                                              uint32_t transparent_color);
    void (*copy_region)(SpiceCanvas *canvas,
                        pixman_region32_t *dest_region,
                        int dx, int dy);
    pixman_image_t *(*get_image)(SpiceCanvas *canvas, int force_opaque);
} SpiceCanvasOps;

void spice_canvas_set_usr_data(SpiceCanvas *canvas, void *data, spice_destroy_fn_t destroy_fn);
void *spice_canvas_get_usr_data(SpiceCanvas *canvas);

struct _SpiceCanvas {
  SpiceCanvasOps *ops;
};

SPICE_END_DECLS

#endif
