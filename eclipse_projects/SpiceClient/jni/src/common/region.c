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
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <spice/macros.h>

#include "region.h"
#include "rect.h"
#include "mem.h"

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1, r2)        \
    (!( ((r1)->x2 <= (r2)->x1)  || \
        ((r1)->x1 >= (r2)->x2)  || \
        ((r1)->y2 <= (r2)->y1)  || \
        ((r1)->y1 >= (r2)->y2) ) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1, r2)        \
    ( ((r1)->x1 <= (r2)->x1) && \
      ((r1)->x2 >= (r2)->x2) && \
      ((r1)->y1 <= (r2)->y1) && \
      ((r1)->y2 >= (r2)->y2) )


void region_init(QRegion *rgn)
{
    pixman_region32_init(rgn);
}

void region_clear(QRegion *rgn)
{
    pixman_region32_fini(rgn);
    pixman_region32_init(rgn);
}

void region_destroy(QRegion *rgn)
{
    pixman_region32_fini(rgn);
}

void region_clone(QRegion *dest, const QRegion *src)
{
    pixman_region32_init(dest);
    pixman_region32_copy(dest, (pixman_region32_t *)src);
}

#define FIND_BAND(r, r_band_end, r_end, ry1)                         \
    do {                                                             \
        ry1 = r->y1;                                                 \
        r_band_end = r + 1;                                          \
        while ((r_band_end != r_end) && (r_band_end->y1 == ry1)) {   \
            r_band_end++;                                            \
        }                                                            \
    } while (0)

static int test_band(int query,
                     int res,
                     pixman_box32_t *r1,
                     pixman_box32_t *r1_end,
                     pixman_box32_t *r2,
                     pixman_box32_t *r2_end)
{
    int x1;
    int x2;

    do {
        x1 = MAX(r1->x1, r2->x1);
        x2 = MIN(r1->x2, r2->x2);

        /*
         * Is there any overlap between the two rectangles?
         */
        if (x1 < x2) {
            res |= REGION_TEST_SHARED;

            if (r1->x1 < r2->x1 || r1->x2 > r2->x2) {
                res |= REGION_TEST_LEFT_EXCLUSIVE;
            }

            if (r2->x1 < r1->x1 || r2->x2 > r1->x2) {
                res |= REGION_TEST_RIGHT_EXCLUSIVE;
            }
        } else {
            /* No overlap at all, the leftmost is exclusive */
            if (r1->x1 < r2->x1) {
                res |= REGION_TEST_LEFT_EXCLUSIVE;
            } else {
                res |= REGION_TEST_RIGHT_EXCLUSIVE;
            }
        }

        if ((res & query) == query) {
            return res;
        }

        /*
         * Advance the pointer(s) with the leftmost right side, since the next
         * rectangle on that list may still overlap the other region's
         * current rectangle.
         */
        if (r1->x2 == x2) {
            r1++;
        }
        if (r2->x2 == x2) {
            r2++;
        }
    } while ((r1 != r1_end) && (r2 != r2_end));

    /*
     * Deal with whichever band (if any) still has rectangles left.
     */
    if (r1 != r1_end) {
        res |= REGION_TEST_LEFT_EXCLUSIVE;
    } else if (r2 != r2_end) {
        res |= REGION_TEST_RIGHT_EXCLUSIVE;
    }

    return res;
}

static int test_generic (pixman_region32_t *reg1,
                         pixman_region32_t *reg2,
                         int query)
{
    pixman_box32_t *r1;             /* Pointer into first region     */
    pixman_box32_t *r2;             /* Pointer into 2d region        */
    pixman_box32_t *r1_end;         /* End of 1st region             */
    pixman_box32_t *r2_end;         /* End of 2d region              */
    int ybot;                       /* Bottom of intersection        */
    int ytop;                       /* Top of intersection           */
    pixman_box32_t * r1_band_end;   /* End of current band in r1     */
    pixman_box32_t * r2_band_end;   /* End of current band in r2     */
    int top;                        /* Top of non-overlapping band   */
    int bot;                        /* Bottom of non-overlapping band*/
    int r1y1;                       /* Temps for r1->y1 and r2->y1   */
    int r2y1;
    int r1_num_rects;
    int r2_num_rects;
    int res;

    r1 = pixman_region32_rectangles(reg1, &r1_num_rects);
    r1_end = r1 + r1_num_rects;

    r2 = pixman_region32_rectangles(reg2, &r2_num_rects);
    r2_end = r2 + r2_num_rects;

    res = 0;

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     *  In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *  For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = MIN(r1->y1, r2->y1);

    do {
        /*
         * This algorithm proceeds one source-band (as opposed to a
         * destination band, which is determined by where the two regions
         * intersect) at a time. r1_band_end and r2_band_end serve to mark the
         * rectangle after the last one in the current band for their
         * respective regions.
         */
        FIND_BAND(r1, r1_band_end, r1_end, r1y1);
        FIND_BAND(r2, r2_band_end, r2_end, r2y1);

        /*
         * First handle the band that doesn't intersect, if any.
         *
         * Note that attention is restricted to one band in the
         * non-intersecting region at once, so if a region has n
         * bands between the current position and the next place it overlaps
         * the other, this entire loop will be passed through n times.
         */
        if (r1y1 < r2y1) {
            top = MAX (r1y1, ybot);
            bot = MIN (r1->y2, r2y1);
            if (top != bot) {
                res |= REGION_TEST_LEFT_EXCLUSIVE;

                if ((res & query) == query) {
                    return res & query;
                }
            }

            ytop = r2y1;
        } else if (r2y1 < r1y1) {
            top = MAX (r2y1, ybot);
            bot = MIN (r2->y2, r1y1);

            if (top != bot) {
                res |= REGION_TEST_RIGHT_EXCLUSIVE;

                if ((res & query) == query) {
                    return res & query;
                }
            }
            ytop = r1y1;
        } else {
            ytop = r1y1;
        }

        /*
         * Now see if we've hit an intersecting band. The two bands only
         * intersect if ybot > ytop
         */
        ybot = MIN (r1->y2, r2->y2);
        if (ybot > ytop) {
            res = test_band(query, res,
                            r1, r1_band_end,
                            r2, r2_band_end);
            if ((res & query) == query) {
                return res & query;
            }
        }

        /*
         * If we've finished with a band (y2 == ybot) we skip forward
         * in the region to the next band.
         */
        if (r1->y2 == ybot) {
            r1 = r1_band_end;
        }

        if (r2->y2 == ybot) {
            r2 = r2_band_end;
        }

    }
    while (r1 != r1_end && r2 != r2_end);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     */

    if (r1 != r1_end) {
        res |= REGION_TEST_LEFT_EXCLUSIVE;
    } else if (r2 != r2_end) {
        res |= REGION_TEST_RIGHT_EXCLUSIVE;
    }

    return res & query;
}

int region_test(const QRegion *_reg1, const QRegion *_reg2, int query)
{
    int res;
    pixman_region32_t *reg1 = (pixman_region32_t *)_reg1;
    pixman_region32_t *reg2 = (pixman_region32_t *)_reg2;

    query = (query) ? query & REGION_TEST_ALL : REGION_TEST_ALL;

    res = 0;

    if (!pixman_region32_not_empty(reg1) || !pixman_region32_not_empty(reg2) ||
        !EXTENTCHECK (&reg1->extents, &reg2->extents)) {
        /* One or more regions are empty or they are disjoint */

        if (pixman_region32_not_empty(reg1)) {
            res |= REGION_TEST_LEFT_EXCLUSIVE;
        }

        if (pixman_region32_not_empty(reg2)) {
            res |= REGION_TEST_RIGHT_EXCLUSIVE;
        }

        return res & query;
    } else if (!reg1->data && !reg2->data) {
        /* Just two rectangles that intersect */
        res |= REGION_TEST_SHARED;

        if (!SUBSUMES(&reg1->extents, &reg2->extents)) {
            res |= REGION_TEST_RIGHT_EXCLUSIVE;
        }

        if (!SUBSUMES(&reg2->extents, &reg1->extents)) {
            res |= REGION_TEST_LEFT_EXCLUSIVE;
        }

        return res & query;
    } else if (!reg2->data && SUBSUMES (&reg2->extents, &reg1->extents)) {
        /* reg2 is just a rect that contains all of reg1 */

        res |= REGION_TEST_SHARED; /* some piece must be shared, because reg is not empty */
        res |= REGION_TEST_RIGHT_EXCLUSIVE; /* reg2 contains all of reg1 and then some */

        return res & query;
    } else if (!reg1->data && SUBSUMES (&reg1->extents, &reg2->extents)) {
        /* reg1 is just a rect that contains all of reg2 */

        res |= REGION_TEST_SHARED; /* some piece must be shared, because reg is not empty */
        res |= REGION_TEST_LEFT_EXCLUSIVE; /* reg1 contains all of reg2 and then some */

        return res & query;
    } else if (reg1 == reg2) {
        res |= REGION_TEST_SHARED;
        return res & query;
    } else {
        /* General purpose intersection */
        return test_generic (reg1, reg2, query);
    }
}

int region_is_valid(const QRegion *rgn)
{
    return pixman_region32_selfcheck((pixman_region32_t *)rgn);
}

int region_is_empty(const QRegion *rgn)
{
    return !pixman_region32_not_empty((pixman_region32_t *)rgn);
}

SpiceRect *region_dup_rects(const QRegion *rgn, uint32_t *num_rects)
{
    pixman_box32_t *boxes;
    SpiceRect *rects;
    int n, i;

    boxes = pixman_region32_rectangles((pixman_region32_t *)rgn, &n);
    if (num_rects) {
        *num_rects = n;
    }
    rects = spice_new(SpiceRect, n);
    for (i = 0; i < n; i++) {
        rects[i].left = boxes[i].x1;
        rects[i].top = boxes[i].y1;
        rects[i].right = boxes[i].x2;
        rects[i].bottom = boxes[i].y2;
    }
    return rects;
}

void region_ret_rects(const QRegion *rgn, SpiceRect *rects, uint32_t num_rects)
{
    pixman_box32_t *boxes;
    unsigned int n, i;

    boxes = pixman_region32_rectangles((pixman_region32_t *)rgn, (int *)&n);
    for (i = 0; i < n && i < num_rects; i++) {
        rects[i].left = boxes[i].x1;
        rects[i].top = boxes[i].y1;
        rects[i].right = boxes[i].x2;
        rects[i].bottom = boxes[i].y2;
    }

    if (i && i != n) {
        unsigned int x;

        for (x = 0; x < (n - num_rects); ++x) {
            rects[i - 1].left = MIN(rects[i - 1].left, boxes[i + x].x1);
            rects[i - 1].top = MIN(rects[i - 1].top, boxes[i + x].y1);
            rects[i - 1].right = MAX(rects[i - 1].right, boxes[i + x].x2);
            rects[i - 1].bottom = MAX(rects[i - 1].bottom, boxes[i + x].y2);
        }
    }
}

void region_extents(const QRegion *rgn, SpiceRect *r)
{
    pixman_box32_t *extents;

    extents = pixman_region32_extents((pixman_region32_t *)rgn);

    r->left = extents->x1;
    r->top = extents->y1;
    r->right = extents->x2;
    r->bottom = extents->y2;
}

int region_is_equal(const QRegion *rgn1, const QRegion *rgn2)
{
    return pixman_region32_equal((pixman_region32_t *)rgn1, (pixman_region32_t *)rgn2);
}

int region_intersects(const QRegion *rgn1, const QRegion *rgn2)
{
    int test_res;

    if (!region_bounds_intersects(rgn1, rgn2)) {
        return FALSE;
    }

    test_res = region_test(rgn1, rgn2, REGION_TEST_SHARED);
    return !!test_res;
}

int region_bounds_intersects(const QRegion *rgn1, const QRegion *rgn2)
{
    pixman_box32_t *extents1, *extents2;

    extents1 = pixman_region32_extents((pixman_region32_t *)rgn1);
    extents2 = pixman_region32_extents((pixman_region32_t *)rgn2);

    return EXTENTCHECK(extents1, extents2);
}

int region_contains(const QRegion *rgn, const QRegion *other)
{
    int test_res;

    test_res = region_test(rgn, other, REGION_TEST_RIGHT_EXCLUSIVE);
    return !test_res;
}

int region_contains_point(const QRegion *rgn, int32_t x, int32_t y)
{
    return pixman_region32_contains_point((pixman_region32_t *)rgn, x, y, NULL);
}

void region_or(QRegion *rgn, const QRegion *other_rgn)
{
    pixman_region32_union(rgn, rgn, (pixman_region32_t *)other_rgn);
}

void region_and(QRegion *rgn, const QRegion *other_rgn)
{
    pixman_region32_intersect(rgn, rgn, (pixman_region32_t *)other_rgn);
}

void region_xor(QRegion *rgn, const QRegion *other_rgn)
{
    pixman_region32_t intersection;

    pixman_region32_copy(&intersection, rgn);
    pixman_region32_intersect(&intersection,
                              &intersection,
                              (pixman_region32_t *)other_rgn);
    pixman_region32_union(rgn, rgn, (pixman_region32_t *)other_rgn);
    pixman_region32_subtract(rgn, rgn, &intersection);
    pixman_region32_fini(&intersection);
}

void region_exclude(QRegion *rgn, const QRegion *other_rgn)
{
    pixman_region32_subtract(rgn, rgn, (pixman_region32_t *)other_rgn);
}

void region_add(QRegion *rgn, const SpiceRect *r)
{
    pixman_region32_union_rect(rgn, rgn, r->left, r->top,
                               r->right - r->left,
                               r->bottom - r->top);
}

void region_remove(QRegion *rgn, const SpiceRect *r)
{
    pixman_region32_t rg;

    pixman_region32_init_rect(&rg, r->left, r->top,
                              r->right - r->left,
                              r->bottom - r->top);
    pixman_region32_subtract(rgn, rgn, &rg);
    pixman_region32_fini(&rg);
}


void region_offset(QRegion *rgn, int32_t dx, int32_t dy)
{
    pixman_region32_translate(rgn, dx, dy);
}

void region_dump(const QRegion *rgn, const char *prefix)
{
    pixman_box32_t *rects, *extents;
    int n_rects, i;

    printf("%sREGION: %p, ", prefix, rgn);

    if (!pixman_region32_not_empty((pixman_region32_t *)rgn)) {
        printf("EMPTY\n");
        return;
    }

    extents = pixman_region32_extents((pixman_region32_t *)rgn);
    rects = pixman_region32_rectangles((pixman_region32_t *)rgn, &n_rects);
    printf("num %u bounds (%d, %d, %d, %d)\n",
           n_rects,
           extents->x1,
           extents->y1,
           extents->x2,
           extents->y2);


    for (i = 0; i < n_rects; i++) {
        printf("%*s  %12d %12d %12d %12d\n",
               (int)strlen(prefix), "",
               rects[i].x1,
               rects[i].y1,
               rects[i].x2,
               rects[i].y2);
    }
}

#ifdef REGION_TEST

static int slow_region_test(const QRegion *rgn, const QRegion *other_rgn, int query)
{
    pixman_region32_t intersection;
    int res;

    pixman_region32_init(&intersection);
    pixman_region32_intersect(&intersection,
                              (pixman_region32_t *)rgn,
                              (pixman_region32_t *)other_rgn);

    res = 0;

    if (query & REGION_TEST_SHARED &&
        pixman_region32_not_empty(&intersection)) {
        res |= REGION_TEST_SHARED;
    }

    if (query & REGION_TEST_LEFT_EXCLUSIVE &&
        !pixman_region32_equal(&intersection, (pixman_region32_t *)rgn)) {
        res |= REGION_TEST_LEFT_EXCLUSIVE;
    }

    if (query & REGION_TEST_RIGHT_EXCLUSIVE &&
        !pixman_region32_equal(&intersection, (pixman_region32_t *)other_rgn)) {
        res |= REGION_TEST_RIGHT_EXCLUSIVE;
    }

    pixman_region32_fini(&intersection);

    return res;
}


static int rect_is_valid(const SpiceRect *r)
{
    if (r->top > r->bottom || r->left > r->right) {
        printf("%s: invalid rect\n", __FUNCTION__);
        return FALSE;
    }
    return TRUE;
}

static void rect_set(SpiceRect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
    r->top = top;
    r->left = left;
    r->bottom = bottom;
    r->right = right;
    spice_assert(rect_is_valid(r));
}

static void random_region(QRegion *reg)
{
    int i;
    int num_rects;
    int x, y, w, h;
    SpiceRect _r;
    SpiceRect *r = &_r;

    region_clear(reg);

    num_rects = rand() % 20;
    for (i = 0; i < num_rects; i++) {
        x = rand()%100;
        y = rand()%100;
        w = rand()%100;
        h = rand()%100;
        rect_set(r,
                 x, y,
                 x+w, y+h);
        region_add(reg, r);
    }
}

static void test(const QRegion *r1, const QRegion *r2, int *expected)
{
    printf("r1 is_empty %s [%s]\n",
           region_is_empty(r1) ? "TRUE" : "FALSE",
           (region_is_empty(r1) == *(expected++)) ? "OK" : "ERR");
    printf("r2 is_empty %s [%s]\n",
           region_is_empty(r2) ? "TRUE" : "FALSE",
           (region_is_empty(r2) == *(expected++)) ? "OK" : "ERR");
    printf("is_equal %s [%s]\n",
           region_is_equal(r1, r2) ? "TRUE" : "FALSE",
           (region_is_equal(r1, r2) == *(expected++)) ? "OK" : "ERR");
    printf("intersects %s [%s]\n",
           region_intersects(r1, r2) ? "TRUE" : "FALSE",
           (region_intersects(r1, r2) == *(expected++)) ? "OK" : "ERR");
    printf("contains %s [%s]\n",
           region_contains(r1, r2) ? "TRUE" : "FALSE",
           (region_contains(r1, r2) == *(expected++)) ? "OK" : "ERR");
}

enum {
    EXPECT_R1_EMPTY,
    EXPECT_R2_EMPTY,
    EXPECT_EQUAL,
    EXPECT_SECT,
    EXPECT_CONT,
};

int main(void)
{
    QRegion _r1, _r2, _r3;
    QRegion *r1 = &_r1;
    QRegion *r2 = &_r2;
    QRegion *r3 = &_r3;
    SpiceRect _r;
    SpiceRect *r = &_r;
    int expected[5];
    int i, j;

    region_init(r1);
    region_init(r2);

    printf("dump r1 empty rgn [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = TRUE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = TRUE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clone(r3, r1);
    printf("dump r3 clone rgn [%s]\n", region_is_valid(r3) ? "VALID" : "INVALID");
    region_dump(r3, "");
    expected[EXPECT_R1_EMPTY] = TRUE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = TRUE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r3, expected);
    region_destroy(r3);
    printf("\n");

    rect_set(r, 0, 0, 100, 100);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r1);
    rect_set(r, 0, 0, 0, 0);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = TRUE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = TRUE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    rect_set(r, -100, -100, 0, 0);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r1);
    rect_set(r, -100, -100, 100, 100);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");


    region_clear(r1);
    region_clear(r2);

    rect_set(r, 100, 100, 200, 200);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    rect_set(r, 300, 300, 400, 400);
    region_add(r1, r);
    printf("dump r1 [%s]\n", region_is_valid(r1) ? "VALID" : "INVALID");
    region_dump(r1, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = TRUE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    rect_set(r, 500, 500, 600, 600);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = FALSE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, 100, 100, 200, 200);
    region_add(r2, r);
    rect_set(r, 300, 300, 400, 400);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = TRUE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, 100, 100, 200, 200);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, -2000, -2000, -1000, -1000);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = FALSE;
    expected[EXPECT_CONT] = FALSE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, -2000, -2000, 1000, 1000);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = FALSE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, 150, 150, 175, 175);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r2, expected);
    printf("\n");

    region_clear(r2);

    rect_set(r, 150, 150, 350, 350);
    region_add(r2, r);
    printf("dump r2 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = FALSE;
    test(r1, r2, expected);
    printf("\n");

    region_and(r2, r1);
    printf("dump r2 and r1 [%s]\n", region_is_valid(r2) ? "VALID" : "INVALID");
    region_dump(r2, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = FALSE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = FALSE;
    test(r2, r1, expected);
    printf("\n");


    region_clone(r3, r1);
    printf("dump r3 clone rgn [%s]\n", region_is_valid(r3) ? "VALID" : "INVALID");
    region_dump(r3, "");
    expected[EXPECT_R1_EMPTY] = FALSE;
    expected[EXPECT_R2_EMPTY] = FALSE;
    expected[EXPECT_EQUAL] = TRUE;
    expected[EXPECT_SECT] = TRUE;
    expected[EXPECT_CONT] = TRUE;
    test(r1, r3, expected);
    printf("\n");

    j = 0;
    for (i = 0; i < 1000000; i++) {
        int res1, res2, test;
        int tests[] = {
            REGION_TEST_LEFT_EXCLUSIVE,
            REGION_TEST_RIGHT_EXCLUSIVE,
            REGION_TEST_SHARED,
            REGION_TEST_LEFT_EXCLUSIVE | REGION_TEST_RIGHT_EXCLUSIVE,
            REGION_TEST_LEFT_EXCLUSIVE | REGION_TEST_SHARED,
            REGION_TEST_RIGHT_EXCLUSIVE | REGION_TEST_SHARED,
            REGION_TEST_LEFT_EXCLUSIVE | REGION_TEST_RIGHT_EXCLUSIVE | REGION_TEST_SHARED
        };

        random_region(r1);
        random_region(r2);

        for (test = 0; test < 7; test++) {
            res1 = region_test(r1, r2, tests[test]);
            res2 = slow_region_test(r1, r2, tests[test]);
            if (res1 != res2) {
                printf ("Error in region_test %d, got %d, expected %d, query=%d\n",
                        j, res1, res2, tests[test]);
                printf ("r1:\n");
                region_dump(r1, "");
                printf ("r2:\n");
                region_dump(r2, "");
            }
            j++;
        }
    }

    region_destroy(r3);
    region_destroy(r1);
    region_destroy(r2);

    return 0;
}

#endif
