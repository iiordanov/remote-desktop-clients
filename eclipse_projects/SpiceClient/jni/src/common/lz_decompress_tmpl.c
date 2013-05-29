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

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

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

// External defines: PLT, RGBX/PLTXX/ALPHA, TO_RGB32.
// If PLT4/1 and TO_RGB32 are defined, we need CAST_PLT_DISTANCE (because then the number of
// pixels differ from the units used in the compression)

/*
    For each output pixel type the following macros are defined:
    OUT_PIXEL                      - the output pixel type
    COPY_PIXEL(p, out)              - assigns the pixel to the place pointed by out and increases
                                      out. Used in RLE. Need special handling because in alpha we
                                      copy only the pad byte.
    COPY_REF_PIXEL(ref, out)      - copies the pixel pointed by ref to the pixel pointed by out.
                                    Increases ref and out.
    COPY_COMP_PIXEL(encoder, out) - copies pixel from the compressed buffer to the decompressed
                                    buffer. Increases out.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(LZ_RGB_ALPHA)
#define COPY_PIXEL(p, out) (*out++ = p)
#define COPY_REF_PIXEL(ref, out) (*out++ = *ref++)
#endif


// decompressing plt to plt
#ifdef LZ_PLT
#ifndef TO_RGB32
#define OUT_PIXEL one_byte_pixel_t
#define FNAME(name) lz_plt_##name
#define COPY_COMP_PIXEL(encoder, out) {out->a = decode(encoder); out++;}
#else // TO_RGB32
#define OUT_PIXEL rgb32_pixel_t
#define COPY_PLT_ENTRY(ent, out) {    \
    (out)->b = ent;                   \
    (out)->g = (ent >> 8);            \
    (out)->r = (ent >> 16);           \
    (out)->pad = 0;                   \
}
#ifdef PLT8
#define FNAME(name) lz_plt8_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out) {                     \
    uint32_t rgb = encoder->palette->ents[decode(encoder)]; \
    COPY_PLT_ENTRY(rgb, out);                               \
    out++;}
#elif defined(PLT4_BE)
#define FNAME(name) lz_plt4_be_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out){                                                             \
    uint8_t byte = decode(encoder);                                                                \
    uint32_t rgb = encoder->palette->ents[((byte >> 4) & 0x0f) % (encoder->palette->num_ents)];    \
    COPY_PLT_ENTRY(rgb, out);                                                                      \
    out++;                                                                                         \
    rgb = encoder->palette->ents[(byte & 0x0f) % (encoder->palette->num_ents)];                    \
    COPY_PLT_ENTRY(rgb, out);                                                                      \
    out++;                                                                                         \
}
#define CAST_PLT_DISTANCE(dist) (dist*2)
#elif  defined(PLT4_LE)
#define FNAME(name) lz_plt4_le_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out){                                                      \
    uint8_t byte = decode(encoder);                                                         \
    uint32_t rgb = encoder->palette->ents[(byte & 0x0f) % (encoder->palette->num_ents)];    \
    COPY_PLT_ENTRY(rgb, out);                                                               \
    out++;                                                                                  \
    rgb = encoder->palette->ents[((byte >> 4) & 0x0f) % (encoder->palette->num_ents)];      \
    COPY_PLT_ENTRY(rgb, out);                                                               \
    out++;                                                                                  \
}
#define CAST_PLT_DISTANCE(dist) (dist*2)
#elif defined(PLT1_BE) // TODO store palette entries for direct access
#define FNAME(name) lz_plt1_be_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out){                                    \
    uint8_t byte = decode(encoder);                                       \
    int i;                                                                \
    uint32_t fore = encoder->palette->ents[1];                            \
    uint32_t back = encoder->palette->ents[0];                            \
    for (i = 7; i >= 0; i--)                                              \
    {                                                                     \
        if ((byte >> i) & 1) {                                            \
            COPY_PLT_ENTRY(fore, out);                                    \
        } else {                                                          \
            COPY_PLT_ENTRY(back, out);                                    \
        }                                                                 \
        out++;                                                            \
    }                                                                     \
}
#define CAST_PLT_DISTANCE(dist) (dist*8)
#elif defined(PLT1_LE)
#define FNAME(name) lz_plt1_le_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out){                                    \
    uint8_t byte = decode(encoder);                                       \
    int i;                                                                \
    uint32_t fore = encoder->palette->ents[1];                            \
    uint32_t back = encoder->palette->ents[0];                            \
    for (i = 0; i < 8; i++)                                               \
    {                                                                     \
        if ((byte >> i) & 1) {                                            \
            COPY_PLT_ENTRY(fore, out);                                    \
        } else {                                                          \
            COPY_PLT_ENTRY(back, out);                                    \
        }                                                                 \
        out++;                                                            \
    }                                                                     \
}
#define CAST_PLT_DISTANCE(dist) (dist*8)
#endif // PLT Type
#endif // TO_RGB32
#endif

#ifdef LZ_A8
#ifndef TO_RGB32
#define OUT_PIXEL one_byte_pixel_t
#define FNAME(name) lz_a8_##name
#define COPY_COMP_PIXEL(encoder, out) {out->a = decode(encoder); out++;}
#else // TO_RGB32
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) lz_a8_to_rgb32_##name
#define COPY_COMP_PIXEL(encoder, out) {                     \
        (out)->b = (out)->g = (out)->r = 0;                 \
        (out)->pad = decode(encoder);                       \
        (out)++;                                            \
    }
#endif
#endif

#ifdef LZ_RGB16
#ifndef TO_RGB32
#define OUT_PIXEL rgb16_pixel_t
#define FNAME(name) lz_rgb16_##name
#define COPY_COMP_PIXEL(e, out) {*out = ((decode(e) << 8) | decode(e)); out++;}
#else
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) lz_rgb16_to_rgb32_##name
#define COPY_COMP_PIXEL(e, out) {                                      \
    out->r = decode(e);                                                \
    out->b = decode(e);                                                \
    out->g = (((out->r) << 6) | ((out->b) >> 2)) & ~0x07;              \
    out->g |= (out->g >> 5);                                           \
    out->r = ((out->r << 1) & ~0x07)| ((out->r >> 4) & 0x07);          \
    out->b =  (out->b << 3) | ((out->b >> 2) & 0x07);                  \
    out->pad = 0;                                                      \
    out++;                                                             \
}
#endif
#endif

#ifdef LZ_RGB24
#define OUT_PIXEL rgb24_pixel_t
#define FNAME(name) lz_rgb24_##name
#define COPY_COMP_PIXEL(e, out) {out->b = decode(e); out->g = decode(e); out->r = decode(e); out++;}
#endif

#ifdef LZ_RGB32
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) lz_rgb32_##name
#define COPY_COMP_PIXEL(e, out) {   \
    out->b = decode(e);             \
    out->g = decode(e);             \
    out->r = decode(e);             \
    out->pad = 0;                   \
    out++;                          \
}
#endif

#ifdef LZ_RGB_ALPHA
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) lz_rgb_alpha_##name
#define COPY_PIXEL(p, out) {out->pad = p.pad; out++;}
#define COPY_REF_PIXEL(ref, out) {out->pad = ref->pad; out++; ref++;}
#define COPY_COMP_PIXEL(e, out) {out->pad = decode(e); out++;}
#endif

// return num of bytes in out_buf
static size_t FNAME(decompress)(Encoder *encoder, OUT_PIXEL *out_buf, int size)
{
    OUT_PIXEL    *op = out_buf;
    OUT_PIXEL    *op_limit = out_buf + size;
    uint32_t ctrl = decode(encoder);
    int loop = TRUE;

    do {
        const OUT_PIXEL *ref = op;
        uint32_t len = ctrl >> 5;
        uint32_t ofs = (ctrl & 31) << 8; // 5 MSb of distance

        if (ctrl >= MAX_COPY) { // reference (dictionary/RLE)
            /* retrieving the reference and the match length */

            uint8_t code;
            len--;
            //ref -= ofs;
            if (len == 7 - 1) { // match length is bigger than 7
                do {
                    code = decode(encoder);
                    len += code;
                } while (code == 255); // remaining of len
            }
            code = decode(encoder);
            ofs += code;

            /* match from 16-bit distance */
            if (LZ_UNEXPECT_CONDITIONAL(code == 255)) {
                if (LZ_EXPECT_CONDITIONAL((ofs - code) == (31 << 8))) {
                    ofs = decode(encoder) << 8;
                    ofs += decode(encoder);
                    ofs += MAX_DISTANCE;
                }
            }

#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA) || defined(LZ_A8)
            len += 3; // length is biased by 2 + 1 (fixing bias)
#elif defined(LZ_RGB16)
            len += 2; // length is biased by 1 + 1 (fixing bias)
#else
            len += 1;
#endif
            ofs += 1; // offset is biased by 1       (fixing bias)

#if defined(TO_RGB32)
#if defined(PLT4_BE) || defined(PLT4_LE) || defined(PLT1_BE) || defined(PLT1_LE)
            ofs = CAST_PLT_DISTANCE(ofs);
            len = CAST_PLT_DISTANCE(len);
#endif
#endif
            ref -= ofs;

            spice_assert(op + len <= op_limit);
            spice_assert(ref + len <= op_limit);
            spice_assert(ref >= out_buf);

            // TODO: optimize by not calling loop at least 3 times when not PLT_TO_RGB32 (len is
            //       always >=3). in PLT_TO_RGB32 len >= 3*number_of_pixels_per_byte

            /* copying the match*/

            if (ref == (op - 1)) { // run // TODO: this will never be called in PLT4/1_TO_RGB
                                          //       because the number of pixel copied is larger
                                          //       then one...
                /* optimize copy for a run */
                OUT_PIXEL b = *ref;
                for (; len; --len) {
                    COPY_PIXEL(b, op);
                    spice_assert(op <= op_limit);
                }
            } else {
                for (; len; --len) {
                    COPY_REF_PIXEL(ref, op);
                    spice_assert(op <= op_limit);
                }
            }
        } else { // copy
            ctrl++; // copy count is biased by 1
#if defined(TO_RGB32) && (defined(PLT4_BE) || defined(PLT4_LE) || defined(PLT1_BE) || \
                                                                                   defined(PLT1_LE))
            spice_assert(op + CAST_PLT_DISTANCE(ctrl) <= op_limit);
#else
            spice_assert(op + ctrl <= op_limit);
#endif
            COPY_COMP_PIXEL(encoder, op);

            spice_assert(op <= op_limit);

            for (--ctrl; ctrl; ctrl--) {
                COPY_COMP_PIXEL(encoder, op);
                spice_assert(op <= op_limit);
            }
        }

        if (LZ_EXPECT_CONDITIONAL(op < op_limit)) {
            ctrl = decode(encoder);
        } else {
            loop = FALSE;
        }
    } while (LZ_EXPECT_CONDITIONAL(loop));

    return (op - out_buf);
}

#undef LZ_PLT
#undef PLT8
#undef PLT4_BE
#undef PLT4_LE
#undef PLT1_BE
#undef PLT1_LE
#undef LZ_RGB16
#undef LZ_RGB24
#undef LZ_RGB32
#undef LZ_A8
#undef LZ_RGB_ALPHA
#undef TO_RGB32
#undef OUT_PIXEL
#undef FNAME
#undef COPY_PIXEL
#undef COPY_REF_PIXEL
#undef COPY_COMP_PIXEL
#undef COPY_PLT_ENTRY
#undef CAST_PLT_DISTANCE
