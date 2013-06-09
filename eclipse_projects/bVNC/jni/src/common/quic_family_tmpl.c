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

#ifdef QUIC_FAMILY_8BPC
#undef QUIC_FAMILY_8BPC
#define FNAME(name) name##_8bpc
#define VNAME(name) name##_8bpc
#define BPC 8
#endif


#ifdef QUIC_FAMILY_5BPC
#undef QUIC_FAMILY_5BPC
#define FNAME(name) name##_5bpc
#define VNAME(name) name##_5bpc
#define BPC 5
#endif


static unsigned int FNAME(golomb_code_len)(const BYTE n, const unsigned int l)
{
    if (n < VNAME(family).nGRcodewords[l]) {
        return (n >> l) + 1 + l;
    } else {
        return VNAME(family).notGRcwlen[l];
    }
}

static void FNAME(golomb_coding)(const BYTE n, const unsigned int l, unsigned int * const codeword,
                                 unsigned int * const codewordlen)
{
    if (n < VNAME(family).nGRcodewords[l]) {
        (*codeword) = bitat[l] | (n & bppmask[l]);
        (*codewordlen) = (n >> l) + l + 1;
    } else {
        (*codeword) = n - VNAME(family).nGRcodewords[l];
        (*codewordlen) = VNAME(family).notGRcwlen[l];
    }
}

static unsigned int FNAME(golomb_decoding)(const unsigned int l, const unsigned int bits,
                                           unsigned int * const codewordlen)
{
    if (bits > VNAME(family).notGRprefixmask[l]) { /*GR*/
        const unsigned int zeroprefix = cnt_l_zeroes(bits);       /* leading zeroes in codeword */
        const unsigned int cwlen = zeroprefix + 1 + l;            /* codeword length */
        (*codewordlen) = cwlen;
        return (zeroprefix << l) | ((bits >> (32 - cwlen)) & bppmask[l]);
    } else { /* not-GR */
        const unsigned int cwlen = VNAME(family).notGRcwlen[l];
        (*codewordlen) = cwlen;
        return VNAME(family).nGRcodewords[l] + ((bits) >> (32 - cwlen) &
                                                bppmask[VNAME(family).notGRsuffixlen[l]]);
    }
}

/* update the bucket using just encoded curval */
static void FNAME(update_model)(CommonState *state, s_bucket * const bucket,
                                const BYTE curval, unsigned int bpp)
{
    COUNTER * const pcounters = bucket->pcounters;
    unsigned int i;
    unsigned int bestcode;
    unsigned int bestcodelen;
    //unsigned int bpp = encoder->bpp;

    /* update counters, find minimum */

    bestcode = bpp - 1;
    bestcodelen = (pcounters[bestcode] += FNAME(golomb_code_len)(curval, bestcode));

    for (i = bpp - 2; i < bpp; i--) { /* NOTE: expression i<bpp for signed int i would be: i>=0 */
        const unsigned int ithcodelen = (pcounters[i] += FNAME(golomb_code_len)(curval, i));

        if (ithcodelen < bestcodelen) {
            bestcode = i;
            bestcodelen = ithcodelen;
        }
    }

    bucket->bestcode = bestcode; /* store the found minimum */

    if (bestcodelen > state->wm_trigger) { /* halving counters? */
        for (i = 0; i < bpp; i++) {
            pcounters[i] >>= 1;
        }
    }
}

static s_bucket *FNAME(find_bucket)(Channel *channel, const unsigned int val)
{
    spice_assert(val < (0x1U << BPC));

    return channel->_buckets_ptrs[val];
}

#undef FNAME
#undef VNAME
#undef BPC
