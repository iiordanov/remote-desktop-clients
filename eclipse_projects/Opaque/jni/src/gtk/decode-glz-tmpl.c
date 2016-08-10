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

// External defines: PLT, RGBX/PLTXX/ALPHA, TO_RGB32.
// If PLT4/1 and TO_RGB32 are defined, we need CAST_PLT_DISTANCE (
// because then the number of pixels differ from the units used in the compression)

/*
    For each output pixel type the following macros are defined:
    OUT_PIXEL                      - the output pixel type
    COPY_PIXEL(p, out)              - assigns the pixel to the place pointed by out and
                                      increases out. Used in RLE.
                                      Need special handling because in alpha we copy only
                                      the pad byte.
    COPY_REF_PIXEL(ref, out)      - copies the pixel pointed by ref to the pixel pointed by out.
                                    Increases ref and out.
    COPY_COMP_PIXEL(encoder, out) - copies pixel from the compressed buffer to the decompressed
                                    buffer. Increases out.
*/

#if !defined(LZ_RGB_ALPHA)
#define COPY_PIXEL(p, out) (*(out++) = p)
#define COPY_REF_PIXEL(ref, out) (*(out++) = *(ref++))
#endif

// decompressing plt to plt
#ifdef LZ_PLT
#ifndef TO_RGB32
#define OUT_PIXEL one_byte_pixel_t
#define FNAME(name) glz_plt_##name
#define COPY_COMP_PIXEL(in, out) {(out)->a = *(in++); out++;}
#else // TO_RGB32
#define OUT_PIXEL rgb32_pixel_t
#define COPY_PLT_ENTRY(ent, out) {\
    (out)->b = ent; (out)->g = (ent >> 8); (out)->r = (ent >> 16); (out)->pad = 0;}
#ifdef PLT8
#define FNAME(name) glz_plt8_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out, palette) {         \
    uint32_t rgb = palette->ents[*(in++)];          \
    COPY_PLT_ENTRY(rgb, out);                       \
    out++;                                          \
}
#elif defined(PLT4_BE)
#define FNAME(name) glz_plt4_be_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out, palette){                                    \
    uint8_t byte = *(in++);                                                   \
    uint32_t rgb = palette->ents[((byte >> 4) & 0x0f) % (palette->num_ents)]; \
    COPY_PLT_ENTRY(rgb, out);                                                 \
    out++;                                                                    \
    rgb = palette->ents[(byte & 0x0f) % (palette->num_ents)];                 \
    COPY_PLT_ENTRY(rgb, out);                                                 \
    out++;                                                                    \
}
#define CAST_PLT_DISTANCE(dist) (dist*2)
#elif  defined(PLT4_LE)
#define FNAME(name) glz_plt4_le_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out, palette){                                \
    uint8_t byte = *(in++);                                               \
    uint32_t rgb = palette->ents[(byte & 0x0f) % (palette->num_ents)];    \
    COPY_PLT_ENTRY(rgb, out);                                             \
    out++;                                                                \
    rgb = palette->ents[((byte >> 4) & 0x0f) % (palette->num_ents)];      \
    COPY_PLT_ENTRY(rgb, out);                                             \
    out++;                                                                \
}
#define CAST_PLT_DISTANCE(dist) (dist*2)
#elif defined(PLT1_BE) // TODO store palette entries for direct access
#define FNAME(name) glz_plt1_be_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out, palette){                                \
    uint8_t byte = *(in++);                                               \
    int i;                                                                \
    uint32_t fore = palette->ents[1];                                     \
    uint32_t back = palette->ents[0];                                     \
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
#define FNAME(name) glz_plt1_le_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out, palette){                                \
    uint8_t byte = *(in++);                                               \
    int i;                                                                \
    uint32_t fore = palette->ents[1];                                     \
    uint32_t back = palette->ents[0];                                     \
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

#ifdef LZ_RGB16
#ifndef TO_RGB32
#define OUT_PIXEL rgb16_pixel_t
#define FNAME(name) glz_rgb16_##name
#define COPY_COMP_PIXEL(in, out) {*out = (*(in++)) << 8; *out |= *(in++); out++;}
#else
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) glz_rgb16_to_rgb32_##name
#define COPY_COMP_PIXEL(in, out) {out->r = *(in++); out->b= *(in++);       \
    out->g = (((out->r) << 6) | ((out->b) >> 2)) & ~0x07;                  \
    out->g |= (out->g >> 5);                                               \
    out->r = ((out->r << 1) & ~0x07) | ((out->r >> 4) & 0x07) ;            \
    out->b = (out->b << 3) | ((out->b >> 2) & 0x07);                       \
    out->pad = 0;                                                          \
    out++;                                                                 \
}
#endif
#endif

#ifdef LZ_RGB24
#define OUT_PIXEL rgb24_pixel_t
#define FNAME(name) glz_rgb24_##name
#define COPY_COMP_PIXEL(in, out) {  \
    out->b = *(in++);               \
    out->g = *(in++);               \
    out->r = *(in++);               \
    out++;                          \
}
#endif

#ifdef LZ_RGB32
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) glz_rgb32_##name
#define COPY_COMP_PIXEL(in, out) {  \
    out->b = *(in++);               \
    out->g = *(in++);               \
    out->r = *(in++);               \
    out->pad = 0;                   \
    out++;                          \
}
#endif

#ifdef LZ_RGB_ALPHA
#define OUT_PIXEL rgb32_pixel_t
#define FNAME(name) glz_rgb_alpha_##name
#define COPY_PIXEL(p, out) {out->pad = p.pad; out++;}
#define COPY_REF_PIXEL(ref, out) {out->pad = ref->pad; out++; ref++;}
#define COPY_COMP_PIXEL(in, out) {out->pad = *(in++); out++;}
#endif

// TODO: separate into routines that decode to dist,len. and to a routine that
// actually copies the data.

/* returns num of bytes read from in buf.
   size should be in PIXEL */
static size_t FNAME(decode)(SpiceGlzDecoderWindow *window,
                            uint8_t* in_buf, uint8_t *out_buf, int size,
                            uint64_t image_id, SpicePalette *plt)
{
    uint8_t      *ip = in_buf;
    OUT_PIXEL    *out_pix_buf = (OUT_PIXEL *)out_buf;
    OUT_PIXEL    *op = out_pix_buf;
    OUT_PIXEL    *op_limit = out_pix_buf + size;

    uint32_t ctrl = *(ip++);
    int loop = true;

    do {
        if (ctrl >= MAX_COPY) { // reference (dictionary/RLE)
            OUT_PIXEL *ref = op;
            uint32_t len = ctrl >> 5;
            uint8_t pixel_flag = (ctrl >> 4) & 0x01;
            uint32_t pixel_ofs = (ctrl & 0x0f);
            uint8_t image_flag;
            uint32_t image_dist;

            /* retrieving the referenced images, the offset of the first pixel,
               and the match length */

            uint8_t code;
            //len--; // TODO: why do we do this?

            if (len == 7) { // match length is bigger than 7
                do {
                    code = *(ip++);
                    len += code;
                } while (code == 255); // remaining of len
            }
            code = *(ip++);
            pixel_ofs += (code << 4);

            code = *(ip++);
            image_flag = (code >> 6) & 0x03;
            if (!pixel_flag) { // short pixel offset
                int i;
                image_dist = code & 0x3f;
                for (i = 0; i < image_flag; i++) {
                    code = *(ip++);
                    image_dist += (code << (6 + (8 * i)));
                }
            } else {
                int i;
                pixel_flag = (code >> 5) & 0x01;
                pixel_ofs += (code & 0x1f) << 12;
                image_dist = 0;
                for (i = 0; i < image_flag; i++) {
                    code = *(ip++);
                    image_dist += (code << 8 * i);
                }


                if (pixel_flag) { // very long pixel offset
                    code = *(ip++);
                    pixel_ofs += code << 17;
                }
            }

#if defined(LZ_PLT) || defined(LZ_RGB_ALPHA)
            len += 2; // length is biased by 2 (fixing bias)
#elif defined(LZ_RGB16)
            len += 1; // length is biased by 1  (fixing bias)
#endif
            if (!image_dist) {
                pixel_ofs += 1; // offset is biased by 1 (fixing bias)
            }

#if defined(TO_RGB32)
#if defined(PLT4_BE) || defined(PLT4_LE) || defined(PLT1_BE) || defined(PLT1_LE)
            pixel_ofs = CAST_PLT_DISTANCE(pixel_ofs);
            len = CAST_PLT_DISTANCE(len);
#endif
#endif

            if (!image_dist) { // reference is inside the same image
                ref -= pixel_ofs;
                g_return_val_if_fail(ref + len <= op_limit, 0);
                g_return_val_if_fail(ref >= out_pix_buf, 0);
            } else {
                ref = glz_decoder_window_bits(window, image_id,
                                              image_dist, pixel_ofs);
            }

            g_return_val_if_fail(ref != NULL, 0);
            g_return_val_if_fail(op + len <= op_limit, 0);

            /* copying the match*/

            if (ref == (op - 1)) { // run (this will never be called in PLT4/1_TO_RGB because the
                                  // number of pixel copied is larger then one...
                /* optimize copy for a run */
                OUT_PIXEL b = *ref;
                for (; len; --len) {
                    COPY_PIXEL(b, op);
                    g_return_val_if_fail(op <= op_limit, 0);
                }
            } else {
                for (; len; --len) {
                    COPY_REF_PIXEL(ref, op);
                    g_return_val_if_fail(op <= op_limit, 0);
                }
            }
        } else { // copy
            ctrl++; // copy count is biased by 1
#if defined(TO_RGB32) && (defined(PLT4_BE) || defined(PLT4_LE) || defined(PLT1_BE) || \
                                                                                   defined(PLT1_LE))
            g_return_val_if_fail(op + CAST_PLT_DISTANCE(ctrl) <= op_limit, 0);
#else
            g_return_val_if_fail(op + ctrl <= op_limit, 0);
#endif

#if defined(TO_RGB32) && defined(LZ_PLT)
            g_return_val_if_fail(plt, 0);
            COPY_COMP_PIXEL(ip, op, plt);
#else
            COPY_COMP_PIXEL(ip, op);
#endif
            g_return_val_if_fail(op <= op_limit, 0);

            for (--ctrl; ctrl; ctrl--) {
#if defined(TO_RGB32) && defined(LZ_PLT)
                g_return_val_if_fail(plt, 0);
                COPY_COMP_PIXEL(ip, op, plt);
#else
                COPY_COMP_PIXEL(ip, op);
#endif
                g_return_val_if_fail(op <= op_limit, 0);
            }
        } // END REF/COPY

        if (LZ_EXPECT_CONDITIONAL(op < op_limit)) {
            ctrl = *(ip++);
        } else {
            loop = false;
        }
    } while (LZ_EXPECT_CONDITIONAL(loop));

    return (ip - in_buf);
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
#undef LZ_RGB_ALPHA
#undef TO_RGB32
#undef OUT_PIXEL
#undef FNAME
#undef COPY_PIXEL
#undef COPY_REF_PIXEL
#undef COPY_COMP_PIXEL
#undef COPY_PLT_ENTRY
#undef CAST_PLT_DISTANCE
