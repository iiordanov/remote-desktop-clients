/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef LINES_H
#define LINES_H

#include <stdlib.h>
#include <string.h>
#include <spice/macros.h>

#include "pixman_utils.h"
#include "draw.h"

SPICE_BEGIN_DECLS

typedef struct lineGC lineGC;

typedef struct {
    void (*FillSpans)(lineGC * pGC,
                      int num_spans, SpicePoint * points, int *widths,
                      int sorted, int foreground);
    void (*FillRects)(lineGC * pGC,
                      int nun_rects, pixman_rectangle32_t * rects,
                      int foreground);
} lineGCOps;

struct lineGC {
    int width;
    int height;
    unsigned char alu;
    unsigned short lineWidth;
    unsigned short dashOffset;
    unsigned short numInDashList;
    unsigned char *dash;
    unsigned int lineStyle:2;
    unsigned int capStyle:2;
    unsigned int joinStyle:2;
    lineGCOps *ops;
};

/* CoordinateMode for drawing routines */

#define CoordModeOrigin         0       /* relative to the origin */
#define CoordModePrevious       1       /* relative to previous point */

/* LineStyle */

#define LineSolid               0
#define LineOnOffDash           1
#define LineDoubleDash          2

/* capStyle */

#define CapNotLast              0
#define CapButt                 1
#define CapRound                2
#define CapProjecting           3

/* joinStyle */

#define JoinMiter               0
#define JoinRound               1
#define JoinBevel               2

extern void spice_canvas_zero_line(lineGC *pgc,
                                   int mode,
                                   int num_points,
                                   SpicePoint * points);
extern void spice_canvas_zero_dash_line(lineGC * pgc,
                                        int mode,
                                        int n_points,
                                        SpicePoint * points);
extern void spice_canvas_wide_dash_line(lineGC * pGC,
                                        int mode,
                                        int num_points,
                                        SpicePoint * points);
extern void spice_canvas_wide_line(lineGC *pGC,
                                   int mode,
                                   int num_points,
                                   SpicePoint * points);
extern int spice_canvas_clip_spans(pixman_region32_t *clip_region,
                                   SpicePoint *points,
                                   int *widths,
                                   int num_spans,
                                   SpicePoint *new_points,
                                   int *new_widths,
                                   int sorted);

SPICE_END_DECLS

#endif /* LINES_H */
