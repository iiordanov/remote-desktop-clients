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

#ifdef ONE_BYTE
#undef ONE_BYTE
#define FNAME(name) quic_one_##name
#define PIXEL one_byte_t
#endif

#ifdef THREE_BYTE
#undef THREE_BYTE
#define FNAME(name) quic_three_##name
#define PIXEL three_bytes_t
#endif

#ifdef FOUR_BYTE
#undef FOUR_BYTE
#define FNAME(name) quic_four_##name
#define PIXEL four_bytes_t
#endif

#define golomb_coding golomb_coding_8bpc
#define golomb_decoding golomb_decoding_8bpc
#define update_model update_model_8bpc
#define find_bucket find_bucket_8bpc
#define family family_8bpc

#define BPC 8
#define BPC_MASK 0xffU

#define _PIXEL_A ((unsigned int)curr[-1].a)
#define _PIXEL_B ((unsigned int)prev[0].a)
#define _PIXEL_C ((unsigned int)prev[-1].a)

#ifdef RLE_PRED_1
#define RLE_PRED_1_IMP                                                              \
if (cur_row[i - 1].a == prev_row[i].a) {                                            \
    if (run_index != i && prev_row[i - 1].a == prev_row[i].a &&                     \
                        i + 1 < end && prev_row[i].a == prev_row[i + 1].a) {        \
        goto do_run;                                                                \
    }                                                                               \
}
#else
#define RLE_PRED_1_IMP
#endif

#ifdef RLE_PRED_2
#define RLE_PRED_2_IMP                                                     \
if (prev_row[i - 1].a == prev_row[i].a) {                                  \
    if (run_index != i && i > 2 && cur_row[i - 1].a == cur_row[i - 2].a) { \
        goto do_run;                                                       \
    }                                                                      \
}
#else
#define RLE_PRED_2_IMP
#endif

#ifdef RLE_PRED_3
#define RLE_PRED_3_IMP                                                  \
if (i > 1 && cur_row[i - 1].a == cur_row[i - 2].a && i != run_index) {  \
    goto do_run;                                                        \
}
#else
#define RLE_PRED_3_IMP
#endif

/*  a  */
static INLINE BYTE FNAME(decorelate_0)(const PIXEL * const curr, const unsigned int bpc_mask)
{
    return family.xlatU2L[(unsigned)((int)curr[0].a - (int)_PIXEL_A) & bpc_mask];
}

static INLINE void FNAME(corelate_0)(PIXEL *curr, const BYTE corelate,
                                     const unsigned int bpc_mask)
{
    curr->a = (family.xlatL2U[corelate] + _PIXEL_A) & bpc_mask;
}

#ifdef PRED_1

/*  (a+b)/2  */
static INLINE BYTE FNAME(decorelate)(const PIXEL *const prev, const PIXEL * const curr,
                                     const unsigned int bpc_mask)
{
    return family.xlatU2L[(unsigned)((int)curr->a - (int)((_PIXEL_A + _PIXEL_B) >> 1)) & bpc_mask];
}


static INLINE void FNAME(corelate)(const PIXEL *prev, PIXEL *curr, const BYTE corelate,
                                   const unsigned int bpc_mask)
{
    curr->a = (family.xlatL2U[corelate] + (int)((_PIXEL_A + _PIXEL_B) >> 1)) & bpc_mask;
}

#endif

#ifdef PRED_2

/*  .75a+.75b-.5c  */
static INLINE BYTE FNAME(decorelate)(const PIXEL *const prev, const PIXEL * const curr,
                                     const unsigned int bpc_mask)
{
    int p = ((int)(3 * (_PIXEL_A + _PIXEL_B)) - (int)(_PIXEL_C << 1)) >> 2;

    if (p < 0) {
        p = 0;
    } else if ((unsigned)p > bpc_mask) {
        p = bpc_mask;
    }

    {
        return family.xlatU2L[(unsigned)((int)curr->a - p) & bpc_mask];
    }
}

static INLINE void FNAME(corelate)(const PIXEL *prev, PIXEL *curr, const BYTE corelate,
                                   const unsigned int bpc_mask)
{
    const int p = ((int)(3 * (_PIXEL_A + _PIXEL_B)) - (int)(_PIXEL_C << 1)) >> 2;
    const unsigned int s = family.xlatL2U[corelate];

    if (!(p & ~bpc_mask)) {
        curr->a = (s + (unsigned)p) & bpc_mask;
    } else if (p < 0) {
        curr->a = s;
    } else {
        curr->a = (s + bpc_mask) & bpc_mask;
    }
}

#endif

static void FNAME(compress_row0_seg)(Encoder *encoder, Channel *channel, int i,
                                     const PIXEL * const cur_row,
                                     const int end,
                                     const unsigned int waitmask,
                                     const unsigned int bpc,
                                     const unsigned int bpc_mask)
{
    BYTE * const decorelate_drow = channel->correlate_row;
    int stopidx;

    spice_assert(end - i > 0);

    if (i == 0) {
        unsigned int codeword, codewordlen;

        decorelate_drow[0] = family.xlatU2L[cur_row->a];
        golomb_coding(decorelate_drow[0], find_bucket(channel, decorelate_drow[-1])->bestcode,
                      &codeword, &codewordlen);
        encode(encoder, codeword, codewordlen);

        if (channel->state.waitcnt) {
            channel->state.waitcnt--;
        } else {
            channel->state.waitcnt = (tabrand(&channel->state.tabrand_seed) & waitmask);
            update_model(&channel->state, find_bucket(channel, decorelate_drow[-1]),
                         decorelate_drow[i], bpc);
        }
        stopidx = ++i + channel->state.waitcnt;
    } else {
        stopidx = i + channel->state.waitcnt;
    }

    while (stopidx < end) {
        for (; i <= stopidx; i++) {
            unsigned int codeword, codewordlen;
            decorelate_drow[i] = FNAME(decorelate_0)(&cur_row[i], bpc_mask);
            golomb_coding(decorelate_drow[i],
                          find_bucket(channel, decorelate_drow[i - 1])->bestcode, &codeword,
                          &codewordlen);
            encode(encoder, codeword, codewordlen);
        }

        update_model(&channel->state, find_bucket(channel, decorelate_drow[stopidx - 1]),
                     decorelate_drow[stopidx], bpc);
        stopidx = i + (tabrand(&channel->state.tabrand_seed) & waitmask);
    }

    for (; i < end; i++) {
        unsigned int codeword, codewordlen;
        decorelate_drow[i] = FNAME(decorelate_0)(&cur_row[i], bpc_mask);
        golomb_coding(decorelate_drow[i], find_bucket(channel, decorelate_drow[i - 1])->bestcode,
                      &codeword, &codewordlen);
        encode(encoder, codeword, codewordlen);
    }
    channel->state.waitcnt = stopidx - end;
}

static void FNAME(compress_row0)(Encoder *encoder, Channel *channel, const PIXEL *cur_row,
                                 unsigned int width)
{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    int pos = 0;

    while ((wmimax > (int)channel->state.wmidx) && (channel->state.wmileft <= width)) {
        if (channel->state.wmileft) {
            FNAME(compress_row0_seg)(encoder, channel, pos, cur_row, pos + channel->state.wmileft,
                                     bppmask[channel->state.wmidx], bpc, bpc_mask);
            width -= channel->state.wmileft;
            pos += channel->state.wmileft;
        }

        channel->state.wmidx++;
        set_wm_trigger(&channel->state);
        channel->state.wmileft = wminext;
    }

    if (width) {
        FNAME(compress_row0_seg)(encoder, channel, pos, cur_row, pos + width,
                                 bppmask[channel->state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)channel->state.wmidx) {
            channel->state.wmileft -= width;
        }
    }

    spice_assert((int)channel->state.wmidx <= wmimax);
    spice_assert(channel->state.wmidx <= 32);
    spice_assert(wminext > 0);
}

static void FNAME(compress_row_seg)(Encoder *encoder, Channel *channel, int i,
                                    const PIXEL * const prev_row,
                                    const PIXEL * const cur_row,
                                    const int end,
                                    const unsigned int waitmask,
                                    const unsigned int bpc,
                                    const unsigned int bpc_mask)
{
    BYTE * const decorelate_drow = channel->correlate_row;
    int stopidx;
#ifdef RLE
    int run_index = 0;
    int run_size;
#endif

    spice_assert(end - i > 0);

    if (!i) {
        unsigned int codeword, codewordlen;

        decorelate_drow[0] = family.xlatU2L[(unsigned)((int)cur_row->a -
                                                       (int)prev_row->a) & bpc_mask];

        golomb_coding(decorelate_drow[0],
                      find_bucket(channel, decorelate_drow[-1])->bestcode,
                      &codeword,
                      &codewordlen);
        encode(encoder, codeword, codewordlen);

        if (channel->state.waitcnt) {
            channel->state.waitcnt--;
        } else {
            channel->state.waitcnt = (tabrand(&channel->state.tabrand_seed) & waitmask);
            update_model(&channel->state, find_bucket(channel, decorelate_drow[-1]),
                         decorelate_drow[0], bpc);
        }
        stopidx = ++i + channel->state.waitcnt;
    } else {
        stopidx = i + channel->state.waitcnt;
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
                decorelate_drow[i] = FNAME(decorelate)(&prev_row[i], &cur_row[i], bpc_mask);
                golomb_coding(decorelate_drow[i],
                              find_bucket(channel, decorelate_drow[i - 1])->bestcode, &codeword,
                              &codewordlen);
                encode(encoder, codeword, codewordlen);
            }

            update_model(&channel->state, find_bucket(channel, decorelate_drow[stopidx - 1]),
                         decorelate_drow[stopidx], bpc);
            stopidx = i + (tabrand(&channel->state.tabrand_seed) & waitmask);
        }

        for (; i < end; i++) {
            unsigned int codeword, codewordlen;
#ifdef RLE
            RLE_PRED_1_IMP;
            RLE_PRED_2_IMP;
            RLE_PRED_3_IMP;
#endif
            decorelate_drow[i] = FNAME(decorelate)(&prev_row[i], &cur_row[i], bpc_mask);
            golomb_coding(decorelate_drow[i], find_bucket(channel,
                                                          decorelate_drow[i - 1])->bestcode,
                          &codeword, &codewordlen);
            encode(encoder, codeword, codewordlen);
        }
        channel->state.waitcnt = stopidx - end;

        return;

#ifdef RLE
do_run:
        run_index = i;
        channel->state.waitcnt = stopidx - i;
        run_size = 0;

        while (cur_row[i].a == cur_row[i - 1].a) {
            run_size++;
            if (++i == end) {
#ifdef RLE_STAT
                encode_channel_run(encoder, channel, run_size);
#else
                encode_run(encoder, run_size);
#endif
                return;
            }
        }
#ifdef RLE_STAT
        encode_channel_run(encoder, channel, run_size);
#else
        encode_run(encoder, run_size);
#endif
        stopidx = i + channel->state.waitcnt;
#endif
    }
}

static void FNAME(compress_row)(Encoder *encoder, Channel *channel,
                                const PIXEL * const prev_row,
                                const PIXEL * const cur_row,
                                unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    unsigned int pos = 0;

    while ((wmimax > (int)channel->state.wmidx) && (channel->state.wmileft <= width)) {
        if (channel->state.wmileft) {
            FNAME(compress_row_seg)(encoder, channel, pos, prev_row, cur_row,
                                    pos + channel->state.wmileft, bppmask[channel->state.wmidx],
                                    bpc, bpc_mask);
            width -= channel->state.wmileft;
            pos += channel->state.wmileft;
        }

        channel->state.wmidx++;
        set_wm_trigger(&channel->state);
        channel->state.wmileft = wminext;
    }

    if (width) {
        FNAME(compress_row_seg)(encoder, channel, pos, prev_row, cur_row, pos + width,
                                bppmask[channel->state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)channel->state.wmidx) {
            channel->state.wmileft -= width;
        }
    }

    spice_assert((int)channel->state.wmidx <= wmimax);
    spice_assert(channel->state.wmidx <= 32);
    spice_assert(wminext > 0);
}

static void FNAME(uncompress_row0_seg)(Encoder *encoder, Channel *channel, int i,
                                       BYTE * const correlate_row,
                                       PIXEL * const cur_row,
                                       const int end,
                                       const unsigned int waitmask,
                                       const unsigned int bpc,
                                       const unsigned int bpc_mask)
{
    int stopidx;

    spice_assert(end - i > 0);

    if (i == 0) {
        unsigned int codewordlen;

        correlate_row[0] = (BYTE)golomb_decoding(find_bucket(channel,
                                                             correlate_row[-1])->bestcode,
                                                 encoder->io_word, &codewordlen);
        cur_row[0].a = (BYTE)family.xlatL2U[correlate_row[0]];
        decode_eatbits(encoder, codewordlen);

        if (channel->state.waitcnt) {
            --channel->state.waitcnt;
        } else {
            channel->state.waitcnt = (tabrand(&channel->state.tabrand_seed) & waitmask);
            update_model(&channel->state, find_bucket(channel, correlate_row[-1]),
                         correlate_row[0], bpc);
        }
        stopidx = ++i + channel->state.waitcnt;
    } else {
        stopidx = i + channel->state.waitcnt;
    }

    while (stopidx < end) {
        struct s_bucket * pbucket = NULL;

        for (; i <= stopidx; i++) {
            unsigned int codewordlen;

            pbucket = find_bucket(channel, correlate_row[i - 1]);
            correlate_row[i] = (BYTE)golomb_decoding(pbucket->bestcode, encoder->io_word,
                                                     &codewordlen);
            FNAME(corelate_0)(&cur_row[i], correlate_row[i], bpc_mask);
            decode_eatbits(encoder, codewordlen);
        }

        update_model(&channel->state, pbucket, correlate_row[stopidx], bpc);

        stopidx = i + (tabrand(&channel->state.tabrand_seed) & waitmask);
    }

    for (; i < end; i++) {
        unsigned int codewordlen;

        correlate_row[i] = (BYTE)golomb_decoding(find_bucket(channel,
                                                             correlate_row[i - 1])->bestcode,
                                                 encoder->io_word, &codewordlen);
        FNAME(corelate_0)(&cur_row[i], correlate_row[i], bpc_mask);
        decode_eatbits(encoder, codewordlen);
    }
    channel->state.waitcnt = stopidx - end;
}

static void FNAME(uncompress_row0)(Encoder *encoder, Channel *channel,
                                   PIXEL * const cur_row,
                                   unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    BYTE * const correlate_row = channel->correlate_row;
    unsigned int pos = 0;

    while ((wmimax > (int)channel->state.wmidx) && (channel->state.wmileft <= width)) {
        if (channel->state.wmileft) {
            FNAME(uncompress_row0_seg)(encoder, channel, pos, correlate_row, cur_row,
                                       pos + channel->state.wmileft, bppmask[channel->state.wmidx],
                                       bpc, bpc_mask);
            pos += channel->state.wmileft;
            width -= channel->state.wmileft;
        }

        channel->state.wmidx++;
        set_wm_trigger(&channel->state);
        channel->state.wmileft = wminext;
    }

    if (width) {
        FNAME(uncompress_row0_seg)(encoder, channel, pos, correlate_row, cur_row, pos + width,
                                   bppmask[channel->state.wmidx], bpc, bpc_mask);
        if (wmimax > (int)channel->state.wmidx) {
            channel->state.wmileft -= width;
        }
    }

    spice_assert((int)channel->state.wmidx <= wmimax);
    spice_assert(channel->state.wmidx <= 32);
    spice_assert(wminext > 0);
}

static void FNAME(uncompress_row_seg)(Encoder *encoder, Channel *channel,
                                      BYTE *correlate_row,
                                      const PIXEL * const prev_row,
                                      PIXEL * const cur_row,
                                      int i,
                                      const int end,
                                      const unsigned int bpc,
                                      const unsigned int bpc_mask)
{
    const unsigned int waitmask = bppmask[channel->state.wmidx];
    int stopidx;
#ifdef RLE
    int run_index = 0;
    int run_end;
#endif

    spice_assert(end - i > 0);

    if (i == 0) {
        unsigned int codewordlen;

        correlate_row[0] = (BYTE)golomb_decoding(find_bucket(channel, correlate_row[-1])->bestcode,
                                                             encoder->io_word, &codewordlen);
        cur_row[0].a = (family.xlatL2U[correlate_row[0]] + prev_row[0].a) & bpc_mask;
        decode_eatbits(encoder, codewordlen);

        if (channel->state.waitcnt) {
            --channel->state.waitcnt;
        } else {
            channel->state.waitcnt = (tabrand(&channel->state.tabrand_seed) & waitmask);
            update_model(&channel->state, find_bucket(channel, correlate_row[-1]),
                         correlate_row[0], bpc);
        }
        stopidx = ++i + channel->state.waitcnt;
    } else {
        stopidx = i + channel->state.waitcnt;
    }
    for (;;) {
        while (stopidx < end) {
            struct s_bucket * pbucket = NULL;

            for (; i <= stopidx; i++) {
                unsigned int codewordlen;
#ifdef RLE
                RLE_PRED_1_IMP;
                RLE_PRED_2_IMP;
                RLE_PRED_3_IMP;
#endif
                pbucket = find_bucket(channel, correlate_row[i - 1]);
                correlate_row[i] = (BYTE)golomb_decoding(pbucket->bestcode, encoder->io_word,
                                                         &codewordlen);
                FNAME(corelate)(&prev_row[i], &cur_row[i], correlate_row[i], bpc_mask);
                decode_eatbits(encoder, codewordlen);
            }

            update_model(&channel->state, pbucket, correlate_row[stopidx], bpc);

            stopidx = i + (tabrand(&channel->state.tabrand_seed) & waitmask);
        }

        for (; i < end; i++) {
            unsigned int codewordlen;
#ifdef RLE
            RLE_PRED_1_IMP;
            RLE_PRED_2_IMP;
            RLE_PRED_3_IMP;
#endif
            correlate_row[i] = (BYTE)golomb_decoding(find_bucket(channel,
                                                                 correlate_row[i - 1])->bestcode,
                                                     encoder->io_word, &codewordlen);
            FNAME(corelate)(&prev_row[i], &cur_row[i], correlate_row[i], bpc_mask);
            decode_eatbits(encoder, codewordlen);
        }

        channel->state.waitcnt = stopidx - end;

        return;

#ifdef RLE
do_run:
        channel->state.waitcnt = stopidx - i;
        run_index = i;
#ifdef RLE_STAT
        run_end = i + decode_channel_run(encoder, channel);
#else
        run_end = i + decode_run(encoder);
#endif

        for (; i < run_end; i++) {
            cur_row[i].a = cur_row[i - 1].a;
        }

        if (i == end) {
            return;
        }

        stopidx = i + channel->state.waitcnt;
#endif
    }
}

static void FNAME(uncompress_row)(Encoder *encoder, Channel *channel,
                                  const PIXEL * const prev_row,
                                  PIXEL * const cur_row,
                                  unsigned int width)

{
    const unsigned int bpc = BPC;
    const unsigned int bpc_mask = BPC_MASK;
    BYTE * const correlate_row = channel->correlate_row;
    unsigned int pos = 0;

    while ((wmimax > (int)channel->state.wmidx) && (channel->state.wmileft <= width)) {
        if (channel->state.wmileft) {
            FNAME(uncompress_row_seg)(encoder, channel, correlate_row, prev_row, cur_row, pos,
                                      pos + channel->state.wmileft, bpc, bpc_mask);
            pos += channel->state.wmileft;
            width -= channel->state.wmileft;
        }

        channel->state.wmidx++;
        set_wm_trigger(&channel->state);
        channel->state.wmileft = wminext;
    }

    if (width) {
        FNAME(uncompress_row_seg)(encoder, channel, correlate_row, prev_row, cur_row, pos,
                                  pos + width, bpc, bpc_mask);
        if (wmimax > (int)channel->state.wmidx) {
            channel->state.wmileft -= width;
        }
    }

    spice_assert((int)channel->state.wmidx <= wmimax);
    spice_assert(channel->state.wmidx <= 32);
    spice_assert(wminext > 0);
}

#undef PIXEL
#undef FNAME
#undef _PIXEL_A
#undef _PIXEL_B
#undef _PIXEL_C
#undef RLE_PRED_1_IMP
#undef RLE_PRED_2_IMP
#undef RLE_PRED_3_IMP
#undef golomb_coding
#undef golomb_deoding
#undef update_model
#undef find_bucket
#undef family
#undef BPC
#undef BPC_MASK
