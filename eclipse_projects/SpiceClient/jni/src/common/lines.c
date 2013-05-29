/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/***********************************************************

Copyright 1989, 1998  The Open Group

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


Copyright 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <spice/macros.h>
#ifdef _XOPEN_SOURCE
#include <math.h>
#else
#define _XOPEN_SOURCE           /* to get prototype for hypot on some systems */
#include <math.h>
#undef _XOPEN_SOURCE
#endif
#include "lines.h"
#include "mem.h"

#define xalloc(i) spice_malloc(i)
#define xrealloc(a,b) spice_realloc(a,b)
#define xfree(i) free(i)

typedef unsigned int CARD32;
typedef int Boolean;
typedef pixman_rectangle32_t xRectangle;
typedef SpicePoint DDXPointRec;
typedef DDXPointRec *DDXPointPtr;
typedef struct lineGC *GCPtr;

/* largest positive value that can fit into a component of a point.
 * Assumes that the point structure is {type x, y;} where type is
 * a signed type.
 */
#define MAX_COORDINATE 2147483647
#define MIN_COORDINATE -2147483647

#define miZeroLine spice_canvas_zero_line
#define miZeroDashLine spice_canvas_zero_dash_line
#define miWideDash spice_canvas_wide_dash_line
#define miWideLine spice_canvas_wide_line

static INLINE int ICEIL (double x)
{
    int _cTmp = (int)x;
    return ((x == _cTmp) || (x < 0.0)) ? _cTmp : _cTmp + 1;
}

typedef struct {
    int count;                  /* number of spans                  */
    DDXPointPtr points;         /* pointer to list of start points  */
    int *widths;                /* pointer to list of widths        */
} Spans;

typedef struct {
    int size;                   /* Total number of *Spans allocated     */
    int count;                  /* Number of *Spans actually in group   */
    Spans *group;               /* List of Spans                        */
    int ymin, ymax;             /* Min, max y values encountered        */
} SpanGroup;

/* Initialize SpanGroup.  MUST BE DONE before use. */
static void miInitSpanGroup (SpanGroup *        /*spanGroup */
    );

/* Add a Spans to a SpanGroup. The spans MUST BE in y-sorted order */
static void miAppendSpans (SpanGroup * /*spanGroup */ ,
                           SpanGroup * /*otherGroup */ ,
                           Spans *      /*spans */
    );

/* Paint a span group, insuring that each pixel is painted at most once */
static void miFillUniqueSpanGroup (GCPtr /*pGC */ ,
                                   SpanGroup * /*spanGroup */ ,
                                   Boolean /* foreground */
    );

/* Free up data in a span group.  MUST BE DONE or you'll suffer memory leaks */
static void miFreeSpanGroup (SpanGroup *        /*spanGroup */
    );

/* Rops which must use span groups */
#define miSpansCarefulRop(rop)  (((rop) & 0xc) == 0x8 || ((rop) & 0x3) == 0x2)
#define miSpansEasyRop(rop)     (!miSpansCarefulRop(rop))

/*
 * Public definitions used for configuring basic pixelization aspects
 * of the sample implementation line-drawing routines provided in
 * {mfb,mi,cfb*} at run-time.
 */

#define XDECREASING     4
#define YDECREASING     2
#define YMAJOR          1

#define OCTANT1                 (1 << (YDECREASING))
#define OCTANT2                 (1 << (YDECREASING|YMAJOR))
#define OCTANT3                 (1 << (XDECREASING|YDECREASING|YMAJOR))
#define OCTANT4                 (1 << (XDECREASING|YDECREASING))
#define OCTANT5                 (1 << (XDECREASING))
#define OCTANT6                 (1 << (XDECREASING|YMAJOR))
#define OCTANT7                 (1 << (YMAJOR))
#define OCTANT8                 (1 << (0))

#define XMAJOROCTANTS           (OCTANT1 | OCTANT4 | OCTANT5 | OCTANT8)

#define DEFAULTZEROLINEBIAS     (OCTANT2 | OCTANT3 | OCTANT4 | OCTANT5)

/*
 * Devices can configure the rendering of routines in mi, mfb, and cfb*
 * by specifying a thin line bias to be applied to a particular screen
 * using the following function.  The bias parameter is an OR'ing of
 * the appropriate OCTANT constants defined above to indicate which
 * octants to bias a line to prefer an axial step when the Bresenham
 * error term is exactly zero.  The octants are mapped as follows:
 *
 *   \    |    /
 *    \ 3 | 2 /
 *     \  |  /
 *    4 \ | / 1
 *       \|/
 *   -----------
 *       /|\
 *    5 / | \ 8
 *     /  |  \
 *    / 6 | 7 \
 *   /    |    \
 *
 * For more information, see "Ambiguities in Incremental Line Rastering,"
 * Jack E. Bresenham, IEEE CG&A, May 1987.
 */

/*
 * Private definitions needed for drawing thin (zero width) lines
 * Used by the mi, mfb, and all cfb* components.
 */

#define X_AXIS  0
#define Y_AXIS  1

#define OUT_LEFT  0x08
#define OUT_RIGHT 0x04
#define OUT_ABOVE 0x02
#define OUT_BELOW 0x01

#define OUTCODES(_result, _x, _y, _pbox) \
    if      ( (_x) <  (_pbox)->x1) (_result) |= OUT_LEFT; \
    else if ( (_x) >= (_pbox)->x2) (_result) |= OUT_RIGHT; \
    if      ( (_y) <  (_pbox)->y1) (_result) |= OUT_ABOVE; \
    else if ( (_y) >= (_pbox)->y2) (_result) |= OUT_BELOW;

#define MIOUTCODES(outcode, x, y, xmin, ymin, xmax, ymax) \
{\
     if (x < xmin) outcode |= OUT_LEFT;\
     if (x > xmax) outcode |= OUT_RIGHT;\
     if (y < ymin) outcode |= OUT_ABOVE;\
     if (y > ymax) outcode |= OUT_BELOW;\
}

#define SWAPINT(i, j) \
{  int _t = i;  i = j;  j = _t; }

#define SWAPPT(i, j) \
{  DDXPointRec _t; _t = i;  i = j; j = _t; }

#define SWAPINT_PAIR(x1, y1, x2, y2)\
{   int t = x1;  x1 = x2;  x2 = t;\
        t = y1;  y1 = y2;  y2 = t;\
}

#define miGetZeroLineBias(_pScreen) (DEFAULTZEROLINEBIAS)

#define CalcLineDeltas(_x1,_y1,_x2,_y2,_adx,_ady,_sx,_sy,_SX,_SY,_octant) \
    (_octant) = 0;                              \
    (_sx) = (_SX);                              \
    if (((_adx) = (_x2) - (_x1)) < 0) {                 \
        (_adx) = -(_adx);                       \
        (_sx = -(_sx));                                 \
        (_octant) |= XDECREASING;               \
    }                                           \
    (_sy) = (_SY);                              \
    if (((_ady) = (_y2) - (_y1)) < 0) {                 \
        (_ady) = -(_ady);                       \
        (_sy = -(_sy));                                 \
        (_octant) |= YDECREASING;               \
    }

#define SetYMajorOctant(_octant)        ((_octant) |= YMAJOR)

#define FIXUP_ERROR(_e, _octant, _bias) \
    (_e) -= (((_bias) >> (_octant)) & 1)

#define IsXMajorOctant(_octant)                 (!((_octant) & YMAJOR))
#define IsYMajorOctant(_octant)                 ((_octant) & YMAJOR)
#define IsXDecreasingOctant(_octant)    ((_octant) & XDECREASING)
#define IsYDecreasingOctant(_octant)    ((_octant) & YDECREASING)

static int miZeroClipLine (int /*xmin */ ,
                           int /*ymin */ ,
                           int /*xmax */ ,
                           int /*ymax */ ,
                           int * /*new_x1 */ ,
                           int * /*new_y1 */ ,
                           int * /*new_x2 */ ,
                           int * /*new_y2 */ ,
                           unsigned int /*adx */ ,
                           unsigned int /*ady */ ,
                           int * /*pt1_clipped */ ,
                           int * /*pt2_clipped */ ,
                           int /*octant */ ,
                           unsigned int /*bias */ ,
                           int /*oc1 */ ,
                           int  /*oc2 */
    );

/*
 * interface data to span-merging polygon filler
 */

typedef struct _SpanData {
    SpanGroup fgGroup, bgGroup;
} SpanDataRec, *SpanDataPtr;

#define AppendSpanGroup(pGC, foreground, spanPtr, spanData) { \
        SpanGroup   *group, *othergroup = NULL; \
        if (foreground) \
        { \
            group = &spanData->fgGroup; \
            if (pGC->lineStyle == LineDoubleDash) \
                othergroup = &spanData->bgGroup; \
        } \
        else \
        { \
            group = &spanData->bgGroup; \
            othergroup = &spanData->fgGroup; \
        } \
        miAppendSpans (group, othergroup, spanPtr); \
}

/*
 * Polygon edge description for integer wide-line routines
 */

typedef struct _PolyEdge {
    int height;                 /* number of scanlines to process */
    int x;                      /* starting x coordinate */
    int stepx;                  /* fixed integral dx */
    int signdx;                 /* variable dx sign */
    int e;                      /* initial error term */
    int dy;
    int dx;
} PolyEdgeRec, *PolyEdgePtr;

#define SQSECANT 108.856472512142       /* 1/sin^2(11/2) - miter limit constant */

/*
 * types for general polygon routines
 */

typedef struct _PolyVertex {
    double x, y;
} PolyVertexRec, *PolyVertexPtr;

typedef struct _PolySlope {
    int dx, dy;
    double k;                   /* x0 * dy - y0 * dx */
} PolySlopeRec, *PolySlopePtr;

/*
 * Line face description for caps/joins
 */

typedef struct _LineFace {
    double xa, ya;
    int dx, dy;
    int x, y;
    double k;
} LineFaceRec, *LineFacePtr;

/*
 * macros for polygon fillers
 */

#define MIPOLYRELOADLEFT    if (!left_height && left_count) { \
                                left_height = left->height; \
                                left_x = left->x; \
                                left_stepx = left->stepx; \
                                left_signdx = left->signdx; \
                                left_e = left->e; \
                                left_dy = left->dy; \
                                left_dx = left->dx; \
                                --left_count; \
                                ++left; \
                            }

#define MIPOLYRELOADRIGHT   if (!right_height && right_count) { \
                                right_height = right->height; \
                                right_x = right->x; \
                                right_stepx = right->stepx; \
                                right_signdx = right->signdx; \
                                right_e = right->e; \
                                right_dy = right->dy; \
                                right_dx = right->dx; \
                                --right_count; \
                                ++right; \
                        }

#define MIPOLYSTEPLEFT  left_x += left_stepx; \
                        left_e += left_dx; \
                        if (left_e > 0) \
                        { \
                            left_x += left_signdx; \
                            left_e -= left_dy; \
                        }

#define MIPOLYSTEPRIGHT right_x += right_stepx; \
                        right_e += right_dx; \
                        if (right_e > 0) \
                        { \
                            right_x += right_signdx; \
                            right_e -= right_dy; \
                        }

static void miRoundJoinClip (LineFacePtr /*pLeft */ ,
                             LineFacePtr /*pRight */ ,
                             PolyEdgePtr /*edge1 */ ,
                             PolyEdgePtr /*edge2 */ ,
                             int * /*y1 */ ,
                             int * /*y2 */ ,
                             Boolean * /*left1 */ ,
                             Boolean *     /*left2 */
    );

static int miRoundCapClip (LineFacePtr /*face */ ,
                           Boolean /*isInt */ ,
                           PolyEdgePtr /*edge */ ,
                           Boolean *       /*leftEdge */
    );

static int miPolyBuildEdge (double x0, double y0, double k, int dx, int dy,
                            int xi, int yi, int left, PolyEdgePtr edge);
static int miPolyBuildPoly (PolyVertexPtr vertices, PolySlopePtr slopes,
                            int count, int xi, int yi, PolyEdgePtr left,
                            PolyEdgePtr right, int *pnleft, int *pnright, int *h);


static void
miStepDash (int dist,           /* distance to step */
            int *pDashIndex,    /* current dash */
            unsigned char *pDash,       /* dash list */
            int numInDashList,  /* total length of dash list */
            int *pDashOffset    /* offset into current dash */
    )
{
    int dashIndex, dashOffset;
    int totallen;
    int i;

    dashIndex = *pDashIndex;
    dashOffset = *pDashOffset;
    if (dist < pDash[dashIndex] - dashOffset) {
        *pDashOffset = dashOffset + dist;
        return;
    }
    dist -= pDash[dashIndex] - dashOffset;
    if (++dashIndex == numInDashList)
        dashIndex = 0;
    totallen = 0;
    for (i = 0; i < numInDashList; i++)
        totallen += pDash[i];
    if (totallen <= dist)
        dist = dist % totallen;
    while (dist >= pDash[dashIndex]) {
        dist -= pDash[dashIndex];
        if (++dashIndex == numInDashList)
            dashIndex = 0;
    }
    *pDashIndex = dashIndex;
    *pDashOffset = dist;
}

/*

These routines maintain lists of Spans, in order to implement the
``touch-each-pixel-once'' rules of wide lines and arcs.

Written by Joel McCormack, Summer 1989.

*/


static void
miInitSpanGroup (SpanGroup * spanGroup)
{
    spanGroup->size = 0;
    spanGroup->count = 0;
    spanGroup->group = NULL;
    spanGroup->ymin = MAX_COORDINATE;
    spanGroup->ymax = MIN_COORDINATE;
}                               /* InitSpanGroup */

#define YMIN(spans) (spans->points[0].y)
#define YMAX(spans)  (spans->points[spans->count-1].y)

static void
miSubtractSpans (SpanGroup * spanGroup, Spans * sub)
{
    int i, subCount, spansCount;
    int ymin, ymax, xmin, xmax;
    Spans *spans;
    DDXPointPtr subPt, spansPt;
    int *subWid, *spansWid;
    int extra;

    ymin = YMIN (sub);
    ymax = YMAX (sub);
    spans = spanGroup->group;
    for (i = spanGroup->count; i; i--, spans++) {
        if (YMIN (spans) <= ymax && ymin <= YMAX (spans)) {
            subCount = sub->count;
            subPt = sub->points;
            subWid = sub->widths;
            spansCount = spans->count;
            spansPt = spans->points;
            spansWid = spans->widths;
            extra = 0;
            for (;;) {
                while (spansCount && spansPt->y < subPt->y) {
                    spansPt++;
                    spansWid++;
                    spansCount--;
                }
                if (!spansCount)
                    break;
                while (subCount && subPt->y < spansPt->y) {
                    subPt++;
                    subWid++;
                    subCount--;
                }
                if (!subCount)
                    break;
                if (subPt->y == spansPt->y) {
                    xmin = subPt->x;
                    xmax = xmin + *subWid;
                    if (xmin >= spansPt->x + *spansWid || spansPt->x >= xmax) {
                        ;
                    } else if (xmin <= spansPt->x) {
                        if (xmax >= spansPt->x + *spansWid) {
                            memmove (spansPt, spansPt + 1, sizeof *spansPt * (spansCount - 1));
                            memmove (spansWid, spansWid + 1, sizeof *spansWid * (spansCount - 1));
                            spansPt--;
                            spansWid--;
                            spans->count--;
                            extra++;
                        } else {
                            *spansWid = *spansWid - (xmax - spansPt->x);
                            spansPt->x = xmax;
                        }
                    } else {
                        if (xmax >= spansPt->x + *spansWid) {
                            *spansWid = xmin - spansPt->x;
                        } else {
                            if (!extra) {
                                DDXPointPtr newPt;
                                int *newwid;

#define EXTRA 8
                                newPt = xrealloc (spans->points,
                                                  (spans->count +
                                                   EXTRA) * sizeof (DDXPointRec));
                                if (!newPt)
                                    break;
                                spansPt = newPt + (spansPt - spans->points);
                                spans->points = newPt;
                                newwid = xrealloc (spans->widths,
                                                   (spans->count + EXTRA) * sizeof (int));
                                if (!newwid)
                                    break;
                                spansWid = newwid + (spansWid - spans->widths);
                                spans->widths = newwid;
                                extra = EXTRA;
                            }
                            memmove (spansPt + 1, spansPt, sizeof *spansPt * (spansCount));
                            memmove (spansWid + 1, spansWid, sizeof *spansWid * (spansCount));
                            spans->count++;
                            extra--;
                            *spansWid = xmin - spansPt->x;
                            spansWid++;
                            spansPt++;
                            *spansWid = *spansWid - (xmax - spansPt->x);
                            spansPt->x = xmax;
                        }
                    }
                }
                spansPt++;
                spansWid++;
                spansCount--;
            }
        }
    }
}

static void
miAppendSpans (SpanGroup * spanGroup, SpanGroup * otherGroup, Spans * spans)
{
    int ymin, ymax;
    int spansCount;

    spansCount = spans->count;
    if (spansCount > 0) {
        if (spanGroup->size == spanGroup->count) {
            spanGroup->size = (spanGroup->size + 8) * 2;
            spanGroup->group =
                xrealloc (spanGroup->group, sizeof (Spans) * spanGroup->size);
        }

        spanGroup->group[spanGroup->count] = *spans;
        (spanGroup->count)++;
        ymin = spans->points[0].y;
        if (ymin < spanGroup->ymin)
            spanGroup->ymin = ymin;
        ymax = spans->points[spansCount - 1].y;
        if (ymax > spanGroup->ymax)
            spanGroup->ymax = ymax;
        if (otherGroup && otherGroup->ymin < ymax && ymin < otherGroup->ymax) {
            miSubtractSpans (otherGroup, spans);
        }
    } else {
        xfree (spans->points);
        xfree (spans->widths);
    }
}                               /* AppendSpans */

static void
miFreeSpanGroup (SpanGroup * spanGroup)
{
    xfree (spanGroup->group);
}

static void
QuickSortSpansX (DDXPointRec points[], int widths[], int numSpans)
{
    int x;
    int i, j, m;
    DDXPointPtr r;

/* Always called with numSpans > 1 */
/* Sorts only by x, as all y should be the same */

#define ExchangeSpans(a, b)                                 \
{                                                           \
    DDXPointRec         tpt;                                \
    int                 tw;                                 \
                                                            \
    tpt = points[a]; points[a] = points[b]; points[b] = tpt;    \
    tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;  \
}

    do {
        if (numSpans < 9) {
            /* Do insertion sort */
            int xprev;

            xprev = points[0].x;
            i = 1;
            do {                /* while i != numSpans */
                x = points[i].x;
                if (xprev > x) {
                    /* points[i] is out of order.  Move into proper location. */
                    DDXPointRec tpt;
                    int tw, k;

                    for (j = 0; x >= points[j].x; j++) {
                    }
                    tpt = points[i];
                    tw = widths[i];
                    for (k = i; k != j; k--) {
                        points[k] = points[k - 1];
                        widths[k] = widths[k - 1];
                    }
                    points[j] = tpt;
                    widths[j] = tw;
                    x = points[i].x;
                }               /* if out of order */
                xprev = x;
                i++;
            } while (i != numSpans);
            return;
        }

        /* Choose partition element, stick in location 0 */
        m = numSpans / 2;
        if (points[m].x > points[0].x)
            ExchangeSpans (m, 0);
        if (points[m].x > points[numSpans - 1].x)
            ExchangeSpans (m, numSpans - 1);
        if (points[m].x > points[0].x)
            ExchangeSpans (m, 0);
        x = points[0].x;

        /* Partition array */
        i = 0;
        j = numSpans;
        do {
            r = &(points[i]);
            do {
                r++;
                i++;
            } while (i != numSpans && r->x < x);
            r = &(points[j]);
            do {
                r--;
                j--;
            } while (x < r->x);
            if (i < j)
                ExchangeSpans (i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeSpans (0, j);

        /* Recurse */
        if (numSpans - j - 1 > 1)
            QuickSortSpansX (&points[j + 1], &widths[j + 1], numSpans - j - 1);
        numSpans = j;
    } while (numSpans > 1);
}                               /* QuickSortSpans */


static int
UniquifySpansX (Spans * spans, DDXPointRec * newPoints, int *newWidths)
{
    int newx1, newx2, oldpt, i, y;
    DDXPointRec *oldPoints;
    int *oldWidths;
    int *startNewWidths;

/* Always called with numSpans > 1 */
/* Uniquify the spans, and stash them into newPoints and newWidths.  Return the
   number of unique spans. */


    startNewWidths = newWidths;

    oldPoints = spans->points;
    oldWidths = spans->widths;

    y = oldPoints->y;
    newx1 = oldPoints->x;
    newx2 = newx1 + *oldWidths;

    for (i = spans->count - 1; i != 0; i--) {
        oldPoints++;
        oldWidths++;
        oldpt = oldPoints->x;
        if (oldpt > newx2) {
            /* Write current span, start a new one */
            newPoints->x = newx1;
            newPoints->y = y;
            *newWidths = newx2 - newx1;
            newPoints++;
            newWidths++;
            newx1 = oldpt;
            newx2 = oldpt + *oldWidths;
        } else {
            /* extend current span, if old extends beyond new */
            oldpt = oldpt + *oldWidths;
            if (oldpt > newx2)
                newx2 = oldpt;
        }
    }                           /* for */

    /* Write final span */
    newPoints->x = newx1;
    *newWidths = newx2 - newx1;
    newPoints->y = y;

    return (newWidths - startNewWidths) + 1;
}                               /* UniquifySpansX */

static void
miDisposeSpanGroup (SpanGroup * spanGroup)
{
    int i;
    Spans *spans;

    for (i = 0; i < spanGroup->count; i++) {
        spans = spanGroup->group + i;
        xfree (spans->points);
        xfree (spans->widths);
    }
}

static void
miFillUniqueSpanGroup (GCPtr pGC, SpanGroup * spanGroup, Boolean foreground)
{
    int i;
    Spans *spans;
    Spans *yspans;
    int *ysizes;
    int ymin, ylength;

    /* Outgoing spans for one big call to FillSpans */
    DDXPointPtr points;
    int *widths;
    int count;

    if (spanGroup->count == 0)
        return;

    if (spanGroup->count == 1) {
        /* Already should be sorted, unique */
        spans = spanGroup->group;
        (*pGC->ops->FillSpans)
            (pGC, spans->count, spans->points, spans->widths, TRUE, foreground);
        xfree (spans->points);
        xfree (spans->widths);
    } else {
        /* Yuck.  Gross.  Radix sort into y buckets, then sort x and uniquify */
        /* This seems to be the fastest thing to do.  I've tried sorting on
           both x and y at the same time rather than creating into all those
           y buckets, but it was somewhat slower. */

        ymin = spanGroup->ymin;
        ylength = spanGroup->ymax - ymin + 1;

        /* Allocate Spans for y buckets */
        yspans = (Spans*)xalloc (ylength * sizeof (Spans));
        ysizes = (int *)xalloc (ylength * sizeof (int));

        if (!yspans || !ysizes) {
            xfree (yspans);
            xfree (ysizes);
            miDisposeSpanGroup (spanGroup);
            return;
        }

        for (i = 0; i != ylength; i++) {
            ysizes[i] = 0;
            yspans[i].count = 0;
            yspans[i].points = NULL;
            yspans[i].widths = NULL;
        }

        /* Go through every single span and put it into the correct bucket */
        count = 0;
        for (i = 0, spans = spanGroup->group; i != spanGroup->count; i++, spans++) {
            int index;
            int j;

            for (j = 0, points = spans->points, widths = spans->widths;
                 j != spans->count; j++, points++, widths++) {
                index = points->y - ymin;
                if (index >= 0 && index < ylength) {
                    Spans *newspans = &(yspans[index]);
                    if (newspans->count == ysizes[index]) {
                        DDXPointPtr newpoints;
                        int *newwidths;
                        ysizes[index] = (ysizes[index] + 8) * 2;
                        newpoints = xrealloc (newspans->points,
                                              ysizes[index] * sizeof (DDXPointRec));
                        newwidths = xrealloc (newspans->widths,
                                              ysizes[index] * sizeof (int));
                        if (!newpoints || !newwidths) {
                            int i;

                            for (i = 0; i < ylength; i++) {
                                xfree (yspans[i].points);
                                xfree (yspans[i].widths);
                            }
                            xfree (yspans);
                            xfree (ysizes);
                            miDisposeSpanGroup (spanGroup);
                            return;
                        }
                        newspans->points = newpoints;
                        newspans->widths = newwidths;
                    }
                    newspans->points[newspans->count] = *points;
                    newspans->widths[newspans->count] = *widths;
                    (newspans->count)++;
                }               /* if y value of span in range */
            }                   /* for j through spans */
            count += spans->count;
            xfree (spans->points);
            spans->points = NULL;
            xfree (spans->widths);
            spans->widths = NULL;
        }                       /* for i thorough Spans */

        /* Now sort by x and uniquify each bucket into the final array */
        points = (DDXPointRec*)xalloc (count * sizeof (DDXPointRec));
        widths = (int *)xalloc (count * sizeof (int));
        if (!points || !widths) {
            int i;

            for (i = 0; i < ylength; i++) {
                xfree (yspans[i].points);
                xfree (yspans[i].widths);
            }
            xfree (yspans);
            xfree (ysizes);
            xfree (points);
            xfree (widths);
            return;
        }
        count = 0;
        for (i = 0; i != ylength; i++) {
            int ycount = yspans[i].count;
            if (ycount > 0) {
                if (ycount > 1) {
                    QuickSortSpansX (yspans[i].points, yspans[i].widths, ycount);
                    count += UniquifySpansX (&(yspans[i]), &(points[count]), &(widths[count]));
                } else {
                    points[count] = yspans[i].points[0];
                    widths[count] = yspans[i].widths[0];
                    count++;
                }
                xfree (yspans[i].points);
                xfree (yspans[i].widths);
            }
        }

        (*pGC->ops->FillSpans) (pGC, count, points, widths, TRUE, foreground);
        xfree (points);
        xfree (widths);
        xfree (yspans);
        xfree (ysizes);         /* use (DE)xalloc for these? */
    }

    spanGroup->count = 0;
    spanGroup->ymin = MAX_COORDINATE;
    spanGroup->ymax = MIN_COORDINATE;
}

/*

The bresenham error equation used in the mi/mfb/cfb line routines is:

        e = error
        dx = difference in raw X coordinates
        dy = difference in raw Y coordinates
        M = # of steps in X direction
        N = # of steps in Y direction
        B = 0 to prefer diagonal steps in a given octant,
            1 to prefer axial steps in a given octant

        For X major lines:
                e = 2Mdy - 2Ndx - dx - B
                -2dx <= e < 0

        For Y major lines:
                e = 2Ndx - 2Mdy - dy - B
                -2dy <= e < 0

At the start of the line, we have taken 0 X steps and 0 Y steps,
so M = 0 and N = 0:

        X major         e = 2Mdy - 2Ndx - dx - B
                  = -dx - B

        Y major         e = 2Ndx - 2Mdy - dy - B
                  = -dy - B

At the end of the line, we have taken dx X steps and dy Y steps,
so M = dx and N = dy:

        X major         e = 2Mdy - 2Ndx - dx - B
                  = 2dxdy - 2dydx - dx - B
                  = -dx - B
        Y major e = 2Ndx - 2Mdy - dy - B
                  = 2dydx - 2dxdy - dy - B
                  = -dy - B

Thus, the error term is the same at the start and end of the line.

Let us consider clipping an X coordinate.  There are 4 cases which
represent the two independent cases of clipping the start vs. the
end of the line and an X major vs. a Y major line.  In any of these
cases, we know the number of X steps (M) and we wish to find the
number of Y steps (N).  Thus, we will solve our error term equation.
If we are clipping the start of the line, we will find the smallest
N that satisfies our error term inequality.  If we are clipping the
end of the line, we will find the largest number of Y steps that
satisfies the inequality.  In that case, since we are representing
the Y steps as (dy - N), we will actually want to solve for the
smallest N in that equation.

Case 1:  X major, starting X coordinate moved by M steps

                -2dx <= 2Mdy - 2Ndx - dx - B < 0
        2Ndx <= 2Mdy - dx - B + 2dx     2Ndx > 2Mdy - dx - B
        2Ndx <= 2Mdy + dx - B           N > (2Mdy - dx - B) / 2dx
        N <= (2Mdy + dx - B) / 2dx

Since we are trying to find the smallest N that satisfies these
equations, we should use the > inequality to find the smallest:

        N = floor((2Mdy - dx - B) / 2dx) + 1
          = floor((2Mdy - dx - B + 2dx) / 2dx)
          = floor((2Mdy + dx - B) / 2dx)

Case 1b: X major, ending X coordinate moved to M steps

Same derivations as Case 1, but we want the largest N that satisfies
the equations, so we use the <= inequality:

        N = floor((2Mdy + dx - B) / 2dx)

Case 2: X major, ending X coordinate moved by M steps

                -2dx <= 2(dx - M)dy - 2(dy - N)dx - dx - B < 0
                -2dx <= 2dxdy - 2Mdy - 2dxdy + 2Ndx - dx - B < 0
                -2dx <= 2Ndx - 2Mdy - dx - B < 0
        2Ndx >= 2Mdy + dx + B - 2dx     2Ndx < 2Mdy + dx + B
        2Ndx >= 2Mdy - dx + B           N < (2Mdy + dx + B) / 2dx
        N >= (2Mdy - dx + B) / 2dx

Since we are trying to find the highest number of Y steps that
satisfies these equations, we need to find the smallest N, so
we should use the >= inequality to find the smallest:

        N = ceiling((2Mdy - dx + B) / 2dx)
          = floor((2Mdy - dx + B + 2dx - 1) / 2dx)
          = floor((2Mdy + dx + B - 1) / 2dx)

Case 2b: X major, starting X coordinate moved to M steps from end

Same derivations as Case 2, but we want the smallest number of Y
steps, so we want the highest N, so we use the < inequality:

        N = ceiling((2Mdy + dx + B) / 2dx) - 1
          = floor((2Mdy + dx + B + 2dx - 1) / 2dx) - 1
          = floor((2Mdy + dx + B + 2dx - 1 - 2dx) / 2dx)
          = floor((2Mdy + dx + B - 1) / 2dx)

Case 3: Y major, starting X coordinate moved by M steps

                -2dy <= 2Ndx - 2Mdy - dy - B < 0
        2Ndx >= 2Mdy + dy + B - 2dy     2Ndx < 2Mdy + dy + B
        2Ndx >= 2Mdy - dy + B           N < (2Mdy + dy + B) / 2dx
        N >= (2Mdy - dy + B) / 2dx

Since we are trying to find the smallest N that satisfies these
equations, we should use the >= inequality to find the smallest:

        N = ceiling((2Mdy - dy + B) / 2dx)
          = floor((2Mdy - dy + B + 2dx - 1) / 2dx)
          = floor((2Mdy - dy + B - 1) / 2dx) + 1

Case 3b: Y major, ending X coordinate moved to M steps

Same derivations as Case 3, but we want the largest N that satisfies
the equations, so we use the < inequality:

        N = ceiling((2Mdy + dy + B) / 2dx) - 1
          = floor((2Mdy + dy + B + 2dx - 1) / 2dx) - 1
          = floor((2Mdy + dy + B + 2dx - 1 - 2dx) / 2dx)
          = floor((2Mdy + dy + B - 1) / 2dx)

Case 4: Y major, ending X coordinate moved by M steps

                -2dy <= 2(dy - N)dx - 2(dx - M)dy - dy - B < 0
                -2dy <= 2dxdy - 2Ndx - 2dxdy + 2Mdy - dy - B < 0
                -2dy <= 2Mdy - 2Ndx - dy - B < 0
        2Ndx <= 2Mdy - dy - B + 2dy     2Ndx > 2Mdy - dy - B
        2Ndx <= 2Mdy + dy - B           N > (2Mdy - dy - B) / 2dx
        N <= (2Mdy + dy - B) / 2dx

Since we are trying to find the highest number of Y steps that
satisfies these equations, we need to find the smallest N, so
we should use the > inequality to find the smallest:

        N = floor((2Mdy - dy - B) / 2dx) + 1

Case 4b: Y major, starting X coordinate moved to M steps from end

Same analysis as Case 4, but we want the smallest number of Y steps
which means the largest N, so we use the <= inequality:

        N = floor((2Mdy + dy - B) / 2dx)

Now let's try the Y coordinates, we have the same 4 cases.

Case 5: X major, starting Y coordinate moved by N steps

                -2dx <= 2Mdy - 2Ndx - dx - B < 0
        2Mdy >= 2Ndx + dx + B - 2dx     2Mdy < 2Ndx + dx + B
        2Mdy >= 2Ndx - dx + B           M < (2Ndx + dx + B) / 2dy
        M >= (2Ndx - dx + B) / 2dy

Since we are trying to find the smallest M, we use the >= inequality:

        M = ceiling((2Ndx - dx + B) / 2dy)
          = floor((2Ndx - dx + B + 2dy - 1) / 2dy)
          = floor((2Ndx - dx + B - 1) / 2dy) + 1

Case 5b: X major, ending Y coordinate moved to N steps

Same derivations as Case 5, but we want the largest M that satisfies
the equations, so we use the < inequality:

        M = ceiling((2Ndx + dx + B) / 2dy) - 1
          = floor((2Ndx + dx + B + 2dy - 1) / 2dy) - 1
          = floor((2Ndx + dx + B + 2dy - 1 - 2dy) / 2dy)
          = floor((2Ndx + dx + B - 1) / 2dy)

Case 6: X major, ending Y coordinate moved by N steps

                -2dx <= 2(dx - M)dy - 2(dy - N)dx - dx - B < 0
                -2dx <= 2dxdy - 2Mdy - 2dxdy + 2Ndx - dx - B < 0
                -2dx <= 2Ndx - 2Mdy - dx - B < 0
        2Mdy <= 2Ndx - dx - B + 2dx     2Mdy > 2Ndx - dx - B
        2Mdy <= 2Ndx + dx - B           M > (2Ndx - dx - B) / 2dy
        M <= (2Ndx + dx - B) / 2dy

Largest # of X steps means smallest M, so use the > inequality:

        M = floor((2Ndx - dx - B) / 2dy) + 1

Case 6b: X major, starting Y coordinate moved to N steps from end

Same derivations as Case 6, but we want the smallest # of X steps
which means the largest M, so use the <= inequality:

        M = floor((2Ndx + dx - B) / 2dy)

Case 7: Y major, starting Y coordinate moved by N steps

                -2dy <= 2Ndx - 2Mdy - dy - B < 0
        2Mdy <= 2Ndx - dy - B + 2dy     2Mdy > 2Ndx - dy - B
        2Mdy <= 2Ndx + dy - B           M > (2Ndx - dy - B) / 2dy
        M <= (2Ndx + dy - B) / 2dy

To find the smallest M, use the > inequality:

        M = floor((2Ndx - dy - B) / 2dy) + 1
          = floor((2Ndx - dy - B + 2dy) / 2dy)
          = floor((2Ndx + dy - B) / 2dy)

Case 7b: Y major, ending Y coordinate moved to N steps

Same derivations as Case 7, but we want the largest M that satisfies
the equations, so use the <= inequality:

        M = floor((2Ndx + dy - B) / 2dy)

Case 8: Y major, ending Y coordinate moved by N steps

                -2dy <= 2(dy - N)dx - 2(dx - M)dy - dy - B < 0
                -2dy <= 2dxdy - 2Ndx - 2dxdy + 2Mdy - dy - B < 0
                -2dy <= 2Mdy - 2Ndx - dy - B < 0
        2Mdy >= 2Ndx + dy + B - 2dy     2Mdy < 2Ndx + dy + B
        2Mdy >= 2Ndx - dy + B           M < (2Ndx + dy + B) / 2dy
        M >= (2Ndx - dy + B) / 2dy

To find the highest X steps, find the smallest M, use the >= inequality:

        M = ceiling((2Ndx - dy + B) / 2dy)
          = floor((2Ndx - dy + B + 2dy - 1) / 2dy)
          = floor((2Ndx + dy + B - 1) / 2dy)

Case 8b: Y major, starting Y coordinate moved to N steps from the end

Same derivations as Case 8, but we want to find the smallest # of X
steps which means the largest M, so we use the < inequality:

        M = ceiling((2Ndx + dy + B) / 2dy) - 1
          = floor((2Ndx + dy + B + 2dy - 1) / 2dy) - 1
          = floor((2Ndx + dy + B + 2dy - 1 - 2dy) / 2dy)
          = floor((2Ndx + dy + B - 1) / 2dy)

So, our equations are:

        1:  X major move x1 to x1+M     floor((2Mdy + dx - B) / 2dx)
        1b: X major move x2 to x1+M     floor((2Mdy + dx - B) / 2dx)
        2:  X major move x2 to x2-M     floor((2Mdy + dx + B - 1) / 2dx)
        2b: X major move x1 to x2-M     floor((2Mdy + dx + B - 1) / 2dx)

        3:  Y major move x1 to x1+M     floor((2Mdy - dy + B - 1) / 2dx) + 1
        3b: Y major move x2 to x1+M     floor((2Mdy + dy + B - 1) / 2dx)
        4:  Y major move x2 to x2-M     floor((2Mdy - dy - B) / 2dx) + 1
        4b: Y major move x1 to x2-M     floor((2Mdy + dy - B) / 2dx)

        5:  X major move y1 to y1+N     floor((2Ndx - dx + B - 1) / 2dy) + 1
        5b: X major move y2 to y1+N     floor((2Ndx + dx + B - 1) / 2dy)
        6:  X major move y2 to y2-N     floor((2Ndx - dx - B) / 2dy) + 1
        6b: X major move y1 to y2-N     floor((2Ndx + dx - B) / 2dy)

        7:  Y major move y1 to y1+N     floor((2Ndx + dy - B) / 2dy)
        7b: Y major move y2 to y1+N     floor((2Ndx + dy - B) / 2dy)
        8:  Y major move y2 to y2-N     floor((2Ndx + dy + B - 1) / 2dy)
        8b: Y major move y1 to y2-N     floor((2Ndx + dy + B - 1) / 2dy)

We have the following constraints on all of the above terms:

        0 < M,N <= 2^15          2^15 can be imposed by miZeroClipLine
        0 <= dx/dy <= 2^16 - 1
        0 <= B <= 1

The floor in all of the above equations can be accomplished with a
simple C divide operation provided that both numerator and denominator
are positive.

Since dx,dy >= 0 and since moving an X coordinate implies that dx != 0
and moving a Y coordinate implies dy != 0, we know that the denominators
are all > 0.

For all lines, (-B) and (B-1) are both either 0 or -1, depending on the
bias.  Thus, we have to show that the 2MNdxy +/- dxy terms are all >= 1
or > 0 to prove that the numerators are positive (or zero).

For X Major lines we know that dx > 0 and since 2Mdy is >= 0 due to the
constraints, the first four equations all have numerators >= 0.

For the second four equations, M > 0, so 2Mdy >= 2dy so (2Mdy - dy) >= dy
So (2Mdy - dy) > 0, since they are Y major lines.  Also, (2Mdy + dy) >= 3dy
or (2Mdy + dy) > 0.  So all of their numerators are >= 0.

For the third set of four equations, N > 0, so 2Ndx >= 2dx so (2Ndx - dx)
>= dx > 0.  Similarly (2Ndx + dx) >= 3dx > 0.  So all numerators >= 0.

For the fourth set of equations, dy > 0 and 2Ndx >= 0, so all numerators
are > 0.

To consider overflow, consider the case of 2 * M,N * dx,dy + dx,dy.  This
is bounded <= 2 * 2^15 * (2^16 - 1) + (2^16 - 1)
           <= 2^16 * (2^16 - 1) + (2^16 - 1)
           <= 2^32 - 2^16 + 2^16 - 1
           <= 2^32 - 1
Since the (-B) and (B-1) terms are all 0 or -1, the maximum value of
the numerator is therefore (2^32 - 1), which does not overflow an unsigned
32 bit variable.

*/

/* Bit codes for the terms of the 16 clipping equations defined below. */

#define T_2NDX          (1 << 0)
#define T_2MDY          (0)     /* implicit term */
#define T_DXNOTY        (1 << 1)
#define T_DYNOTX        (0)     /* implicit term */
#define T_SUBDXORY      (1 << 2)
#define T_ADDDX                 (T_DXNOTY)      /* composite term */
#define T_SUBDX                 (T_DXNOTY | T_SUBDXORY) /* composite term */
#define T_ADDDY                 (T_DYNOTX)      /* composite term */
#define T_SUBDY                 (T_DYNOTX | T_SUBDXORY) /* composite term */
#define T_BIASSUBONE    (1 << 3)
#define T_SUBBIAS       (0)     /* implicit term */
#define T_DIV2DX        (1 << 4)
#define T_DIV2DY        (0)     /* implicit term */
#define T_ADDONE        (1 << 5)

/* Bit masks defining the 16 equations used in miZeroClipLine. */

#define EQN1    (T_2MDY | T_ADDDX | T_SUBBIAS    | T_DIV2DX)
#define EQN1B   (T_2MDY | T_ADDDX | T_SUBBIAS    | T_DIV2DX)
#define EQN2    (T_2MDY | T_ADDDX | T_BIASSUBONE | T_DIV2DX)
#define EQN2B   (T_2MDY | T_ADDDX | T_BIASSUBONE | T_DIV2DX)

#define EQN3    (T_2MDY | T_SUBDY | T_BIASSUBONE | T_DIV2DX | T_ADDONE)
#define EQN3B   (T_2MDY | T_ADDDY | T_BIASSUBONE | T_DIV2DX)
#define EQN4    (T_2MDY | T_SUBDY | T_SUBBIAS    | T_DIV2DX | T_ADDONE)
#define EQN4B   (T_2MDY | T_ADDDY | T_SUBBIAS    | T_DIV2DX)

#define EQN5    (T_2NDX | T_SUBDX | T_BIASSUBONE | T_DIV2DY | T_ADDONE)
#define EQN5B   (T_2NDX | T_ADDDX | T_BIASSUBONE | T_DIV2DY)
#define EQN6    (T_2NDX | T_SUBDX | T_SUBBIAS    | T_DIV2DY | T_ADDONE)
#define EQN6B   (T_2NDX | T_ADDDX | T_SUBBIAS    | T_DIV2DY)

#define EQN7    (T_2NDX | T_ADDDY | T_SUBBIAS    | T_DIV2DY)
#define EQN7B   (T_2NDX | T_ADDDY | T_SUBBIAS    | T_DIV2DY)
#define EQN8    (T_2NDX | T_ADDDY | T_BIASSUBONE | T_DIV2DY)
#define EQN8B   (T_2NDX | T_ADDDY | T_BIASSUBONE | T_DIV2DY)

/* miZeroClipLine
 *
 * returns:  1 for partially clipped line
 *          -1 for completely clipped line
 *
 */
static int
miZeroClipLine (int xmin, int ymin, int xmax, int ymax,
                int *new_x1, int *new_y1, int *new_x2, int *new_y2,
                unsigned int adx, unsigned int ady,
                int *pt1_clipped, int *pt2_clipped, int octant, unsigned int bias, int oc1, int oc2)
{
    int swapped = 0;
    int clipDone = 0;
    CARD32 utmp = 0;
    int clip1, clip2;
    int x1, y1, x2, y2;
    int x1_orig, y1_orig, x2_orig, y2_orig;
    int xmajor;
    int negslope = 0, anchorval = 0;
    unsigned int eqn = 0;

    x1 = x1_orig = *new_x1;
    y1 = y1_orig = *new_y1;
    x2 = x2_orig = *new_x2;
    y2 = y2_orig = *new_y2;

    clip1 = 0;
    clip2 = 0;

    xmajor = IsXMajorOctant (octant);
    bias = ((bias >> octant) & 1);

    while (1) {
        if ((oc1 & oc2) != 0) { /* trivial reject */
            clipDone = -1;
            clip1 = oc1;
            clip2 = oc2;
            break;
        } else if ((oc1 | oc2) == 0) {  /* trivial accept */
            clipDone = 1;
            if (swapped) {
                SWAPINT_PAIR (x1, y1, x2, y2);
                SWAPINT (clip1, clip2);
            }
            break;
        } else {                /* have to clip */

            /* only clip one point at a time */
            if (oc1 == 0) {
                SWAPINT_PAIR (x1, y1, x2, y2);
                SWAPINT_PAIR (x1_orig, y1_orig, x2_orig, y2_orig);
                SWAPINT (oc1, oc2);
                SWAPINT (clip1, clip2);
                swapped = !swapped;
            }

            clip1 |= oc1;
            if (oc1 & OUT_LEFT) {
                negslope = IsYDecreasingOctant (octant);
                utmp = xmin - x1_orig;
                if (utmp <= 32767) {    /* clip based on near endpt */
                    if (xmajor)
                        eqn = (swapped) ? EQN2 : EQN1;
                    else
                        eqn = (swapped) ? EQN4 : EQN3;
                    anchorval = y1_orig;
                } else {        /* clip based on far endpt */

                    utmp = x2_orig - xmin;
                    if (xmajor)
                        eqn = (swapped) ? EQN1B : EQN2B;
                    else
                        eqn = (swapped) ? EQN3B : EQN4B;
                    anchorval = y2_orig;
                    negslope = !negslope;
                }
                x1 = xmin;
            } else if (oc1 & OUT_ABOVE) {
                negslope = IsXDecreasingOctant (octant);
                utmp = ymin - y1_orig;
                if (utmp <= 32767) {    /* clip based on near endpt */
                    if (xmajor)
                        eqn = (swapped) ? EQN6 : EQN5;
                    else
                        eqn = (swapped) ? EQN8 : EQN7;
                    anchorval = x1_orig;
                } else {        /* clip based on far endpt */

                    utmp = y2_orig - ymin;
                    if (xmajor)
                        eqn = (swapped) ? EQN5B : EQN6B;
                    else
                        eqn = (swapped) ? EQN7B : EQN8B;
                    anchorval = x2_orig;
                    negslope = !negslope;
                }
                y1 = ymin;
            } else if (oc1 & OUT_RIGHT) {
                negslope = IsYDecreasingOctant (octant);
                utmp = x1_orig - xmax;
                if (utmp <= 32767) {    /* clip based on near endpt */
                    if (xmajor)
                        eqn = (swapped) ? EQN2 : EQN1;
                    else
                        eqn = (swapped) ? EQN4 : EQN3;
                    anchorval = y1_orig;
                } else {        /* clip based on far endpt */

                    /*
                     * Technically since the equations can handle
                     * utmp == 32768, this overflow code isn't
                     * needed since X11 protocol can't generate
                     * a line which goes more than 32768 pixels
                     * to the right of a clip rectangle.
                     */
                    utmp = xmax - x2_orig;
                    if (xmajor)
                        eqn = (swapped) ? EQN1B : EQN2B;
                    else
                        eqn = (swapped) ? EQN3B : EQN4B;
                    anchorval = y2_orig;
                    negslope = !negslope;
                }
                x1 = xmax;
            } else if (oc1 & OUT_BELOW) {
                negslope = IsXDecreasingOctant (octant);
                utmp = y1_orig - ymax;
                if (utmp <= 32767) {    /* clip based on near endpt */
                    if (xmajor)
                        eqn = (swapped) ? EQN6 : EQN5;
                    else
                        eqn = (swapped) ? EQN8 : EQN7;
                    anchorval = x1_orig;
                } else {        /* clip based on far endpt */

                    /*
                     * Technically since the equations can handle
                     * utmp == 32768, this overflow code isn't
                     * needed since X11 protocol can't generate
                     * a line which goes more than 32768 pixels
                     * below the bottom of a clip rectangle.
                     */
                    utmp = ymax - y2_orig;
                    if (xmajor)
                        eqn = (swapped) ? EQN5B : EQN6B;
                    else
                        eqn = (swapped) ? EQN7B : EQN8B;
                    anchorval = x2_orig;
                    negslope = !negslope;
                }
                y1 = ymax;
            }

            if (swapped)
                negslope = !negslope;

            utmp <<= 1;         /* utmp = 2N or 2M */
            if (eqn & T_2NDX)
                utmp = (utmp * adx);
            else                /* (eqn & T_2MDY) */
                utmp = (utmp * ady);
            if (eqn & T_DXNOTY)
                if (eqn & T_SUBDXORY)
                    utmp -= adx;
                else
                    utmp += adx;
            else /* (eqn & T_DYNOTX) */ if (eqn & T_SUBDXORY)
                utmp -= ady;
            else
                utmp += ady;
            if (eqn & T_BIASSUBONE)
                utmp += bias - 1;
            else                /* (eqn & T_SUBBIAS) */
                utmp -= bias;
            if (eqn & T_DIV2DX)
                utmp /= (adx << 1);
            else                /* (eqn & T_DIV2DY) */
                utmp /= (ady << 1);
            if (eqn & T_ADDONE)
                utmp++;

            if (negslope)
                utmp = (uint32_t)(-(int32_t)utmp);

            if (eqn & T_2NDX)   /* We are calculating X steps */
                x1 = anchorval + utmp;
            else                /* else, Y steps */
                y1 = anchorval + utmp;

            oc1 = 0;
            MIOUTCODES (oc1, x1, y1, xmin, ymin, xmax, ymax);
        }
    }

    *new_x1 = x1;
    *new_y1 = y1;
    *new_x2 = x2;
    *new_y2 = y2;

    *pt1_clipped = clip1;
    *pt2_clipped = clip2;

    return clipDone;
}

/* Draw lineSolid, fillStyle-independent zero width lines.
 *
 * Must keep X and Y coordinates in "ints" at least until after they're
 * translated and clipped to accomodate CoordModePrevious lines with very
 * large coordinates.
 *
 * Draws the same pixels regardless of sign(dx) or sign(dy).
 *
 * Ken Whaley
 *
 */

#define MI_OUTPUT_POINT(xx, yy)\
{\
    if ( !new_span && yy == current_y)\
    {\
        if (xx < spans->x)\
            spans->x = xx;\
        ++*widths;\
    }\
    else\
    {\
        ++Nspans;\
        ++spans;\
        ++widths;\
        spans->x = xx;\
        spans->y = yy;\
        *widths = 1;\
        current_y = yy;\
        new_span = FALSE;\
    }\
}

void
miZeroLine (GCPtr pGC, int mode,        /* Origin or Previous */
            int npt,            /* number of points */
            DDXPointPtr pptInit)
{
    int Nspans, current_y = 0;
    DDXPointPtr ppt;
    DDXPointPtr pspanInit, spans;
    int *pwidthInit, *widths, list_len;
    int xleft, ytop, xright, ybottom;
    int new_x1, new_y1, new_x2, new_y2;
    int x = 0, y = 0, x1, y1, x2, y2, xstart, ystart;
    int oc1, oc2;
    int result;
    int pt1_clipped, pt2_clipped = 0;
    Boolean new_span;
    int signdx, signdy;
    int clipdx, clipdy;
    int width, height;
    int adx, ady;
    int octant;
    unsigned int bias = miGetZeroLineBias (screen);
    int e, e1, e2, e3;          /* Bresenham error terms */
    int length;                 /* length of lines == # of pixels on major axis */

    xleft = 0;
    ytop = 0;
    xright = pGC->width - 1;
    ybottom = pGC->height - 1;

    /* it doesn't matter whether we're in drawable or screen coordinates,
     * FillSpans simply cannot take starting coordinates outside of the
     * range of a DDXPointRec component.
     */
    if (xright > MAX_COORDINATE)
        xright = MAX_COORDINATE;
    if (ybottom > MAX_COORDINATE)
        ybottom = MAX_COORDINATE;

    /* since we're clipping to the drawable's boundaries & coordinate
     * space boundaries, we're guaranteed that the larger of width/height
     * is the longest span we'll need to output
     */
    width = xright - xleft + 1;
    height = ybottom - ytop + 1;
    list_len = (height >= width) ? height : width;
    pspanInit = (DDXPointRec *)xalloc (list_len * sizeof (DDXPointRec));
    pwidthInit = (int *)xalloc (list_len * sizeof (int));
    if (!pspanInit || !pwidthInit)
        goto out;

    Nspans = 0;
    new_span = TRUE;
    spans = pspanInit - 1;
    widths = pwidthInit - 1;
    ppt = pptInit;

    xstart = ppt->x;
    ystart = ppt->y;

    /* x2, y2, oc2 copied to x1, y1, oc1 at top of loop to simplify
     * iteration logic
     */
    x2 = xstart;
    y2 = ystart;
    oc2 = 0;
    MIOUTCODES (oc2, x2, y2, xleft, ytop, xright, ybottom);

    while (--npt > 0) {
        if (Nspans > 0)
            (*pGC->ops->FillSpans) (pGC, Nspans, pspanInit, pwidthInit, FALSE, TRUE);
        Nspans = 0;
        new_span = TRUE;
        spans = pspanInit - 1;
        widths = pwidthInit - 1;

        x1 = x2;
        y1 = y2;
        oc1 = oc2;
        ++ppt;

        x2 = ppt->x;
        y2 = ppt->y;
        if (mode == CoordModePrevious) {
            x2 += x1;
            y2 += y1;
        }

        oc2 = 0;
        MIOUTCODES (oc2, x2, y2, xleft, ytop, xright, ybottom);

        CalcLineDeltas (x1, y1, x2, y2, adx, ady, signdx, signdy, 1, 1, octant);

        if (adx > ady) {
            e1 = ady << 1;
            e2 = e1 - (adx << 1);
            e = e1 - adx;
            length = adx;       /* don't draw endpoint in main loop */

            FIXUP_ERROR (e, octant, bias);

            new_x1 = x1;
            new_y1 = y1;
            new_x2 = x2;
            new_y2 = y2;
            pt1_clipped = 0;
            pt2_clipped = 0;

            if ((oc1 | oc2) != 0) {
                result = miZeroClipLine (xleft, ytop, xright, ybottom,
                                         &new_x1, &new_y1, &new_x2, &new_y2,
                                         adx, ady,
                                         &pt1_clipped, &pt2_clipped, octant, bias, oc1, oc2);
                if (result == -1)
                    continue;

                length = abs (new_x2 - new_x1);

                /* if we've clipped the endpoint, always draw the full length
                 * of the segment, because then the capstyle doesn't matter
                 */
                if (pt2_clipped)
                    length++;

                if (pt1_clipped) {
                    /* must calculate new error terms */
                    clipdx = abs (new_x1 - x1);
                    clipdy = abs (new_y1 - y1);
                    e += (clipdy * e2) + ((clipdx - clipdy) * e1);
                }
            }

            /* draw the segment */

            x = new_x1;
            y = new_y1;

            e3 = e2 - e1;
            e = e - e1;

            while (length--) {
                MI_OUTPUT_POINT (x, y);
                e += e1;
                if (e >= 0) {
                    y += signdy;
                    e += e3;
                }
                x += signdx;
            }
        } else {                /* Y major line */

            e1 = adx << 1;
            e2 = e1 - (ady << 1);
            e = e1 - ady;
            length = ady;       /* don't draw endpoint in main loop */

            SetYMajorOctant (octant);
            FIXUP_ERROR (e, octant, bias);

            new_x1 = x1;
            new_y1 = y1;
            new_x2 = x2;
            new_y2 = y2;
            pt1_clipped = 0;
            pt2_clipped = 0;

            if ((oc1 | oc2) != 0) {
                result = miZeroClipLine (xleft, ytop, xright, ybottom,
                                         &new_x1, &new_y1, &new_x2, &new_y2,
                                         adx, ady,
                                         &pt1_clipped, &pt2_clipped, octant, bias, oc1, oc2);
                if (result == -1)
                    continue;

                length = abs (new_y2 - new_y1);

                /* if we've clipped the endpoint, always draw the full length
                 * of the segment, because then the capstyle doesn't matter
                 */
                if (pt2_clipped)
                    length++;

                if (pt1_clipped) {
                    /* must calculate new error terms */
                    clipdx = abs (new_x1 - x1);
                    clipdy = abs (new_y1 - y1);
                    e += (clipdx * e2) + ((clipdy - clipdx) * e1);
                }
            }

            /* draw the segment */

            x = new_x1;
            y = new_y1;

            e3 = e2 - e1;
            e = e - e1;

            while (length--) {
                MI_OUTPUT_POINT (x, y);
                e += e1;
                if (e >= 0) {
                    x += signdx;
                    e += e3;
                }
                y += signdy;
            }
        }
    }

    /* only do the capnotlast check on the last segment
     * and only if the endpoint wasn't clipped.  And then, if the last
     * point is the same as the first point, do not draw it, unless the
     * line is degenerate
     */
    if ((!pt2_clipped) && (pGC->capStyle != CapNotLast) &&
        (((xstart != x2) || (ystart != y2)) || (ppt == pptInit + 1))) {
        MI_OUTPUT_POINT (x, y);
    }

    if (Nspans > 0)
        (*pGC->ops->FillSpans) (pGC, Nspans, pspanInit, pwidthInit, FALSE, TRUE);

out:
    xfree (pwidthInit);
    xfree (pspanInit);
}

void
miZeroDashLine (GCPtr pgc, int mode, int nptInit,       /* number of points in polyline */
                DDXPointRec * pptInit   /* points in the polyline */
    )
{
    /* XXX kludge until real zero-width dash code is written */
    pgc->lineWidth = 1;
    miWideDash (pgc, mode, nptInit, pptInit);
    pgc->lineWidth = 0;
}

static void miLineArc (GCPtr pGC,
                       Boolean foreground, SpanDataPtr spanData,
                       LineFacePtr leftFace,
                       LineFacePtr rightFace, double xorg, double yorg, Boolean isInt);


/*
 * spans-based polygon filler
 */

static void
miFillPolyHelper (GCPtr pGC, Boolean foreground,
                  SpanDataPtr spanData, int y, int overall_height,
                  PolyEdgePtr left, PolyEdgePtr right, int left_count, int right_count)
{
    int left_x = 0, left_e = 0;
    int left_stepx = 0;
    int left_signdx = 0;
    int left_dy = 0, left_dx = 0;

    int right_x = 0, right_e = 0;
    int right_stepx = 0;
    int right_signdx = 0;
    int right_dy = 0, right_dx = 0;

    int height = 0;
    int left_height = 0, right_height = 0;

    DDXPointPtr ppt;
    DDXPointPtr pptInit = NULL;
    int *pwidth;
    int *pwidthInit = NULL;
    int xorg;
    Spans spanRec;

    left_height = 0;
    right_height = 0;

    if (!spanData) {
        pptInit = (DDXPointRec *)xalloc (overall_height * sizeof (*ppt));
        if (!pptInit)
            return;
        pwidthInit = (int *)xalloc (overall_height * sizeof (*pwidth));
        if (!pwidthInit) {
            xfree (pptInit);
            return;
        }
        ppt = pptInit;
        pwidth = pwidthInit;
    } else {
        spanRec.points = (DDXPointRec *)xalloc (overall_height * sizeof (*ppt));
        if (!spanRec.points)
            return;
        spanRec.widths = (int *)xalloc (overall_height * sizeof (int));
        if (!spanRec.widths) {
            xfree (spanRec.points);
            return;
        }
        ppt = spanRec.points;
        pwidth = spanRec.widths;
    }

    xorg = 0;
    while ((left_count || left_height) && (right_count || right_height)) {
        MIPOLYRELOADLEFT MIPOLYRELOADRIGHT height = left_height;
        if (height > right_height)
            height = right_height;

        left_height -= height;
        right_height -= height;

        while (--height >= 0) {
            if (right_x >= left_x) {
                ppt->y = y;
                ppt->x = left_x + xorg;
                ppt++;
                *pwidth++ = right_x - left_x + 1;
            }
            y++;

        MIPOLYSTEPLEFT MIPOLYSTEPRIGHT}
    }
    if (!spanData) {
        (*pGC->ops->FillSpans) (pGC, ppt - pptInit, pptInit, pwidthInit, TRUE, foreground);
        xfree (pwidthInit);
        xfree (pptInit);
    } else {
        spanRec.count = ppt - spanRec.points;
        AppendSpanGroup (pGC, foreground, &spanRec, spanData)
    }
}

static void
miFillRectPolyHelper (GCPtr pGC, Boolean foreground, SpanDataPtr spanData, int x, int y, int w, int h)
{
    DDXPointPtr ppt;
    int *pwidth;
    Spans spanRec;
    xRectangle rect;

    if (!spanData) {
        rect.x = x;
        rect.y = y;
        rect.width = w;
        rect.height = h;
        (*pGC->ops->FillRects) (pGC, 1, &rect, foreground);
    } else {
        spanRec.points = (DDXPointRec *)xalloc (h * sizeof (*ppt));
        if (!spanRec.points)
            return;
        spanRec.widths = (int *)xalloc (h * sizeof (int));
        if (!spanRec.widths) {
            xfree (spanRec.points);
            return;
        }
        ppt = spanRec.points;
        pwidth = spanRec.widths;

        while (h--) {
            ppt->x = x;
            ppt->y = y;
            ppt++;
            *pwidth++ = w;
            y++;
        }
        spanRec.count = ppt - spanRec.points;
        AppendSpanGroup (pGC, foreground, &spanRec, spanData)
    }
}

static int
miPolyBuildEdge (double x0, double y0, double k,        /* x0 * dy - y0 * dx */
                 int dx, int dy, int xi, int yi, int left, PolyEdgePtr edge)
{
    int x, y, e;
    int xady;

    if (dy < 0) {
        dy = -dy;
        dx = -dx;
        k = -k;
    }
#ifdef NOTDEF
    {
        double realk, kerror;
        realk = x0 * dy - y0 * dx;
        kerror = Fabs (realk - k);
        if (kerror > .1)
            printf ("realk: %g k: %g\n", realk, k);
    }
#endif
    y = ICEIL (y0);
    xady = ICEIL (k) + y * dx;

    if (xady <= 0)
        x = -(-xady / dy) - 1;
    else
        x = (xady - 1) / dy;

    e = xady - x * dy;

    if (dx >= 0) {
        edge->signdx = 1;
        edge->stepx = dx / dy;
        edge->dx = dx % dy;
    } else {
        edge->signdx = -1;
        edge->stepx = -(-dx / dy);
        edge->dx = -dx % dy;
        e = dy - e + 1;
    }
    edge->dy = dy;
    edge->x = x + left + xi;
    edge->e = e - dy;           /* bias to compare against 0 instead of dy */
    return y + yi;
}

#define StepAround(v, incr, max) (((v) + (incr) < 0) ? (max - 1) : ((v) + (incr) == max) ? 0 : ((v) + (incr)))

static int
miPolyBuildPoly (PolyVertexPtr vertices,
                 PolySlopePtr slopes,
                 int count,
                 int xi,
                 int yi, PolyEdgePtr left, PolyEdgePtr right, int *pnleft, int *pnright, int *h)
{
    int top, bottom;
    double miny, maxy;
    int i;
    int j;
    int clockwise;
    int slopeoff;
    int s;
    int nright, nleft;
    int y, lasty = 0, bottomy, topy = 0;

    /* find the top of the polygon */
    maxy = miny = vertices[0].y;
    bottom = top = 0;
    for (i = 1; i < count; i++) {
        if (vertices[i].y < miny) {
            top = i;
            miny = vertices[i].y;
        }
        if (vertices[i].y >= maxy) {
            bottom = i;
            maxy = vertices[i].y;
        }
    }
    clockwise = 1;
    slopeoff = 0;

    i = top;
    j = StepAround (top, -1, count);

    if (slopes[j].dy * slopes[i].dx > slopes[i].dy * slopes[j].dx) {
        clockwise = -1;
        slopeoff = -1;
    }

    bottomy = ICEIL (maxy) + yi;

    nright = 0;

    s = StepAround (top, slopeoff, count);
    i = top;
    while (i != bottom) {
        if (slopes[s].dy != 0) {
            y = miPolyBuildEdge (vertices[i].x, vertices[i].y,
                                 slopes[s].k,
                                 slopes[s].dx, slopes[s].dy, xi, yi, 0, &right[nright]);
            if (nright != 0)
                right[nright - 1].height = y - lasty;
            else
                topy = y;
            nright++;
            lasty = y;
        }

        i = StepAround (i, clockwise, count);
        s = StepAround (s, clockwise, count);
    }
    if (nright != 0)
        right[nright - 1].height = bottomy - lasty;

    if (slopeoff == 0)
        slopeoff = -1;
    else
        slopeoff = 0;

    nleft = 0;
    s = StepAround (top, slopeoff, count);
    i = top;
    while (i != bottom) {
        if (slopes[s].dy != 0) {
            y = miPolyBuildEdge (vertices[i].x, vertices[i].y,
                                 slopes[s].k, slopes[s].dx, slopes[s].dy, xi, yi, 1, &left[nleft]);

            if (nleft != 0)
                left[nleft - 1].height = y - lasty;
            nleft++;
            lasty = y;
        }
        i = StepAround (i, -clockwise, count);
        s = StepAround (s, -clockwise, count);
    }
    if (nleft != 0)
        left[nleft - 1].height = bottomy - lasty;
    *pnleft = nleft;
    *pnright = nright;
    *h = bottomy - topy;
    return topy;
}

static void
miLineOnePoint (GCPtr pGC, Boolean foreground, SpanDataPtr spanData, int x, int y)
{
    DDXPointRec pt;
    int wid;

    wid = 1;
    pt.x = x;
    pt.y = y;
    (*pGC->ops->FillSpans) (pGC, 1, &pt, &wid, TRUE, foreground);
}

static void
miLineJoin (GCPtr pGC, Boolean foreground, SpanDataPtr spanData, LineFacePtr pLeft, LineFacePtr pRight)
{
    double mx = 0, my = 0;
    double denom = 0.0;
    PolyVertexRec vertices[4];
    PolySlopeRec slopes[4];
    int edgecount;
    PolyEdgeRec left[4], right[4];
    int nleft, nright;
    int y, height;
    int swapslopes;
    int joinStyle = pGC->joinStyle;
    int lw = pGC->lineWidth;

    if (lw == 1 && !spanData) {
        /* See if one of the lines will draw the joining pixel */
        if (pLeft->dx > 0 || (pLeft->dx == 0 && pLeft->dy > 0))
            return;
        if (pRight->dx > 0 || (pRight->dx == 0 && pRight->dy > 0))
            return;
        if (joinStyle != JoinRound) {
            denom = -pLeft->dx * (double) pRight->dy + pRight->dx * (double) pLeft->dy;
            if (denom == 0)
                return;         /* no join to draw */
        }
        if (joinStyle != JoinMiter) {
            miLineOnePoint (pGC, foreground, spanData, pLeft->x, pLeft->y);
            return;
        }
    } else {
        if (joinStyle == JoinRound) {
            miLineArc (pGC, foreground, spanData, pLeft, pRight, (double) 0.0, (double) 0.0, TRUE);
            return;
        }
        denom = -pLeft->dx * (double) pRight->dy + pRight->dx * (double) pLeft->dy;
        if (denom == 0.0)
            return;             /* no join to draw */
    }

    swapslopes = 0;
    if (denom > 0) {
        pLeft->xa = -pLeft->xa;
        pLeft->ya = -pLeft->ya;
        pLeft->dx = -pLeft->dx;
        pLeft->dy = -pLeft->dy;
    } else {
        swapslopes = 1;
        pRight->xa = -pRight->xa;
        pRight->ya = -pRight->ya;
        pRight->dx = -pRight->dx;
        pRight->dy = -pRight->dy;
    }

    vertices[0].x = pRight->xa;
    vertices[0].y = pRight->ya;
    slopes[0].dx = -pRight->dy;
    slopes[0].dy = pRight->dx;
    slopes[0].k = 0;

    vertices[1].x = 0;
    vertices[1].y = 0;
    slopes[1].dx = pLeft->dy;
    slopes[1].dy = -pLeft->dx;
    slopes[1].k = 0;

    vertices[2].x = pLeft->xa;
    vertices[2].y = pLeft->ya;

    if (joinStyle == JoinMiter) {
        my = (pLeft->dy * (pRight->xa * pRight->dy - pRight->ya * pRight->dx) -
              pRight->dy * (pLeft->xa * pLeft->dy - pLeft->ya * pLeft->dx)) / denom;
        if (pLeft->dy != 0) {
            mx = pLeft->xa + (my - pLeft->ya) * (double) pLeft->dx / (double) pLeft->dy;
        } else {
            mx = pRight->xa + (my - pRight->ya) * (double) pRight->dx / (double) pRight->dy;
        }
        /* check miter limit */
        if ((mx * mx + my * my) * 4 > SQSECANT * lw * lw)
            joinStyle = JoinBevel;
    }

    if (joinStyle == JoinMiter) {
        slopes[2].dx = pLeft->dx;
        slopes[2].dy = pLeft->dy;
        slopes[2].k = pLeft->k;
        if (swapslopes) {
            slopes[2].dx = -slopes[2].dx;
            slopes[2].dy = -slopes[2].dy;
            slopes[2].k = -slopes[2].k;
        }
        vertices[3].x = mx;
        vertices[3].y = my;
        slopes[3].dx = pRight->dx;
        slopes[3].dy = pRight->dy;
        slopes[3].k = pRight->k;
        if (swapslopes) {
            slopes[3].dx = -slopes[3].dx;
            slopes[3].dy = -slopes[3].dy;
            slopes[3].k = -slopes[3].k;
        }
        edgecount = 4;
    } else {
        double scale, dx, dy, adx, ady;

        adx = dx = pRight->xa - pLeft->xa;
        ady = dy = pRight->ya - pLeft->ya;
        if (adx < 0)
            adx = -adx;
        if (ady < 0)
            ady = -ady;
        scale = ady;
        if (adx > ady)
            scale = adx;
        slopes[2].dx = (int) ((dx * 65536) / scale);
        slopes[2].dy = (int) ((dy * 65536) / scale);
        slopes[2].k = ((pLeft->xa + pRight->xa) * slopes[2].dy -
                       (pLeft->ya + pRight->ya) * slopes[2].dx) / 2.0;
        edgecount = 3;
    }

    y = miPolyBuildPoly (vertices, slopes, edgecount, pLeft->x, pLeft->y,
                         left, right, &nleft, &nright, &height);
    miFillPolyHelper (pGC, foreground, spanData, y, height, left, right, nleft, nright);
}

static int
miLineArcI (GCPtr pGC, int xorg, int yorg, DDXPointPtr points, int *widths)
{
    DDXPointPtr tpts, bpts;
    int *twids, *bwids;
    int x, y, e, ex, slw;

    tpts = points;
    twids = widths;
    slw = pGC->lineWidth;
    if (slw == 1) {
        tpts->x = xorg;
        tpts->y = yorg;
        *twids = 1;
        return 1;
    }
    bpts = tpts + slw;
    bwids = twids + slw;
    y = (slw >> 1) + 1;
    if (slw & 1)
        e = -((y << 2) + 3);
    else
        e = -(y << 3);
    ex = -4;
    x = 0;
    while (y) {
        e += (y << 3) - 4;
        while (e >= 0) {
            x++;
            e += (ex = -((x << 3) + 4));
        }
        y--;
        slw = (x << 1) + 1;
        if ((e == ex) && (slw > 1))
            slw--;
        tpts->x = xorg - x;
        tpts->y = yorg - y;
        tpts++;
        *twids++ = slw;
        if ((y != 0) && ((slw > 1) || (e != ex))) {
            bpts--;
            bpts->x = xorg - x;
            bpts->y = yorg + y;
            *--bwids = slw;
        }
    }
    return (pGC->lineWidth);
}

#define CLIPSTEPEDGE(edgey,edge,edgeleft) \
    if (ybase == edgey) \
    { \
        if (edgeleft) \
        { \
            if (edge->x > xcl) \
                xcl = edge->x; \
        } \
        else \
        { \
            if (edge->x < xcr) \
                xcr = edge->x; \
        } \
        edgey++; \
        edge->x += edge->stepx; \
        edge->e += edge->dx; \
        if (edge->e > 0) \
        { \
            edge->x += edge->signdx; \
            edge->e -= edge->dy; \
        } \
    }

static int
miLineArcD (GCPtr pGC,
            double xorg,
            double yorg,
            DDXPointPtr points,
            int *widths,
            PolyEdgePtr edge1,
            int edgey1, Boolean edgeleft1, PolyEdgePtr edge2, int edgey2, Boolean edgeleft2)
{
    DDXPointPtr pts;
    int *wids;
    double radius, x0, y0, el, er, yk, xlk, xrk, k;
    int xbase, ybase, y, boty, xl, xr, xcl, xcr;
    int ymin, ymax;
    Boolean edge1IsMin, edge2IsMin;
    int ymin1, ymin2;

    pts = points;
    wids = widths;
    xbase = (int)floor (xorg);
    x0 = xorg - xbase;
    ybase = ICEIL (yorg);
    y0 = yorg - ybase;
    xlk = x0 + x0 + 1.0;
    xrk = x0 + x0 - 1.0;
    yk = y0 + y0 - 1.0;
    radius = ((double) pGC->lineWidth) / 2.0;
    y = (int)floor (radius - y0 + 1.0);
    ybase -= y;
    ymin = ybase;
    ymax = 65536;
    edge1IsMin = FALSE;
    ymin1 = edgey1;
    if (edge1->dy >= 0) {
        if (!edge1->dy) {
            if (edgeleft1)
                edge1IsMin = TRUE;
            else
                ymax = edgey1;
            edgey1 = 65536;
        } else {
            if ((edge1->signdx < 0) == edgeleft1)
                edge1IsMin = TRUE;
        }
    }
    edge2IsMin = FALSE;
    ymin2 = edgey2;
    if (edge2->dy >= 0) {
        if (!edge2->dy) {
            if (edgeleft2)
                edge2IsMin = TRUE;
            else
                ymax = edgey2;
            edgey2 = 65536;
        } else {
            if ((edge2->signdx < 0) == edgeleft2)
                edge2IsMin = TRUE;
        }
    }
    if (edge1IsMin) {
        ymin = ymin1;
        if (edge2IsMin && ymin1 > ymin2)
            ymin = ymin2;
    } else if (edge2IsMin)
        ymin = ymin2;
    el = radius * radius - ((y + y0) * (y + y0)) - (x0 * x0);
    er = el + xrk;
    xl = 1;
    xr = 0;
    if (x0 < 0.5) {
        xl = 0;
        el -= xlk;
    }
    boty = (y0 < -0.5) ? 1 : 0;
    if (ybase + y - boty > ymax)
        boty = ymax - ybase - y;
    while (y > boty) {
        k = (y << 1) + yk;
        er += k;
        while (er > 0.0) {
            xr++;
            er += xrk - (xr << 1);
        }
        el += k;
        while (el >= 0.0) {
            xl--;
            el += (xl << 1) - xlk;
        }
        y--;
        ybase++;
        if (ybase < ymin)
            continue;
        xcl = xl + xbase;
        xcr = xr + xbase;
        CLIPSTEPEDGE (edgey1, edge1, edgeleft1);
        CLIPSTEPEDGE (edgey2, edge2, edgeleft2);
        if (xcr >= xcl) {
            pts->x = xcl;
            pts->y = ybase;
            pts++;
            *wids++ = xcr - xcl + 1;
        }
    }
    er = xrk - (xr << 1) - er;
    el = (xl << 1) - xlk - el;
    boty = (int)floor (-y0 - radius + 1.0);
    if (ybase + y - boty > ymax)
        boty = ymax - ybase - y;
    while (y > boty) {
        k = (y << 1) + yk;
        er -= k;
        while ((er >= 0.0) && (xr >= 0)) {
            xr--;
            er += xrk - (xr << 1);
        }
        el -= k;
        while ((el > 0.0) && (xl <= 0)) {
            xl++;
            el += (xl << 1) - xlk;
        }
        y--;
        ybase++;
        if (ybase < ymin)
            continue;
        xcl = xl + xbase;
        xcr = xr + xbase;
        CLIPSTEPEDGE (edgey1, edge1, edgeleft1);
        CLIPSTEPEDGE (edgey2, edge2, edgeleft2);
        if (xcr >= xcl) {
            pts->x = xcl;
            pts->y = ybase;
            pts++;
            *wids++ = xcr - xcl + 1;
        }
    }
    return (pts - points);
}

static int
miRoundJoinFace (LineFacePtr face, PolyEdgePtr edge, Boolean * leftEdge)
{
    int y;
    int dx, dy;
    double xa, ya;
    Boolean left;

    dx = -face->dy;
    dy = face->dx;
    xa = face->xa;
    ya = face->ya;
    left = 1;
    if (ya > 0) {
        ya = 0.0;
        xa = 0.0;
    }
    if (dy < 0 || (dy == 0 && dx > 0)) {
        dx = -dx;
        dy = -dy;
        left = !left;
    }
    if (dx == 0 && dy == 0)
        dy = 1;
    if (dy == 0) {
        y = ICEIL (face->ya) + face->y;
        edge->x = -32767;
        edge->stepx = 0;
        edge->signdx = 0;
        edge->e = -1;
        edge->dy = 0;
        edge->dx = 0;
        edge->height = 0;
    } else {
        y = miPolyBuildEdge (xa, ya, 0.0, dx, dy, face->x, face->y, !left, edge);
        edge->height = 32767;
    }
    *leftEdge = !left;
    return y;
}

static void
miRoundJoinClip (LineFacePtr pLeft, LineFacePtr pRight,
                 PolyEdgePtr edge1, PolyEdgePtr edge2, int *y1, int *y2, Boolean * left1, Boolean * left2)
{
    double denom;

    denom = -pLeft->dx * (double) pRight->dy + pRight->dx * (double) pLeft->dy;

    if (denom >= 0) {
        pLeft->xa = -pLeft->xa;
        pLeft->ya = -pLeft->ya;
    } else {
        pRight->xa = -pRight->xa;
        pRight->ya = -pRight->ya;
    }
    *y1 = miRoundJoinFace (pLeft, edge1, left1);
    *y2 = miRoundJoinFace (pRight, edge2, left2);
}

static int
miRoundCapClip (LineFacePtr face, Boolean isInt, PolyEdgePtr edge, Boolean * leftEdge)
{
    int y;
    int dx, dy;
    double xa, ya, k;
    Boolean left;

    dx = -face->dy;
    dy = face->dx;
    xa = face->xa;
    ya = face->ya;
    k = 0.0;
    if (!isInt)
        k = face->k;
    left = 1;
    if (dy < 0 || (dy == 0 && dx > 0)) {
        dx = -dx;
        dy = -dy;
        xa = -xa;
        ya = -ya;
        left = !left;
    }
    if (dx == 0 && dy == 0)
        dy = 1;
    if (dy == 0) {
        y = ICEIL (face->ya) + face->y;
        edge->x = -32767;
        edge->stepx = 0;
        edge->signdx = 0;
        edge->e = -1;
        edge->dy = 0;
        edge->dx = 0;
        edge->height = 0;
    } else {
        y = miPolyBuildEdge (xa, ya, k, dx, dy, face->x, face->y, !left, edge);
        edge->height = 32767;
    }
    *leftEdge = !left;
    return y;
}

static void
miLineArc (GCPtr pGC,
           Boolean foreground,
           SpanDataPtr spanData,
           LineFacePtr leftFace, LineFacePtr rightFace, double xorg, double yorg, Boolean isInt)
{
    DDXPointPtr points;
    int *widths;
    int xorgi = 0, yorgi = 0;
    Spans spanRec;
    int n;
    PolyEdgeRec edge1, edge2;
    int edgey1, edgey2;
    Boolean edgeleft1, edgeleft2;

    if (isInt) {
        xorgi = leftFace ? leftFace->x : rightFace->x;
        yorgi = leftFace ? leftFace->y : rightFace->y;
    }
    edgey1 = 65536;
    edgey2 = 65536;
    edge1.x = 0;                /* not used, keep memory checkers happy */
    edge1.dy = -1;
    edge2.x = 0;                /* not used, keep memory checkers happy */
    edge2.dy = -1;
    edgeleft1 = FALSE;
    edgeleft2 = FALSE;
    if ((pGC->lineStyle != LineSolid || pGC->lineWidth > 2) &&
        ((pGC->capStyle == CapRound && pGC->joinStyle != JoinRound) ||
         (pGC->joinStyle == JoinRound && pGC->capStyle == CapButt))) {
        if (isInt) {
            xorg = (double) xorgi;
            yorg = (double) yorgi;
        }
        if (leftFace && rightFace) {
            miRoundJoinClip (leftFace, rightFace, &edge1, &edge2,
                             &edgey1, &edgey2, &edgeleft1, &edgeleft2);
        } else if (leftFace) {
            edgey1 = miRoundCapClip (leftFace, isInt, &edge1, &edgeleft1);
        } else if (rightFace) {
            edgey2 = miRoundCapClip (rightFace, isInt, &edge2, &edgeleft2);
        }
        isInt = FALSE;
    }
    if (!spanData) {
        points = (DDXPointRec *)xalloc (sizeof (DDXPointRec) * pGC->lineWidth);
        if (!points)
            return;
        widths = (int *)xalloc (sizeof (int) * pGC->lineWidth);
        if (!widths) {
            xfree (points);
            return;
        }
    } else {
        points = (DDXPointRec *)xalloc (pGC->lineWidth * sizeof (DDXPointRec));
        if (!points)
            return;
        widths = (int *)xalloc (pGC->lineWidth * sizeof (int));
        if (!widths) {
            xfree (points);
            return;
        }
        spanRec.points = points;
        spanRec.widths = widths;
    }
    if (isInt)
        n = miLineArcI (pGC, xorgi, yorgi, points, widths);
    else
        n = miLineArcD (pGC, xorg, yorg, points, widths,
                        &edge1, edgey1, edgeleft1, &edge2, edgey2, edgeleft2);

    if (!spanData) {
        (*pGC->ops->FillSpans) (pGC, n, points, widths, TRUE, foreground);
        xfree (widths);
        xfree (points);
    } else {
        spanRec.count = n;
        AppendSpanGroup (pGC, foreground, &spanRec, spanData)
    }
}

static void
miLineProjectingCap (GCPtr pGC, Boolean foreground,
                     SpanDataPtr spanData, LineFacePtr face, Boolean isLeft,
                     double xorg, double yorg, Boolean isInt)
{
    int xorgi = 0, yorgi = 0;
    int lw;
    PolyEdgeRec lefts[2], rights[2];
    int lefty, righty, topy, bottomy;
    PolyEdgePtr left, right;
    PolyEdgePtr top, bottom;
    double xa, ya;
    double k;
    double xap, yap;
    int dx, dy;
    double projectXoff, projectYoff;
    double maxy;
    int finaly;

    if (isInt) {
        xorgi = face->x;
        yorgi = face->y;
    }
    lw = pGC->lineWidth;
    dx = face->dx;
    dy = face->dy;
    k = face->k;
    if (dy == 0) {
        lefts[0].height = lw;
        lefts[0].x = xorgi;
        if (isLeft)
            lefts[0].x -= (lw >> 1);
        lefts[0].stepx = 0;
        lefts[0].signdx = 1;
        lefts[0].e = -lw;
        lefts[0].dx = 0;
        lefts[0].dy = lw;
        rights[0].height = lw;
        rights[0].x = xorgi;
        if (!isLeft)
            rights[0].x += ((lw + 1) >> 1);
        rights[0].stepx = 0;
        rights[0].signdx = 1;
        rights[0].e = -lw;
        rights[0].dx = 0;
        rights[0].dy = lw;
        miFillPolyHelper (pGC, foreground, spanData, yorgi - (lw >> 1), lw, lefts, rights, 1, 1);
    } else if (dx == 0) {
        if (dy < 0) {
            dy = -dy;
            isLeft = !isLeft;
        }
        topy = yorgi;
        bottomy = yorgi + dy;
        if (isLeft)
            topy -= (lw >> 1);
        else
            bottomy += (lw >> 1);
        lefts[0].height = bottomy - topy;
        lefts[0].x = xorgi - (lw >> 1);
        lefts[0].stepx = 0;
        lefts[0].signdx = 1;
        lefts[0].e = -dy;
        lefts[0].dx = dx;
        lefts[0].dy = dy;

        rights[0].height = bottomy - topy;
        rights[0].x = lefts[0].x + (lw - 1);
        rights[0].stepx = 0;
        rights[0].signdx = 1;
        rights[0].e = -dy;
        rights[0].dx = dx;
        rights[0].dy = dy;
        miFillPolyHelper (pGC, foreground, spanData, topy, bottomy - topy, lefts, rights, 1, 1);
    } else {
        xa = face->xa;
        ya = face->ya;
        projectXoff = -ya;
        projectYoff = xa;
        if (dx < 0) {
            right = &rights[1];
            left = &lefts[0];
            top = &rights[0];
            bottom = &lefts[1];
        } else {
            right = &rights[0];
            left = &lefts[1];
            top = &lefts[0];
            bottom = &rights[1];
        }
        if (isLeft) {
            righty = miPolyBuildEdge (xa, ya, k, dx, dy, xorgi, yorgi, 0, right);

            xa = -xa;
            ya = -ya;
            k = -k;
            lefty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
                                     k, dx, dy, xorgi, yorgi, 1, left);
            if (dx > 0) {
                ya = -ya;
                xa = -xa;
            }
            xap = xa - projectXoff;
            yap = ya - projectYoff;
            topy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
                                    -dy, dx, xorgi, yorgi, dx > 0, top);
            bottomy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, xorgi, yorgi, dx < 0, bottom);
            maxy = -ya;
        } else {
            righty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
                                      k, dx, dy, xorgi, yorgi, 0, right);

            xa = -xa;
            ya = -ya;
            k = -k;
            lefty = miPolyBuildEdge (xa, ya, k, dx, dy, xorgi, yorgi, 1, left);
            if (dx > 0) {
                ya = -ya;
                xa = -xa;
            }
            xap = xa - projectXoff;
            yap = ya - projectYoff;
            topy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, xorgi, xorgi, dx > 0, top);
            bottomy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
                                       -dy, dx, xorgi, xorgi, dx < 0, bottom);
            maxy = -ya + projectYoff;
        }
        finaly = ICEIL (maxy) + yorgi;
        if (dx < 0) {
            left->height = bottomy - lefty;
            right->height = finaly - righty;
            top->height = righty - topy;
        } else {
            right->height = bottomy - righty;
            left->height = finaly - lefty;
            top->height = lefty - topy;
        }
        bottom->height = finaly - bottomy;
        miFillPolyHelper (pGC, foreground, spanData, topy,
                          bottom->height + bottomy - topy, lefts, rights, 2, 2);
    }
}

static void
miWideSegment (GCPtr pGC,
               Boolean foreground,
               SpanDataPtr spanData,
               int x1,
               int y1,
               int x2,
               int y2,
               Boolean projectLeft, Boolean projectRight, LineFacePtr leftFace, LineFacePtr rightFace)
{
    double l, L, r;
    double xa, ya;
    double projectXoff = 0.0, projectYoff = 0.0;
    double k;
    double maxy;
    int x, y;
    int dx, dy;
    int finaly;
    PolyEdgePtr left, right;
    PolyEdgePtr top, bottom;
    int lefty, righty, topy, bottomy;
    int signdx;
    PolyEdgeRec lefts[2], rights[2];
    LineFacePtr tface;
    int lw = pGC->lineWidth;

    /* draw top-to-bottom always */
    if (y2 < y1 || (y2 == y1 && x2 < x1)) {
        x = x1;
        x1 = x2;
        x2 = x;

        y = y1;
        y1 = y2;
        y2 = y;

        x = projectLeft;
        projectLeft = projectRight;
        projectRight = x;

        tface = leftFace;
        leftFace = rightFace;
        rightFace = tface;
    }

    dy = y2 - y1;
    signdx = 1;
    dx = x2 - x1;
    if (dx < 0)
        signdx = -1;

    leftFace->x = x1;
    leftFace->y = y1;
    leftFace->dx = dx;
    leftFace->dy = dy;

    rightFace->x = x2;
    rightFace->y = y2;
    rightFace->dx = -dx;
    rightFace->dy = -dy;

    if (dy == 0) {
        rightFace->xa = 0;
        rightFace->ya = (double) lw / 2.0;
        rightFace->k = -(double) (lw * dx) / 2.0;
        leftFace->xa = 0;
        leftFace->ya = -rightFace->ya;
        leftFace->k = rightFace->k;
        x = x1;
        if (projectLeft)
            x -= (lw >> 1);
        y = y1 - (lw >> 1);
        dx = x2 - x;
        if (projectRight)
            dx += ((lw + 1) >> 1);
        dy = lw;
        miFillRectPolyHelper (pGC, foreground, spanData, x, y, dx, dy);
    } else if (dx == 0) {
        leftFace->xa = (double) lw / 2.0;
        leftFace->ya = 0;
        leftFace->k = (double) (lw * dy) / 2.0;
        rightFace->xa = -leftFace->xa;
        rightFace->ya = 0;
        rightFace->k = leftFace->k;
        y = y1;
        if (projectLeft)
            y -= lw >> 1;
        x = x1 - (lw >> 1);
        dy = y2 - y;
        if (projectRight)
            dy += ((lw + 1) >> 1);
        dx = lw;
        miFillRectPolyHelper (pGC, foreground, spanData, x, y, dx, dy);
    } else {
        l = ((double) lw) / 2.0;
        L = hypot ((double) dx, (double) dy);

        if (dx < 0) {
            right = &rights[1];
            left = &lefts[0];
            top = &rights[0];
            bottom = &lefts[1];
        } else {
            right = &rights[0];
            left = &lefts[1];
            top = &lefts[0];
            bottom = &rights[1];
        }
        r = l / L;

        /* coord of upper bound at integral y */
        ya = -r * dx;
        xa = r * dy;

        if (projectLeft | projectRight) {
            projectXoff = -ya;
            projectYoff = xa;
        }

        /* xa * dy - ya * dx */
        k = l * L;

        leftFace->xa = xa;
        leftFace->ya = ya;
        leftFace->k = k;
        rightFace->xa = -xa;
        rightFace->ya = -ya;
        rightFace->k = k;

        if (projectLeft)
            righty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
                                      k, dx, dy, x1, y1, 0, right);
        else
            righty = miPolyBuildEdge (xa, ya, k, dx, dy, x1, y1, 0, right);

        /* coord of lower bound at integral y */
        ya = -ya;
        xa = -xa;

        /* xa * dy - ya * dx */
        k = -k;

        if (projectLeft)
            lefty = miPolyBuildEdge (xa - projectXoff, ya - projectYoff,
                                     k, dx, dy, x1, y1, 1, left);
        else
            lefty = miPolyBuildEdge (xa, ya, k, dx, dy, x1, y1, 1, left);

        /* coord of top face at integral y */

        if (signdx > 0) {
            ya = -ya;
            xa = -xa;
        }

        if (projectLeft) {
            double xap = xa - projectXoff;
            double yap = ya - projectYoff;
            topy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy, -dy, dx, x1, y1, dx > 0, top);
        } else
            topy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, x1, y1, dx > 0, top);

        /* coord of bottom face at integral y */

        if (projectRight) {
            double xap = xa + projectXoff;
            double yap = ya + projectYoff;
            bottomy = miPolyBuildEdge (xap, yap, xap * dx + yap * dy,
                                       -dy, dx, x2, y2, dx < 0, bottom);
            maxy = -ya + projectYoff;
        } else {
            bottomy = miPolyBuildEdge (xa, ya, 0.0, -dy, dx, x2, y2, dx < 0, bottom);
            maxy = -ya;
        }

        finaly = ICEIL (maxy) + y2;

        if (dx < 0) {
            left->height = bottomy - lefty;
            right->height = finaly - righty;
            top->height = righty - topy;
        } else {
            right->height = bottomy - righty;
            left->height = finaly - lefty;
            top->height = lefty - topy;
        }
        bottom->height = finaly - bottomy;
        miFillPolyHelper (pGC, foreground, spanData, topy,
                          bottom->height + bottomy - topy, lefts, rights, 2, 2);
    }
}

static SpanDataPtr
miSetupSpanData (GCPtr pGC, SpanDataPtr spanData, int npt)
{
    if ((npt < 3 && pGC->capStyle != CapRound) || miSpansEasyRop (pGC->alu))
        return (SpanDataPtr) NULL;
    if (pGC->lineStyle == LineDoubleDash)
        miInitSpanGroup (&spanData->bgGroup);
    miInitSpanGroup (&spanData->fgGroup);
    return spanData;
}

static void
miCleanupSpanData (GCPtr pGC, SpanDataPtr spanData)
{
    if (pGC->lineStyle == LineDoubleDash) {
        miFillUniqueSpanGroup (pGC, &spanData->bgGroup, FALSE);
        miFreeSpanGroup (&spanData->bgGroup);
    }
    miFillUniqueSpanGroup (pGC, &spanData->fgGroup, TRUE);
    miFreeSpanGroup (&spanData->fgGroup);
}

void
miWideLine (GCPtr pGC, int mode, int npt, DDXPointPtr pPts)
{
    int x1, y1, x2, y2;
    SpanDataRec spanDataRec;
    SpanDataPtr spanData;
    Boolean projectLeft, projectRight;
    LineFaceRec leftFace, rightFace, prevRightFace;
    LineFaceRec firstFace;
    int first;
    Boolean somethingDrawn = FALSE;
    Boolean selfJoin;

    spanData = miSetupSpanData (pGC, &spanDataRec, npt);
    x2 = pPts->x;
    y2 = pPts->y;
    first = TRUE;
    selfJoin = FALSE;
    if (npt > 1) {
        if (mode == CoordModePrevious) {
            int nptTmp;
            DDXPointPtr pPtsTmp;

            x1 = x2;
            y1 = y2;
            nptTmp = npt;
            pPtsTmp = pPts + 1;
            while (--nptTmp) {
                x1 += pPtsTmp->x;
                y1 += pPtsTmp->y;
                ++pPtsTmp;
            }
            if (x2 == x1 && y2 == y1)
                selfJoin = TRUE;
        } else if (x2 == pPts[npt - 1].x && y2 == pPts[npt - 1].y) {
            selfJoin = TRUE;
        }
    }
    projectLeft = pGC->capStyle == CapProjecting && !selfJoin;
    projectRight = FALSE;
    while (--npt) {
        x1 = x2;
        y1 = y2;
        ++pPts;
        x2 = pPts->x;
        y2 = pPts->y;
        if (mode == CoordModePrevious) {
            x2 += x1;
            y2 += y1;
        }
        if (x1 != x2 || y1 != y2) {
            somethingDrawn = TRUE;
            if (npt == 1 && pGC->capStyle == CapProjecting && !selfJoin)
                projectRight = TRUE;
            miWideSegment (pGC, TRUE, spanData, x1, y1, x2, y2,
                           projectLeft, projectRight, &leftFace, &rightFace);
            if (first) {
                if (selfJoin)
                    firstFace = leftFace;
                else if (pGC->capStyle == CapRound) {
                    if (pGC->lineWidth == 1 && !spanData)
                        miLineOnePoint (pGC, TRUE, spanData, x1, y1);
                    else
                        miLineArc (pGC, TRUE, spanData,
                                   &leftFace, (LineFacePtr) NULL, (double) 0.0, (double) 0.0, TRUE);
                }
            } else {
                miLineJoin (pGC, TRUE, spanData, &leftFace, &prevRightFace);
            }
            prevRightFace = rightFace;
            first = FALSE;
            projectLeft = FALSE;
        }
        if (npt == 1 && somethingDrawn) {
            if (selfJoin)
                miLineJoin (pGC, TRUE, spanData, &firstFace, &rightFace);
            else if (pGC->capStyle == CapRound) {
                if (pGC->lineWidth == 1 && !spanData)
                    miLineOnePoint (pGC, TRUE, spanData, x2, y2);
                else
                    miLineArc (pGC, TRUE, spanData,
                               (LineFacePtr) NULL, &rightFace, (double) 0.0, (double) 0.0, TRUE);
            }
        }
    }
    /* handle crock where all points are coincedent */
    if (!somethingDrawn) {
        projectLeft = pGC->capStyle == CapProjecting;
        miWideSegment (pGC, TRUE, spanData,
                       x2, y2, x2, y2, projectLeft, projectLeft, &leftFace, &rightFace);
        if (pGC->capStyle == CapRound) {
            miLineArc (pGC, TRUE, spanData,
                       &leftFace, (LineFacePtr) NULL, (double) 0.0, (double) 0.0, TRUE);
            rightFace.dx = -1;  /* sleezy hack to make it work */
            miLineArc (pGC, TRUE, spanData,
                       (LineFacePtr) NULL, &rightFace, (double) 0.0, (double) 0.0, TRUE);
        }
    }
    if (spanData)
        miCleanupSpanData (pGC, spanData);
}

#define V_TOP       0
#define V_RIGHT     1
#define V_BOTTOM    2
#define V_LEFT      3

static void
miWideDashSegment (GCPtr pGC,
                   SpanDataPtr spanData,
                   int *pDashOffset,
                   int *pDashIndex,
                   int x1,
                   int y1,
                   int x2,
                   int y2,
                   Boolean projectLeft, Boolean projectRight, LineFacePtr leftFace, LineFacePtr rightFace)
{
    int dashIndex, dashRemain;
    unsigned char *pDash;
    double L, l;
    double k;
    PolyVertexRec vertices[4];
    PolyVertexRec saveRight = { 0 }, saveBottom;
    PolySlopeRec slopes[4];
    PolyEdgeRec left[2], right[2];
    LineFaceRec lcapFace, rcapFace;
    int nleft, nright;
    int h;
    int y;
    int dy, dx;
    Boolean foreground;
    double LRemain;
    double r;
    double rdx, rdy;
    double dashDx, dashDy;
    double saveK = 0.0;
    Boolean first = TRUE;
    double lcenterx, lcentery, rcenterx = 0.0, rcentery = 0.0;

    dx = x2 - x1;
    dy = y2 - y1;
    dashIndex = *pDashIndex;
    pDash = pGC->dash;
    dashRemain = pDash[dashIndex] - *pDashOffset;

    l = ((double) pGC->lineWidth) / 2.0;
    if (dx == 0) {
        L = dy;
        rdx = 0;
        rdy = l;
        if (dy < 0) {
            L = -dy;
            rdy = -l;
        }
    } else if (dy == 0) {
        L = dx;
        rdx = l;
        rdy = 0;
        if (dx < 0) {
            L = -dx;
            rdx = -l;
        }
    } else {
        L = hypot ((double) dx, (double) dy);
        r = l / L;

        rdx = r * dx;
        rdy = r * dy;
    }
    k = l * L;
    LRemain = L;
    /* All position comments are relative to a line with dx and dy > 0,
     * but the code does not depend on this */
    /* top */
    slopes[V_TOP].dx = dx;
    slopes[V_TOP].dy = dy;
    slopes[V_TOP].k = k;
    /* right */
    slopes[V_RIGHT].dx = -dy;
    slopes[V_RIGHT].dy = dx;
    slopes[V_RIGHT].k = 0;
    /* bottom */
    slopes[V_BOTTOM].dx = -dx;
    slopes[V_BOTTOM].dy = -dy;
    slopes[V_BOTTOM].k = k;
    /* left */
    slopes[V_LEFT].dx = dy;
    slopes[V_LEFT].dy = -dx;
    slopes[V_LEFT].k = 0;

    /* preload the start coordinates */
    vertices[V_RIGHT].x = vertices[V_TOP].x = rdy;
    vertices[V_RIGHT].y = vertices[V_TOP].y = -rdx;

    vertices[V_BOTTOM].x = vertices[V_LEFT].x = -rdy;
    vertices[V_BOTTOM].y = vertices[V_LEFT].y = rdx;

    if (projectLeft) {
        vertices[V_TOP].x -= rdx;
        vertices[V_TOP].y -= rdy;

        vertices[V_LEFT].x -= rdx;
        vertices[V_LEFT].y -= rdy;

        slopes[V_LEFT].k = rdx * dx + rdy * dy;
    }

    lcenterx = x1;
    lcentery = y1;

    if (pGC->capStyle == CapRound) {
        lcapFace.dx = dx;
        lcapFace.dy = dy;
        lcapFace.x = x1;
        lcapFace.y = y1;

        rcapFace.dx = -dx;
        rcapFace.dy = -dy;
        rcapFace.x = x1;
        rcapFace.y = y1;
    }
    while (LRemain > dashRemain) {
        dashDx = (dashRemain * dx) / L;
        dashDy = (dashRemain * dy) / L;

        rcenterx = lcenterx + dashDx;
        rcentery = lcentery + dashDy;

        vertices[V_RIGHT].x += dashDx;
        vertices[V_RIGHT].y += dashDy;

        vertices[V_BOTTOM].x += dashDx;
        vertices[V_BOTTOM].y += dashDy;

        slopes[V_RIGHT].k = vertices[V_RIGHT].x * dx + vertices[V_RIGHT].y * dy;

        if (pGC->lineStyle == LineDoubleDash || !(dashIndex & 1)) {
            if (pGC->lineStyle == LineOnOffDash && pGC->capStyle == CapProjecting) {
                saveRight = vertices[V_RIGHT];
                saveBottom = vertices[V_BOTTOM];
                saveK = slopes[V_RIGHT].k;

                if (!first) {
                    vertices[V_TOP].x -= rdx;
                    vertices[V_TOP].y -= rdy;

                    vertices[V_LEFT].x -= rdx;
                    vertices[V_LEFT].y -= rdy;

                    slopes[V_LEFT].k = vertices[V_LEFT].x *
                        slopes[V_LEFT].dy - vertices[V_LEFT].y * slopes[V_LEFT].dx;
                }

                vertices[V_RIGHT].x += rdx;
                vertices[V_RIGHT].y += rdy;

                vertices[V_BOTTOM].x += rdx;
                vertices[V_BOTTOM].y += rdy;

                slopes[V_RIGHT].k = vertices[V_RIGHT].x *
                    slopes[V_RIGHT].dy - vertices[V_RIGHT].y * slopes[V_RIGHT].dx;
            }
            y = miPolyBuildPoly (vertices, slopes, 4, x1, y1, left, right, &nleft, &nright, &h);
            foreground = (dashIndex & 1) == 0;
            miFillPolyHelper (pGC, foreground, spanData, y, h, left, right, nleft, nright);

            if (pGC->lineStyle == LineOnOffDash) {
                switch (pGC->capStyle) {
                case CapProjecting:
                    vertices[V_BOTTOM] = saveBottom;
                    vertices[V_RIGHT] = saveRight;
                    slopes[V_RIGHT].k = saveK;
                    break;
                case CapRound:
                    if (!first) {
                        if (dx < 0) {
                            lcapFace.xa = -vertices[V_LEFT].x;
                            lcapFace.ya = -vertices[V_LEFT].y;
                            lcapFace.k = slopes[V_LEFT].k;
                        } else {
                            lcapFace.xa = vertices[V_TOP].x;
                            lcapFace.ya = vertices[V_TOP].y;
                            lcapFace.k = -slopes[V_LEFT].k;
                        }
                        miLineArc (pGC, foreground, spanData,
                                   &lcapFace, (LineFacePtr) NULL, lcenterx, lcentery, FALSE);
                    }
                    if (dx < 0) {
                        rcapFace.xa = vertices[V_BOTTOM].x;
                        rcapFace.ya = vertices[V_BOTTOM].y;
                        rcapFace.k = slopes[V_RIGHT].k;
                    } else {
                        rcapFace.xa = -vertices[V_RIGHT].x;
                        rcapFace.ya = -vertices[V_RIGHT].y;
                        rcapFace.k = -slopes[V_RIGHT].k;
                    }
                    miLineArc (pGC, foreground, spanData,
                               (LineFacePtr) NULL, &rcapFace, rcenterx, rcentery, FALSE);
                    break;
                }
            }
        }
        LRemain -= dashRemain;
        ++dashIndex;
        if (dashIndex == pGC->numInDashList)
            dashIndex = 0;
        dashRemain = pDash[dashIndex];

        lcenterx = rcenterx;
        lcentery = rcentery;

        vertices[V_TOP] = vertices[V_RIGHT];
        vertices[V_LEFT] = vertices[V_BOTTOM];
        slopes[V_LEFT].k = -slopes[V_RIGHT].k;
        first = FALSE;
    }

    if (pGC->lineStyle == LineDoubleDash || !(dashIndex & 1)) {
        vertices[V_TOP].x -= dx;
        vertices[V_TOP].y -= dy;

        vertices[V_LEFT].x -= dx;
        vertices[V_LEFT].y -= dy;

        vertices[V_RIGHT].x = rdy;
        vertices[V_RIGHT].y = -rdx;

        vertices[V_BOTTOM].x = -rdy;
        vertices[V_BOTTOM].y = rdx;


        if (projectRight) {
            vertices[V_RIGHT].x += rdx;
            vertices[V_RIGHT].y += rdy;

            vertices[V_BOTTOM].x += rdx;
            vertices[V_BOTTOM].y += rdy;
            slopes[V_RIGHT].k = vertices[V_RIGHT].x *
                slopes[V_RIGHT].dy - vertices[V_RIGHT].y * slopes[V_RIGHT].dx;
        } else
            slopes[V_RIGHT].k = 0;

        if (!first && pGC->lineStyle == LineOnOffDash && pGC->capStyle == CapProjecting) {
            vertices[V_TOP].x -= rdx;
            vertices[V_TOP].y -= rdy;

            vertices[V_LEFT].x -= rdx;
            vertices[V_LEFT].y -= rdy;
            slopes[V_LEFT].k = vertices[V_LEFT].x *
                slopes[V_LEFT].dy - vertices[V_LEFT].y * slopes[V_LEFT].dx;
        } else
            slopes[V_LEFT].k += dx * dx + dy * dy;


        y = miPolyBuildPoly (vertices, slopes, 4, x2, y2, left, right, &nleft, &nright, &h);

        foreground = (dashIndex & 1) == 0;
        miFillPolyHelper (pGC, foreground, spanData, y, h, left, right, nleft, nright);
        if (!first && pGC->lineStyle == LineOnOffDash && pGC->capStyle == CapRound) {
            lcapFace.x = x2;
            lcapFace.y = y2;
            if (dx < 0) {
                lcapFace.xa = -vertices[V_LEFT].x;
                lcapFace.ya = -vertices[V_LEFT].y;
                lcapFace.k = slopes[V_LEFT].k;
            } else {
                lcapFace.xa = vertices[V_TOP].x;
                lcapFace.ya = vertices[V_TOP].y;
                lcapFace.k = -slopes[V_LEFT].k;
            }
            miLineArc (pGC, foreground, spanData,
                       &lcapFace, (LineFacePtr) NULL, rcenterx, rcentery, FALSE);
        }
    }
    dashRemain = (int)(((double) dashRemain) - LRemain);
    if (dashRemain == 0) {
        dashIndex++;
        if (dashIndex == pGC->numInDashList)
            dashIndex = 0;
        dashRemain = pDash[dashIndex];
    }

    leftFace->x = x1;
    leftFace->y = y1;
    leftFace->dx = dx;
    leftFace->dy = dy;
    leftFace->xa = rdy;
    leftFace->ya = -rdx;
    leftFace->k = k;

    rightFace->x = x2;
    rightFace->y = y2;
    rightFace->dx = -dx;
    rightFace->dy = -dy;
    rightFace->xa = -rdy;
    rightFace->ya = rdx;
    rightFace->k = k;

    *pDashIndex = dashIndex;
    *pDashOffset = pDash[dashIndex] - dashRemain;
}

void
miWideDash (GCPtr pGC, int mode, int npt, DDXPointPtr pPts)
{
    int x1, y1, x2, y2;
    Boolean foreground;
    Boolean projectLeft, projectRight;
    LineFaceRec leftFace, rightFace, prevRightFace;
    LineFaceRec firstFace;
    int first;
    int dashIndex, dashOffset;
    int prevDashIndex;
    SpanDataRec spanDataRec;
    SpanDataPtr spanData;
    Boolean somethingDrawn = FALSE;
    Boolean selfJoin;
    Boolean endIsFg = FALSE, startIsFg = FALSE;
    Boolean firstIsFg = FALSE, prevIsFg = FALSE;

    if (npt == 0)
        return;
    spanData = miSetupSpanData (pGC, &spanDataRec, npt);
    x2 = pPts->x;
    y2 = pPts->y;
    first = TRUE;
    selfJoin = FALSE;
    if (mode == CoordModePrevious) {
        int nptTmp;
        DDXPointPtr pPtsTmp;

        x1 = x2;
        y1 = y2;
        nptTmp = npt;
        pPtsTmp = pPts + 1;
        while (--nptTmp) {
            x1 += pPtsTmp->x;
            y1 += pPtsTmp->y;
            ++pPtsTmp;
        }
        if (x2 == x1 && y2 == y1)
            selfJoin = TRUE;
    } else if (x2 == pPts[npt - 1].x && y2 == pPts[npt - 1].y) {
        selfJoin = TRUE;
    }
    projectLeft = pGC->capStyle == CapProjecting && !selfJoin;
    projectRight = FALSE;
    dashIndex = 0;
    dashOffset = 0;
    miStepDash ((int) pGC->dashOffset, &dashIndex,
                pGC->dash, (int) pGC->numInDashList, &dashOffset);
    while (--npt) {
        x1 = x2;
        y1 = y2;
        ++pPts;
        x2 = pPts->x;
        y2 = pPts->y;
        if (mode == CoordModePrevious) {
            x2 += x1;
            y2 += y1;
        }
        if (x1 != x2 || y1 != y2) {
            somethingDrawn = TRUE;
            if (npt == 1 && pGC->capStyle == CapProjecting && (!selfJoin || !firstIsFg))
                projectRight = TRUE;
            prevDashIndex = dashIndex;
            miWideDashSegment (pGC, spanData, &dashOffset, &dashIndex,
                               x1, y1, x2, y2, projectLeft, projectRight, &leftFace, &rightFace);
            startIsFg = !(prevDashIndex & 1);
            endIsFg = (dashIndex & 1) ^ (dashOffset != 0);
            if (pGC->lineStyle == LineDoubleDash || startIsFg) {
                foreground = startIsFg;
                if (first || (pGC->lineStyle == LineOnOffDash && !prevIsFg)) {
                    if (first && selfJoin) {
                        firstFace = leftFace;
                        firstIsFg = startIsFg;
                    } else if (pGC->capStyle == CapRound)
                        miLineArc (pGC, foreground, spanData,
                                   &leftFace, (LineFacePtr) NULL, (double) 0.0, (double) 0.0, TRUE);
                } else {
                    miLineJoin (pGC, foreground, spanData, &leftFace, &prevRightFace);
                }
            }
            prevRightFace = rightFace;
            prevIsFg = endIsFg;
            first = FALSE;
            projectLeft = FALSE;
        }
        if (npt == 1 && somethingDrawn) {
            if (pGC->lineStyle == LineDoubleDash || endIsFg) {
                foreground = endIsFg;
                if (selfJoin && (pGC->lineStyle == LineDoubleDash || firstIsFg)) {
                    miLineJoin (pGC, foreground, spanData, &firstFace, &rightFace);
                } else {
                    if (pGC->capStyle == CapRound)
                        miLineArc (pGC, foreground, spanData,
                                   (LineFacePtr) NULL, &rightFace,
                                   (double) 0.0, (double) 0.0, TRUE);
                }
            } else {
                /* glue a cap to the start of the line if
                 * we're OnOffDash and ended on odd dash
                 */
                if (selfJoin && firstIsFg) {
                    foreground = TRUE;
                    if (pGC->capStyle == CapProjecting)
                        miLineProjectingCap (pGC, foreground, spanData,
                                             &firstFace, TRUE, (double) 0.0, (double) 0.0, TRUE);
                    else if (pGC->capStyle == CapRound)
                        miLineArc (pGC, foreground, spanData,
                                   &firstFace, (LineFacePtr) NULL,
                                   (double) 0.0, (double) 0.0, TRUE);
                }
            }
        }
    }
    /* handle crock where all points are coincident */
    if (!somethingDrawn && (pGC->lineStyle == LineDoubleDash || !(dashIndex & 1))) {
        /* not the same as endIsFg computation above */
        foreground = (dashIndex & 1) == 0;
        switch (pGC->capStyle) {
        case CapRound:
            miLineArc (pGC, foreground, spanData,
                       (LineFacePtr) NULL, (LineFacePtr) NULL, (double) x2, (double) y2, FALSE);
            break;
        case CapProjecting:
            x1 = pGC->lineWidth;
            miFillRectPolyHelper (pGC, foreground, spanData,
                                  x2 - (x1 >> 1), y2 - (x1 >> 1), x1, x1);
            break;
        }
    }
    if (spanData)
        miCleanupSpanData (pGC, spanData);
}

#undef ExchangeSpans
#define ExchangeSpans(a, b)                                 \
{                                                           \
    DDXPointRec tpt;                                        \
    int         tw;                                         \
                                                            \
    tpt = spans[a]; spans[a] = spans[b]; spans[b] = tpt;    \
    tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;  \
}

static void QuickSortSpans(
    DDXPointRec spans[],
    int         widths[],
    int         numSpans)
{
    int     y;
    int     i, j, m;
    DDXPointPtr    r;

    /* Always called with numSpans > 1 */
    /* Sorts only by y, doesn't bother to sort by x */

    do
    {
        if (numSpans < 9)
        {
            /* Do insertion sort */
            int yprev;

            yprev = spans[0].y;
            i = 1;
            do
            { /* while i != numSpans */
                y = spans[i].y;
                if (yprev > y)
                {
                    /* spans[i] is out of order.  Move into proper location. */
                    DDXPointRec tpt;
                    int     tw, k;

                    for (j = 0; y >= spans[j].y; j++) {}
                    tpt = spans[i];
                    tw  = widths[i];
                    for (k = i; k != j; k--)
                    {
                        spans[k] = spans[k-1];
                        widths[k] = widths[k-1];
                    }
                    spans[j] = tpt;
                    widths[j] = tw;
                    y = spans[i].y;
                } /* if out of order */
                yprev = y;
                i++;
            } while (i != numSpans);
            return;
        }

        /* Choose partition element, stick in location 0 */
        m = numSpans / 2;
        if (spans[m].y > spans[0].y)            ExchangeSpans(m, 0);
        if (spans[m].y > spans[numSpans-1].y)   ExchangeSpans(m, numSpans-1);
        if (spans[m].y > spans[0].y)            ExchangeSpans(m, 0);
        y = spans[0].y;

        /* Partition array */
        i = 0;
        j = numSpans;
        do
        {
            r = &(spans[i]);
            do
            {
                r++;
                i++;
            } while (i != numSpans && r->y < y);
            r = &(spans[j]);
            do
            {
                r--;
                j--;
            } while (y < r->y);
            if (i < j)
                ExchangeSpans(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeSpans(0, j);

        /* Recurse */
        if (numSpans-j-1 > 1)
            QuickSortSpans(&spans[j+1], &widths[j+1], numSpans-j-1);
        numSpans = j;
    } while (numSpans > 1);
}

#define NextBand()                                                  \
{                                                                   \
    clipy1 = pboxBandStart->y1;                                     \
    clipy2 = pboxBandStart->y2;                                     \
    pboxBandEnd = pboxBandStart + 1;                                \
    while (pboxBandEnd != pboxLast && pboxBandEnd->y1 == clipy1) {  \
        pboxBandEnd++;                                              \
    }                                                               \
    for (; ppt != pptLast && ppt->y < clipy1; ppt++, pwidth++) {} \
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

int spice_canvas_clip_spans(pixman_region32_t *prgnDst,
                            DDXPointPtr ppt,
                            int         *pwidth,
                            int                 nspans,
                            DDXPointPtr         pptNew,
                            int                 *pwidthNew,
                            int                 fSorted)
{
    DDXPointPtr pptLast;
    int         *pwidthNewStart;        /* the vengeance of Xerox! */
    int         y, x1, x2;
    int         numRects;
    pixman_box32_t *pboxBandStart;

    pptLast = ppt + nspans;
    pwidthNewStart = pwidthNew;

    pboxBandStart = pixman_region32_rectangles (prgnDst, &numRects);

    if (numRects == 1) {
        /* Do special fast code with clip boundaries in registers(?) */
        /* It doesn't pay much to make use of fSorted in this case,
           so we lump everything together. */

        int clipx1, clipx2, clipy1, clipy2;

        clipx1 = pboxBandStart->x1;
        clipy1 = pboxBandStart->y1;
        clipx2 = pboxBandStart->x2;
        clipy2 = pboxBandStart->y2;

        for (; ppt != pptLast; ppt++, pwidth++) {
            y = ppt->y;
            x1 = ppt->x;
            if (clipy1 <= y && y < clipy2) {
                x2 = x1 + *pwidth;
                if (x1 < clipx1)
                    x1 = clipx1;
                if (x2 > clipx2)
                    x2 = clipx2;
                if (x1 < x2) {
                    /* part of span in clip rectangle */
                    pptNew->x = x1;
                    pptNew->y = y;
                    *pwidthNew = x2 - x1;
                    pptNew++;
                    pwidthNew++;
                }
            }
        } /* end for */
    } else if (numRects != 0) {
        /* Have to clip against many boxes */
        pixman_box32_t *pboxBandEnd, *pbox, *pboxLast;
        int clipy1, clipy2;

        /* In this case, taking advantage of sorted spans gains more than
           the sorting costs. */
        if ((! fSorted) && (nspans > 1))
            QuickSortSpans(ppt, pwidth, nspans);

        pboxLast = pboxBandStart + numRects;

        NextBand();

        for (; ppt != pptLast; ) {
            y = ppt->y;
            if (y < clipy2) {
                /* span is in the current band */
                pbox = pboxBandStart;
                x1 = ppt->x;
                x2 = x1 + *pwidth;
                do { /* For each box in band */
                    int newx1, newx2;

                    newx1 = x1;
                    newx2 = x2;
                    if (newx1 < pbox->x1)
                        newx1 = pbox->x1;
                    if (newx2 > pbox->x2)
                        newx2 = pbox->x2;
                    if (newx1 < newx2) {
                        /* Part of span in clip rectangle */
                        pptNew->x = newx1;
                        pptNew->y = y;
                        *pwidthNew = newx2 - newx1;
                        pptNew++;
                        pwidthNew++;
                    }
                    pbox++;
                } while (pbox != pboxBandEnd);
                ppt++;
                pwidth++;
            } else {
                /* Move to next band, adjust ppt as needed */
                pboxBandStart = pboxBandEnd;
                if (pboxBandStart == pboxLast)
                    break; /* We're completely done */
                NextBand();
            }
        }
    }
    return (pwidthNew - pwidthNewStart);
}
