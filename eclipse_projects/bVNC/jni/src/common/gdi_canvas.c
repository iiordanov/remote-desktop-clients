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

#include <windows.h>
#include <wingdi.h>
#include "gdi_canvas.h"
#define GDI_CANVAS
#include "canvas_base.c"
#include "rop3.h"
#include "rect.h"
#include "region.h"
#include "threads.h"

typedef struct GdiCanvas GdiCanvas;

struct GdiCanvas {
    CanvasBase base;
    HDC dc;
    RecurciveMutex* lock;
};


struct BitmapData {
    HBITMAP hbitmap;
    HBITMAP prev_hbitmap;
    SpicePoint pos;
    uint8_t flags;
    HDC dc;
    int cache;
    int from_surface;
};

#define _rop3_brush 0xf0
#define _rop3_src 0xcc
#define _rop3_dest 0xaa

uint32_t raster_ops[] = {
    0x00000042,
    0x00010289,
    0x00020C89,
    0x000300AA,
    0x00040C88,
    0x000500A9,
    0x00060865,
    0x000702C5,
    0x00080F08,
    0x00090245,
    0x000A0329,
    0x000B0B2A,
    0x000C0324,
    0x000D0B25,
    0x000E08A5,
    0x000F0001,
    0x00100C85,
    0x001100A6,
    0x00120868,
    0x001302C8,
    0x00140869,
    0x001502C9,
    0x00165CCA,
    0x00171D54,
    0x00180D59,
    0x00191CC8,
    0x001A06C5,
    0x001B0768,
    0x001C06CA,
    0x001D0766,
    0x001E01A5,
    0x001F0385,
    0x00200F09,
    0x00210248,
    0x00220326,
    0x00230B24,
    0x00240D55,
    0x00251CC5,
    0x002606C8,
    0x00271868,
    0x00280369,
    0x002916CA,
    0x002A0CC9,
    0x002B1D58,
    0x002C0784,
    0x002D060A,
    0x002E064A,
    0x002F0E2A,
    0x0030032A,
    0x00310B28,
    0x00320688,
    0x00330008,
    0x003406C4,
    0x00351864,
    0x003601A8,
    0x00370388,
    0x0038078A, // PSDPoax
    0x00390604, // SPDnox
    0x003A0644, // SPDSxox
    0x003B0E24, // SPDnoan
    0x003C004A, // PSx
    0x003D18A4, // SPDSonox
    0x003E1B24, // SPDSnaox
    0x003F00EA, // PSan
    0x00400F0A, // PSDnaa
    0x00410249, // DPSxon
    0x00420D5D, // SDxPDxa
    0x00431CC4, // SPDSanaxn
    0x00440328, // SDna SRCERASE
    0x00450B29, // DPSnaon
    0x004606C6, // DSPDaox
    0x0047076A, // PSDPxaxn
    0x00480368, // SDPxa
    0x004916C5, // PDSPDaoxxn
    0x004A0789, // DPSDoax
    0x004B0605, // PDSnox
    0x004C0CC8, // SDPana
    0x004D1954, // SSPxDSxoxn
    0x004E0645, // PDSPxox
    0x004F0E25, // PDSnoan
    0x00500325, // PDna
    0x00510B26, // DSPnaon
    0x005206C9, // DPSDaox
    0x00530764, // SPDSxaxn
    0x005408A9, // DPSonon
    0x00550009, // Dn DSTINVERT
    0x005601A9, // DPSox
    0x00570389, // DPSoan
    0x00580785, // PDSPoax
    0x00590609, // DPSnox
    0x005A0049, // DPx PATINVERT
    0x005B18A9, // DPSDonox
    0x005C0649, // DPSDxox
    0x005D0E29, // DPSnoan
    0x005E1B29, // DPSDnaox
    0x005F00E9, // DPan
    0x00600365, // PDSxa
    0x006116C6, // DSPDSaoxxn
    0x00620786, // DSPDoax
    0x00630608, // SDPnox
    0x00640788, // SDPSoax
    0x00650606, // DSPnox
    0x00660046, // DSx SRCINVERT
    0x006718A8, // SDPSonox
    0x006858A6, // DSPDSonoxxn
    0x00690145, // PDSxxn
    0x006A01E9, // DPSax
    0x006B178A, // PSDPSoaxxn
    0x006C01E8, // SDPax
    0x006D1785, // PDSPDoaxxn
    0x006E1E28, // SDPSnoax
    0x006F0C65, // PDSxnan
    0x00700CC5, // PDSana
    0x00711D5C, // SSDxPDxaxn
    0x00720648, // SDPSxox
    0x00730E28, // SDPnoan
    0x00740646, // DSPDxox
    0x00750E26, // DSPnoan
    0x00761B28, // SDPSnaox
    0x007700E6, // DSan
    0x007801E5, // PDSax
    0x00791786, // DSPDSoaxxn
    0x007A1E29, // DPSDnoax
    0x007B0C68, // SDPxnan
    0x007C1E24, // SPDSnoax
    0x007D0C69, // DPSxnan
    0x007E0955, // SPxDSxo
    0x007F03C9, // DPSaan
    0x008003E9, // DPSaa
    0x00810975, // SPxDSxon
    0x00820C49, // DPSxna
    0x00831E04, // SPDSnoaxn
    0x00840C48, // SDPxna
    0x00851E05, // PDSPnoaxn
    0x008617A6, // DSPDSoaxx
    0x008701C5, // PDSaxn
    0x008800C6, // DSa SRCAND
    0x00891B08, // SDPSnaoxn
    0x008A0E06, // DSPnoa
    0x008B0666, // DSPDxoxn
    0x008C0E08, // SDPnoa
    0x008D0668, // SDPSxoxn
    0x008E1D7C, // SSDxPDxax
    0x008F0CE5, // PDSanan
    0x00900C45, // PDSxna
    0x00911E08, // SDPSnoaxn
    0x009217A9, // DPSDPoaxx
    0x009301C4, // SPDaxn
    0x009417AA, // PSDPSoaxx
    0x009501C9, // DPSaxn
    0x00960169, // DPSxx
    0x0097588A, // PSDPSonoxx
    0x00981888, // SDPSonoxn
    0x00990066, // DSxn
    0x009A0709, // DPSnax
    0x009B07A8, // SDPSoaxn
    0x009C0704, // SPDnax
    0x009D07A6, // DSPDoaxn
    0x009E16E6, // DSPDSaoxx
    0x009F0345, // PDSxan
    0x00A000C9, // DPa
    0x00A11B05, // PDSPnaoxn
    0x00A20E09, // DPSnoa
    0x00A30669, // DPSDxoxn
    0x00A41885, // PDSPonoxn
    0x00A50065, // PDxn
    0x00A60706, // DSPnax
    0x00A707A5, // PDSPoaxn
    0x00A803A9, // DPSoa
    0x00A90189, // DPSoxn
    0x00AA0029, // D
    0x00AB0889, // DPSono
    0x00AC0744, // SPDSxax
    0x00AD06E9, // DPSDaoxn
    0x00AE0B06, // DSPnao
    0x00AF0229, // DPno
    0x00B00E05, // PDSnoa
    0x00B10665, // PDSPxoxn
    0x00B21974, // SSPxDSxox
    0x00B30CE8, // SDPanan
    0x00B4070A, // PSDnax
    0x00B507A9, // DPSDoaxn
    0x00B616E9, // DPSDPaoxx
    0x00B70348, // SDPxan
    0x00B8074A, // PSDPxax
    0x00B906E6, // DSPDaoxn
    0x00BA0B09, // DPSnao
    0x00BB0226, // DSno MERGEPAINT
    0x00BC1CE4, // SPDSanax
    0x00BD0D7D, // SDxPDxan
    0x00BE0269, // DPSxo
    0x00BF08C9, // DPSano
    0x00C000CA, // PSa MERGECOPY
    0x00C11B04, // SPDSnaoxn
    0x00C21884, // SPDSonoxn
    0x00C3006A, // PSxn
    0x00C40E04, // SPDnoa
    0x00C50664, // SPDSxoxn
    0x00C60708, // SDPnax
    0x00C707AA, // PSDPoaxn
    0x00C803A8, // SDPoa
    0x00C90184, // SPDoxn
    0x00CA0749, // DPSDxax
    0x00CB06E4, // SPDSaoxn
    0x00CC0020, // S SRCCOPY
    0x00CD0888, // SDPono
    0x00CE0B08, // SDPnao
    0x00CF0224, // SPno
    0x00D00E0A, // PSDnoa
    0x00D1066A, // PSDPxoxn
    0x00D20705, // PDSnax
    0x00D307A4, // SPDSoaxn
    0x00D41D78, // SSPxPDxax
    0x00D50CE9, // DPSanan
    0x00D616EA, // PSDPSaoxx
    0x00D70349, // DPSxan
    0x00D80745, // PDSPxax
    0x00D906E8, // SDPSaoxn
    0x00DA1CE9, // DPSDanax
    0x00DB0D75, // SPxDSxan
    0x00DC0B04, // SPDnao
    0x00DD0228, // SDno
    0x00DE0268, // SDPxo
    0x00DF08C8, // SDPano
    0x00E003A5, // PDSoa
    0x00E10185, // PDSoxn
    0x00E20746, // DSPDxax
    0x00E306EA, // PSDPaoxn
    0x00E40748, // SDPSxax
    0x00E506E5, // PDSPaoxn
    0x00E61CE8, // SDPSanax
    0x00E70D79, // SPxPDxan
    0x00E81D74, // SSPxDSxax
    0x00E95CE6, // DSPDSanaxxn
    0x00EA02E9, // DPSao
    0x00EB0849, // DPSxno
    0x00EC02E8, // SDPao
    0x00ED0848, // SDPxno
    0x00EE0086, // DSo SRCPAINT
    0x00EF0A08, // SDPnoo
    0x00F00021, // P PATCOPY
    0x00F10885, // PDSono
    0x00F20B05, // PDSnao
    0x00F3022A, // PSno
    0x00F40B0A, // PSDnao
    0x00F50225, // PDno
    0x00F60265, // PDSxo
    0x00F708C5, // PDSano
    0x00F802E5, // PDSao
    0x00F90845, // PDSxno
    0x00FA0089, // DPo
    0x00FB0A09, // DPSnoo PATPAINT
    0x00FC008A, // PSo
    0x00FD0A0A, // PSDnoo
    0x00FE02A9, // DPSoo
    0x00FF0062 // 1 WHITENESS
};

static void set_path(GdiCanvas *canvas, SpicePath *s)
{
    unsigned int i;

    for (i = 0; i < s->num_segments; i++) {
        SpicePathSeg* seg = s->segments[0];
        SpicePointFix* point = seg->points;
        SpicePointFix* end_point = point + seg->count;

        if (seg->flags & SPICE_PATH_BEGIN) {
            BeginPath(canvas->dc);
            if (!MoveToEx(canvas->dc, (int)fix_to_double(point->x), (int)fix_to_double(point->y),
                          NULL)) {
                spice_critical("MoveToEx failed");
                return;
            }
            point++;
        }

        if (seg->flags & SPICE_PATH_BEZIER) {
            spice_return_if_fail((point - end_point) % 3 == 0);
            for (; point + 2 < end_point; point += 3) {
                POINT points[3];

                points[0].x = (int)fix_to_double(point[0].x);
                points[0].y = (int)fix_to_double(point[0].y);
                points[1].x = (int)fix_to_double(point[1].x);
                points[1].y = (int)fix_to_double(point[1].y);
                points[2].x = (int)fix_to_double(point[2].x);
                points[2].y = (int)fix_to_double(point[2].y);
                if (!PolyBezierTo(canvas->dc, points, 3)) {
                    spice_critical("PolyBezierTo failed");
                    return;
                }
            }
        } else {
            for (; point < end_point; point++) {
                if (!LineTo(canvas->dc, (int)fix_to_double(point->x),
                            (int)fix_to_double(point->y))) {
                    spice_critical("LineTo failed");
                }
            }
        }

        if (seg->flags & SPICE_PATH_END) {

            if (seg->flags & SPICE_PATH_CLOSE) {
                if (!CloseFigure(canvas->dc)) {
                    spice_critical("CloseFigure failed");
                }
            }

            if (!EndPath(canvas->dc)) {
                spice_critical("EndPath failed");
            }
        }

    }
}

static void set_scale_mode(GdiCanvas *canvas, uint8_t scale_mode)
{
    if (scale_mode == SPICE_IMAGE_SCALE_MODE_INTERPOLATE) {
        SetStretchBltMode(canvas->dc, HALFTONE);
    } else if (scale_mode == SPICE_IMAGE_SCALE_MODE_NEAREST) {
        SetStretchBltMode(canvas->dc, COLORONCOLOR);
    } else {
        spice_critical("Unknown ScaleMode");
    }
}

static void set_clip(GdiCanvas *canvas, SpiceClip *clip)
{
    switch (clip->type) {
    case SPICE_CLIP_TYPE_NONE:
        if (SelectClipRgn(canvas->dc, NULL) == ERROR) {
            spice_critical("SelectClipRgn failed");
        }
        break;
    case SPICE_CLIP_TYPE_RECTS: {
        uint32_t n = clip->rects->num_rects;

        SpiceRect *now = clip->rects->rects;
        SpiceRect *end = now + n;

        if (now < end) {
            HRGN main_hrgn;

            main_hrgn = CreateRectRgn(now->left, now->top, now->right, now->bottom);
            if (!main_hrgn) {
                return;
            }
            now++;
            for (; now < end; now++) {
                HRGN combaine_hrgn;
                combaine_hrgn = CreateRectRgn(now->left, now->top, now->right,
                                              now->bottom);
                if (!combaine_hrgn) {
                    spice_critical("Unable to CreateRectRgn");
                    DeleteObject(main_hrgn);
                    return;
                }
                if (CombineRgn(main_hrgn, main_hrgn, combaine_hrgn, RGN_OR) == ERROR) {
                    spice_critical("Unable to CombineRgn");
                    DeleteObject(combaine_hrgn);
                    return;
                }
                DeleteObject(combaine_hrgn);
            }
            if (SelectClipRgn(canvas->dc, main_hrgn) == ERROR) {
                spice_critical("Unable to SelectClipRgn");
            }
            DeleteObject(main_hrgn);
        }
        break;
    }
    default:
        spice_warn_if_reached();
        return;
    }
}

static void copy_bitmap(const uint8_t *src_image, int height, int src_stride,
                        uint8_t *dest_bitmap, int dest_stride)
{
    int copy_width = MIN(dest_stride, src_stride);
    int y = 0;

    spice_return_if_fail(dest_stride >= 0 && src_stride >= 0);

    while (y < height) {
        memcpy(dest_bitmap, src_image, copy_width);
        src_image += src_stride;
        dest_bitmap += dest_stride;
        y++;
    }
}

static void copy_bitmap_alpha(const uint8_t *src_alpha, int height, int width, int src_stride,
                              uint8_t *dest_bitmap, int dest_stride, int alpha_bits_size)
{
    int y = 0;
    uint8_t i_offset;
    int i_count = 0;
    int i = 0;

    if (alpha_bits_size == 1) {
        i_offset = 1;
    } else {
        i_offset = 8;
    }


    while (y < height) {
        int x;

        for (x = 0; x < width; ++x) {
            uint8_t alphaval;
            double alpha;

            alphaval = src_alpha[i];
            alphaval = alphaval >> (i_count * i_offset);
            alphaval &= ((uint8_t)0xff >> (8 - i_offset));
            alphaval = ((255 * alphaval) / ((uint8_t)0xff >> (8 - i_offset)));

            dest_bitmap[x * 4 + 3] = alphaval;
            alpha = (double)alphaval / 0xff;
            dest_bitmap[x * 4 + 2] = (uint8_t)(alpha * dest_bitmap[x * 4 + 2]);
            dest_bitmap[x * 4 + 1] = (uint8_t)(alpha * dest_bitmap[x * 4 + 1]);
            dest_bitmap[x * 4] = (uint8_t)(alpha * dest_bitmap[x * 4]);

            i_count++;
            if (i_count == (8 / i_offset)) {
                i++;
                i_count = 0;
            }
        }

        dest_bitmap += width * 4;
        i = 0;
        src_alpha += src_stride;
        i_count = 0;
        y++;
    }
}

static uint8_t *create_bitmap(HBITMAP *bitmap, HBITMAP *prev_bitmap, HDC *dc,
                              const uint8_t *bitmap_data, int width, int height,
                              int stride, int bits, int rotate)
{
    uint8_t *data;
    const uint8_t *src_data;
    uint32_t nstride;
    struct {
        BITMAPINFO inf;
        RGBQUAD palette[255];
    } bitmap_info;

    memset(&bitmap_info, 0, sizeof(bitmap_info));
    bitmap_info.inf.bmiHeader.biSize = sizeof(bitmap_info.inf.bmiHeader);
    bitmap_info.inf.bmiHeader.biWidth = width;
    if (stride < 0) {
        bitmap_info.inf.bmiHeader.biHeight = height;
    } else {
        bitmap_info.inf.bmiHeader.biHeight = -height;
    }

    if (rotate) {
        bitmap_info.inf.bmiHeader.biHeight = -bitmap_info.inf.bmiHeader.biHeight;
    }

    bitmap_info.inf.bmiHeader.biPlanes = 1;
    bitmap_info.inf.bmiHeader.biBitCount = bits;
    bitmap_info.inf.bmiHeader.biCompression = BI_RGB;

    *dc = create_compatible_dc();
    if (!*dc) {
        spice_critical("create_compatible_dc() failed");
        return NULL;
    }

    *bitmap = CreateDIBSection(*dc, &bitmap_info.inf, 0, (VOID **)&data, NULL, 0);
    if (!*bitmap) {
        spice_critical("Unable to CreateDIBSection");
        DeleteDC(*dc);
        return NULL;
    }
    *prev_bitmap = (HBITMAP)SelectObject(*dc, *bitmap);

    if (stride < 0) {
        src_data = bitmap_data - (height - 1) * -stride;
    } else {
        src_data = bitmap_data;
    }

    switch (bits) {
    case 1:
        nstride = SPICE_ALIGN(width, 32) / 8;
        break;
    case 8:
        nstride = SPICE_ALIGN(width, 4);
        break;
    case 16:
        nstride = SPICE_ALIGN(width * 2, 4);
        break;
    case 32:
        nstride = width * 4;
        break;
    default:
        spice_warn_if_reached();
        return NULL;
    }

    if (bitmap_data) {
        if (stride < 0) {
            copy_bitmap(src_data, height, -stride, data, nstride);
        } else {
            copy_bitmap(src_data, height, stride, data, nstride);
        }
    }

    return data;
}

static uint8_t *create_bitmap_from_pixman(HBITMAP *bitmap, HBITMAP *prev_bitmap, HDC *dc,
                                          pixman_image_t *surface, int rotate)
{
    return create_bitmap(bitmap, prev_bitmap, dc,
                         (uint8_t*)pixman_image_get_data(surface),
                         pixman_image_get_width(surface),
                         pixman_image_get_height(surface),
                         pixman_image_get_stride(surface),
                         spice_pixman_image_get_bpp(surface),
                         rotate);
}


static void release_bitmap(HDC dc, HBITMAP bitmap, HBITMAP prev_bitmap, int cache)
{
    bitmap = (HBITMAP)SelectObject(dc, prev_bitmap);
    if (!cache) {
        DeleteObject(bitmap);
    }
    DeleteDC(dc);
}

static inline uint8_t get_converted_color(uint8_t color)
{
    uint8_t msb;

    msb = color & 0xE0;
    msb = msb >> 5;
    color |= msb;
    return color;
}

static inline COLORREF get_color_ref(GdiCanvas *canvas, uint32_t color)
{
    int shift = canvas->base.color_shift == 8 ? 0 : 3;
    uint8_t r, g, b;

    b = (color & canvas->base.color_mask);
    color >>= canvas->base.color_shift;
    g = (color & canvas->base.color_mask);
    color >>= canvas->base.color_shift;
    r = (color & canvas->base.color_mask);
    if (shift) {
        r = get_converted_color(r << shift);
        g = get_converted_color(g << shift);
        b = get_converted_color(b << shift);
    }
    return RGB(r, g, b);
}

static HBRUSH get_brush(GdiCanvas *canvas, SpiceBrush *brush, RecurciveMutex **brush_lock)
{
    HBRUSH hbrush;

    *brush_lock = NULL;

    switch (brush->type) {
    case SPICE_BRUSH_TYPE_SOLID:
        if (!(hbrush = CreateSolidBrush(get_color_ref(canvas, brush->u.color)))) {
            spice_critical("CreateSolidBrush failed");
            return NULL;
        }
        return hbrush;
    case SPICE_BRUSH_TYPE_PATTERN: {
        GdiCanvas *gdi_surface = NULL;
        HBRUSH hbrush;
        pixman_image_t *surface = NULL;
        HDC dc;
        HBITMAP bitmap;
        HBITMAP prev_bitmap;

        gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, brush->u.pattern.pat);
        if (gdi_surface) {
            bitmap = (HBITMAP)GetCurrentObject(gdi_surface->dc, OBJ_BITMAP);
            if (!bitmap) {
                spice_critical("GetCurrentObject failed");
                return NULL;
            }
            *brush_lock = gdi_surface->lock;
        } else {
            surface = canvas_get_image(&canvas->base, brush->u.pattern.pat, FALSE);
            if (!create_bitmap_from_pixman(&bitmap, &prev_bitmap, &dc, surface, 0)) {
                spice_critical("create_bitmap failed");
                return NULL;
            }
        }

        if (!(hbrush = CreatePatternBrush(bitmap))) {
            spice_critical("CreatePatternBrush failed");
            return NULL;
        }

        if (!gdi_surface) {
            release_bitmap(dc, bitmap, prev_bitmap, 0);
            pixman_image_unref(surface);
        }
        return hbrush;
    }
    case SPICE_BRUSH_TYPE_NONE:
        return NULL;
    default:
        spice_warn_if_reached();
        return NULL;
    }
}

static HBRUSH set_brush(HDC dc, HBRUSH hbrush, SpiceBrush *brush)
{
    switch (brush->type) {
    case SPICE_BRUSH_TYPE_SOLID: {
        return (HBRUSH)SelectObject(dc, hbrush);
    }
    case SPICE_BRUSH_TYPE_PATTERN: {
        HBRUSH prev_hbrush;
        prev_hbrush = (HBRUSH)SelectObject(dc, hbrush);
        if (!SetBrushOrgEx(dc, brush->u.pattern.pos.x, brush->u.pattern.pos.y, NULL)) {
            spice_critical("SetBrushOrgEx failed");
            return NULL;
        }
        return prev_hbrush;
    }
    default:
        spice_warn_if_reached();
        return NULL;
    }
}

static void unset_brush(HDC dc, HBRUSH prev_hbrush)
{
    if (!prev_hbrush) {
        return;
    }
    prev_hbrush = (HBRUSH)SelectObject(dc, prev_hbrush);
    DeleteObject(prev_hbrush);
}

uint8_t calc_rop3(uint16_t rop3_bits, int brush)
{
    uint8_t rop3 = 0;
    uint8_t rop3_src = _rop3_src;
    uint8_t rop3_dest = _rop3_dest;
    uint8_t rop3_brush = _rop3_brush;
    uint8_t rop3_src_brush;

    if (rop3_bits & SPICE_ROPD_INVERS_SRC) {
        rop3_src = ~rop3_src;
    }
    if (rop3_bits & SPICE_ROPD_INVERS_BRUSH) {
        rop3_brush = ~rop3_brush;
    }
    if (rop3_bits & SPICE_ROPD_INVERS_DEST) {
        rop3_dest = ~rop3_dest;
    }

    if (brush) {
        rop3_src_brush = rop3_brush;
    } else {
        rop3_src_brush = rop3_src;
    }

    if (rop3_bits & SPICE_ROPD_OP_PUT) {
        rop3 = rop3_src_brush;
    }
    if (rop3_bits & SPICE_ROPD_OP_OR) {
        rop3 = rop3_src_brush | rop3_dest;
    }
    if (rop3_bits & SPICE_ROPD_OP_AND) {
        rop3 = rop3_src_brush & rop3_dest;
    }
    if (rop3_bits & SPICE_ROPD_OP_XOR) {
        rop3 = rop3_src_brush ^ rop3_dest;
    }
    if (rop3_bits & SPICE_ROPD_INVERS_RES) {
        rop3 = ~rop3_dest;
    }

    if (rop3_bits & SPICE_ROPD_OP_BLACKNESS || rop3_bits & SPICE_ROPD_OP_WHITENESS ||
        rop3_bits & SPICE_ROPD_OP_INVERS) {
        spice_warning("invalid rop3 type");
        return 0;
    }
    return rop3;
}

uint8_t calc_rop3_src_brush(uint16_t rop3_bits)
{
    uint8_t rop3 = 0;
    uint8_t rop3_src = _rop3_src;
    uint8_t rop3_brush = _rop3_brush;

    if (rop3_bits & SPICE_ROPD_INVERS_SRC) {
        rop3_src = ~rop3_src;
    }
    if (rop3_bits & SPICE_ROPD_INVERS_BRUSH) {
        rop3_brush = ~rop3_brush;
    }

    if (rop3_bits & SPICE_ROPD_OP_OR) {
        rop3 = rop3_src | rop3_brush;
    }
    if (rop3_bits & SPICE_ROPD_OP_AND) {
        rop3 = rop3_src & rop3_brush;
    }
    if (rop3_bits & SPICE_ROPD_OP_XOR) {
        rop3 = rop3_src ^ rop3_brush;
    }

    return rop3;
}

static struct BitmapData get_mask_bitmap(struct GdiCanvas *canvas, struct SpiceQMask *mask)
{
    GdiCanvas *gdi_surface;
    pixman_image_t *surface;
    struct BitmapData bitmap;
    PixmanData *pixman_data;

    bitmap.hbitmap = NULL;
    if (!mask->bitmap) {
        return bitmap;
    }

    gdi_surface = (GdiCanvas *)canvas_get_surface_mask(&canvas->base, mask->bitmap);
    if (gdi_surface) {
        HBITMAP _bitmap;

        _bitmap = (HBITMAP)GetCurrentObject(gdi_surface->dc, OBJ_BITMAP);
        if (!_bitmap) {
            spice_critical("GetCurrentObject failed");
            return bitmap;
        }
        bitmap.dc = gdi_surface->dc;
        bitmap.hbitmap = _bitmap;
        bitmap.prev_hbitmap = (HBITMAP)0;
        bitmap.cache = 0;
        bitmap.from_surface = 1;
    } else {

        if (!(surface = canvas_get_mask(&canvas->base, mask, NULL))) {
            return bitmap;
        }

        pixman_data = (PixmanData *)pixman_image_get_destroy_data (surface);
        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            bitmap.dc = create_compatible_dc();
            bitmap.prev_hbitmap = (HBITMAP)SelectObject(bitmap.dc, pixman_data->bitmap);
            bitmap.hbitmap = pixman_data->bitmap;
            ReleaseMutex(pixman_data->mutex);
            bitmap.cache = 1;
        } else if (!create_bitmap_from_pixman(&bitmap.hbitmap, &bitmap.prev_hbitmap, &bitmap.dc,
                                               surface, 0)) {
            bitmap.hbitmap = NULL;
        } else {
            bitmap.cache = 0;
        }

        bitmap.from_surface = 0;
    }

    bitmap.flags = mask->flags;
    bitmap.pos = mask->pos;

    return bitmap;
}

static void gdi_draw_bitmap(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                            HDC src_dc, struct BitmapData *bitmapmask, uint32_t rop3_val)
{
    uint32_t rast_oper;

    rast_oper = raster_ops[rop3_val];

    if (!bitmapmask || !bitmapmask->hbitmap) {
        if ((dest->right - dest->left) == (src->right - src->left) &&
                                           (dest->bottom - dest->top) == (src->bottom - src->top)) {
            if (!BitBlt(dest_dc, dest->left, dest->top, dest->right - dest->left,
                        dest->bottom - dest->top, src_dc, src->left, src->top, rast_oper)) {
                spice_critical("BitBlt failed");
                return;
            }
        } else {
            if (!StretchBlt(dest_dc, dest->left, dest->top, dest->right - dest->left,
                            dest->bottom - dest->top, src_dc, src->left, src->top,
                            src->right - src->left, src->bottom - src->top, rast_oper)) {
                spice_critical("StretchBlt failed");
                return;
            }
        }
    } else {
        rast_oper = MAKEROP4(rast_oper, raster_ops[_rop3_dest]);

        if (!MaskBlt(dest_dc, dest->left, dest->top, dest->right - dest->left,
                     dest->bottom - dest->top, src_dc, src->left, src->top,
                     bitmapmask->hbitmap, bitmapmask->pos.x, bitmapmask->pos.y,
                     rast_oper)) {
            spice_critical("MaskBlt failed");
            return;
        }
    }
}

static void gdi_draw_bitmap_redrop(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                                   HDC src_dc, struct BitmapData *bitmapmask,
                                   uint16_t rop, int brush)
{
    uint32_t rop3_val;

    rop3_val = calc_rop3(rop, brush);
    gdi_draw_bitmap(dest_dc, src, dest, src_dc, bitmapmask, rop3_val);
}

static void free_mask(struct BitmapData *bitmap)
{
    if (bitmap->hbitmap) {
        if (!bitmap->from_surface) {
            release_bitmap(bitmap->dc, bitmap->hbitmap, bitmap->prev_hbitmap, bitmap->cache);
        }
    }
}

static void draw_str_mask_bitmap(struct GdiCanvas *canvas,
                                 SpiceString *str, int n, SpiceRect *dest,
                                 SpiceRect *src, SpiceBrush *brush)
{
    pixman_image_t *surface;
    struct BitmapData bitmap;
    SpicePoint pos;
    int dest_stride;
    uint8_t *bitmap_data;
    HBRUSH prev_hbrush;
    HBRUSH hbrush;
    RecurciveMutex *brush_lock;

    bitmap.hbitmap = (HBITMAP)1;
    if (!(surface = canvas_get_str_mask(&canvas->base, str, n, &pos))) {
        spice_critical("unable to canvas_get_str_mask");
        return;
    }

    bitmap.from_surface = 0;
    bitmap.cache = 0;
    bitmap_data = create_bitmap(&bitmap.hbitmap, &bitmap.prev_hbitmap,
                                &bitmap.dc, NULL,
                                pixman_image_get_width(surface),
                                pixman_image_get_height(surface),
                                pixman_image_get_stride(surface), 32, 0);

    if (!bitmap_data) {
        return;
    }

    bitmap.flags = 0;
    bitmap.pos.x = 0;
    bitmap.pos.y = 0;

    dest->left = pos.x;
    dest->top = pos.y;
    dest->right = pos.x + pixman_image_get_width(surface);
    dest->bottom = pos.y + pixman_image_get_height(surface);
    src->left = 0;
    src->top = 0;
    src->right = pixman_image_get_width(surface);
    src->bottom = pixman_image_get_height(surface);

    dest_stride = pixman_image_get_width(surface);
    switch (n) {
    case 1:
        dest_stride = dest_stride / 8;
        break;
    case 4:
        dest_stride = dest_stride / 2;
        break;
    case 32:
        dest_stride = dest_stride * 4;
        break;
    default:
        spice_warn_if_reached();
        return;
    }
    dest_stride = dest_stride + 3;
    dest_stride = dest_stride & ~3;

    hbrush = get_brush(canvas, brush, &brush_lock);
    prev_hbrush = set_brush(bitmap.dc, hbrush, brush);
    if (brush_lock) {
        RecurciveLock b_lock(*brush_lock);
        gdi_draw_bitmap(bitmap.dc, src, src, bitmap.dc, NULL, _rop3_brush);
    } else {
        gdi_draw_bitmap(bitmap.dc, src, src, bitmap.dc, NULL, _rop3_brush);
    }

    unset_brush(bitmap.dc, prev_hbrush);

    copy_bitmap_alpha((uint8_t *)pixman_image_get_data(surface),
                      pixman_image_get_height(surface),
                      pixman_image_get_width(surface),
                      pixman_image_get_stride(surface),
                      bitmap_data, dest_stride, n);

    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

    RecurciveLock lock(*canvas->lock);
    AlphaBlend(canvas->dc, dest->left, dest->top, dest->right - dest->left,
               dest->bottom - dest->top, bitmap.dc, src->left, src->top,
               src->right - src->left, src->bottom - src->top, bf);

    free_mask(&bitmap);
}

static void gdi_draw_image(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                           pixman_image_t *image, struct BitmapData *bitmapmask, uint16_t rop,
                           int rotate)
{
    HDC dc;
    HBITMAP bitmap;
    HBITMAP prev_bitmap;

    create_bitmap_from_pixman(&bitmap, &prev_bitmap, &dc, image, rotate);

    gdi_draw_bitmap_redrop(dest_dc, src, dest, dc, bitmapmask, rop, 0);

    release_bitmap(dc, bitmap, prev_bitmap, 0);
}

static void gdi_draw_image_rop3(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                                pixman_image_t *image, struct BitmapData *bitmapmask, uint8_t rop3,
                                int rotate)
{
    HDC dc;
    HBITMAP bitmap;
    HBITMAP prev_bitmap;

    create_bitmap_from_pixman(&bitmap, &prev_bitmap, &dc, image, rotate);

    gdi_draw_bitmap(dest_dc, src, dest, dc, bitmapmask, rop3);

    release_bitmap(dc, bitmap, prev_bitmap, 0);
}

static void gdi_canvas_draw_fill(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceFill *fill)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    HBRUSH prev_hbrush;
    HBRUSH brush;
    struct BitmapData bitmapmask;
    RecurciveMutex *brush_lock;

    RecurciveLock lock(*canvas->lock);

    brush = get_brush(canvas, &fill->brush, &brush_lock);
    spice_return_if_fail(brush != NULL);

    bitmapmask = get_mask_bitmap(canvas, &fill->mask);

    set_clip(canvas, clip);
    prev_hbrush = set_brush(canvas->dc, brush, &fill->brush);
    if (brush_lock) {
        RecurciveLock b_lock(*brush_lock);
        gdi_draw_bitmap_redrop(canvas->dc, bbox, bbox, canvas->dc, &bitmapmask,
                               fill->rop_descriptor, fill->brush.type != SPICE_BRUSH_TYPE_NONE);
    } else {
        gdi_draw_bitmap_redrop(canvas->dc, bbox, bbox, canvas->dc, &bitmapmask,
                               fill->rop_descriptor, fill->brush.type != SPICE_BRUSH_TYPE_NONE);
    }

    free_mask(&bitmapmask);
    unset_brush(canvas->dc, prev_hbrush);
}

static void gdi_canvas_draw_copy(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceCopy *copy)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    GdiCanvas *gdi_surface;
    pixman_image_t *surface;
    struct BitmapData bitmapmask;
    PixmanData *pixman_data;

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, copy->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        bitmapmask = get_mask_bitmap(canvas, &copy->mask);
        set_scale_mode(canvas, copy->scale_mode);
        set_clip(canvas, clip);
        gdi_draw_bitmap_redrop(canvas->dc, &copy->src_area, bbox, gdi_surface->dc,
                               &bitmapmask, copy->rop_descriptor, 0);
    } else {
        surface = canvas_get_image(&canvas->base, copy->src_bitmap, FALSE);
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);

        RecurciveLock lock(*canvas->lock);
        bitmapmask = get_mask_bitmap(canvas, &copy->mask);
        set_scale_mode(canvas, copy->scale_mode);
        set_clip(canvas, clip);

        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            gdi_draw_bitmap_redrop(canvas->dc, &copy->src_area, bbox, dc,
                                   &bitmapmask, copy->rop_descriptor, 0);
            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            gdi_draw_image(canvas->dc, &copy->src_area, bbox, surface, &bitmapmask,
                           copy->rop_descriptor, 0);
        }

        pixman_image_unref(surface);
    }
    free_mask(&bitmapmask);
}

static void gdi_canvas_put_image(SpiceCanvas *spice_canvas, HDC dc, const SpiceRect *dest, const uint8_t *src_data,
                                 uint32_t src_width, uint32_t src_height, int src_stride,
                                 const QRegion *clip)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    SpiceRect src;
    src.top = 0;
    src.bottom = src_height;
    src.left = 0;
    src.right = src_width;
    int num_rects;
    pixman_box32_t *rects;

    RecurciveLock lock(*canvas->lock);
    set_scale_mode(canvas, SPICE_IMAGE_SCALE_MODE_NEAREST);
    if (clip) {
        rects = pixman_region32_rectangles((pixman_region32_t*)clip, &num_rects);
        if (num_rects == 0) {
            return;
        } else {
            HRGN main_hrgn;
            int i;

            main_hrgn = CreateRectRgn(rects[0].x1, rects[0].y1, rects[0].x2,
                                      rects[0].y2);
            if (!main_hrgn) {
                return;
            }

            for (i = 1; i < num_rects; i++) {
                HRGN combaine_hrgn;

                combaine_hrgn = CreateRectRgn(rects[i].x1, rects[i].y1,
                                              rects[i].x2,
                                              rects[i].y2);
                if (!combaine_hrgn) {
                    spice_critical("CreateRectRgn failed");
                    DeleteObject(main_hrgn);
                    return;
                }
                if (!CombineRgn(main_hrgn, main_hrgn, combaine_hrgn, RGN_OR)) {
                    spice_critical("CombineRgn failed in put_image");
                    return;
                }
                DeleteObject(combaine_hrgn);
            }
            if (SelectClipRgn(canvas->dc, main_hrgn) == ERROR) {
                spice_critical("SelectClipRgn failed in put_image");
                DeleteObject(main_hrgn);
                return;
            }
            DeleteObject(main_hrgn);
        }
    } else {
        SelectClipRgn(canvas->dc, NULL);
    }

    if (dc) {
        gdi_draw_bitmap_redrop(canvas->dc, &src, dest, dc,
                               NULL, SPICE_ROPD_OP_PUT, 0);
    } else {
        pixman_image_t *image = pixman_image_create_bits(PIXMAN_a8r8g8b8, src_width, src_height,
                                                         (uint32_t *)src_data, src_stride);
        gdi_draw_image(canvas->dc, &src, dest, image, NULL, SPICE_ROPD_OP_PUT, 0);
        pixman_image_unref(image);
    }
}

static void gdi_draw_bitmap_transparent(GdiCanvas *canvas, HDC dest_dc, const SpiceRect *src,
                                        const SpiceRect *dest, HDC src_dc, uint32_t color)
{
    TransparentBlt(dest_dc, dest->left, dest->top, dest->right - dest->left,
                   dest->bottom - dest->top, src_dc, src->left, src->top,
                   src->right - src->left, src->bottom - src->top,
                   RGB(((uint8_t*)&color)[2], ((uint8_t*)&color)[1], ((uint8_t*)&color)[0]));
}

static void gdi_draw_image_transparent(GdiCanvas *canvas, HDC dest_dc, const SpiceRect *src,
                                       const SpiceRect *dest, pixman_image_t *image,
                                       uint32_t color, int rotate)
{
    HDC dc;
    HBITMAP bitmap;
    HBITMAP prev_bitmap;

    create_bitmap_from_pixman(&bitmap, &prev_bitmap, &dc, image, rotate);

    gdi_draw_bitmap_transparent(canvas, dest_dc, src, dest, dc, color);

    release_bitmap(dc, bitmap, prev_bitmap, 0);
}

static void gdi_canvas_draw_transparent(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip,
                                        SpiceTransparent* transparent)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    GdiCanvas *gdi_surface;
    pixman_image_t *surface;
    PixmanData *pixman_data;

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, transparent->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        set_clip(canvas, clip);
        gdi_draw_bitmap_transparent(canvas, canvas->dc, &transparent->src_area, bbox,
                                    gdi_surface->dc, transparent->true_color);
    } else {
        surface = canvas_get_image(&canvas->base, transparent->src_bitmap, FALSE);
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);
        RecurciveLock lock(*canvas->lock);
        set_clip(canvas, clip);
        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            gdi_draw_bitmap_transparent(canvas, canvas->dc, &transparent->src_area, bbox, dc,
                                        transparent->true_color);

            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            gdi_draw_image_transparent(canvas, canvas->dc, &transparent->src_area, bbox, surface,
                                       transparent->true_color, 0);
        }

        pixman_image_unref(surface);
    }
}

static void gdi_draw_bitmap_alpha(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                                  HDC src_dc, uint8_t alpha, int use_bitmap_alpha)
{
    BLENDFUNCTION bf;

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = alpha;

    if (use_bitmap_alpha) {
        bf.AlphaFormat = AC_SRC_ALPHA;
    } else {
        bf.AlphaFormat = 0;
    }

    if (!AlphaBlend(dest_dc, dest->left, dest->top, dest->right - dest->left,
                    dest->bottom - dest->top, src_dc, src->left, src->top,
                    src->right - src->left, src->bottom - src->top, bf)) {
        spice_critical("AlphaBlend failed");
        return;
    }
}

static void gdi_draw_image_alpha(HDC dest_dc, const SpiceRect *src, const SpiceRect *dest,
                                 pixman_image_t *image, uint8_t alpha,
                                 int rotate, int use_bitmap_alpha)
{
    HDC dc;
    HBITMAP bitmap;
    HBITMAP prev_bitmap;

    create_bitmap_from_pixman(&bitmap, &prev_bitmap, &dc, image, rotate);

    gdi_draw_bitmap_alpha(dest_dc, src, dest, dc, alpha, use_bitmap_alpha);

    release_bitmap(dc, bitmap, prev_bitmap, 0);
}

static void gdi_canvas_draw_alpha_blend(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceAlphaBlend* alpha_blend)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    GdiCanvas *gdi_surface;
    pixman_image_t *surface;
    PixmanData *pixman_data;
    int use_bitmap_alpha;

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, alpha_blend->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        set_clip(canvas, clip);
        use_bitmap_alpha = alpha_blend->alpha_flags & SPICE_ALPHA_FLAGS_SRC_SURFACE_HAS_ALPHA;
        gdi_draw_bitmap_alpha(canvas->dc, &alpha_blend->src_area, bbox, gdi_surface->dc,
                              alpha_blend->alpha, use_bitmap_alpha);
    } else {
        surface = canvas_get_image(&canvas->base, alpha_blend->src_bitmap, TRUE);
        use_bitmap_alpha = pixman_image_get_depth(surface) == 32;
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);

        RecurciveLock lock(*canvas->lock);
        set_clip(canvas, clip);
        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            gdi_draw_bitmap_alpha(canvas->dc, &alpha_blend->src_area, bbox, dc, alpha_blend->alpha,
                                  use_bitmap_alpha);
            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            gdi_draw_image_alpha(canvas->dc, &alpha_blend->src_area, bbox, surface,
                                 alpha_blend->alpha, 0, use_bitmap_alpha);
        }

        pixman_image_unref(surface);
    }
}

static void gdi_canvas_draw_opaque(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceOpaque *opaque)
{
    GdiCanvas *gdi_surface;
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    pixman_image_t *surface;
    struct BitmapData bitmapmask;
    PixmanData *pixman_data;
    HBRUSH prev_hbrush;
    HBRUSH hbrush;
    uint8_t rop3;
    RecurciveMutex *brush_lock;

    rop3 = calc_rop3_src_brush(opaque->rop_descriptor);

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, opaque->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        bitmapmask = get_mask_bitmap(canvas, &opaque->mask);
        hbrush = get_brush(canvas, &opaque->brush, &brush_lock);
        set_scale_mode(canvas, opaque->scale_mode);
        set_clip(canvas, clip);
        prev_hbrush = set_brush(canvas->dc, hbrush, &opaque->brush);
        if (brush_lock) {
            RecurciveLock b_lock(*brush_lock);
            gdi_draw_bitmap(canvas->dc, &opaque->src_area, bbox, gdi_surface->dc, &bitmapmask, rop3);
        } else {
            gdi_draw_bitmap(canvas->dc, &opaque->src_area, bbox, gdi_surface->dc, &bitmapmask, rop3);
        }
        unset_brush(canvas->dc, prev_hbrush);
    } else {
        surface = canvas_get_image(&canvas->base, opaque->src_bitmap, FALSE);
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);

        RecurciveLock lock(*canvas->lock);
        bitmapmask = get_mask_bitmap(canvas, &opaque->mask);
        hbrush = get_brush(canvas, &opaque->brush, &brush_lock);
        set_scale_mode(canvas, opaque->scale_mode);
        set_clip(canvas, clip);
        prev_hbrush = set_brush(canvas->dc, hbrush, &opaque->brush);

        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            if (brush_lock) {
                RecurciveLock b_lock(*brush_lock);
                gdi_draw_bitmap(canvas->dc, &opaque->src_area, bbox, dc, &bitmapmask, rop3);
            } else {
                gdi_draw_bitmap(canvas->dc, &opaque->src_area, bbox, dc, &bitmapmask, rop3);
            }
            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            if (brush_lock) {
                RecurciveLock b_lock(*brush_lock);
                gdi_draw_image_rop3(canvas->dc, &opaque->src_area, bbox, surface, &bitmapmask, rop3, 0);
            } else {
                gdi_draw_image_rop3(canvas->dc, &opaque->src_area, bbox, surface, &bitmapmask, rop3, 0);
            }
        }
        unset_brush(canvas->dc, prev_hbrush);
        pixman_image_unref(surface);
    }

    free_mask(&bitmapmask);
}

static void gdi_canvas_draw_blend(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlend *blend)
{
    GdiCanvas *gdi_surface;
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    pixman_image_t *surface;
    struct BitmapData bitmapmask;
    PixmanData *pixman_data;

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, blend->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        bitmapmask = get_mask_bitmap(canvas, &blend->mask);
        set_scale_mode(canvas, blend->scale_mode);
        set_clip(canvas, clip);
        gdi_draw_bitmap_redrop(canvas->dc, &blend->src_area, bbox, gdi_surface->dc,
                               &bitmapmask, blend->rop_descriptor, 0);
    }  else {
        surface = canvas_get_image(&canvas->base, blend->src_bitmap, FALSE);
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);

        RecurciveLock lock(*canvas->lock);
        bitmapmask = get_mask_bitmap(canvas, &blend->mask);
        set_scale_mode(canvas, blend->scale_mode);
        set_clip(canvas, clip);

        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            gdi_draw_bitmap_redrop(canvas->dc, &blend->src_area, bbox, dc,
                                   &bitmapmask, blend->rop_descriptor, 0);
            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            gdi_draw_image(canvas->dc, &blend->src_area, bbox, surface,
                           &bitmapmask, blend->rop_descriptor, 0);
        }

        pixman_image_unref(surface);
    }

    free_mask(&bitmapmask);
}

static void gdi_canvas_draw_blackness(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceBlackness *blackness)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    struct BitmapData bitmapmask;

    RecurciveLock lock(*canvas->lock);
    bitmapmask = get_mask_bitmap(canvas, &blackness->mask);
    set_clip(canvas, clip);
    gdi_draw_bitmap(canvas->dc, bbox, bbox, canvas->dc, &bitmapmask, 0x0);

    free_mask(&bitmapmask);
}

static void gdi_canvas_draw_invers(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceInvers *invers)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    struct BitmapData bitmapmask;

    RecurciveLock lock(*canvas->lock);
    bitmapmask = get_mask_bitmap(canvas, &invers->mask);
    set_clip(canvas, clip);
    gdi_draw_bitmap(canvas->dc, bbox, bbox, canvas->dc, &bitmapmask, 0x55);

    free_mask(&bitmapmask);
}

static void gdi_canvas_draw_whiteness(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceWhiteness *whiteness)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    struct BitmapData bitmapmask;

    RecurciveLock lock(*canvas->lock);
    bitmapmask = get_mask_bitmap(canvas, &whiteness->mask);
    set_clip(canvas, clip);
    gdi_draw_bitmap(canvas->dc, bbox, bbox, canvas->dc, &bitmapmask, 0xff);

    free_mask(&bitmapmask);
}

static void gdi_canvas_draw_rop3(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceRop3 *rop3)
{
    GdiCanvas *gdi_surface;
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    pixman_image_t *surface;
    struct BitmapData bitmapmask;
    HBRUSH prev_hbrush;
    HBRUSH hbrush;
    PixmanData *pixman_data;
    RecurciveMutex *brush_lock;

    gdi_surface = (GdiCanvas *)canvas_get_surface(&canvas->base, rop3->src_bitmap);
    if (gdi_surface) {
        RecurciveLock lock(*canvas->lock);
        RecurciveLock s_lock(*gdi_surface->lock);
        hbrush = get_brush(canvas, &rop3->brush, &brush_lock);
        bitmapmask = get_mask_bitmap(canvas, &rop3->mask);
        set_scale_mode(canvas, rop3->scale_mode);
        set_clip(canvas, clip);
        prev_hbrush = set_brush(canvas->dc, hbrush, &rop3->brush);
        if (brush_lock) {
            RecurciveLock b_lock(*brush_lock);
            gdi_draw_bitmap(canvas->dc, &rop3->src_area, bbox, gdi_surface->dc,
                            &bitmapmask, rop3->rop3);
        } else {
            gdi_draw_bitmap(canvas->dc, &rop3->src_area, bbox, gdi_surface->dc,
                            &bitmapmask, rop3->rop3);
        }
        unset_brush(canvas->dc, prev_hbrush);
    } else {
        surface = canvas_get_image(&canvas->base, rop3->src_bitmap, FALSE);
        pixman_data = (PixmanData *)pixman_image_get_destroy_data(surface);
        RecurciveLock lock(*canvas->lock);
        hbrush = get_brush(canvas, &rop3->brush, &brush_lock);
        bitmapmask = get_mask_bitmap(canvas, &rop3->mask);
        set_scale_mode(canvas, rop3->scale_mode);
        set_clip(canvas, clip);
        prev_hbrush = set_brush(canvas->dc, hbrush, &rop3->brush);

        if (pixman_data && (WaitForSingleObject(pixman_data->mutex, INFINITE) != WAIT_FAILED)) {
            HDC dc;
            HBITMAP prev_bitmap;

            dc = create_compatible_dc();
            prev_bitmap = (HBITMAP)SelectObject(dc, pixman_data->bitmap);
            if (brush_lock) {
                RecurciveLock b_lock(*brush_lock);
                gdi_draw_bitmap(canvas->dc, &rop3->src_area, bbox, dc,
                                &bitmapmask, rop3->rop3);
            } else {
                gdi_draw_bitmap(canvas->dc, &rop3->src_area, bbox, dc,
                                &bitmapmask, rop3->rop3);
            }
            SelectObject(dc, prev_bitmap);
            DeleteObject(dc);
            ReleaseMutex(pixman_data->mutex);
        } else {
            if (brush_lock) {
                RecurciveLock b_lock(*brush_lock);
                gdi_draw_image_rop3(canvas->dc, &rop3->src_area, bbox, surface, &bitmapmask, rop3->rop3, 0);
            } else {
                gdi_draw_image_rop3(canvas->dc, &rop3->src_area, bbox, surface, &bitmapmask, rop3->rop3, 0);
            }
        }

        unset_brush(canvas->dc, prev_hbrush);
        pixman_image_unref(surface);
    }

    free_mask(&bitmapmask);
}

static void gdi_canvas_copy_bits(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpicePoint *src_pos)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    RecurciveLock lock(*canvas->lock);

    set_clip(canvas, clip);

    BitBlt(canvas->dc, bbox->left, bbox->top, bbox->right - bbox->left,
           bbox->bottom - bbox->top, canvas->dc, src_pos->x, src_pos->y, SRCCOPY);
}

static void gdi_canvas_draw_text(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceText *text)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    SpiceString *str;
    RecurciveMutex *brush_lock;

    RecurciveLock lock(*canvas->lock);
    set_clip(canvas, clip);
    lock.unlock();

    if (!rect_is_empty(&text->back_area)) {
        HBRUSH prev_hbrush;
        HBRUSH hbrush;

        RecurciveLock lock(*canvas->lock);
        hbrush = get_brush(canvas, &text->back_brush, &brush_lock);
        prev_hbrush = set_brush(canvas->dc, hbrush, &text->back_brush);
        if (brush_lock) {
            RecurciveLock b_lock(*brush_lock);
            gdi_draw_bitmap_redrop(canvas->dc, bbox, bbox, canvas->dc, NULL,
                                   text->back_mode, 1);
        } else {
            gdi_draw_bitmap_redrop(canvas->dc, bbox, bbox, canvas->dc, NULL,
                                   text->back_mode, 1);
        }
        unset_brush(canvas->dc, prev_hbrush);
    }

    str = (SpiceString *)SPICE_GET_ADDRESS(text->str);

    if (str->flags & SPICE_STRING_FLAGS_RASTER_A1) {
        SpiceRect dest;
        SpiceRect src;

        draw_str_mask_bitmap(canvas, str, 1, &dest, &src, &text->fore_brush);
    } else if (str->flags & SPICE_STRING_FLAGS_RASTER_A4) {
        SpiceRect dest;
        SpiceRect src;

        draw_str_mask_bitmap(canvas, str, 4, &dest, &src, &text->fore_brush);
    } else if (str->flags & SPICE_STRING_FLAGS_RASTER_A8) {
        spice_warning("untested path A8 glyphs, doing nothing");
        if (0) {
            SpiceRect dest;
            SpiceRect src;

            draw_str_mask_bitmap(canvas, str, 8, &dest, &src, &text->fore_brush);
        }
    } else {
        spice_warning("untested path vector glyphs, doing nothing");
        if (0) {
        }
    }
}

static uint32_t *gdi_get_userstyle(GdiCanvas *canvas, uint8_t nseg, SPICE_FIXED28_4* style, int start_is_gap)
{
    uint32_t *local_style;
    int i;

    spice_return_val_if_fail(nseg != 0, NULL);

    local_style = spice_new(uint32_t , nseg);

    if (start_is_gap) {
        local_style[nseg - 1] = (uint32_t)fix_to_double(*style);
        style++;

        for (i = 0; i < nseg - 1; i++, style++) {
            local_style[i] = (uint32_t)fix_to_double(*style);
        }
    } else {
        for (i = 0; i < nseg; i++, style++) {
            local_style[i] = (uint32_t)fix_to_double(*style);
        }
    }

    return local_style;
}

static void gdi_canvas_draw_stroke(SpiceCanvas *spice_canvas, SpiceRect *bbox, SpiceClip *clip, SpiceStroke *stroke)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    HPEN hpen;
    HPEN prev_hpen;
    LOGBRUSH logbrush;
    uint32_t *user_style = NULL;
    pixman_image_t *surface = NULL;

    if (stroke->brush.type == SPICE_BRUSH_TYPE_PATTERN) {
        surface = canvas_get_image(&canvas->base, stroke->brush.u.pattern.pat, FALSE);
    }

    RecurciveLock lock(*canvas->lock);
    set_clip(canvas, clip);

    switch (stroke->fore_mode) {
    case SPICE_ROPD_OP_WHITENESS:
        SetROP2(canvas->dc, R2_WHITE);    //0
        break;
    case SPICE_ROPD_OP_BLACKNESS:
        SetROP2(canvas->dc, R2_BLACK);    //1
        break;
    case SPICE_ROPD_OP_INVERS:
        SetROP2(canvas->dc, R2_NOT);    //Dn
        break;
    case SPICE_ROPD_OP_PUT:
        SetROP2(canvas->dc, R2_COPYPEN);    //P
        break;
    case SPICE_ROPD_OP_OR:
        SetROP2(canvas->dc, R2_MERGEPEN);    //DPo
        break;
    case SPICE_ROPD_OP_XOR:
        SetROP2(canvas->dc, R2_XORPEN);    //DPx
        break;
    case SPICE_ROPD_OP_AND:
        SetROP2(canvas->dc, R2_MASKPEN);    //DPa
        break;
    case SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_PUT:    //Pn
        SetROP2(canvas->dc, R2_NOTCOPYPEN);
        break;
    case SPICE_ROPD_OP_XOR | SPICE_ROPD_INVERS_RES:
        SetROP2(canvas->dc, R2_NOTXORPEN);    //DPxn
        break;
    case SPICE_ROPD_OP_OR | SPICE_ROPD_INVERS_RES:
        SetROP2(canvas->dc, R2_NOTMERGEPEN);    //DPon
        break;
    case SPICE_ROPD_OP_AND | SPICE_ROPD_INVERS_RES:
        SetROP2(canvas->dc, R2_NOTMASKPEN);    //DPan
        break;
    case SPICE_ROPD_INVERS_DEST | SPICE_ROPD_OP_AND:
        SetROP2(canvas->dc, R2_MASKPENNOT);    //PDna
        break;
    case SPICE_ROPD_INVERS_BRUSH | SPICE_ROPD_OP_AND:
        SetROP2(canvas->dc, R2_MASKNOTPEN);    //DPna
        break;
    case SPICE_ROPD_OP_OR | SPICE_ROPD_INVERS_BRUSH:
        SetROP2(canvas->dc, R2_MERGENOTPEN);    //DPno
        break;
    case SPICE_ROPD_OP_OR | SPICE_ROPD_INVERS_DEST:
        SetROP2(canvas->dc, R2_MERGEPENNOT);    //PDno
        break;
    default:
        SetROP2(canvas->dc, R2_NOP);    //D
    }


    if (stroke->brush.type == SPICE_BRUSH_TYPE_SOLID) {
        logbrush.lbStyle = BS_SOLID | DIB_RGB_COLORS;
        logbrush.lbHatch = 0;
        logbrush.lbColor = get_color_ref(canvas, stroke->brush.u.color);
    } else if (stroke->brush.type == SPICE_BRUSH_TYPE_PATTERN) {
#if 0
        struct {
            BITMAPINFO inf;
            RGBQUAD palette[255];
        } bitmap_info;
        GdiImage image;
#endif
#if 0
        spice_return_if_fail(surface != NULL)
        surface_to_image(surface, &image);

        memset(&bitmap_info, 0, sizeof(bitmap_info));
        bitmap_info.inf.bmiHeader.biSize = sizeof(bitmap_info.inf.bmiHeader);
        bitmap_info.inf.bmiHeader.biWidth = image.width;
        if (image.stride < 0) {
            bitmap_info.inf.bmiHeader.biHeight = image.height;
        } else {
            bitmap_info.inf.bmiHeader.biHeight = -image.height;
        }
        bitmap_info.inf.bmiHeader.biPlanes = 1;
        bitmap_info.inf.bmiHeader.biBitCount = 32;
        bitmap_info.inf.bmiHeader.biCompression = BI_RGB;

        if (image.stride < 0) {
            logbrush.lbHatch = (LONG)GlobalAlloc(GMEM_MOVEABLE,
                                                 image.height * -image.stride + sizeof(BITMAPINFO));
            if (!logbrush.lbHatch) {
                spice_critical("GlobalAlloc failed");
                return;
            }
            copy_bitmap(image.pixels - (image.height - 1) * -image.stride,
                        image.height, -image.stride,
                        (uint8_t *)logbrush.lbHatch, image.width);
        } else {
            logbrush.lbHatch = (LONG)GlobalAlloc(GMEM_MOVEABLE,
                                                 image.height * image.stride + sizeof(BITMAPINFO));
            if (!logbrush.lbHatch) {
                spice_critical("GlobalAlloc failed");
                return;
            }
            copy_bitmap(image.pixels, image.height, image.stride,
                        (uint8_t *)logbrush.lbHatch, image.width);
        }

        memcpy((void *)logbrush.lbHatch, &bitmap_info.inf, sizeof(BITMAPINFO));

        logbrush.lbStyle = BS_DIBPATTERN | DIB_RGB_COLORS;
        logbrush.lbColor = 0;
#endif
        pixman_image_unref(surface);
    }

    if (stroke->attr.flags & SPICE_LINE_FLAGS_STYLED) {
        user_style = gdi_get_userstyle(canvas, stroke->attr.style_nseg,
                                       stroke->attr.style,
                                       !!(stroke->attr.flags & SPICE_LINE_FLAGS_START_WITH_GAP));
        hpen = ExtCreatePen(PS_COSMETIC | PS_USERSTYLE,
                            1,
                            &logbrush, stroke->attr.style_nseg, (DWORD *)user_style);
    } else {
        hpen = ExtCreatePen(PS_COSMETIC,
                            1,
                            &logbrush, 0, NULL);
    }
    prev_hpen = (HPEN)SelectObject(canvas->dc, hpen);

    set_path(canvas, stroke->path);

    StrokePath(canvas->dc);

    SelectObject(canvas->dc, prev_hpen);
    DeleteObject(hpen);

#if 0
    if (stroke->brush.type == SPICE_BRUSH_TYPE_PATTERN) {
        GlobalFree((HGLOBAL)logbrush.lbHatch);
    }
#endif

    free(user_style);
}

static void gdi_canvas_clear(SpiceCanvas *spice_canvas)
{
}

static void gdi_canvas_destroy(SpiceCanvas *spice_canvas)
{
    GdiCanvas *canvas = (GdiCanvas *)spice_canvas;
    if (!canvas) {
        return;
    }
    canvas_base_destroy(&canvas->base);
    free(canvas);
}

static int need_init = 1;
static SpiceCanvasOps gdi_canvas_ops;

SpiceCanvas *gdi_canvas_create(int width, int height,
                               HDC dc, RecurciveMutex* lock, uint32_t format
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
    GdiCanvas *canvas;

    if (need_init) {
        return NULL;
    }
    canvas = spice_new0(GdiCanvas, 1);
    canvas_base_init(&canvas->base, &gdi_canvas_ops,
                     width, height, format,
#ifdef SW_CANVAS_CACHE
                     bits_cache,
                     palette_cache,
#elif defined(SW_CANVAS_IMAGE_CACHE)
                     bits_cache,
#endif
                     surfaces,
                     glz_decoder,
                     jpeg_decoder,
                     zlib_decoder);
    canvas->dc = dc;
    canvas->lock = lock;
    return (SpiceCanvas *)canvas;
}

void gdi_canvas_init(void) //unsafe global function
{
    if (!need_init) {
        return;
    }
    need_init = 0;

    canvas_base_init_ops(&gdi_canvas_ops);
    gdi_canvas_ops.draw_fill = gdi_canvas_draw_fill;
    gdi_canvas_ops.draw_copy = gdi_canvas_draw_copy;
    gdi_canvas_ops.draw_opaque = gdi_canvas_draw_opaque;
    gdi_canvas_ops.copy_bits = gdi_canvas_copy_bits;
    gdi_canvas_ops.draw_text = gdi_canvas_draw_text;
    gdi_canvas_ops.draw_stroke = gdi_canvas_draw_stroke;
    gdi_canvas_ops.draw_rop3 = gdi_canvas_draw_rop3;
    gdi_canvas_ops.draw_blend = gdi_canvas_draw_blend;
    gdi_canvas_ops.draw_blackness = gdi_canvas_draw_blackness;
    gdi_canvas_ops.draw_whiteness = gdi_canvas_draw_whiteness;
    gdi_canvas_ops.draw_invers = gdi_canvas_draw_invers;
    gdi_canvas_ops.draw_transparent = gdi_canvas_draw_transparent;
    gdi_canvas_ops.draw_alpha_blend = gdi_canvas_draw_alpha_blend;
    gdi_canvas_ops.put_image = gdi_canvas_put_image;
    gdi_canvas_ops.clear = gdi_canvas_clear;
    gdi_canvas_ops.destroy = gdi_canvas_destroy;

    rop3_init();
}
