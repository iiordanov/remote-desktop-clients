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

#ifdef QUIC_RGB32
#undef QUIC_RGB32
#define PIXEL rgb32_pixel_t
#define FNAME(name) quic_rgb32_##name
#define golomb_coding golomb_coding_8bpc
#define golomb_decoding golomb_decoding_8bpc
#define update_model update_model_8bpc
#define find_bucket find_bucket_8bpc
#define family family_8bpc
#define BPC 8
#define BPC_MASK 0xffU
#define COMPRESS_IMP
#define SET_r(pix, val) ((pix)->r = val)
#define GET_r(pix) ((pix)->r)
#define SET_g(pix, val) ((pix)->g = val)
#define GET_g(pix) ((pix)->g)
#define SET_b(pix, val) ((pix)->b = val)
#define GET_b(pix) ((pix)->b)
#define UNCOMPRESS_PIX_START(pix) ((pix)->pad = 0)
#endif

#ifdef QUIC_RGB24
#undef QUIC_RGB24
#define PIXEL rgb24_pixel_t
#define FNAME(name) quic_rgb24_##name
#define golomb_coding golomb_coding_8bpc
#define golomb_decoding golomb_decoding_8bpc
#define update_model update_model_8bpc
#define find_bucket find_bucket_8bpc
#define family family_8bpc
#define BPC 8
#define BPC_MASK 0xffU
#define COMPRESS_IMP
#define SET_r(pix, val) ((pix)->r = val)
#define GET_r(pix) ((pix)->r)
#define SET_g(pix, val) ((pix)->g = val)
#define GET_g(pix) ((pix)->g)
#define SET_b(pix, val) ((pix)->b = val)
#define GET_b(pix) ((pix)->b)
#define UNCOMPRESS_PIX_START(pix)
#endif

#ifdef QUIC_RGB16
#undef QUIC_RGB16
#define PIXEL rgb16_pixel_t
#define FNAME(name) quic_rgb16_##name
#define golomb_coding golomb_coding_5bpc
#define golomb_decoding golomb_decoding_5bpc
#define update_model update_model_5bpc
#define find_bucket find_bucket_5bpc
#define family family_5bpc
#define BPC 5
#define BPC_MASK 0x1fU
#define COMPRESS_IMP
#define SET_r(pix, val) (*(pix) = (*(pix) & ~(0x1f << 10)) | ((val) << 10))
#define GET_r(pix) ((*(pix) >> 10) & 0x1f)
#define SET_g(pix, val) (*(pix) = (*(pix) & ~(0x1f << 5)) | ((val) << 5))
#define GET_g(pix) ((*(pix) >> 5) & 0x1f)
#define SET_b(pix, val) (*(pix) = (*(pix) & ~0x1f) | (val))
#define GET_b(pix) (*(pix) & 0x1f)
#define UNCOMPRESS_PIX_START(pix) (*(pix) = 0)
#endif

#ifdef QUIC_RGB16_TO_32
#undef QUIC_RGB16_TO_32
#define PIXEL rgb32_pixel_t
#define FNAME(name) quic_rgb16_to_32_##name
#define golomb_coding golomb_coding_5bpc
#define golomb_decoding golomb_decoding_5bpc
#define update_model update_model_5bpc
#define find_bucket find_bucket_5bpc
#define family family_5bpc
#define BPC 5
#define BPC_MASK 0x1fU

#define SET_r(pix, val) ((pix)->r = ((val) << 3) | (((val) & 0x1f) >> 2))
#define GET_r(pix) ((pix)->r >> 3)
#define SET_g(pix, val) ((pix)->g = ((val) << 3) | (((val) & 0x1f) >> 2))
#define GET_g(pix) ((pix)->g >> 3)
#define SET_b(pix, val) ((pix)->b = ((val) << 3) | (((val) & 0x1f) >> 2))
#define GET_b(pix) ((pix)->b >> 3)
#define UNCOMPRESS_PIX_START(pix) ((pix)->pad = 0)
#endif

#define SAME_PIXEL(p1, p2)                                 \
    (GET_r(p1) == GET_r(p2) && GET_g(p1) == GET_g(p2) &&   \
     GET_b(p1) == GET_b(p2))


#define _PIXEL_A(channel, curr) ((unsigned int)GET_##channel((curr) - 1))
#define _PIXEL_B(channel, prev) ((unsigned int)GET_##channel(prev))
#define _PIXEL_C(channel, prev) ((unsigned int)GET_##channel((prev) - 1))

/*  a  */

#define DECORELATE_0(channel, curr, bpc_mask)\
    family.xlatU2L[(unsigned)((int)GET_##channel(curr) - (int)_PIXEL_A(channel, curr)) & bpc_mask]

#define CORELATE_0(channel, curr, correlate, bpc_mask)\
    ((family.xlatL2U[correlate] + _PIXEL_A(channel, curr)) & bpc_mask)

#ifdef PRED_1

/*  (a+b)/2  */
#define DECORELATE(channel, prev, curr, bpc_mask, r)                                            \
    r = family.xlatU2L[(unsigned)((int)GET_##channel(curr) - (int)((_PIXEL_A(channel, curr) +   \
                        _PIXEL_B(channel, prev)) >> 1)) & bpc_mask]

#define CORELATE(channel, prev, curr, correlate, bpc_mask, r)                                   \
    SET_##channel(r, ((family.xlatL2U[correlate] +                                              \
          (int)((_PIXEL_A(channel, curr) + _PIXEL_B(channel, prev)) >> 1)) & bpc_mask))
#endif

#ifdef PRED_2

/*  .75a+.75b-.5c  */
#define DECORELATE(channel, prev, curr, bpc_mask, r) {                          \
    int p = ((int)(3 * (_PIXEL_A(channel, curr) + _PIXEL_B(channel, prev))) -   \
                        (int)(_PIXEL_C(channel, prev) << 1)) >> 2;              \
    if (p < 0) {                                                                \
        p = 0;                                                                  \
    } else if ((unsigned)p > bpc_mask) {                                        \
        p = bpc_mask;                                                           \
    }                                                                           \
    r = family.xlatU2L[(unsigned)((int)GET_##channel(curr) - p) & bpc_mask];    \
}

#define CORELATE(channel, prev, curr, correlate, bpc_mask, r) {                         \
    const int p = ((int)(3 * (_PIXEL_A(channel, curr) + _PIXEL_B(channel, prev))) -     \
                        (int)(_PIXEL_C(channel, prev) << 1) ) >> 2;                     \
    const unsigned int s = family.xlatL2U[correlate];                                   \
    if (!(p & ~bpc_mask)) {                                                             \
        SET_##channel(r, (s + (unsigned)p) & bpc_mask);                                 \
    } else if (p < 0) {                                                                 \
        SET_##channel(r, s);                                                            \
    } else {                                                                            \
        SET_##channel(r, (s + bpc_mask) & bpc_mask);                                    \
    }                                                                                   \
}

#endif


#define COMPRESS_ONE_ROW0_0(channel)                                                \
    correlate_row_##channel[0] = family.xlatU2L[GET_##channel(cur_row)];            \
    golomb_coding(correlate_row_##channel[0], find_bucket(channel_##channel,        \
                  correlate_row_##channel[-1])->bestcode,                           \
                  &codeword, &codewordlen);                                         \
    encode(encoder, codeword, codewordlen);

#define COMPRESS_ONE_ROW0(channel, index)                                               \
    correlate_row_##channel[index] = DECORELATE_0(channel, &cur_row[index], bpc_mask);  \
    golomb_coding(correlate_row_##channel[index], find_bucket(channel_##channel,        \
                  correlate_row_##channel[index -1])->bestcode,                         \
                  &codeword, &codewordlen);                                             \
    encode(encoder, codeword, codewordlen);

#define UPDATE_MODEL(index)                                                                 \
    update_model(&encoder->rgb_state, find_bucket(channel_r, correlate_row_r[index - 1]),   \
                correlate_row_r[index], bpc);                                               \
    update_model(&encoder->rgb_state, find_bucket(channel_g, correlate_row_g[index - 1]),   \
                correlate_row_g[index], bpc);                                               \
    update_model(&encoder->rgb_state, find_bucket(channel_b, correlate_row_b[index - 1]),   \
                correlate_row_b[index], bpc);


#ifdef RLE_PRED_1
#define RLE_PRED_1_IMP                                                                          \
if (SAME_PIXEL(&cur_row[i - 1], &prev_row[i])) {                                                \
    if (run_index != i && SAME_PIXEL(&prev_row[i - 1], &prev_row[i]) &&                         \
                                i + 1 < end && SAME_PIXEL(&prev_row[i], &prev_row[i + 1])) {    \
        goto do_run;                                                                            \
    }                                                                                           \
}
#else
#define RLE_PRED_1_IMP
#endif

#ifdef RLE_PRED_2
#define RLE_PRED_2_IMP                                                              \
if (SAME_PIXEL(&prev_row[i - 1], &prev_row[i])) {                                   \
    if (run_index != i && i > 2 && SAME_PIXEL(&cur_row[i - 1], &cur_row[i - 2])) {  \
        goto do_run;                                                                \
    }                                                                               \
}
#else
#define RLE_PRED_2_IMP
#endif

#ifdef RLE_PRED_3
#define RLE_PRED_3_IMP                                                              \
if (i > 1 &&  SAME_PIXEL(&cur_row[i - 1], &cur_row[i - 2]) && i != run_index) {     \
    goto do_run;                                                                    \
}
#else
#define RLE_PRED_3_IMP
#endif

#ifdef COMPRESS_IMP

static void FNAME(compress_row0_seg)(Encoder *encoder, int i,
                                     const PIXEL * const cur_row,
                                     const int end,
                                     const unsigned int waitmask,
                                     const unsigned int bpc,
                                     const unsigned int bpc_mask)
{
    Channel * const channel_r = encoder->channels;
    Channel * const channel_g = channel_r + 1;
    Channel * const channel_b = channel_g + 1;

    BYTE * const correlate_row_r = channel_r->correlate_row;
    BYTE * const correlate_row_g = channel_g->correlate_row;
    BYTE * const correlate_row_b = channel_b->correlate_row;
    int stopidx;

    spice_assert(end - i > 0);

    if (!i) {
        unsigned int codeword, codewordlen;

        COMPRESS_ONE_ROW0_0(r);
        COMPRESS_ONE_ROW0_0(g);
        COMPRESS_ONE_ROW0_0(b);

        if (encoder->rgb_state.waitcnt) {
            encoder->rgb_state.waitcnt--;
        } else {
            encoder->rgb_state.waitcnt = (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
            UPDATE_MODEL(0);
        }
        stopidx = ++i + encoder->rgb_state.waitcnt;
    } else {
        stopidx = i + encoder->rgb_state.waitcnt;
    }

    while (stopidx < end) {
        for (; i <= stopidx; i++) {
            unsigned int codeword, codewordlen;
            COMPRESS_ONE_ROW0(r, i);
            COMPRESS_ONE_ROW0(g, i);
            COMPRESS_ONE_ROW0(b, i);
        }

        UPDATE_MODEL(stopidx);
        stopidx = i + (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
    }

    for (; i < end; i++) {
        unsigned int codeword, codewordlen;

        COMPRESS_ONE_ROW0(r, i);
        COMPRESS_ONE_ROW0(g, i);
        COMPRESS_ONE_ROW0(b, i);
    }
    encoder->rgb_state.waitcnt = stopidx - end;
}

static void FNAME(compress_row0)(Encoder *encoder, const PIXEL *cur_row,
                                 unsigned int width)
{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    int pos = 0;

    while ((wmimax > (int)encoder->rgb_state.wmidx) && (encoder->rgb_state.wmileft <= width)) {
        if (encoder->rgb_state.wmileft) {
            FNAME(compress_row0_seg)(encoder, pos, cur_row, pos + encoder->rgb_state.wmileft,
                                     bppmask[encoder->rgb_state.wmidx], bpc, bpc_mask);
            width -= encoder->rgb_state.wmileft;
            pos += encoder->rgb_state.wmileft;
        }

        encoder->rgb_state.wmidx++;
        set_wm_trigger(&encoder->rgb_state);
        encoder->rgb_state.wmileft = wminext;
    }

    if (width) {
        FNAME(compress_row0_seg)(encoder, pos, cur_row, pos + width,
                                 bppmask[encoder->rgb_state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)encoder->rgb_state.wmidx) {
            encoder->rgb_state.wmileft -= width;
        }
    }

    spice_assert((int)encoder->rgb_state.wmidx <= wmimax);
    spice_assert(encoder->rgb_state.wmidx <= 32);
    spice_assert(wminext > 0);
}

#define COMPRESS_ONE_0(channel) \
    correlate_row_##channel[0] = family.xlatU2L[(unsigned)((int)GET_##channel(cur_row) -    \
                                                (int)GET_##channel(prev_row) ) & bpc_mask]; \
    golomb_coding(correlate_row_##channel[0],                                               \
                  find_bucket(channel_##channel, correlate_row_##channel[-1])->bestcode,    \
                  &codeword, &codewordlen);                                                 \
    encode(encoder, codeword, codewordlen);

#define COMPRESS_ONE(channel, index)                                                            \
    DECORELATE(channel, &prev_row[index], &cur_row[index],bpc_mask,                             \
               correlate_row_##channel[index]);                                                 \
    golomb_coding(correlate_row_##channel[index],                                               \
                 find_bucket(channel_##channel, correlate_row_##channel[index - 1])->bestcode,  \
                 &codeword, &codewordlen);                                                      \
    encode(encoder, codeword, codewordlen);

static void FNAME(compress_row_seg)(Encoder *encoder, int i,
                                    const PIXEL * const prev_row,
                                    const PIXEL * const cur_row,
                                    const int end,
                                    const unsigned int waitmask,
                                    const unsigned int bpc,
                                    const unsigned int bpc_mask)
{
    Channel * const channel_r = encoder->channels;
    Channel * const channel_g = channel_r + 1;
    Channel * const channel_b = channel_g + 1;

    BYTE * const correlate_row_r = channel_r->correlate_row;
    BYTE * const correlate_row_g = channel_g->correlate_row;
    BYTE * const correlate_row_b = channel_b->correlate_row;
    int stopidx;
#ifdef RLE
    int run_index = 0;
    int run_size;
#endif

    spice_assert(end - i > 0);

    if (!i) {
        unsigned int codeword, codewordlen;

        COMPRESS_ONE_0(r);
        COMPRESS_ONE_0(g);
        COMPRESS_ONE_0(b);

        if (encoder->rgb_state.waitcnt) {
            encoder->rgb_state.waitcnt--;
        } else {
            encoder->rgb_state.waitcnt = (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
            UPDATE_MODEL(0);
        }
        stopidx = ++i + encoder->rgb_state.waitcnt;
    } else {
        stopidx = i + encoder->rgb_state.waitcnt;
    }
    for (;;) {
        while (stopidx < end) {
            for (; i <= stopidx; i++) {
                unsigned int codeword, codewordlen;
#ifdef RLE
                RLE_PRED_1_IMP;
                RLE_PRED_2_IMP;
                RLE_PRED_3_IMP;
#endif
                COMPRESS_ONE(r, i);
                COMPRESS_ONE(g, i);
                COMPRESS_ONE(b, i);
            }

            UPDATE_MODEL(stopidx);
            stopidx = i + (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
        }

        for (; i < end; i++) {
            unsigned int codeword, codewordlen;
#ifdef RLE
            RLE_PRED_1_IMP;
            RLE_PRED_2_IMP;
            RLE_PRED_3_IMP;
#endif
            COMPRESS_ONE(r, i);
            COMPRESS_ONE(g, i);
            COMPRESS_ONE(b, i);
        }
        encoder->rgb_state.waitcnt = stopidx - end;

        return;

#ifdef RLE
do_run:
        run_index = i;
        encoder->rgb_state.waitcnt = stopidx - i;
        run_size = 0;

        while (SAME_PIXEL(&cur_row[i], &cur_row[i - 1])) {
            run_size++;
            if (++i == end) {
                encode_run(encoder, run_size);
                return;
            }
        }
        encode_run(encoder, run_size);
        stopidx = i + encoder->rgb_state.waitcnt;
#endif
    }
}

static void FNAME(compress_row)(Encoder *encoder,
                                const PIXEL * const prev_row,
                                const PIXEL * const cur_row,
                                unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    unsigned int pos = 0;

    while ((wmimax > (int)encoder->rgb_state.wmidx) && (encoder->rgb_state.wmileft <= width)) {
        if (encoder->rgb_state.wmileft) {
            FNAME(compress_row_seg)(encoder, pos, prev_row, cur_row,
                                    pos + encoder->rgb_state.wmileft,
                                    bppmask[encoder->rgb_state.wmidx],
                                    bpc, bpc_mask);
            width -= encoder->rgb_state.wmileft;
            pos += encoder->rgb_state.wmileft;
        }

        encoder->rgb_state.wmidx++;
        set_wm_trigger(&encoder->rgb_state);
        encoder->rgb_state.wmileft = wminext;
    }

    if (width) {
        FNAME(compress_row_seg)(encoder, pos, prev_row, cur_row, pos + width,
                                bppmask[encoder->rgb_state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)encoder->rgb_state.wmidx) {
            encoder->rgb_state.wmileft -= width;
        }
    }

    spice_assert((int)encoder->rgb_state.wmidx <= wmimax);
    spice_assert(encoder->rgb_state.wmidx <= 32);
    spice_assert(wminext > 0);
}

#endif

#define UNCOMPRESS_ONE_ROW0_0(channel)                                                          \
    correlate_row_##channel[0] = (BYTE)golomb_decoding(find_bucket(channel_##channel,           \
                                                       correlate_row_##channel[-1])->bestcode,  \
                                                       encoder->io_word, &codewordlen);         \
    SET_##channel(&cur_row[0], (BYTE)family.xlatL2U[correlate_row_##channel[0]]);               \
    decode_eatbits(encoder, codewordlen);

#define UNCOMPRESS_ONE_ROW0(channel)                                                              \
    correlate_row_##channel[i] = (BYTE)golomb_decoding(find_bucket(channel_##channel,             \
                                                       correlate_row_##channel[i - 1])->bestcode, \
                                                       encoder->io_word,                          \
                                                       &codewordlen);                             \
    SET_##channel(&cur_row[i], CORELATE_0(channel, &cur_row[i], correlate_row_##channel[i],       \
                  bpc_mask));                                                                     \
    decode_eatbits(encoder, codewordlen);

static void FNAME(uncompress_row0_seg)(Encoder *encoder, int i,
                                       PIXEL * const cur_row,
                                       const int end,
                                       const unsigned int waitmask,
                                       const unsigned int bpc,
                                       const unsigned int bpc_mask)
{
    Channel * const channel_r = encoder->channels;
    Channel * const channel_g = channel_r + 1;
    Channel * const channel_b = channel_g + 1;

    BYTE * const correlate_row_r = channel_r->correlate_row;
    BYTE * const correlate_row_g = channel_g->correlate_row;
    BYTE * const correlate_row_b = channel_b->correlate_row;
    int stopidx;

    spice_assert(end - i > 0);

    if (!i) {
        unsigned int codewordlen;

        UNCOMPRESS_PIX_START(&cur_row[i]);
        UNCOMPRESS_ONE_ROW0_0(r);
        UNCOMPRESS_ONE_ROW0_0(g);
        UNCOMPRESS_ONE_ROW0_0(b);

        if (encoder->rgb_state.waitcnt) {
            --encoder->rgb_state.waitcnt;
        } else {
            encoder->rgb_state.waitcnt = (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
            UPDATE_MODEL(0);
        }
        stopidx = ++i + encoder->rgb_state.waitcnt;
    } else {
        stopidx = i + encoder->rgb_state.waitcnt;
    }

    while (stopidx < end) {
        for (; i <= stopidx; i++) {
            unsigned int codewordlen;

            UNCOMPRESS_PIX_START(&cur_row[i]);
            UNCOMPRESS_ONE_ROW0(r);
            UNCOMPRESS_ONE_ROW0(g);
            UNCOMPRESS_ONE_ROW0(b);
        }
        UPDATE_MODEL(stopidx);
        stopidx = i + (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
    }

    for (; i < end; i++) {
        unsigned int codewordlen;

        UNCOMPRESS_PIX_START(&cur_row[i]);
        UNCOMPRESS_ONE_ROW0(r);
        UNCOMPRESS_ONE_ROW0(g);
        UNCOMPRESS_ONE_ROW0(b);
    }
    encoder->rgb_state.waitcnt = stopidx - end;
}

static void FNAME(uncompress_row0)(Encoder *encoder,
                                   PIXEL * const cur_row,
                                   unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    unsigned int pos = 0;

    while ((wmimax > (int)encoder->rgb_state.wmidx) && (encoder->rgb_state.wmileft <= width)) {
        if (encoder->rgb_state.wmileft) {
            FNAME(uncompress_row0_seg)(encoder, pos, cur_row,
                                       pos + encoder->rgb_state.wmileft,
                                       bppmask[encoder->rgb_state.wmidx],
                                       bpc, bpc_mask);
            pos += encoder->rgb_state.wmileft;
            width -= encoder->rgb_state.wmileft;
        }

        encoder->rgb_state.wmidx++;
        set_wm_trigger(&encoder->rgb_state);
        encoder->rgb_state.wmileft = wminext;
    }

    if (width) {
        FNAME(uncompress_row0_seg)(encoder, pos, cur_row, pos + width,
                                   bppmask[encoder->rgb_state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)encoder->rgb_state.wmidx) {
            encoder->rgb_state.wmileft -= width;
        }
    }

    spice_assert((int)encoder->rgb_state.wmidx <= wmimax);
    spice_assert(encoder->rgb_state.wmidx <= 32);
    spice_assert(wminext > 0);
}

#define UNCOMPRESS_ONE_0(channel) \
    correlate_row_##channel[0] = (BYTE)golomb_decoding(find_bucket(channel_##channel,           \
                                                       correlate_row_##channel[-1])->bestcode,  \
                                                       encoder->io_word, &codewordlen);         \
    SET_##channel(&cur_row[0], (family.xlatL2U[correlate_row_##channel[0]] +                    \
                          GET_##channel(prev_row)) & bpc_mask);                                 \
    decode_eatbits(encoder, codewordlen);

#define UNCOMPRESS_ONE(channel)                                                                   \
    correlate_row_##channel[i] = (BYTE)golomb_decoding(find_bucket(channel_##channel,             \
                                                       correlate_row_##channel[i - 1])->bestcode, \
                                                       encoder->io_word,                          \
                                                       &codewordlen);                             \
    CORELATE(channel, &prev_row[i], &cur_row[i], correlate_row_##channel[i], bpc_mask,            \
             &cur_row[i]);                                                                        \
    decode_eatbits(encoder, codewordlen);

static void FNAME(uncompress_row_seg)(Encoder *encoder,
                                      const PIXEL * const prev_row,
                                      PIXEL * const cur_row,
                                      int i,
                                      const int end,
                                      const unsigned int bpc,
                                      const unsigned int bpc_mask)
{
    Channel * const channel_r = encoder->channels;
    Channel * const channel_g = channel_r + 1;
    Channel * const channel_b = channel_g + 1;

    BYTE * const correlate_row_r = channel_r->correlate_row;
    BYTE * const correlate_row_g = channel_g->correlate_row;
    BYTE * const correlate_row_b = channel_b->correlate_row;
    const unsigned int waitmask = bppmask[encoder->rgb_state.wmidx];
    int stopidx;
#ifdef RLE
    int run_index = 0;
    int run_end;
#endif

    spice_assert(end - i > 0);

    if (!i) {
        unsigned int codewordlen;

        UNCOMPRESS_PIX_START(&cur_row[i]);
        UNCOMPRESS_ONE_0(r);
        UNCOMPRESS_ONE_0(g);
        UNCOMPRESS_ONE_0(b);

        if (encoder->rgb_state.waitcnt) {
            --encoder->rgb_state.waitcnt;
        } else {
            encoder->rgb_state.waitcnt = (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
            UPDATE_MODEL(0);
        }
        stopidx = ++i + encoder->rgb_state.waitcnt;
    } else {
        stopidx = i + encoder->rgb_state.waitcnt;
    }
    for (;;) {
        while (stopidx < end) {
            for (; i <= stopidx; i++) {
                unsigned int codewordlen;
#ifdef RLE
                RLE_PRED_1_IMP;
                RLE_PRED_2_IMP;
                RLE_PRED_3_IMP;
#endif
                UNCOMPRESS_PIX_START(&cur_row[i]);
                UNCOMPRESS_ONE(r);
                UNCOMPRESS_ONE(g);
                UNCOMPRESS_ONE(b);
            }

            UPDATE_MODEL(stopidx);

            stopidx = i + (tabrand(&encoder->rgb_state.tabrand_seed) & waitmask);
        }

        for (; i < end; i++) {
            unsigned int codewordlen;
#ifdef RLE
            RLE_PRED_1_IMP;
            RLE_PRED_2_IMP;
            RLE_PRED_3_IMP;
#endif
            UNCOMPRESS_PIX_START(&cur_row[i]);
            UNCOMPRESS_ONE(r);
            UNCOMPRESS_ONE(g);
            UNCOMPRESS_ONE(b);
        }

        encoder->rgb_state.waitcnt = stopidx - end;

        return;

#ifdef RLE
do_run:
        encoder->rgb_state.waitcnt = stopidx - i;
        run_index = i;
        run_end = i + decode_run(encoder);

        for (; i < run_end; i++) {
            UNCOMPRESS_PIX_START(&cur_row[i]);
            SET_r(&cur_row[i], GET_r(&cur_row[i - 1]));
            SET_g(&cur_row[i], GET_g(&cur_row[i - 1]));
            SET_b(&cur_row[i], GET_b(&cur_row[i - 1]));
        }

        if (i == end) {
            return;
        }

        stopidx = i + encoder->rgb_state.waitcnt;
#endif
    }
}

static void FNAME(uncompress_row)(Encoder *encoder,
                                  const PIXEL * const prev_row,
                                  PIXEL * const cur_row,
                                  unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    unsigned int pos = 0;

    while ((wmimax > (int)encoder->rgb_state.wmidx) && (encoder->rgb_state.wmileft <= width)) {
        if (encoder->rgb_state.wmileft) {
            FNAME(uncompress_row_seg)(encoder, prev_row, cur_row, pos,
                                      pos + encoder->rgb_state.wmileft, bpc, bpc_mask);
            pos += encoder->rgb_state.wmileft;
            width -= encoder->rgb_state.wmileft;
        }

        encoder->rgb_state.wmidx++;
        set_wm_trigger(&encoder->rgb_state);
        encoder->rgb_state.wmileft = wminext;
    }

    if (width) {
        FNAME(uncompress_row_seg)(encoder, prev_row, cur_row, pos,
                                  pos + width, bpc, bpc_mask);
        if (wmimax > (int)encoder->rgb_state.wmidx) {
            encoder->rgb_state.wmileft -= width;
        }
    }

    spice_assert((int)encoder->rgb_state.wmidx <= wmimax);
    spice_assert(encoder->rgb_state.wmidx <= 32);
    spice_assert(wminext > 0);
}

#undef PIXEL
#undef FNAME
#undef _PIXEL_A
#undef _PIXEL_B
#undef _PIXEL_C
#undef SAME_PIXEL
#undef RLE_PRED_1_IMP
#undef RLE_PRED_2_IMP
#undef RLE_PRED_3_IMP
#undef UPDATE_MODEL
#undef DECORELATE_0
#undef DECORELATE
#undef COMPRESS_ONE_ROW0_0
#undef COMPRESS_ONE_ROW0
#undef COMPRESS_ONE_0
#undef COMPRESS_ONE
#undef CORELATE_0
#undef CORELATE
#undef UNCOMPRESS_ONE_ROW0_0
#undef UNCOMPRESS_ONE_ROW0
#undef UNCOMPRESS_ONE_0
#undef UNCOMPRESS_ONE
#undef golomb_coding
#undef golomb_decoding
#undef update_model
#undef find_bucket
#undef family
#undef BPC
#undef BPC_MASK
#undef COMPRESS_IMP
#undef SET_r
#undef GET_r
#undef SET_g
#undef GET_g
#undef SET_b
#undef GET_b
#undef UNCOMPRESS_PIX_START
