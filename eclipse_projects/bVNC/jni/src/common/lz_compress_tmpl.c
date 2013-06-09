/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*

 Copyright (C) 2009 Red Hat, Inc. and/or its affiliates.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

 This file incorporates work covered by the following copyright and
 permission notice:
   Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)
   Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
   Copyright (C) 2005 Ariya Hidayat (ariya@kde.org)

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DJB2_START 5381;
#define DJB2_HASH(hash, c) (hash = ((hash << 5) + hash) ^ (c)) //|{hash = ((hash << 5) + hash) + c;}

/*
    For each pixel type the following macros are defined:
    PIXEL                         : input type
    FNAME(name)
    ENCODE_PIXEL(encoder, pixel) : writing a pixel to the compressed buffer (byte by byte)
    SAME_PIXEL(pix1, pix2)         : comparing two pixels
    HASH_FUNC(value, pix_ptr)    : hash func of 3 consecutive pixels
*/

#ifdef LZ_PLT
#define PIXEL one_byte_pixel_t
#define FNAME(name) lz_plt_##name
#define ENCODE_PIXEL(e, pix) encode(e, (pix).a)   // gets the pixel and write only the needed bytes
                                                  // from the pixel
#define SAME_PIXEL(pix1, pix2) ((pix1).a == (pix2).a)
#define HASH_FUNC(v, p) {  \
    v = DJB2_START;        \
    DJB2_HASH(v, p[0].a);  \
    DJB2_HASH(v, p[1].a);  \
    DJB2_HASH(v, p[2].a);  \
    v &= HASH_MASK;        \
    }
#endif

#ifdef LZ_A8
#define PIXEL one_byte_pixel_t
#define FNAME(name) lz_a8_##name
#define ENCODE_PIXEL(e, pix) encode(e, (pix).a)   // gets the pixel and write only the needed bytes
                                                  // from the pixel
#define SAME_PIXEL(pix1, pix2) ((pix1).a == (pix2).a)
#define HASH_FUNC(v, p) {  \
    v = DJB2_START;        \
    DJB2_HASH(v, p[0].a);  \
    DJB2_HASH(v, p[1].a);  \
    DJB2_HASH(v, p[2].a);  \
    v &= HASH_MASK;        \
    }
#endif

#ifdef LZ_RGB_ALPHA
//#undef LZ_RGB_ALPHA
#define PIXEL rgb32_pixel_t
#define FNAME(name) lz_rgb_alpha_##name
#define ENCODE_PIXEL(e, pix) {encode(e, (pix).pad);}
#define SAME_PIXEL(pix1, pix2) ((pix1).pad == (pix2).pad)
#define HASH_FUNC(v, p) {    \
    v = DJB2_START;          \
    DJB2_HASH(v, p[0].pad);  \
    DJB2_HASH(v, p[1].pad);  \
    DJB2_HASH(v, p[2].pad);  \
    v &= HASH_MASK;          \
    }
#endif


#ifdef LZ_RGB16
#define PIXEL rgb16_pixel_t
#define FNAME(name) lz_rgb16_##name
#define GET_r(pix) (((pix) >> 10) & 0x1f)
#define GET_g(pix) (((pix) >> 5) & 0x1f)
#define GET_b(pix) ((pix) & 0x1f)
#define ENCODE_PIXEL(e, pix) {encode(e, (pix) >> 8); encode(e, (pix) & 0xff);}

#define HASH_FUNC(v, p) {                 \
    v = DJB2_START;                       \
    DJB2_HASH(v, p[0] & (0x00ff));        \
    DJB2_HASH(v, (p[0] >> 8) & (0x007f)); \
    DJB2_HASH(v, p[1]&(0x00ff));          \
    DJB2_HASH(v, (p[1] >> 8) & (0x007f)); \
    DJB2_HASH(v, p[2] & (0x00ff));        \
    DJB2_HASH(v, (p[2] >> 8) & (0x007f)); \
    v &= HASH_MASK;                       \
}
#endif

#ifdef LZ_RGB24
#define PIXEL rgb24_pixel_t
#define FNAME(name) lz_rgb24_##name
#define ENCODE_PIXEL(e, pix) {encode(e, (pix).b); encode(e, (pix).g); encode(e, (pix).r);}
#endif

#ifdef LZ_RGB32
#define PIXEL rgb32_pixel_t
#define FNAME(name) lz_rgb32_##name
#define ENCODE_PIXEL(e, pix) {encode(e, (pix).b); encode(e, (pix).g); encode(e, (pix).r);}
#endif


#if  defined(LZ_RGB24) || defined(LZ_RGB32)
#define GET_r(pix) ((pix).r)
#define GET_g(pix) ((pix).g)
#define GET_b(pix) ((pix).b)
#define HASH_FUNC(v, p) {    \
    v = DJB2_START;          \
    DJB2_HASH(v, p[0].r);    \
    DJB2_HASH(v, p[0].g);    \
    DJB2_HASH(v, p[0].b);    \
    DJB2_HASH(v, p[1].r);    \
    DJB2_HASH(v, p[1].g);    \
    DJB2_HASH(v, p[1].b);    \
    DJB2_HASH(v, p[2].r);    \
    DJB2_HASH(v, p[2].g);    \
    DJB2_HASH(v, p[2].b);    \
    v &= HASH_MASK;          \
    }
#endif

#if defined(LZ_RGB16) || defined(LZ_RGB24) || defined(LZ_RGB32)
#define SAME_PIXEL(p1, p2) (GET_r(p1) == GET_r(p2) && GET_g(p1) == GET_g(p2) && \
                            GET_b(p1) == GET_b(p2))

#endif

#define PIXEL_ID(pix_ptr, seg_ptr) (pix_ptr - ((PIXEL *)seg_ptr->lines) + seg_ptr->size_delta)

// when encoding, the ref can be in previous segment, and we should check that it doesn't
// exceeds its bounds.
// TODO: optimization: when only one chunk exists or when the reference is in the same segment,
//       don't make checks if we reach end of segments
// TODO: optimize to continue match between segments?
// TODO: check hash function
// TODO: check times

/* compresses one segment starting from 'from'.*/
static void FNAME(compress_seg)(Encoder *encoder, LzImageSegment *seg, PIXEL *from, int copied)
{
    const PIXEL *ip = from;
    const PIXEL *ip_bound = (PIXEL *)(seg->lines_end) - BOUND_OFFSET;
    const PIXEL *ip_limit = (PIXEL *)(seg->lines_end) - LIMIT_OFFSET;
    HashEntry    *hslot;
    int hval;
    int copy = copied;

    if (copy == 0) {
        encode_copy_count(encoder, MAX_COPY - 1);
    }


    while (LZ_EXPECT_CONDITIONAL(ip < ip_limit)) {   // TODO: maybe change ip_limit and enabling
                                                     //       moving to the next seg
        const PIXEL            *ref;
        const PIXEL            *ref_limit;
        size_t distance;

        /* minimum match length */
#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_A8)
        size_t len = 3;
#elif defined(LZ_RGB16)
        size_t len = 2;
#else
        size_t len = 1;
#endif
        /* comparison starting-point */
        const PIXEL            *anchor = ip;



        // TODO: RLE without checking if not first byte.
        // TODO: optimize comparisons

        /* check for a run */ // TODO for RGB we can use less pixels
        if (LZ_EXPECT_CONDITIONAL(ip > (PIXEL *)(seg->lines))) {
            if (SAME_PIXEL(ip[-1], ip[0]) && SAME_PIXEL(ip[0], ip[1]) && SAME_PIXEL(ip[1], ip[2])) {
                distance = 1;
                ip += 3;
                ref = anchor + 2;
                ref_limit = (PIXEL *)(seg->lines_end);
#if defined(LZ_RGB16) || defined(LZ_RGB24) || defined(LZ_RGB32)
                len = 3;
#endif
                goto match;
            }
        }

        /* find potential match */
        HASH_FUNC(hval, ip);
        hslot = encoder->htab + hval;
        ref = (PIXEL *)(hslot->ref);
        ref_limit = (PIXEL *)(hslot->image_seg->lines_end);

        /* calculate distance to the match */
        distance = PIXEL_ID(anchor, seg) - PIXEL_ID(ref, hslot->image_seg);

        /* update hash table */
        hslot->image_seg = seg;
        hslot->ref = (uint8_t *)anchor;

        /* is this a match? check the first 3 pixels */
        if (distance == 0 || (distance >= MAX_FARDISTANCE)) {
            goto literal;
        }
        /* check if the hval key identical*/
        // no need to check ref limit here because the word size in the htab is 3 pixels
        if (!SAME_PIXEL(*ref, *ip)) {
            ref++;
            ip++;
            goto literal;
        }
        ref++;
        ip++;

        /* minimum match length for rgb16 is 2 and for plt and alpha is 3 */
#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_RGB16) || defined(LZ_A8)
        if (!SAME_PIXEL(*ref, *ip)) {
            ref++;
            ip++;
            goto literal;
        }
        ref++;
        ip++;
#endif

#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_A8)
        if (!SAME_PIXEL(*ref, *ip)) {
            ref++;
            ip++;
            goto literal;
        }
        ref++;
        ip++;
#endif
        /* far, needs at least 5-byte match */
        if (distance >= MAX_DISTANCE) {
#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_A8)
            if (ref >= (ref_limit - 1)) {
                goto literal;
            }
#else
            if (ref > (ref_limit - 1)) {
                goto literal;
            }
#endif
            if (!SAME_PIXEL(*ref, *ip)) {
                ref++;
                ip++;
                goto literal;
            }
            ref++;
            ip++;
            len++;
#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_A8)
            if (!SAME_PIXEL(*ref, *ip)) {
                ref++;
                ip++;
                goto literal;
            }
            ref++;
            ip++;
            len++;
#endif
        }
match:        // RLE or dictionary (both are encoded by distance from ref (-1) and length)

        /* distance is biased */
        distance--;

        // ip is located now at the position of the second mismatch.
        // later it will be subtracted by 3

        if (!distance) {
            /* zero distance means a run */
            PIXEL x = *ref;
            while ((ip < ip_bound) && (ref < ref_limit)) { // TODO: maybe separate a run from
                                                           //       the same seg or from different
                                                           //       ones in order to spare
                                                           //       ref < ref_limit
                if (!SAME_PIXEL(*ref, x)) {
                    ref++;
                    break;
                } else {
                    ref++;
                    ip++;
                }
            }
        } else {
            // TODO: maybe separate a run from the same seg or from different ones in order
            //       to spare ref < ref_limit and that way we can also perform 8 calls of
            //       (ref++ != ip++) outside a loop
            for (;;) {
                while ((ip < ip_bound) && (ref < ref_limit)) {
                    if (!SAME_PIXEL(*ref, *ip)) {
                        ref++;
                        ip++;
                        break;
                    } else {
                        ref++;
                        ip++;
                    }
                }
                break;
            }
        }

        /* if we have copied something, adjust the copy count */
        if (copy) {
            /* copy is biased, '0' means 1 byte copy */
            update_copy_count(encoder, copy - 1);
        } else {
            /* back, to overwrite the copy count */
            compress_output_prev(encoder);
        }

        /* reset literal counter */
        copy = 0;

        /* length is biased, '1' means a match of 3 pixels for PLT and alpha*/
        /* for RGB 16 1 means 2 */
        /* for RGB24/32 1 means 1...*/
        ip -= 3;
        len = ip - anchor;
#if defined(LZ_RGB16)
        len++;
#elif defined(LZ_RGB24) || defined(LZ_RGB32)
        len += 2;
#endif
        /* encode the match (like fastlz level 2)*/
        if (distance < MAX_DISTANCE) { // MAX_DISTANCE is 2^13 - 1
            // when copy is performed, the byte that holds the copy count is smaller than 32.
            // When there is a reference, the first byte is always larger then 32

            // 3 bits = length, 5 bits = 5 MSB of distance, 8 bits = 8 LSB of distance
            if (len < 7) {
                encode(encoder, (uint8_t)((len << 5) + (distance >> 8)));
                encode(encoder, (uint8_t)(distance & 255));
            } else { // more than 3 bits are needed for length
                    // 3 bits 7, 5 bits = 5 MSB of distance, next bytes are 255 till we
                    // receive a smaller number, last byte = 8 LSB of distance
                encode(encoder, (uint8_t)((7 << 5) + (distance >> 8)));
                for (len -= 7; len >= 255; len -= 255) {
                    encode(encoder, 255);
                }
                encode(encoder, (uint8_t)len);
                encode(encoder, (uint8_t)(distance & 255));
            }
        } else {
            /* far away */
            if (len < 7) { // the max_far_distance is ~2^16+2^13 so two more bytes are needed
                // 3 bits = length, 5 bits = 5 MSB of MAX_DISTANCE, 8 bits = 8 LSB of MAX_DISTANCE,
                // 8 bits = 8 MSB distance-MAX_distance (smaller than 2^16),8 bits=8 LSB of
                // distance-MAX_distance
                distance -= MAX_DISTANCE;
                encode(encoder, (uint8_t)((len << 5) + 31));
                encode(encoder, (uint8_t)255);
                encode(encoder, (uint8_t)(distance >> 8));
                encode(encoder, (uint8_t)(distance & 255));
            } else {
                // same as before, but the first byte is followed by the left overs of len
                distance -= MAX_DISTANCE;
                encode(encoder, (uint8_t)((7 << 5) + 31));
                for (len -= 7; len >= 255; len -= 255) {
                    encode(encoder, 255);
                }
                encode(encoder, (uint8_t)len);
                encode(encoder, 255);
                encode(encoder, (uint8_t)(distance >> 8));
                encode(encoder, (uint8_t)(distance & 255));
            }
        }

        /* update the hash at match boundary */
#if defined(LZ_RGB16) || defined(LZ_RGB24) || defined(LZ_RGB32)
        if (ip > anchor) {
#endif
        HASH_FUNC(hval, ip);
        encoder->htab[hval].ref = (uint8_t *)ip;
        ip++;
        encoder->htab[hval].image_seg = seg;
#if defined(LZ_RGB16) || defined(LZ_RGB24) || defined(LZ_RGB32)
    } else {ip++;
    }
#endif
#if defined(LZ_RGB24) || defined(LZ_RGB32)
        if (ip > anchor) {
#endif
        HASH_FUNC(hval, ip);
        encoder->htab[hval].ref = (uint8_t *)ip;
        ip++;
        encoder->htab[hval].image_seg = seg;
#if defined(LZ_RGB24) || defined(LZ_RGB32)
    } else {ip++;
    }
#endif
        /* assuming literal copy */
        encode_copy_count(encoder, MAX_COPY - 1);
        continue;

literal:
        ENCODE_PIXEL(encoder, *anchor);
        anchor++;
        ip = anchor;
        copy++;

        if (LZ_UNEXPECT_CONDITIONAL(copy == MAX_COPY)) {
            copy = 0;
            encode_copy_count(encoder, MAX_COPY - 1);
        }
    } // END LOOP (ip < ip_limit)


    /* left-over as literal copy */
    ip_bound++;
    while (ip <= ip_bound) {
        ENCODE_PIXEL(encoder, *ip);
        ip++;
        copy++;
        if (copy == MAX_COPY) {
            copy = 0;
            encode_copy_count(encoder, MAX_COPY - 1);
        }
    }

    /* if we have copied something, adjust the copy length */
    if (copy) {
        update_copy_count(encoder, copy - 1);
    } else {
        compress_output_prev(encoder); // in case we created a new buffer for copy, check that
                                       // red_worker could handle size that do not contain the
                                       // ne buffer
    }
}


/*    initializes the hash table. if the file is very small, copies it.
    copies the first two pixels of the first segment, and sends the segments
    one by one to compress_seg.
    the number of bytes compressed are stored inside encoder.
    */
static void FNAME(compress)(Encoder *encoder)
{
    LzImageSegment    *cur_seg = encoder->head_image_segs;
    HashEntry        *hslot;
    PIXEL            *ip;
    PIXEL            *ip_start;

    // fetch the first image segment that is not too small
    while (cur_seg && ((((PIXEL *)cur_seg->lines_end) - ((PIXEL *)cur_seg->lines)) < 4)) {
        ip_start = (PIXEL *)cur_seg->lines;
        // coping the segment
        if (cur_seg->lines != cur_seg->lines_end) {
            ip = (PIXEL *)cur_seg->lines;
            // Note: we assume MAX_COPY > 3
            encode_copy_count(encoder, (uint8_t)(
                                  (((PIXEL *)cur_seg->lines_end) - ((PIXEL *)cur_seg->lines)) - 1));
            while (ip < (PIXEL *)cur_seg->lines_end) {
                ENCODE_PIXEL(encoder, *ip);
                ip++;
            }
        }
        cur_seg = cur_seg->next;
    }

    if (!cur_seg) {
        return;
    }

    ip = (PIXEL *)cur_seg->lines;

    /* initialize hash table */
    for (hslot = encoder->htab; hslot < encoder->htab + HASH_SIZE; hslot++) {
        hslot->ref = (uint8_t*)ip;
        hslot->image_seg = cur_seg;
    }

    encode_copy_count(encoder, MAX_COPY - 1);
    ENCODE_PIXEL(encoder, *ip);
    ip++;
    ENCODE_PIXEL(encoder, *ip);
    ip++;

    // compressing the first segment
    FNAME(compress_seg)(encoder, cur_seg, ip, 2);

    // compressing the next segments
    for (cur_seg = cur_seg->next; cur_seg; cur_seg = cur_seg->next) {
        FNAME(compress_seg)(encoder, cur_seg, (PIXEL *)cur_seg->lines, 0);
    }
}

#undef FNAME
#undef PIXEL_ID
#undef PIXEL
#undef ENCODE_PIXEL
#undef SAME_PIXEL
#undef LZ_READU16
#undef HASH_FUNC
#undef BYTES_TO_16
#undef HASH_FUNC_16
#undef GET_r
#undef GET_g
#undef GET_b
#undef GET_CODE
#undef LZ_PLT
#undef LZ_RGB_ALPHA
#undef LZ_RGB16
#undef LZ_RGB24
#undef LZ_RGB32
#undef LZ_A8
#undef HASH_FUNC2
