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

// Red Hat image compression based on SFALIC by Roman Starosolski
// http://sun.iinf.polsl.gliwice.pl/~rstaros/sfalic/index.html

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "quic.h"
#include "spice_common.h"
#include "bitops.h"

#define RLE
#define RLE_STAT
#define PRED_1
//#define RLE_PRED_1
#define RLE_PRED_2
//#define RLE_PRED_3
#define QUIC_RGB

#define QUIC_MAGIC (*(uint32_t *)"QUIC")
#define QUIC_VERSION_MAJOR 0U
#define QUIC_VERSION_MINOR 1U
#define QUIC_VERSION ((QUIC_VERSION_MAJOR << 16) | (QUIC_VERSION_MAJOR & 0xffff))

typedef uint8_t BYTE;

/* maximum number of codes in family */
#define MAXNUMCODES 8

/* model evolution, warning: only 1,3 and 5 allowed */
#define DEFevol 3
#define MINevol 0
#define MAXevol 5

/* starting wait mask index */
#define DEFwmistart 0
#define MINwmistart 0

/* codeword length limit */
#define DEFmaxclen 26

/* target wait mask index */
#define DEFwmimax 6

/* number of symbols to encode before increasing wait mask index */
#define DEFwminext 2048
#define MINwminext 1
#define MAXwminext 100000000

typedef struct QuicFamily {
    unsigned int nGRcodewords[MAXNUMCODES];      /* indexed by code number, contains number of
                                                    unmodified GR codewords in the code */
    unsigned int notGRcwlen[MAXNUMCODES];        /* indexed by code number, contains codeword
                                                    length of the not-GR codeword */
    unsigned int notGRprefixmask[MAXNUMCODES];   /* indexed by code number, contains mask to
                                                    determine if the codeword is GR or not-GR */
    unsigned int notGRsuffixlen[MAXNUMCODES];    /* indexed by code number, contains suffix
                                                    length of the not-GR codeword */

    /* array for translating distribution U to L for depths up to 8 bpp,
    initialized by decorelateinit() */
    BYTE xlatU2L[256];

    /* array for translating distribution L to U for depths up to 8 bpp,
       initialized by corelateinit() */
    unsigned int xlatL2U[256];
} QuicFamily;

static QuicFamily family_8bpc;
static QuicFamily family_5bpc;

typedef unsigned COUNTER;   /* counter in the array of counters in bucket of the data model */

typedef struct s_bucket {
    COUNTER *pcounters;     /* pointer to array of counters */
    unsigned int bestcode;  /* best code so far */
} s_bucket;

typedef struct Encoder Encoder;

typedef struct CommonState {
    Encoder *encoder;

    unsigned int waitcnt;
    unsigned int tabrand_seed;
    unsigned int wm_trigger;
    unsigned int wmidx;
    unsigned int wmileft;

#ifdef RLE_STAT
    int melcstate; /* index to the state array */

    int melclen;  /* contents of the state array location
                     indexed by melcstate: the "expected"
                     run length is 2^melclen, shorter runs are
                     encoded by a 1 followed by the run length
                     in binary representation, wit a fixed length
                     of melclen bits */

    unsigned long melcorder;  /* 2^ melclen */
#endif
} CommonState;


#define MAX_CHANNELS 4

typedef struct FamilyStat {
    s_bucket **buckets_ptrs;
    s_bucket *buckets_buf;
    COUNTER *counters;
} FamilyStat;

typedef struct Channel {
    Encoder *encoder;

    int correlate_row_width;
    BYTE *correlate_row;

    s_bucket **_buckets_ptrs;

    FamilyStat family_stat_8bpc;
    FamilyStat family_stat_5bpc;

    CommonState state;
} Channel;

struct Encoder {
    QuicUsrContext *usr;
    QuicImageType type;
    unsigned int width;
    unsigned int height;
    unsigned int num_channels;

    unsigned int n_buckets_8bpc;
    unsigned int n_buckets_5bpc;

    unsigned int io_available_bits;
    uint32_t io_word;
    uint32_t io_next_word;
    uint32_t *io_now;
    uint32_t *io_end;
    uint32_t io_words_count;

    int rows_completed;

    Channel channels[MAX_CHANNELS];

    CommonState rgb_state;
};

/* target wait mask index */
static int wmimax = DEFwmimax;

/* number of symbols to encode before increasing wait mask index */
static int wminext = DEFwminext;

/* model evolution mode */
static int evol = DEFevol;

/* bppmask[i] contains i ones as lsb-s */
static const unsigned long int bppmask[33] = {
    0x00000000, /* [0] */
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff /* [32] */
};

static const unsigned int bitat[32] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000 /* [31]*/
};


#define TABRAND_TABSIZE 256
#define TABRAND_SEEDMASK 0x0ff

static const unsigned int tabrand_chaos[TABRAND_TABSIZE] = {
    0x02c57542, 0x35427717, 0x2f5a2153, 0x9244f155, 0x7bd26d07, 0x354c6052, 0x57329b28, 0x2993868e,
    0x6cd8808c, 0x147b46e0, 0x99db66af, 0xe32b4cac, 0x1b671264, 0x9d433486, 0x62a4c192, 0x06089a4b,
    0x9e3dce44, 0xdaabee13, 0x222425ea, 0xa46f331d, 0xcd589250, 0x8bb81d7f, 0xc8b736b9, 0x35948d33,
    0xd7ac7fd0, 0x5fbe2803, 0x2cfbc105, 0x013dbc4e, 0x7a37820f, 0x39f88e9e, 0xedd58794, 0xc5076689,
    0xfcada5a4, 0x64c2f46d, 0xb3ba3243, 0x8974b4f9, 0x5a05aebd, 0x20afcd00, 0x39e2b008, 0x88a18a45,
    0x600bde29, 0xf3971ace, 0xf37b0a6b, 0x7041495b, 0x70b707ab, 0x06beffbb, 0x4206051f, 0xe13c4ee3,
    0xc1a78327, 0x91aa067c, 0x8295f72a, 0x732917a6, 0x1d871b4d, 0x4048f136, 0xf1840e7e, 0x6a6048c1,
    0x696cb71a, 0x7ff501c3, 0x0fc6310b, 0x57e0f83d, 0x8cc26e74, 0x11a525a2, 0x946934c7, 0x7cd888f0,
    0x8f9d8604, 0x4f86e73b, 0x04520316, 0xdeeea20c, 0xf1def496, 0x67687288, 0xf540c5b2, 0x22401484,
    0x3478658a, 0xc2385746, 0x01979c2c, 0x5dad73c8, 0x0321f58b, 0xf0fedbee, 0x92826ddf, 0x284bec73,
    0x5b1a1975, 0x03df1e11, 0x20963e01, 0xa17cf12b, 0x740d776e, 0xa7a6bf3c, 0x01b5cce4, 0x1118aa76,
    0xfc6fac0a, 0xce927e9b, 0x00bf2567, 0x806f216c, 0xbca69056, 0x795bd3e9, 0xc9dc4557, 0x8929b6c2,
    0x789d52ec, 0x3f3fbf40, 0xb9197368, 0xa38c15b5, 0xc3b44fa8, 0xca8333b0, 0xb7e8d590, 0xbe807feb,
    0xbf5f8360, 0xd99e2f5c, 0x372928e1, 0x7c757c4c, 0x0db5b154, 0xc01ede02, 0x1fc86e78, 0x1f3985be,
    0xb4805c77, 0x00c880fa, 0x974c1b12, 0x35ab0214, 0xb2dc840d, 0x5b00ae37, 0xd313b026, 0xb260969d,
    0x7f4c8879, 0x1734c4d3, 0x49068631, 0xb9f6a021, 0x6b863e6f, 0xcee5debf, 0x29f8c9fb, 0x53dd6880,
    0x72b61223, 0x1f67a9fd, 0x0a0f6993, 0x13e59119, 0x11cca12e, 0xfe6b6766, 0x16b6effc, 0x97918fc4,
    0xc2b8a563, 0x94f2f741, 0x0bfa8c9a, 0xd1537ae8, 0xc1da349c, 0x873c60ca, 0x95005b85, 0x9b5c080e,
    0xbc8abbd9, 0xe1eab1d2, 0x6dac9070, 0x4ea9ebf1, 0xe0cf30d4, 0x1ef5bd7b, 0xd161043e, 0x5d2fa2e2,
    0xff5d3cae, 0x86ed9f87, 0x2aa1daa1, 0xbd731a34, 0x9e8f4b22, 0xb1c2c67a, 0xc21758c9, 0xa182215d,
    0xccb01948, 0x8d168df7, 0x04238cfe, 0x368c3dbc, 0x0aeadca5, 0xbad21c24, 0x0a71fee5, 0x9fc5d872,
    0x54c152c6, 0xfc329483, 0x6783384a, 0xeddb3e1c, 0x65f90e30, 0x884ad098, 0xce81675a, 0x4b372f7d,
    0x68bf9a39, 0x43445f1e, 0x40f8d8cb, 0x90d5acb6, 0x4cd07282, 0x349eeb06, 0x0c9d5332, 0x520b24ef,
    0x80020447, 0x67976491, 0x2f931ca3, 0xfe9b0535, 0xfcd30220, 0x61a9e6cc, 0xa487d8d7, 0x3f7c5dd1,
    0x7d0127c5, 0x48f51d15, 0x60dea871, 0xc9a91cb7, 0x58b53bb3, 0x9d5e0b2d, 0x624a78b4, 0x30dbee1b,
    0x9bdf22e7, 0x1df5c299, 0x2d5643a7, 0xf4dd35ff, 0x03ca8fd6, 0x53b47ed8, 0x6f2c19aa, 0xfeb0c1f4,
    0x49e54438, 0x2f2577e6, 0xbf876969, 0x72440ea9, 0xfa0bafb8, 0x74f5b3a0, 0x7dd357cd, 0x89ce1358,
    0x6ef2cdda, 0x1e7767f3, 0xa6be9fdb, 0x4f5f88f8, 0xba994a3a, 0x08ca6b65, 0xe0893818, 0x9e00a16a,
    0xf42bfc8f, 0x9972eedc, 0x749c8b51, 0x32c05f5e, 0xd706805f, 0x6bfbb7cf, 0xd9210a10, 0x31a1db97,
    0x923a9559, 0x37a7a1f6, 0x059f8861, 0xca493e62, 0x65157e81, 0x8f6467dd, 0xab85ff9f, 0x9331aff2,
    0x8616b9f5, 0xedbd5695, 0xee7e29b1, 0x313ac44f, 0xb903112f, 0x432ef649, 0xdc0a36c0, 0x61cf2bba,
    0x81474925, 0xa8b6c7ad, 0xee5931de, 0xb2f8158d, 0x59fb7409, 0x2e3dfaed, 0x9af25a3f, 0xe1fed4d5,
};

static unsigned int stabrand(void)
{
    //spice_assert( !(TABRAND_SEEDMASK & TABRAND_TABSIZE));
    //spice_assert( TABRAND_SEEDMASK + 1 == TABRAND_TABSIZE );

    return TABRAND_SEEDMASK;
}

static unsigned int tabrand(unsigned int *tabrand_seed)
{
    return tabrand_chaos[++*tabrand_seed & TABRAND_SEEDMASK];
}

static const unsigned short besttrigtab[3][11] = { /* array of wm_trigger for waitmask and evol,
                                                    used by set_wm_trigger() */
    /* 1 */ { 550, 900, 800, 700, 500, 350, 300, 200, 180, 180, 160},
    /* 3 */ { 110, 550, 900, 800, 550, 400, 350, 250, 140, 160, 140},
    /* 5 */ { 100, 120, 550, 900, 700, 500, 400, 300, 220, 250, 160}
};

/* set wm_trigger knowing waitmask (param) and evol (glob)*/
static void set_wm_trigger(CommonState *state)
{
    unsigned int wm = state->wmidx;
    if (wm > 10) {
        wm = 10;
    }

    spice_assert(evol < 6);

    state->wm_trigger = besttrigtab[evol / 2][wm];

    spice_assert(state->wm_trigger <= 2000);
    spice_assert(state->wm_trigger >= 1);
}

static int ceil_log_2(int val) /* ceil(log_2(val)) */
{
    int result;

    //spice_assert(val>0);

    if (val == 1) {
        return 0;
    }

    result = 1;
    val -= 1;
    while (val >>= 1) {
        result++;
    }

    return result;
}

/* number of leading zeroes in the byte, used by cntlzeroes(uint)*/
static const BYTE lzeroes[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* count leading zeroes */
static unsigned int cnt_l_zeroes(const unsigned int bits)
{
    if (bits & 0xff800000) {
        return lzeroes[bits >> 24];
    } else if (bits & 0xffff8000) {
        return 8 + lzeroes[(bits >> 16) & 0x000000ff];
    } else if (bits & 0xffffff80) {
        return 16 + lzeroes[(bits >> 8) & 0x000000ff];
    } else {
        return 24 + lzeroes[bits & 0x000000ff];
    }
}

#define QUIC_FAMILY_8BPC
#include "quic_family_tmpl.c"

#ifdef QUIC_RGB
#define QUIC_FAMILY_5BPC
#include "quic_family_tmpl.c"
#endif

static void decorelate_init(QuicFamily *family, int bpc)
{
    const unsigned int pixelbitmask = bppmask[bpc];
    const unsigned int pixelbitmaskshr = pixelbitmask >> 1;
    unsigned int s;

    //spice_assert(bpc <= 8);

    for (s = 0; s <= pixelbitmask; s++) {
        if (s <= pixelbitmaskshr) {
            family->xlatU2L[s] = s << 1;
        } else {
            family->xlatU2L[s] = ((pixelbitmask - s) << 1) + 1;
        }
    }
}

static void corelate_init(QuicFamily *family, int bpc)
{
    const unsigned long int pixelbitmask = bppmask[bpc];
    unsigned long int s;

    //spice_assert(bpc <= 8);

    for (s = 0; s <= pixelbitmask; s++) {
        if (s & 0x01) {
            family->xlatL2U[s] = pixelbitmask - (s >> 1);
        } else {
            family->xlatL2U[s] = (s >> 1);
        }
    }
}

static void family_init(QuicFamily *family, int bpc, int limit)
{
    int l;

    for (l = 0; l < bpc; l++) { /* fill arrays indexed by code number */
        int altprefixlen, altcodewords;

        altprefixlen = limit - bpc;
        if (altprefixlen > (int)(bppmask[bpc - l])) {
            altprefixlen = bppmask[bpc - l];
        }

        altcodewords = bppmask[bpc] + 1 - (altprefixlen << l);

        family->nGRcodewords[l] = (altprefixlen << l);
        family->notGRcwlen[l] = altprefixlen + ceil_log_2(altcodewords);
        family->notGRprefixmask[l] = bppmask[32 - altprefixlen]; /* needed for decoding only */
        family->notGRsuffixlen[l] = ceil_log_2(altcodewords); /* needed for decoding only */
    }

    decorelate_init(family, bpc);
    corelate_init(family, bpc);
}

static void more_io_words(Encoder *encoder)
{
    uint32_t *io_ptr;
    int num_io_words = encoder->usr->more_space(encoder->usr, &io_ptr, encoder->rows_completed);
    if (num_io_words <= 0) {
        encoder->usr->error(encoder->usr, "%s: no more words\n", __FUNCTION__);
    }
    spice_assert(io_ptr);
    encoder->io_words_count += num_io_words;
    encoder->io_now = io_ptr;
    encoder->io_end = encoder->io_now + num_io_words;
}

static void __write_io_word(Encoder *encoder)
{
    more_io_words(encoder);
    *(encoder->io_now++) = encoder->io_word;
}

static void (*__write_io_word_ptr)(Encoder *encoder) = __write_io_word;

static INLINE void write_io_word(Encoder *encoder)
{
    if (encoder->io_now == encoder->io_end) {
        __write_io_word_ptr(encoder); //disable inline optimizations
        return;
    }
    *(encoder->io_now++) = encoder->io_word;
}

static INLINE void encode(Encoder *encoder, unsigned int word, unsigned int len)
{
    int delta;

    spice_assert(len > 0 && len < 32);
    spice_assert(!(word & ~bppmask[len]));
    if ((delta = ((int)encoder->io_available_bits - len)) >= 0) {
        encoder->io_available_bits = delta;
        encoder->io_word |= word << encoder->io_available_bits;
        return;
    }
    delta = -delta;
    encoder->io_word |= word >> delta;
    write_io_word(encoder);
    encoder->io_available_bits = 32 - delta;
    encoder->io_word = word << encoder->io_available_bits;

    spice_assert(encoder->io_available_bits < 32);
    spice_assert((encoder->io_word & bppmask[encoder->io_available_bits]) == 0);
}

static INLINE void encode_32(Encoder *encoder, unsigned int word)
{
    encode(encoder, word >> 16, 16);
    encode(encoder, word & 0x0000ffff, 16);
}

static INLINE void flush(Encoder *encoder)
{
    if (encoder->io_available_bits > 0 && encoder->io_available_bits != 32) {
        encode(encoder, 0, encoder->io_available_bits);
    }
    encode_32(encoder, 0);
    encode(encoder, 0, 1);
}

static void __read_io_word(Encoder *encoder)
{
    more_io_words(encoder);
    encoder->io_next_word = *(encoder->io_now++);
}

static void (*__read_io_word_ptr)(Encoder *encoder) = __read_io_word;


static INLINE void read_io_word(Encoder *encoder)
{
    if (encoder->io_now == encoder->io_end) {
        __read_io_word_ptr(encoder); //disable inline optimizations
        return;
    }
    spice_assert(encoder->io_now < encoder->io_end);
    encoder->io_next_word = *(encoder->io_now++);
}

static INLINE void decode_eatbits(Encoder *encoder, int len)
{
    int delta;

    spice_assert(len > 0 && len < 32);
    encoder->io_word <<= len;

    if ((delta = ((int)encoder->io_available_bits - len)) >= 0) {
        encoder->io_available_bits = delta;
        encoder->io_word |= encoder->io_next_word >> encoder->io_available_bits;
        return;
    }

    delta = -delta;
    encoder->io_word |= encoder->io_next_word << delta;
    read_io_word(encoder);
    encoder->io_available_bits = 32 - delta;
    encoder->io_word |= (encoder->io_next_word >> encoder->io_available_bits);
}

static INLINE void decode_eat32bits(Encoder *encoder)
{
    decode_eatbits(encoder, 16);
    decode_eatbits(encoder, 16);
}

#ifdef RLE

#ifdef RLE_STAT

static INLINE void encode_ones(Encoder *encoder, unsigned int n)
{
    unsigned int count;

    for (count = n >> 5; count; count--) {
        encode(encoder, ~0U, 32);
    }

    if ((n &= 0x1f)) {
        encode(encoder, (1U << n) - 1, n);
    }
}

#define MELCSTATES 32 /* number of melcode states */

static int zeroLUT[256]; /* table to find out number of leading zeros */

static int J[MELCSTATES] = {
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7,
    7, 8, 9, 10, 11, 12, 13, 14, 15
};

/* creates the bit counting look-up table. */
static void init_zeroLUT(void)
{
    int i, j, k, l;

    j = k = 1;
    l = 8;
    for (i = 0; i < 256; ++i) {
        zeroLUT[i] = l;
        --k;
        if (k == 0) {
            k = j;
            --l;
            j *= 2;
        }
    }
}

static void encoder_init_rle(CommonState *state)
{
    state->melcstate = 0;
    state->melclen = J[0];
    state->melcorder = 1 << state->melclen;
}

#ifdef QUIC_RGB

static void encode_run(Encoder *encoder, unsigned int runlen) //todo: try use end of line
{
    int hits = 0;

    while (runlen >= encoder->rgb_state.melcorder) {
        hits++;
        runlen -= encoder->rgb_state.melcorder;
        if (encoder->rgb_state.melcstate < MELCSTATES) {
            encoder->rgb_state.melclen = J[++encoder->rgb_state.melcstate];
            encoder->rgb_state.melcorder = (1L << encoder->rgb_state.melclen);
        }
    }

    /* send the required number of "hit" bits (one per occurrence
       of a run of length melcorder). This number is never too big:
       after 31 such "hit" bits, each "hit" would represent a run of 32K
       pixels.
    */
    encode_ones(encoder, hits);

    encode(encoder, runlen, encoder->rgb_state.melclen + 1);

    /* adjust melcoder parameters */
    if (encoder->rgb_state.melcstate) {
        encoder->rgb_state.melclen = J[--encoder->rgb_state.melcstate];
        encoder->rgb_state.melcorder = (1L << encoder->rgb_state.melclen);
    }
}

#endif

static void encode_channel_run(Encoder *encoder, Channel *channel, unsigned int runlen)
{
    //todo: try use end of line
    int hits = 0;

    while (runlen >= channel->state.melcorder) {
        hits++;
        runlen -= channel->state.melcorder;
        if (channel->state.melcstate < MELCSTATES) {
            channel->state.melclen = J[++channel->state.melcstate];
            channel->state.melcorder = (1L << channel->state.melclen);
        }
    }

    /* send the required number of "hit" bits (one per occurrence
       of a run of length melcorder). This number is never too big:
       after 31 such "hit" bits, each "hit" would represent a run of 32K
       pixels.
    */
    encode_ones(encoder, hits);

    encode(encoder, runlen, channel->state.melclen + 1);

    /* adjust melcoder parameters */
    if (channel->state.melcstate) {
        channel->state.melclen = J[--channel->state.melcstate];
        channel->state.melcorder = (1L << channel->state.melclen);
    }
}

/* decoding routine: reads bits from the input and returns a run length. */
/* argument is the number of pixels left to end-of-line (bound on run length) */

#ifdef QUIC_RGB
static int decode_run(Encoder *encoder)
{
    int runlen = 0;

    do {
        register int temp, hits;
        temp = zeroLUT[(BYTE)(~(encoder->io_word >> 24))];/* number of leading ones in the
                                                                      input stream, up to 8 */
        for (hits = 1; hits <= temp; hits++) {
            runlen += encoder->rgb_state.melcorder;

            if (encoder->rgb_state.melcstate < MELCSTATES) {
                encoder->rgb_state.melclen = J[++encoder->rgb_state.melcstate];
                encoder->rgb_state.melcorder = (1U << encoder->rgb_state.melclen);
            }
        }
        if (temp != 8) {
            decode_eatbits(encoder, temp + 1);  /* consume the leading
                                                            0 of the remainder encoding */
            break;
        }
        decode_eatbits(encoder, 8);
    } while (1);

    /* read the length of the remainder */
    if (encoder->rgb_state.melclen) {
        runlen += encoder->io_word >> (32 - encoder->rgb_state.melclen);
        decode_eatbits(encoder, encoder->rgb_state.melclen);
    }

    /* adjust melcoder parameters */
    if (encoder->rgb_state.melcstate) {
        encoder->rgb_state.melclen = J[--encoder->rgb_state.melcstate];
        encoder->rgb_state.melcorder = (1U << encoder->rgb_state.melclen);
    }

    return runlen;
}

#endif

static int decode_channel_run(Encoder *encoder, Channel *channel)
{
    int runlen = 0;

    do {
        register int temp, hits;
        temp = zeroLUT[(BYTE)(~(encoder->io_word >> 24))];/* number of leading ones in the
                                                                      input stream, up to 8 */
        for (hits = 1; hits <= temp; hits++) {
            runlen += channel->state.melcorder;

            if (channel->state.melcstate < MELCSTATES) {
                channel->state.melclen = J[++channel->state.melcstate];
                channel->state.melcorder = (1U << channel->state.melclen);
            }
        }
        if (temp != 8) {
            decode_eatbits(encoder, temp + 1);  /* consume the leading
                                                            0 of the remainder encoding */
            break;
        }
        decode_eatbits(encoder, 8);
    } while (1);

    /* read the length of the remainder */
    if (channel->state.melclen) {
        runlen += encoder->io_word >> (32 - channel->state.melclen);
        decode_eatbits(encoder, channel->state.melclen);
    }

    /* adjust melcoder parameters */
    if (channel->state.melcstate) {
        channel->state.melclen = J[--channel->state.melcstate];
        channel->state.melcorder = (1U << channel->state.melclen);
    }

    return runlen;
}

#else

static INLINE void encode_run(Encoder *encoder, unsigned int len)
{
    int odd = len & 1U;
    int msb;

    len &= ~1U;

    while ((msb = spice_bit_find_msb(len))) {
        len &= ~(1 << (msb - 1));
        spice_assert(msb < 32);
        encode(encoder, (1 << (msb)) - 1, msb);
        encode(encoder, 0, 1);
    }

    if (odd) {
        encode(encoder, 2, 2);
    } else {
        encode(encoder, 0, 1);
    }
}

static INLINE unsigned int decode_run(Encoder *encoder)
{
    unsigned int len = 0;
    int count;

    do {
        count = 0;
        while (encoder->io_word & (1U << 31)) {
            decode_eatbits(encoder, 1);
            count++;
            spice_assert(count < 32);
        }
        decode_eatbits(encoder, 1);
        len += (1U << count) >> 1;
    } while (count > 1);

    return len;
}

#endif
#endif

static INLINE void init_decode_io(Encoder *encoder)
{
    encoder->io_next_word = encoder->io_word = *(encoder->io_now++);
    encoder->io_available_bits = 0;
}

#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#pragma pack(push)
#pragma pack(1)
#endif

typedef struct ATTR_PACKED one_byte_pixel_t {
    BYTE a;
} one_byte_t;

typedef struct ATTR_PACKED three_bytes_pixel_t {
    BYTE a;
    BYTE b;
    BYTE c;
} three_bytes_t;

typedef struct ATTR_PACKED four_bytes_pixel_t {
    BYTE a;
    BYTE b;
    BYTE c;
    BYTE d;
} four_bytes_t;

typedef struct ATTR_PACKED rgb32_pixel_t {
    BYTE b;
    BYTE g;
    BYTE r;
    BYTE pad;
} rgb32_pixel_t;

typedef struct ATTR_PACKED rgb24_pixel_t {
    BYTE b;
    BYTE g;
    BYTE r;
} rgb24_pixel_t;

typedef uint16_t rgb16_pixel_t;

#ifndef __GNUC__
#pragma pack(pop)
#endif

#undef ATTR_PACKED

#define ONE_BYTE
#include "quic_tmpl.c"

#define FOUR_BYTE
#include "quic_tmpl.c"

#ifdef QUIC_RGB

#define QUIC_RGB32
#include "quic_rgb_tmpl.c"

#define QUIC_RGB24
#include "quic_rgb_tmpl.c"

#define QUIC_RGB16
#include "quic_rgb_tmpl.c"

#define QUIC_RGB16_TO_32
#include "quic_rgb_tmpl.c"

#else

#define THREE_BYTE
#include "quic_tmpl.c"

#endif

static void fill_model_structures(Encoder *encoder, FamilyStat *family_stat,
                                  unsigned int rep_first, unsigned int first_size,
                                  unsigned int rep_next, unsigned int mul_size,
                                  unsigned int levels, unsigned int ncounters,
                                  unsigned int nbuckets, unsigned int n_buckets_ptrs)
{
    unsigned int
    bsize,
    bstart,
    bend = 0,
    repcntr,
    bnumber;

    COUNTER * free_counter = family_stat->counters;/* first free location in the array of
                                                      counters */

    bnumber = 0;

    repcntr = rep_first + 1;    /* first bucket */
    bsize = first_size;

    do { /* others */
        if (bnumber) {
            bstart = bend + 1;
        } else {
            bstart = 0;
        }

        if (!--repcntr) {
            repcntr = rep_next;
            bsize *= mul_size;
        }

        bend = bstart + bsize - 1;
        if (bend + bsize >= levels) {
            bend = levels - 1;
        }

        family_stat->buckets_buf[bnumber].pcounters = free_counter;
        free_counter += ncounters;

        spice_assert(bstart < n_buckets_ptrs);
        {
            unsigned int i;
            spice_assert(bend < n_buckets_ptrs);
            for (i = bstart; i <= bend; i++) {
                family_stat->buckets_ptrs[i] = family_stat->buckets_buf + bnumber;
            }
        }

        bnumber++;
    } while (bend < levels - 1);

    spice_assert(free_counter - family_stat->counters == nbuckets * ncounters);
}

static void find_model_params(Encoder *encoder,
                              const int bpc,
                              unsigned int *ncounters,
                              unsigned int *levels,
                              unsigned int *n_buckets_ptrs,
                              unsigned int *repfirst,
                              unsigned int *firstsize,
                              unsigned int *repnext,
                              unsigned int *mulsize,
                              unsigned int *nbuckets)
{
    unsigned int bsize;              /* bucket size */
    unsigned int bstart, bend = 0;   /* bucket start and end, range : 0 to levels-1*/
    unsigned int repcntr;            /* helper */

    spice_assert(bpc <= 8 && bpc > 0);


    *ncounters = 8;

    *levels = 0x1 << bpc;

    *n_buckets_ptrs = 0;  /* ==0 means: not set yet */

    switch (evol) {   /* set repfirst firstsize repnext mulsize */
    case 1: /* buckets contain following numbers of contexts: 1 1 1 2 2 4 4 8 8 ... */
        *repfirst = 3;
        *firstsize = 1;
        *repnext = 2;
        *mulsize = 2;
        break;
    case 3: /* 1 2 4 8 16 32 64 ... */
        *repfirst = 1;
        *firstsize = 1;
        *repnext = 1;
        *mulsize = 2;
        break;
    case 5:                     /* 1 4 16 64 256 1024 4096 16384 65536 */
        *repfirst = 1;
        *firstsize = 1;
        *repnext = 1;
        *mulsize = 4;
        break;
    case 0: /* obsolete */
    case 2: /* obsolete */
    case 4: /* obsolete */
        encoder->usr->error(encoder->usr, "findmodelparams(): evol value obsolete!!!\n");
    default:
        encoder->usr->error(encoder->usr, "findmodelparams(): evol out of range!!!\n");
    }

    *nbuckets = 0;
    repcntr = *repfirst + 1;    /* first bucket */
    bsize = *firstsize;

    do { /* other buckets */
        if (nbuckets) {         /* bucket start */
            bstart = bend + 1;
        } else {
            bstart = 0;
        }

        if (!--repcntr) {         /* bucket size */
            repcntr = *repnext;
            bsize *= *mulsize;
        }

        bend = bstart + bsize - 1;  /* bucket end */
        if (bend + bsize >= *levels) {  /* if following bucked was bigger than current one */
            bend = *levels - 1;     /* concatenate them */
        }

        if (!*n_buckets_ptrs) {           /* array size not set yet? */
            *n_buckets_ptrs = *levels;
 #if 0
            if (bend == *levels - 1) {   /* this bucket is last - all in the first array */
                *n_buckets_ptrs = *levels;
            } else if (bsize >= 256) { /* this bucket is allowed to reside in the 2nd table */
                b_lo_ptrs = bstart;
                spice_assert(bstart);     /* previous bucket exists */
            }
 #endif
        }

        (*nbuckets)++;
    } while (bend < *levels - 1);
}

static int init_model_structures(Encoder *encoder, FamilyStat *family_stat,
                                 unsigned int rep_first, unsigned int first_size,
                                 unsigned int rep_next, unsigned int mul_size,
                                 unsigned int levels, unsigned int ncounters,
                                 unsigned int n_buckets_ptrs, unsigned int n_buckets)
{
    family_stat->buckets_ptrs = (s_bucket **)encoder->usr->malloc(encoder->usr,
                                                                  n_buckets_ptrs *
                                                                  sizeof(s_bucket *));
    if (!family_stat->buckets_ptrs) {
        return FALSE;
    }

    family_stat->counters = (COUNTER *)encoder->usr->malloc(encoder->usr,
                                                            n_buckets * sizeof(COUNTER) *
                                                            MAXNUMCODES);
    if (!family_stat->counters) {
        goto error_1;
    }

    family_stat->buckets_buf = (s_bucket *)encoder->usr->malloc(encoder->usr,
                                                                n_buckets * sizeof(s_bucket));
    if (!family_stat->buckets_buf) {
        goto error_2;
    }

    fill_model_structures(encoder, family_stat, rep_first, first_size, rep_next, mul_size, levels,
                          ncounters, n_buckets, n_buckets_ptrs);

    return TRUE;

error_2:
    encoder->usr->free(encoder->usr, family_stat->counters);

error_1:
    encoder->usr->free(encoder->usr, family_stat->buckets_ptrs);

    return FALSE;
}

static void free_family_stat(QuicUsrContext *usr, FamilyStat *family_stat)
{
    usr->free(usr, family_stat->buckets_ptrs);
    usr->free(usr, family_stat->counters);
    usr->free(usr, family_stat->buckets_buf);
}

static int init_channel(Encoder *encoder, Channel *channel)
{
    unsigned int ncounters;
    unsigned int levels;
    unsigned int rep_first;
    unsigned int first_size;
    unsigned int rep_next;
    unsigned int mul_size;
    unsigned int n_buckets;
    unsigned int n_buckets_ptrs;

    channel->encoder = encoder;
    channel->state.encoder = encoder;
    channel->correlate_row_width = 0;
    channel->correlate_row = NULL;

    find_model_params(encoder, 8, &ncounters, &levels, &n_buckets_ptrs, &rep_first,
                      &first_size, &rep_next, &mul_size, &n_buckets);
    encoder->n_buckets_8bpc = n_buckets;
    if (!init_model_structures(encoder, &channel->family_stat_8bpc, rep_first, first_size,
                               rep_next, mul_size, levels, ncounters, n_buckets_ptrs,
                               n_buckets)) {
        return FALSE;
    }

    find_model_params(encoder, 5, &ncounters, &levels, &n_buckets_ptrs, &rep_first,
                      &first_size, &rep_next, &mul_size, &n_buckets);
    encoder->n_buckets_5bpc = n_buckets;
    if (!init_model_structures(encoder, &channel->family_stat_5bpc, rep_first, first_size,
                               rep_next, mul_size, levels, ncounters, n_buckets_ptrs,
                               n_buckets)) {
        free_family_stat(encoder->usr, &channel->family_stat_8bpc);
        return FALSE;
    }

    return TRUE;
}

static void destroy_channel(Channel *channel)
{
    QuicUsrContext *usr = channel->encoder->usr;
    if (channel->correlate_row) {
        usr->free(usr, channel->correlate_row - 1);
    }
    free_family_stat(usr, &channel->family_stat_8bpc);
    free_family_stat(usr, &channel->family_stat_5bpc);
}

static int init_encoder(Encoder *encoder, QuicUsrContext *usr)
{
    int i;

    encoder->usr = usr;
    encoder->rgb_state.encoder = encoder;

    for (i = 0; i < MAX_CHANNELS; i++) {
        if (!init_channel(encoder, &encoder->channels[i])) {
            for (--i; i >= 0; i--) {
                destroy_channel(&encoder->channels[i]);
            }
            return FALSE;
        }
    }
    return TRUE;
}

static int encoder_reste(Encoder *encoder, uint32_t *io_ptr, uint32_t *io_ptr_end)
{
    spice_assert(((unsigned long)io_ptr % 4) == ((unsigned long)io_ptr_end % 4));
    spice_assert(io_ptr <= io_ptr_end);

    encoder->rgb_state.waitcnt = 0;
    encoder->rgb_state.tabrand_seed = stabrand();
    encoder->rgb_state.wmidx = DEFwmistart;
    encoder->rgb_state.wmileft = wminext;
    set_wm_trigger(&encoder->rgb_state);

#if defined(RLE) && defined(RLE_STAT)
    encoder_init_rle(&encoder->rgb_state);
#endif

    encoder->io_words_count = io_ptr_end - io_ptr;
    encoder->io_now = io_ptr;
    encoder->io_end = io_ptr_end;
    encoder->rows_completed = 0;

    return TRUE;
}

static int encoder_reste_channels(Encoder *encoder, int channels, int width, int bpc)
{
    int i;

    encoder->num_channels = channels;

    for (i = 0; i < channels; i++) {
        s_bucket *bucket;
        s_bucket *end_bucket;

        if (encoder->channels[i].correlate_row_width < width) {
            encoder->channels[i].correlate_row_width = 0;
            if (encoder->channels[i].correlate_row) {
                encoder->usr->free(encoder->usr, encoder->channels[i].correlate_row - 1);
            }
            if (!(encoder->channels[i].correlate_row = (BYTE *)encoder->usr->malloc(encoder->usr,
                                                                                    width + 1))) {
                return FALSE;
            }
            encoder->channels[i].correlate_row++;
            encoder->channels[i].correlate_row_width = width;
        }

        if (bpc == 8) {
            MEMCLEAR(encoder->channels[i].family_stat_8bpc.counters,
                     encoder->n_buckets_8bpc * sizeof(COUNTER) * MAXNUMCODES);
            bucket = encoder->channels[i].family_stat_8bpc.buckets_buf;
            end_bucket = bucket + encoder->n_buckets_8bpc;
            for (; bucket < end_bucket; bucket++) {
                bucket->bestcode = /*BPC*/ 8 - 1;
            }
            encoder->channels[i]._buckets_ptrs = encoder->channels[i].family_stat_8bpc.buckets_ptrs;
        } else if (bpc == 5) {
            MEMCLEAR(encoder->channels[i].family_stat_5bpc.counters,
                     encoder->n_buckets_5bpc * sizeof(COUNTER) * MAXNUMCODES);
            bucket = encoder->channels[i].family_stat_5bpc.buckets_buf;
            end_bucket = bucket + encoder->n_buckets_5bpc;
            for (; bucket < end_bucket; bucket++) {
                bucket->bestcode = /*BPC*/ 5 - 1;
            }
            encoder->channels[i]._buckets_ptrs = encoder->channels[i].family_stat_5bpc.buckets_ptrs;
        } else {
            encoder->usr->warn(encoder->usr, "%s: bad bpc %d\n", __FUNCTION__, bpc);
            return FALSE;
        }

        encoder->channels[i].state.waitcnt = 0;
        encoder->channels[i].state.tabrand_seed = stabrand();
        encoder->channels[i].state.wmidx = DEFwmistart;
        encoder->channels[i].state.wmileft = wminext;
        set_wm_trigger(&encoder->channels[i].state);

#if defined(RLE) && defined(RLE_STAT)
        encoder_init_rle(&encoder->channels[i].state);
#endif
    }
    return TRUE;
}

static void quic_image_params(Encoder *encoder, QuicImageType type, int *channels, int *bpc)
{
    spice_assert(channels && bpc);
    switch (type) {
    case QUIC_IMAGE_TYPE_GRAY:
        *channels = 1;
        *bpc = 8;
        break;
    case QUIC_IMAGE_TYPE_RGB16:
        *channels = 3;
        *bpc = 5;
#ifndef QUIC_RGB
        encoder->usr->error(encoder->usr, "not implemented\n");
#endif
        break;
    case QUIC_IMAGE_TYPE_RGB24:
        *channels = 3;
        *bpc = 8;
        break;
    case QUIC_IMAGE_TYPE_RGB32:
        *channels = 3;
        *bpc = 8;
        break;
    case QUIC_IMAGE_TYPE_RGBA:
        *channels = 4;
        *bpc = 8;
        break;
    case QUIC_IMAGE_TYPE_INVALID:
    default:
        *channels = 0;
        *bpc = 0;
        encoder->usr->error(encoder->usr, "bad image type\n");
    }
}

#define FILL_LINES() {                                                  \
    if (line == lines_end) {                                            \
        int n = encoder->usr->more_lines(encoder->usr, &line);          \
        if (n <= 0 || line == NULL) {                                   \
            encoder->usr->error(encoder->usr, "more lines failed\n");   \
        }                                                               \
        lines_end = line + n * stride;                                  \
    }                                                                   \
}

#define NEXT_LINE() {       \
    line += stride;         \
    FILL_LINES();           \
}

#define QUIC_COMPRESS_RGB(bits)                                                                 \
        encoder->channels[0].correlate_row[-1] = 0;                                             \
        encoder->channels[1].correlate_row[-1] = 0;                                             \
        encoder->channels[2].correlate_row[-1] = 0;                                             \
        quic_rgb##bits##_compress_row0(encoder, (rgb##bits##_pixel_t *)(line), width);          \
        encoder->rows_completed++;                                                              \
        for (row = 1; row < height; row++) {                                                    \
            prev = line;                                                                        \
            NEXT_LINE();                                                                        \
            encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];     \
            encoder->channels[1].correlate_row[-1] = encoder->channels[1].correlate_row[0];     \
            encoder->channels[2].correlate_row[-1] = encoder->channels[2].correlate_row[0];     \
            quic_rgb##bits##_compress_row(encoder, (rgb##bits##_pixel_t *)prev,                 \
                                          (rgb##bits##_pixel_t *)line, width);                  \
            encoder->rows_completed++;                                                          \
        }

int quic_encode(QuicContext *quic, QuicImageType type, int width, int height,
                uint8_t *line, unsigned int num_lines, int stride,
                uint32_t *io_ptr, unsigned int num_io_words)
{
    Encoder *encoder = (Encoder *)quic;
    uint32_t *io_ptr_end = io_ptr + num_io_words;
    uint8_t *lines_end;
    int row;
    uint8_t *prev;
    int channels;
    int bpc;
#ifndef QUIC_RGB
    int i;
#endif

    lines_end = line + num_lines * stride;
    if (line == NULL && lines_end != line) {
        spice_warn_if_reached();
        return QUIC_ERROR;
    }

    quic_image_params(encoder, type, &channels, &bpc);

    if (!encoder_reste(encoder, io_ptr, io_ptr_end) ||
        !encoder_reste_channels(encoder, channels, width, bpc)) {
        return QUIC_ERROR;
    }

    encoder->io_word = 0;
    encoder->io_available_bits = 32;

    encode_32(encoder, QUIC_MAGIC);
    encode_32(encoder, QUIC_VERSION);
    encode_32(encoder, type);
    encode_32(encoder, width);
    encode_32(encoder, height);

    FILL_LINES();

    switch (type) {
#ifdef QUIC_RGB
    case QUIC_IMAGE_TYPE_RGB32:
        spice_assert(ABS(stride) >= width * 4);
        QUIC_COMPRESS_RGB(32);
        break;
    case QUIC_IMAGE_TYPE_RGB24:
        spice_assert(ABS(stride) >= width * 3);
        QUIC_COMPRESS_RGB(24);
        break;
    case QUIC_IMAGE_TYPE_RGB16:
        spice_assert(ABS(stride) >= width * 2);
        QUIC_COMPRESS_RGB(16);
        break;
    case QUIC_IMAGE_TYPE_RGBA:
        spice_assert(ABS(stride) >= width * 4);

        encoder->channels[0].correlate_row[-1] = 0;
        encoder->channels[1].correlate_row[-1] = 0;
        encoder->channels[2].correlate_row[-1] = 0;
        quic_rgb32_compress_row0(encoder, (rgb32_pixel_t *)(line), width);

        encoder->channels[3].correlate_row[-1] = 0;
        quic_four_compress_row0(encoder, &encoder->channels[3], (four_bytes_t *)(line + 3), width);

        encoder->rows_completed++;

        for (row = 1; row < height; row++) {
            prev = line;
            NEXT_LINE();
            encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];
            encoder->channels[1].correlate_row[-1] = encoder->channels[1].correlate_row[0];
            encoder->channels[2].correlate_row[-1] = encoder->channels[2].correlate_row[0];
            quic_rgb32_compress_row(encoder, (rgb32_pixel_t *)prev, (rgb32_pixel_t *)line, width);

            encoder->channels[3].correlate_row[-1] = encoder->channels[3].correlate_row[0];
            quic_four_compress_row(encoder, &encoder->channels[3], (four_bytes_t *)(prev + 3),
                                   (four_bytes_t *)(line + 3), width);
            encoder->rows_completed++;
        }
        break;
#else
    case QUIC_IMAGE_TYPE_RGB24:
        spice_assert(ABS(stride) >= width * 3);
        for (i = 0; i < 3; i++) {
            encoder->channels[i].correlate_row[-1] = 0;
            quic_three_compress_row0(encoder, &encoder->channels[i], (three_bytes_t *)(line + i),
                                     width);
        }
        encoder->rows_completed++;
        for (row = 1; row < height; row++) {
            prev = line;
            NEXT_LINE();
            for (i = 0; i < 3; i++) {
                encoder->channels[i].correlate_row[-1] = encoder->channels[i].correlate_row[0];
                quic_three_compress_row(encoder, &encoder->channels[i], (three_bytes_t *)(prev + i),
                                        (three_bytes_t *)(line + i), width);
            }
            encoder->rows_completed++;
        }
        break;
    case QUIC_IMAGE_TYPE_RGB32:
    case QUIC_IMAGE_TYPE_RGBA:
        spice_assert(ABS(stride) >= width * 4);
        for (i = 0; i < channels; i++) {
            encoder->channels[i].correlate_row[-1] = 0;
            quic_four_compress_row0(encoder, &encoder->channels[i], (four_bytes_t *)(line + i),
                                    width);
        }
        encoder->rows_completed++;
        for (row = 1; row < height; row++) {
            prev = line;
            NEXT_LINE();
            for (i = 0; i < channels; i++) {
                encoder->channels[i].correlate_row[-1] = encoder->channels[i].correlate_row[0];
                quic_four_compress_row(encoder, &encoder->channels[i], (four_bytes_t *)(prev + i),
                                       (four_bytes_t *)(line + i), width);
            }
            encoder->rows_completed++;
        }
        break;
#endif
    case QUIC_IMAGE_TYPE_GRAY:
        spice_assert(ABS(stride) >= width);
        encoder->channels[0].correlate_row[-1] = 0;
        quic_one_compress_row0(encoder, &encoder->channels[0], (one_byte_t *)line, width);
        encoder->rows_completed++;
        for (row = 1; row < height; row++) {
            prev = line;
            NEXT_LINE();
            encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];
            quic_one_compress_row(encoder, &encoder->channels[0], (one_byte_t *)prev,
                                  (one_byte_t *)line, width);
            encoder->rows_completed++;
        }
        break;
    case QUIC_IMAGE_TYPE_INVALID:
    default:
        encoder->usr->error(encoder->usr, "bad image type\n");
    }

    flush(encoder);
    encoder->io_words_count -= (encoder->io_end - encoder->io_now);

    return encoder->io_words_count;
}

int quic_decode_begin(QuicContext *quic, uint32_t *io_ptr, unsigned int num_io_words,
                      QuicImageType *out_type, int *out_width, int *out_height)
{
    Encoder *encoder = (Encoder *)quic;
    uint32_t *io_ptr_end = io_ptr + num_io_words;
    QuicImageType type;
    int width;
    int height;
    uint32_t magic;
    uint32_t version;
    int channels;
    int bpc;

    if (!encoder_reste(encoder, io_ptr, io_ptr_end)) {
        return QUIC_ERROR;
    }

    init_decode_io(encoder);

    magic = encoder->io_word;
    decode_eat32bits(encoder);
    if (magic != QUIC_MAGIC) {
        encoder->usr->warn(encoder->usr, "bad magic\n");
        return QUIC_ERROR;
    }

    version = encoder->io_word;
    decode_eat32bits(encoder);
    if (version != QUIC_VERSION) {
        encoder->usr->warn(encoder->usr, "bad version\n");
        return QUIC_ERROR;
    }

    type = (QuicImageType)encoder->io_word;
    decode_eat32bits(encoder);

    width = encoder->io_word;
    decode_eat32bits(encoder);

    height = encoder->io_word;
    decode_eat32bits(encoder);

    quic_image_params(encoder, type, &channels, &bpc);

    if (!encoder_reste_channels(encoder, channels, width, bpc)) {
        return QUIC_ERROR;
    }

    *out_width = encoder->width = width;
    *out_height = encoder->height = height;
    *out_type = encoder->type = type;
    return QUIC_OK;
}

#ifndef QUIC_RGB
static void clear_row(four_bytes_t *row, int width)
{
    four_bytes_t *end;
    for (end = row + width; row < end; row++) {
        row->a = 0;
    }
}

#endif

#ifdef QUIC_RGB

static void uncompress_rgba(Encoder *encoder, uint8_t *buf, int stride)
{
    unsigned int row;
    uint8_t *prev;

    encoder->channels[0].correlate_row[-1] = 0;
    encoder->channels[1].correlate_row[-1] = 0;
    encoder->channels[2].correlate_row[-1] = 0;
    quic_rgb32_uncompress_row0(encoder, (rgb32_pixel_t *)buf, encoder->width);

    encoder->channels[3].correlate_row[-1] = 0;
    quic_four_uncompress_row0(encoder, &encoder->channels[3], (four_bytes_t *)(buf + 3),
                              encoder->width);

    encoder->rows_completed++;
    for (row = 1; row < encoder->height; row++) {
        prev = buf;
        buf += stride;

        encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];
        encoder->channels[1].correlate_row[-1] = encoder->channels[1].correlate_row[0];
        encoder->channels[2].correlate_row[-1] = encoder->channels[2].correlate_row[0];
        quic_rgb32_uncompress_row(encoder, (rgb32_pixel_t *)prev, (rgb32_pixel_t *)buf,
                                  encoder->width);

        encoder->channels[3].correlate_row[-1] = encoder->channels[3].correlate_row[0];
        quic_four_uncompress_row(encoder, &encoder->channels[3], (four_bytes_t *)(prev + 3),
                                 (four_bytes_t *)(buf + 3), encoder->width);

        encoder->rows_completed++;
    }
}

#endif

static void uncompress_gray(Encoder *encoder, uint8_t *buf, int stride)
{
    unsigned int row;
    uint8_t *prev;

    encoder->channels[0].correlate_row[-1] = 0;
    quic_one_uncompress_row0(encoder, &encoder->channels[0], (one_byte_t *)buf, encoder->width);
    encoder->rows_completed++;
    for (row = 1; row < encoder->height; row++) {
        prev = buf;
        buf += stride;
        encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];
        quic_one_uncompress_row(encoder, &encoder->channels[0], (one_byte_t *)prev,
                                (one_byte_t *)buf, encoder->width);
        encoder->rows_completed++;
    }
}

#define QUIC_UNCOMPRESS_RGB(prefix, type)                                                       \
        encoder->channels[0].correlate_row[-1] = 0;                                             \
        encoder->channels[1].correlate_row[-1] = 0;                                             \
        encoder->channels[2].correlate_row[-1] = 0;                                             \
        quic_rgb##prefix##_uncompress_row0(encoder, (type *)buf, encoder->width);  \
        encoder->rows_completed++;                                                              \
        for (row = 1; row < encoder->height; row++) {                                           \
            prev = buf;                                                                         \
            buf += stride;                                                                      \
            encoder->channels[0].correlate_row[-1] = encoder->channels[0].correlate_row[0];     \
            encoder->channels[1].correlate_row[-1] = encoder->channels[1].correlate_row[0];     \
            encoder->channels[2].correlate_row[-1] = encoder->channels[2].correlate_row[0];     \
            quic_rgb##prefix##_uncompress_row(encoder, (type *)prev, (type *)buf,               \
                                              encoder->width);                                  \
            encoder->rows_completed++;                                                          \
        }

int quic_decode(QuicContext *quic, QuicImageType type, uint8_t *buf, int stride)
{
    Encoder *encoder = (Encoder *)quic;
    unsigned int row;
    uint8_t *prev;
#ifndef QUIC_RGB
    int i;
#endif

    spice_assert(buf);

    switch (encoder->type) {
#ifdef QUIC_RGB
    case QUIC_IMAGE_TYPE_RGB32:
    case QUIC_IMAGE_TYPE_RGB24:
        if (type == QUIC_IMAGE_TYPE_RGB32) {
            spice_assert(ABS(stride) >= (int)encoder->width * 4);
            QUIC_UNCOMPRESS_RGB(32, rgb32_pixel_t);
            break;
        } else if (type == QUIC_IMAGE_TYPE_RGB24) {
            spice_assert(ABS(stride) >= (int)encoder->width * 3);
            QUIC_UNCOMPRESS_RGB(24, rgb24_pixel_t);
            break;
        }
        encoder->usr->warn(encoder->usr, "unsupported output format\n");
        return QUIC_ERROR;
    case QUIC_IMAGE_TYPE_RGB16:
        if (type == QUIC_IMAGE_TYPE_RGB16) {
            spice_assert(ABS(stride) >= (int)encoder->width * 2);
            QUIC_UNCOMPRESS_RGB(16, rgb16_pixel_t);
        } else if (type == QUIC_IMAGE_TYPE_RGB32) {
            spice_assert(ABS(stride) >= (int)encoder->width * 4);
            QUIC_UNCOMPRESS_RGB(16_to_32, rgb32_pixel_t);
        } else {
            encoder->usr->warn(encoder->usr, "unsupported output format\n");
            return QUIC_ERROR;
        }

        break;
    case QUIC_IMAGE_TYPE_RGBA:

        if (type != QUIC_IMAGE_TYPE_RGBA) {
            encoder->usr->warn(encoder->usr, "unsupported output format\n");
            return QUIC_ERROR;
        }
        spice_assert(ABS(stride) >= (int)encoder->width * 4);
        uncompress_rgba(encoder, buf, stride);
        break;
#else
    case QUIC_IMAGE_TYPE_RGB24:
        spice_assert(ABS(stride) >= (int)encoder->width * 3);
        for (i = 0; i < 3; i++) {
            encoder->channels[i].correlate_row[-1] = 0;
            quic_three_uncompress_row0(encoder, &encoder->channels[i], (three_bytes_t *)(buf + i),
                                       encoder->width);
        }
        encoder->rows_completed++;
        for (row = 1; row < encoder->height; row++) {
            prev = buf;
            buf += stride;
            for (i = 0; i < 3; i++) {
                encoder->channels[i].correlate_row[-1] = encoder->channels[i].correlate_row[0];
                quic_three_uncompress_row(encoder, &encoder->channels[i],
                                          (three_bytes_t *)(prev + i),
                                          (three_bytes_t *)(buf + i),
                                          encoder->width);
            }
            encoder->rows_completed++;
        }
        break;
    case QUIC_IMAGE_TYPE_RGB32:
        spice_assert(ABS(stride) >= encoder->width * 4);
        for (i = 0; i < 3; i++) {
            encoder->channels[i].correlate_row[-1] = 0;
            quic_four_uncompress_row0(encoder, &encoder->channels[i], (four_bytes_t *)(buf + i),
                                      encoder->width);
        }
        clear_row((four_bytes_t *)(buf + 3), encoder->width);
        encoder->rows_completed++;
        for (row = 1; row < encoder->height; row++) {
            prev = buf;
            buf += stride;
            for (i = 0; i < 3; i++) {
                encoder->channels[i].correlate_row[-1] = encoder->channels[i].correlate_row[0];
                quic_four_uncompress_row(encoder, &encoder->channels[i],
                                         (four_bytes_t *)(prev + i),
                                         (four_bytes_t *)(buf + i),
                                         encoder->width);
            }
            clear_row((four_bytes_t *)(buf + 3), encoder->width);
            encoder->rows_completed++;
        }
        break;
    case QUIC_IMAGE_TYPE_RGBA:
        spice_assert(ABS(stride) >= encoder->width * 4);
        for (i = 0; i < 4; i++) {
            encoder->channels[i].correlate_row[-1] = 0;
            quic_four_uncompress_row0(encoder, &encoder->channels[i], (four_bytes_t *)(buf + i),
                                      encoder->width);
        }
        encoder->rows_completed++;
        for (row = 1; row < encoder->height; row++) {
            prev = buf;
            buf += stride;
            for (i = 0; i < 4; i++) {
                encoder->channels[i].correlate_row[-1] = encoder->channels[i].correlate_row[0];
                quic_four_uncompress_row(encoder, &encoder->channels[i],
                                         (four_bytes_t *)(prev + i),
                                         (four_bytes_t *)(buf + i),
                                         encoder->width);
            }
            encoder->rows_completed++;
        }
        break;
#endif
    case QUIC_IMAGE_TYPE_GRAY:

        if (type != QUIC_IMAGE_TYPE_GRAY) {
            encoder->usr->warn(encoder->usr, "unsupported output format\n");
            return QUIC_ERROR;
        }
        spice_assert(ABS(stride) >= (int)encoder->width);
        uncompress_gray(encoder, buf, stride);
        break;
    case QUIC_IMAGE_TYPE_INVALID:
    default:
        encoder->usr->error(encoder->usr, "bad image type\n");
    }
    return QUIC_OK;
}

static int need_init = TRUE;

QuicContext *quic_create(QuicUsrContext *usr)
{
    Encoder *encoder;

    if (!usr || need_init || !usr->error || !usr->warn || !usr->info || !usr->malloc ||
        !usr->free || !usr->more_space || !usr->more_lines) {
        return NULL;
    }

    if (!(encoder = (Encoder *)usr->malloc(usr, sizeof(Encoder)))) {
        return NULL;
    }

    if (!init_encoder(encoder, usr)) {
        usr->free(usr, encoder);
        return NULL;
    }
    return (QuicContext *)encoder;
}

void quic_destroy(QuicContext *quic)
{
    Encoder *encoder = (Encoder *)quic;
    int i;

    if (!quic) {
        return;
    }

    for (i = 0; i < MAX_CHANNELS; i++) {
        destroy_channel(&encoder->channels[i]);
    }
    encoder->usr->free(encoder->usr, encoder);
}

void quic_init(void)
{
    if (!need_init) {
        return;
    }
    need_init = FALSE;

    family_init(&family_8bpc, 8, DEFmaxclen);
    family_init(&family_5bpc, 5, DEFmaxclen);
#if defined(RLE) && defined(RLE_STAT)
    init_zeroLUT();
#endif
}
