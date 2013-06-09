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

#include "rop3.h"
#include "spice_common.h"

typedef void (*rop3_with_pattern_handler_t)(pixman_image_t *d, pixman_image_t *s,
                                            SpicePoint *src_pos, pixman_image_t *p,
                                            SpicePoint *pat_pos);

typedef void (*rop3_with_color_handler_t)(pixman_image_t *d, pixman_image_t *s,
                                          SpicePoint *src_pos, uint32_t rgb);

typedef void (*rop3_test_handler_t)(void);

#define ROP3_NUM_OPS 256

static rop3_with_pattern_handler_t rop3_with_pattern_handlers_32[ROP3_NUM_OPS];
static rop3_with_pattern_handler_t rop3_with_pattern_handlers_16[ROP3_NUM_OPS];
static rop3_with_color_handler_t rop3_with_color_handlers_32[ROP3_NUM_OPS];
static rop3_with_color_handler_t rop3_with_color_handlers_16[ROP3_NUM_OPS];
static rop3_test_handler_t rop3_test_handlers_32[ROP3_NUM_OPS];
static rop3_test_handler_t rop3_test_handlers_16[ROP3_NUM_OPS];


static void default_rop3_with_pattern_handler(pixman_image_t *d, pixman_image_t *s,
                                              SpicePoint *src_pos, pixman_image_t *p,
                                              SpicePoint *pat_pos)
{
    spice_critical("not implemented");
}

static void default_rop3_withe_color_handler(pixman_image_t *d, pixman_image_t *s, SpicePoint *src_pos,
                                             uint32_t rgb)
{
    spice_critical("not implemented");
}

static void default_rop3_test_handler(void)
{
}

#define ROP3_HANDLERS_DEPTH(name, formula, index, depth)                            \
static void rop3_handle_p##depth##_##name(pixman_image_t *d, pixman_image_t *s,                 \
                                          SpicePoint *src_pos,                                  \
                                          pixman_image_t *p, SpicePoint *pat_pos)               \
{                                                                                               \
    int width = pixman_image_get_width(d);                                                      \
    int height = pixman_image_get_height(d);                                                    \
    uint8_t *dest_line = (uint8_t *)pixman_image_get_data(d);                                   \
    int dest_stride = pixman_image_get_stride(d);                                               \
    uint8_t *end_line = dest_line + height * dest_stride;                                       \
                                                                                                \
    int pat_width = pixman_image_get_width(p);                                                  \
    int pat_height = pixman_image_get_height(p);                                                \
    uint8_t *pat_base = (uint8_t *)pixman_image_get_data(p);                                    \
    int pat_stride = pixman_image_get_stride(p);                                                \
    int pat_v_offset = pat_pos->y;                                                              \
                                                                                                \
    int src_stride = pixman_image_get_stride(s);                                                \
    uint8_t *src_line;                                                                          \
    src_line = (uint8_t *)pixman_image_get_data(s) + src_pos->y * src_stride + (src_pos->x * depth / 8); \
                                                                                                \
    for (; dest_line < end_line; dest_line += dest_stride, src_line += src_stride) {            \
        uint##depth##_t *dest = (uint##depth##_t *)dest_line;                                   \
        uint##depth##_t *end = dest + width;                                                    \
        uint##depth##_t *src = (uint##depth##_t *)src_line;                                     \
                                                                                                \
        int pat_h_offset = pat_pos->x;                                                          \
                                                                                                \
        for (; dest < end; dest++, src++) {                                                     \
            uint##depth##_t *pat;                                                               \
            pat  = (uint##depth##_t *)                                                          \
                        (pat_base + pat_v_offset * pat_stride + (pat_h_offset * depth / 8));    \
            *dest = formula;                                                                    \
            pat_h_offset = (pat_h_offset + 1) % pat_width;                                      \
        }                                                                                       \
                                                                                                \
        pat_v_offset = (pat_v_offset + 1) % pat_height;                                         \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static void rop3_handle_c##depth##_##name(pixman_image_t *d, pixman_image_t *s,                 \
                                          SpicePoint *src_pos,                                  \
                                          uint32_t rgb)                                         \
{                                                                                               \
    int width = pixman_image_get_width(d);                                                      \
    int height = pixman_image_get_height(d);                                                    \
    uint8_t *dest_line = (uint8_t *)pixman_image_get_data(d);                                   \
    int dest_stride = pixman_image_get_stride(d);                                               \
    uint8_t *end_line = dest_line + height * dest_stride;                                       \
    uint##depth##_t _pat = rgb;                                                                \
    uint##depth##_t *pat = &_pat;                                                               \
                                                                                                \
    int src_stride = pixman_image_get_stride(s);                                                \
    uint8_t *src_line;                                                                          \
    src_line = (uint8_t *)                                                                      \
        pixman_image_get_data(s) + src_pos->y * src_stride + (src_pos->x * depth / 8);          \
                                                                                                \
    for (; dest_line < end_line; dest_line += dest_stride, src_line += src_stride) {            \
        uint##depth##_t *dest = (uint##depth##_t *)dest_line;                                   \
        uint##depth##_t *end = dest + width;                                                    \
        uint##depth##_t *src = (uint##depth##_t *)src_line;                                     \
        for (; dest < end; dest++, src++) {                                                     \
            *dest = formula;                                                                    \
        }                                                                                       \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static void rop3_test##depth##_##name(void)                                                     \
{                                                                                               \
    uint8_t d = 0xaa;                                                                           \
    uint8_t s = 0xcc;                                                                           \
    uint8_t p = 0xf0;                                                                           \
    uint8_t *pat = &p;                                                                          \
    uint8_t *src = &s;                                                                          \
    uint8_t *dest = &d;                                                                         \
                                                                                                \
    d = formula;                                                                                \
    if (d != index) {                                                                           \
        printf("%s: failed, result is 0x%x expect 0x%x\n", __FUNCTION__, d, index);             \
    }                                                                                           \
}

#define ROP3_HANDLERS(name, formula, index) \
    ROP3_HANDLERS_DEPTH(name, formula, index, 32)  \
    ROP3_HANDLERS_DEPTH(name, formula, index, 16)

ROP3_HANDLERS(DPSoon, ~(*pat | *src | *dest), 0x01);
ROP3_HANDLERS(DPSona, ~(*pat | *src) & *dest, 0x02);
ROP3_HANDLERS(SDPona, ~(*pat | *dest) & *src, 0x04);
ROP3_HANDLERS(PDSxnon, ~(~(*src ^ *dest) | *pat), 0x06);
ROP3_HANDLERS(PDSaon, ~((*src & *dest) | *pat), 0x07);
ROP3_HANDLERS(SDPnaa, ~*pat & *dest & *src, 0x08);
ROP3_HANDLERS(PDSxon, ~((*src ^ *dest) | *pat), 0x09);
ROP3_HANDLERS(PSDnaon, ~((~*dest & *src) | *pat), 0x0b);
ROP3_HANDLERS(PDSnaon, ~((~*src & *dest) | *pat), 0x0d);
ROP3_HANDLERS(PDSonon, ~(~(*src | *dest) | *pat), 0x0e);
ROP3_HANDLERS(PDSona, ~(*src | *dest) & *pat, 0x10);
ROP3_HANDLERS(SDPxnon, ~(~(*pat ^ *dest) | *src), 0x12);
ROP3_HANDLERS(SDPaon, ~((*pat & *dest) | *src), 0x13);
ROP3_HANDLERS(DPSxnon, ~(~(*pat ^ *src) | *dest), 0x14);
ROP3_HANDLERS(DPSaon, ~((*pat & *src) | *dest), 0x15);
ROP3_HANDLERS(PSDPSanaxx, (~(*pat & *src) & *dest) ^ *src ^ *pat, 0x16);
ROP3_HANDLERS(SSPxDSxaxn, ~(((*src ^ *dest) & (*src ^ *pat)) ^ *src), 0x17);
ROP3_HANDLERS(SPxPDxa, (*src ^ *pat) & (*pat ^ *dest), 0x18);
ROP3_HANDLERS(SDPSanaxn, ~((~(*pat & *src) & *dest) ^ *src), 0x19);
ROP3_HANDLERS(PDSPaox, ((*pat & *src) | *dest) ^ *pat, 0x1a);
ROP3_HANDLERS(SDPSxaxn, ~(((*pat ^ *src) & *dest) ^ *src), 0x1b);
ROP3_HANDLERS(PSDPaox, ((*pat & *dest) | *src) ^ *pat, 0x1c);
ROP3_HANDLERS(DSPDxaxn, ~(((*pat ^ *dest) & *src) ^ *dest), 0x1d);
ROP3_HANDLERS(PDSox, (*dest | *src) ^ *pat, 0x1e);
ROP3_HANDLERS(PDSoan, ~((*src | *dest) & *pat), 0x1f);
ROP3_HANDLERS(DPSnaa, ~*src & *pat & *dest, 0x20);
ROP3_HANDLERS(SDPxon, ~((*pat ^ *dest) | *src), 0x21);
ROP3_HANDLERS(SPDnaon, ~((~*dest & *pat) | *src), 0x23);
ROP3_HANDLERS(SPxDSxa, (*src ^ *pat) & (*dest ^ *src), 0x24);
ROP3_HANDLERS(PDSPanaxn, ~((~(*src & *pat) & *dest) ^ *pat), 0x25);
ROP3_HANDLERS(SDPSaox, ((*src & *pat) | *dest) ^ *src, 0x26);
ROP3_HANDLERS(SDPSxnox, (~(*src ^ *pat) | *dest) ^ *src, 0x27);
ROP3_HANDLERS(DPSxa, (*pat ^ *src) & *dest, 0x28);
ROP3_HANDLERS(PSDPSaoxxn, ~(((*src & *pat) | *dest) ^ *src ^ *pat), 0x29);
ROP3_HANDLERS(DPSana, ~(*src & *pat) & *dest, 0x2a);
ROP3_HANDLERS(SSPxPDxaxn, ~(((*pat ^ *dest) & (*src ^ *pat)) ^ *src), 0x2b);
ROP3_HANDLERS(SPDSoax, ((*src | *dest) & *pat) ^ *src, 0x2c);
ROP3_HANDLERS(PSDnox, (~*dest | *src) ^ *pat, 0x2d);
ROP3_HANDLERS(PSDPxox, ((*pat ^ *dest) | *src) ^ *pat, 0x2e);
ROP3_HANDLERS(PSDnoan, ~((~*dest | *src) & *pat), 0x2f);
ROP3_HANDLERS(SDPnaon, ~((~*pat & *dest) | *src), 0x31);
ROP3_HANDLERS(SDPSoox, (*src | *pat | *dest) ^ *src, 0x32);
ROP3_HANDLERS(SPDSaox, ((*src & *dest) | *pat) ^ *src, 0x34);
ROP3_HANDLERS(SPDSxnox, (~(*src ^ *dest) | *pat) ^ *src, 0x35);
ROP3_HANDLERS(SDPox, (*pat | *dest) ^ *src, 0x36);
ROP3_HANDLERS(SDPoan, ~((*pat | *dest) & *src), 0x37);
ROP3_HANDLERS(PSDPoax, ((*pat | *dest) & *src) ^ *pat, 0x38);
ROP3_HANDLERS(SPDnox, (~*dest | *pat) ^ *src, 0x39);
ROP3_HANDLERS(SPDSxox, ((*src ^ *dest) | *pat) ^ *src, 0x3a);
ROP3_HANDLERS(SPDnoan, ~((~*dest | *pat) & *src), 0x3b);
ROP3_HANDLERS(SPDSonox, (~(*src | *dest) | *pat) ^ *src, 0x3d);
ROP3_HANDLERS(SPDSnaox, ((~*src & *dest) | *pat) ^ *src, 0x3e);
ROP3_HANDLERS(PSDnaa, ~*dest & *src & *pat, 0x40);
ROP3_HANDLERS(DPSxon, ~((*src ^ *pat) | *dest), 0x41);
ROP3_HANDLERS(SDxPDxa, (*src ^ *dest) & (*pat ^ *dest), 0x42);
ROP3_HANDLERS(SPDSanaxn, ~((~(*src & *dest) & *pat) ^ *src), 0x43);
ROP3_HANDLERS(DPSnaon, ~((~*src & *pat) | *dest), 0x45);
ROP3_HANDLERS(DSPDaox, ((*dest & *pat) | *src) ^ *dest, 0x46);
ROP3_HANDLERS(PSDPxaxn, ~(((*pat ^ *dest) & *src) ^ *pat), 0x47);
ROP3_HANDLERS(SDPxa, (*pat ^ *dest) & *src, 0x48);
ROP3_HANDLERS(PDSPDaoxxn, ~(((*dest & *pat) | *src) ^ *dest ^ *pat), 0x49);
ROP3_HANDLERS(DPSDoax, ((*dest | *src) & *pat) ^ *dest, 0x4a);
ROP3_HANDLERS(PDSnox, (~*src | *dest) ^ *pat, 0x4b);
ROP3_HANDLERS(SDPana, ~(*pat & *dest) & *src, 0x4c);
ROP3_HANDLERS(SSPxDSxoxn, ~(((*src ^ *dest) | (*src ^ *pat)) ^ *src), 0x4d);
ROP3_HANDLERS(PDSPxox, ((*pat ^ *src) | *dest) ^ *pat, 0x4e);
ROP3_HANDLERS(PDSnoan, ~((~*src | *dest) & *pat), 0x4f);
ROP3_HANDLERS(DSPnaon, ~((~*pat & *src) | *dest), 0x51);
ROP3_HANDLERS(DPSDaox, ((*dest & *src) | *pat) ^ *dest, 0x52);
ROP3_HANDLERS(SPDSxaxn, ~(((*src ^ *dest) & *pat) ^ *src), 0x53);
ROP3_HANDLERS(DPSonon, ~(~(*src | *pat) | *dest), 0x54);
ROP3_HANDLERS(DPSox, (*src | *pat) ^ *dest, 0x56);
ROP3_HANDLERS(DPSoan, ~((*src | *pat) & *dest), 0x57);
ROP3_HANDLERS(PDSPoax, ((*pat | *src) & *dest) ^ *pat, 0x58);
ROP3_HANDLERS(DPSnox, (~*src | *pat) ^ *dest, 0x59);
ROP3_HANDLERS(DPSDonox, (~(*dest | *src) | *pat) ^ *dest, 0x5b);
ROP3_HANDLERS(DPSDxox, ((*dest ^ *src) | *pat) ^ *dest, 0x5c);
ROP3_HANDLERS(DPSnoan, ~((~*src | *pat) & *dest), 0x5d);
ROP3_HANDLERS(DPSDnaox, ((~*dest & *src) | *pat) ^ *dest, 0x5e);
ROP3_HANDLERS(PDSxa, (*src ^ *dest) & *pat, 0x60);
ROP3_HANDLERS(DSPDSaoxxn, ~(((*src & *dest) | *pat) ^ *src ^ *dest), 0x61);
ROP3_HANDLERS(DSPDoax, ((*dest | *pat) & *src) ^ *dest, 0x62);
ROP3_HANDLERS(SDPnox, (~*pat | *dest) ^ *src, 0x63);
ROP3_HANDLERS(SDPSoax, ((*src | *pat) & *dest) ^ *src, 0x64);
ROP3_HANDLERS(DSPnox, (~*pat | *src) ^ *dest, 0x65);
ROP3_HANDLERS(SDPSonox, (~(*src | *pat) | *dest) ^ *src, 0x67);
ROP3_HANDLERS(DSPDSonoxxn, ~((~(*src | *dest) | *pat) ^ *src ^ *dest), 0x68);
ROP3_HANDLERS(PDSxxn, ~(*src ^ *dest ^ *pat), 0x69);
ROP3_HANDLERS(DPSax, (*src & *pat) ^ *dest, 0x6a);
ROP3_HANDLERS(PSDPSoaxxn, ~(((*src | *pat) & *dest) ^ *src ^ *pat), 0x6b);
ROP3_HANDLERS(SDPax, (*pat & *dest) ^ *src, 0x6c);
ROP3_HANDLERS(PDSPDoaxxn, ~(((*dest | *pat) & *src) ^ *dest ^ *pat), 0x6d);
ROP3_HANDLERS(SDPSnoax, ((~*src | *pat) & *dest) ^ *src, 0x6e);
ROP3_HANDLERS(PDSxnan, ~(~(*src ^ *dest) & *pat), 0x6f);
ROP3_HANDLERS(PDSana, ~(*src & *dest) & *pat, 0x70);
ROP3_HANDLERS(SSDxPDxaxn, ~(((*dest ^ *pat) & (*src ^ *dest)) ^ *src), 0x71);
ROP3_HANDLERS(SDPSxox, ((*src ^ *pat) | *dest) ^ *src, 0x72);
ROP3_HANDLERS(SDPnoan, ~((~*pat | *dest) & *src), 0x73);
ROP3_HANDLERS(DSPDxox, ((*dest ^ *pat) | *src) ^ *dest, 0x74);
ROP3_HANDLERS(DSPnoan, ~((~*pat | *src) & *dest), 0x75);
ROP3_HANDLERS(SDPSnaox, ((~*src & *pat) | *dest) ^ *src, 0x76);
ROP3_HANDLERS(PDSax, (*src & *dest) ^ *pat, 0x78);
ROP3_HANDLERS(DSPDSoaxxn, ~(((*src | *dest) & *pat) ^ *src ^ *dest), 0x79);
ROP3_HANDLERS(DPSDnoax, ((~*dest | *src) & *pat) ^ *dest, 0x7a);
ROP3_HANDLERS(SDPxnan, ~(~(*pat ^ *dest) & *src), 0x7b);
ROP3_HANDLERS(SPDSnoax, ((~*src | *dest) & *pat) ^ *src, 0x7c);
ROP3_HANDLERS(DPSxnan, ~(~(*src ^ *pat) & *dest), 0x7d);
ROP3_HANDLERS(SPxDSxo, (*src ^ *dest) | (*pat ^ *src), 0x7e);
ROP3_HANDLERS(DPSaan, ~(*src & *pat & *dest), 0x7f);
ROP3_HANDLERS(DPSaa, *src & *pat & *dest, 0x80);
ROP3_HANDLERS(SPxDSxon, ~((*src ^ *dest) | (*pat ^ *src)), 0x81);
ROP3_HANDLERS(DPSxna, ~(*src ^ *pat) & *dest, 0x82);
ROP3_HANDLERS(SPDSnoaxn, ~(((~*src | *dest) & *pat) ^ *src), 0x83);
ROP3_HANDLERS(SDPxna, ~(*pat ^ *dest) & *src, 0x84);
ROP3_HANDLERS(PDSPnoaxn, ~(((~*pat | *src) & *dest) ^ *pat), 0x85);
ROP3_HANDLERS(DSPDSoaxx, ((*src | *dest) & *pat) ^ *src ^ *dest, 0x86);
ROP3_HANDLERS(PDSaxn, ~((*src & *dest) ^ *pat), 0x87);
ROP3_HANDLERS(SDPSnaoxn, ~(((~*src & *pat) | *dest) ^ *src), 0x89);
ROP3_HANDLERS(DSPnoa, (~*pat | *src) & *dest, 0x8a);
ROP3_HANDLERS(DSPDxoxn, ~(((*dest ^ *pat) | *src) ^ *dest), 0x8b);
ROP3_HANDLERS(SDPnoa, (~*pat | *dest) & *src, 0x8c);
ROP3_HANDLERS(SDPSxoxn, ~(((*src ^ *pat) | *dest) ^ *src), 0x8d);
ROP3_HANDLERS(SSDxPDxax, ((*dest ^ *pat) & (*dest ^ *src)) ^ *src, 0x8e);
ROP3_HANDLERS(PDSanan, ~(~(*src & *dest) & *pat), 0x8f);
ROP3_HANDLERS(PDSxna, ~(*src ^ *dest) & *pat, 0x90);
ROP3_HANDLERS(SDPSnoaxn, ~(((~*src | *pat) & *dest) ^ *src), 0x91);
ROP3_HANDLERS(DPSDPoaxx, ((*pat | *dest) & *src) ^ *pat ^ *dest, 0x92);
ROP3_HANDLERS(SPDaxn, ~((*dest & *pat) ^ *src), 0x93);
ROP3_HANDLERS(PSDPSoaxx, ((*src | *pat) & *dest) ^ *src ^ *pat, 0x94);
ROP3_HANDLERS(DPSaxn, ~((*src & *pat) ^ *dest), 0x95);
ROP3_HANDLERS(DPSxx, *src ^ *pat ^ *dest, 0x96);
ROP3_HANDLERS(PSDPSonoxx, (~(*src | *pat) | *dest) ^ *src ^ *pat, 0x97);
ROP3_HANDLERS(SDPSonoxn, ~((~(*src | *pat) | *dest) ^ *src), 0x98);
ROP3_HANDLERS(DPSnax, (~*src & *pat) ^ *dest, 0x9a);
ROP3_HANDLERS(SDPSoaxn, ~(((*src | *pat) & *dest) ^ *src), 0x9b);
ROP3_HANDLERS(SPDnax, (~*dest & *pat) ^ *src, 0x9c);
ROP3_HANDLERS(DSPDoaxn, ~(((*dest | *pat) & *src) ^ *dest), 0x9d);
ROP3_HANDLERS(DSPDSaoxx, ((*src & *dest) | *pat) ^ *src ^ *dest, 0x9e);
ROP3_HANDLERS(PDSxan, ~((*src ^ *dest) & *pat), 0x9f);
ROP3_HANDLERS(PDSPnaoxn, ~(((~*pat & *src) | *dest) ^ *pat), 0xa1);
ROP3_HANDLERS(DPSnoa, (~*src | *pat) & *dest, 0xa2);
ROP3_HANDLERS(DPSDxoxn, ~(((*dest ^ *src) | *pat) ^ *dest), 0xa3);
ROP3_HANDLERS(PDSPonoxn, ~((~(*pat | *src) | *dest) ^ *pat), 0xa4);
ROP3_HANDLERS(DSPnax, (~*pat & *src) ^ *dest, 0xa6);
ROP3_HANDLERS(PDSPoaxn, ~(((*pat | *src) & *dest) ^ *pat), 0xa7);
ROP3_HANDLERS(DPSoa, (*src | *pat) & *dest, 0xa8);
ROP3_HANDLERS(DPSoxn, ~((*src | *pat) ^ *dest), 0xa9);
ROP3_HANDLERS(DPSono, ~(*src | *pat) | *dest, 0xab);
ROP3_HANDLERS(SPDSxax, ((*src ^ *dest) & *pat) ^ *src, 0xac);
ROP3_HANDLERS(DPSDaoxn, ~(((*dest & *src) | *pat) ^ *dest), 0xad);
ROP3_HANDLERS(DSPnao, (~*pat & *src) | *dest, 0xae);
ROP3_HANDLERS(PDSnoa, (~*src | *dest) & *pat, 0xb0);
ROP3_HANDLERS(PDSPxoxn, ~(((*pat ^ *src) | *dest) ^ *pat), 0xb1);
ROP3_HANDLERS(SSPxDSxox, ((*src ^ *dest) | (*pat ^ *src)) ^ *src, 0xb2);
ROP3_HANDLERS(SDPanan, ~(~(*pat & *dest) & *src), 0xb3);
ROP3_HANDLERS(PSDnax, (~*dest & *src) ^ *pat, 0xb4);
ROP3_HANDLERS(DPSDoaxn, ~(((*dest | *src) & *pat) ^ *dest), 0xb5);
ROP3_HANDLERS(DPSDPaoxx, ((*pat & *dest) | *src) ^ *pat ^ *dest, 0xb6);
ROP3_HANDLERS(SDPxan, ~((*pat ^ *dest) & *src), 0xb7);
ROP3_HANDLERS(PSDPxax, ((*dest ^ *pat) & *src) ^ *pat, 0xb8);
ROP3_HANDLERS(DSPDaoxn, ~(((*dest & *pat) | *src) ^ *dest), 0xb9);
ROP3_HANDLERS(DPSnao, (~*src & *pat) | *dest, 0xba);
ROP3_HANDLERS(SPDSanax, (~(*src & *dest) & *pat) ^ *src, 0xbc);
ROP3_HANDLERS(SDxPDxan, ~((*dest ^ *pat) & (*dest ^ *src)), 0xbd);
ROP3_HANDLERS(DPSxo, (*src ^ *pat) | *dest, 0xbe);
ROP3_HANDLERS(DPSano, ~(*src & *pat) | *dest, 0xbf);
ROP3_HANDLERS(SPDSnaoxn, ~(((~*src & *dest) | *pat) ^ *src), 0xc1);
ROP3_HANDLERS(SPDSonoxn, ~((~(*src | *dest) | *pat) ^ *src), 0xc2);
ROP3_HANDLERS(SPDnoa, (~*dest | *pat) & *src, 0xc4);
ROP3_HANDLERS(SPDSxoxn, ~(((*src ^ *dest) | *pat) ^ *src), 0xc5);
ROP3_HANDLERS(SDPnax, (~*pat & *dest) ^ *src, 0xc6);
ROP3_HANDLERS(PSDPoaxn, ~(((*pat | *dest) & *src) ^ *pat), 0xc7);
ROP3_HANDLERS(SDPoa, (*pat | *dest) & *src, 0xc8);
ROP3_HANDLERS(SPDoxn, ~((*dest | *pat) ^ *src), 0xc9);
ROP3_HANDLERS(DPSDxax, ((*dest ^ *src) & *pat) ^ *dest, 0xca);
ROP3_HANDLERS(SPDSaoxn, ~(((*src & *dest) | *pat) ^ *src), 0xcb);
ROP3_HANDLERS(SDPono, ~(*pat | *dest) | *src, 0xcd);
ROP3_HANDLERS(SDPnao, (~*pat & *dest) | *src, 0xce);
ROP3_HANDLERS(PSDnoa, (~*dest | *src) & *pat, 0xd0);
ROP3_HANDLERS(PSDPxoxn, ~(((*pat ^ *dest) | *src) ^ *pat), 0xd1);
ROP3_HANDLERS(PDSnax, (~*src & *dest) ^ *pat, 0xd2);
ROP3_HANDLERS(SPDSoaxn, ~(((*src | *dest) & *pat) ^ *src), 0xd3);
ROP3_HANDLERS(SSPxPDxax, ((*dest ^ *pat) & (*pat ^ *src)) ^ *src, 0xd4);
ROP3_HANDLERS(DPSanan, ~(~(*src & *pat) & *dest), 0xd5);
ROP3_HANDLERS(PSDPSaoxx, ((*src & *pat) | *dest) ^ *src ^ *pat, 0xd6);
ROP3_HANDLERS(DPSxan, ~((*src ^ *pat) & *dest), 0xd7);
ROP3_HANDLERS(PDSPxax, ((*pat ^ *src) & *dest) ^ *pat, 0xd8);
ROP3_HANDLERS(SDPSaoxn, ~(((*src & *pat) | *dest) ^ *src), 0xd9);
ROP3_HANDLERS(DPSDanax, (~(*dest & *src) & *pat) ^ *dest, 0xda);
ROP3_HANDLERS(SPxDSxan, ~((*src ^ *dest) & (*pat ^ *src)), 0xdb);
ROP3_HANDLERS(SPDnao, (~*dest & *pat) | *src, 0xdc);
ROP3_HANDLERS(SDPxo, (*pat ^ *dest) | *src, 0xde);
ROP3_HANDLERS(SDPano, ~(*pat & *dest) | *src, 0xdf);
ROP3_HANDLERS(PDSoa, (*src | *dest) & *pat, 0xe0);
ROP3_HANDLERS(PDSoxn, ~((*src | *dest) ^ *pat), 0xe1);
ROP3_HANDLERS(DSPDxax, ((*dest ^ *pat) & *src) ^ *dest, 0xe2);
ROP3_HANDLERS(PSDPaoxn, ~(((*pat & *dest) | *src) ^ *pat), 0xe3);
ROP3_HANDLERS(SDPSxax, ((*src ^ *pat) & *dest) ^ *src, 0xe4);
ROP3_HANDLERS(PDSPaoxn, ~(((*pat & *src) | *dest) ^ *pat), 0xe5);
ROP3_HANDLERS(SDPSanax, (~(*src & *pat) & *dest) ^ *src, 0xe6);
ROP3_HANDLERS(SPxPDxan, ~((*dest ^ *pat) & (*pat ^ *src)), 0xe7);
ROP3_HANDLERS(SSPxDSxax, ((*src ^ *dest) & (*pat ^ *src)) ^ *src, 0xe8);
ROP3_HANDLERS(DSPDSanaxxn, ~((~(*src & *dest) & *pat) ^ *src ^ *dest), 0xe9);
ROP3_HANDLERS(DPSao, (*src & *pat) | *dest, 0xea);
ROP3_HANDLERS(DPSxno, ~(*src ^ *pat) | *dest, 0xeb);
ROP3_HANDLERS(SDPao, (*pat & *dest) | *src, 0xec);
ROP3_HANDLERS(SDPxno, ~(*pat ^ *dest) | *src, 0xed);
ROP3_HANDLERS(SDPnoo, ~*pat | *dest | *src, 0xef);
ROP3_HANDLERS(PDSono, ~(*src | *dest) | *pat, 0xf1);
ROP3_HANDLERS(PDSnao, (~*src & *dest) | *pat, 0xf2);
ROP3_HANDLERS(PSDnao, (~*dest & *src) | *pat, 0xf4);
ROP3_HANDLERS(PDSxo, (*src ^ *dest) | *pat, 0xf6);
ROP3_HANDLERS(PDSano, ~(*src & *dest) | *pat, 0xf7);
ROP3_HANDLERS(PDSao, (*src & *dest) | *pat, 0xf8);
ROP3_HANDLERS(PDSxno, ~(*src ^ *dest) | *pat, 0xf9);
ROP3_HANDLERS(DPSnoo, ~*src | *pat | *dest, 0xfb);
ROP3_HANDLERS(PSDnoo, ~*dest | *src | *pat, 0xfd);
ROP3_HANDLERS(DPSoo, *src | *pat | *dest, 0xfe);


#define ROP3_FILL_HANDLERS(op, index)                       \
    rop3_with_pattern_handlers_32[index] = rop3_handle_p32_##op; \
    rop3_with_pattern_handlers_16[index] = rop3_handle_p16_##op; \
    rop3_with_color_handlers_32[index] = rop3_handle_c32_##op;   \
    rop3_with_color_handlers_16[index] = rop3_handle_c16_##op;   \
    rop3_test_handlers_32[index] = rop3_test32_##op;             \
    rop3_test_handlers_16[index] = rop3_test16_##op;

void rop3_init(void)
{
    static int need_init = 1;
    int i;

    if (!need_init) {
        return;
    }
    need_init = 0;

    for (i = 0; i < ROP3_NUM_OPS; i++) {
        rop3_with_pattern_handlers_32[i] = default_rop3_with_pattern_handler;
        rop3_with_pattern_handlers_16[i] = default_rop3_with_pattern_handler;
        rop3_with_color_handlers_32[i] = default_rop3_withe_color_handler;
        rop3_with_color_handlers_16[i] = default_rop3_withe_color_handler;
        rop3_test_handlers_32[i] = default_rop3_test_handler;
        rop3_test_handlers_16[i] = default_rop3_test_handler;
    }

    ROP3_FILL_HANDLERS(DPSoon, 0x01);
    ROP3_FILL_HANDLERS(DPSona, 0x02);
    ROP3_FILL_HANDLERS(SDPona, 0x04);
    ROP3_FILL_HANDLERS(PDSxnon, 0x06);
    ROP3_FILL_HANDLERS(PDSaon, 0x07);
    ROP3_FILL_HANDLERS(SDPnaa, 0x08);
    ROP3_FILL_HANDLERS(PDSxon, 0x09);
    ROP3_FILL_HANDLERS(PSDnaon, 0x0b);
    ROP3_FILL_HANDLERS(PDSnaon, 0x0d);
    ROP3_FILL_HANDLERS(PDSonon, 0x0e);
    ROP3_FILL_HANDLERS(PDSona, 0x10);
    ROP3_FILL_HANDLERS(SDPxnon, 0x12);
    ROP3_FILL_HANDLERS(SDPaon, 0x13);
    ROP3_FILL_HANDLERS(DPSxnon, 0x14);
    ROP3_FILL_HANDLERS(DPSaon, 0x15);
    ROP3_FILL_HANDLERS(PSDPSanaxx, 0x16);
    ROP3_FILL_HANDLERS(SSPxDSxaxn, 0x17);
    ROP3_FILL_HANDLERS(SPxPDxa, 0x18);
    ROP3_FILL_HANDLERS(SDPSanaxn, 0x19);
    ROP3_FILL_HANDLERS(PDSPaox, 0x1a);
    ROP3_FILL_HANDLERS(SDPSxaxn, 0x1b);
    ROP3_FILL_HANDLERS(PSDPaox, 0x1c);
    ROP3_FILL_HANDLERS(DSPDxaxn, 0x1d);
    ROP3_FILL_HANDLERS(PDSox, 0x1e);
    ROP3_FILL_HANDLERS(PDSoan, 0x1f);
    ROP3_FILL_HANDLERS(DPSnaa, 0x20);
    ROP3_FILL_HANDLERS(SDPxon, 0x21);
    ROP3_FILL_HANDLERS(SPDnaon, 0x23);
    ROP3_FILL_HANDLERS(SPxDSxa, 0x24);
    ROP3_FILL_HANDLERS(PDSPanaxn, 0x25);
    ROP3_FILL_HANDLERS(SDPSaox, 0x26);
    ROP3_FILL_HANDLERS(SDPSxnox, 0x27);
    ROP3_FILL_HANDLERS(DPSxa, 0x28);
    ROP3_FILL_HANDLERS(PSDPSaoxxn, 0x29);
    ROP3_FILL_HANDLERS(DPSana, 0x2a);
    ROP3_FILL_HANDLERS(SSPxPDxaxn, 0x2b);
    ROP3_FILL_HANDLERS(SPDSoax, 0x2c);
    ROP3_FILL_HANDLERS(PSDnox, 0x2d);
    ROP3_FILL_HANDLERS(PSDPxox, 0x2e);
    ROP3_FILL_HANDLERS(PSDnoan, 0x2f);
    ROP3_FILL_HANDLERS(SDPnaon, 0x31);
    ROP3_FILL_HANDLERS(SDPSoox, 0x32);
    ROP3_FILL_HANDLERS(SPDSaox, 0x34);
    ROP3_FILL_HANDLERS(SPDSxnox, 0x35);
    ROP3_FILL_HANDLERS(SDPox, 0x36);
    ROP3_FILL_HANDLERS(SDPoan, 0x37);
    ROP3_FILL_HANDLERS(PSDPoax, 0x38);
    ROP3_FILL_HANDLERS(SPDnox, 0x39);
    ROP3_FILL_HANDLERS(SPDSxox, 0x3a);
    ROP3_FILL_HANDLERS(SPDnoan, 0x3b);
    ROP3_FILL_HANDLERS(SPDSonox, 0x3d);
    ROP3_FILL_HANDLERS(SPDSnaox, 0x3e);
    ROP3_FILL_HANDLERS(PSDnaa, 0x40);
    ROP3_FILL_HANDLERS(DPSxon, 0x41);
    ROP3_FILL_HANDLERS(SDxPDxa, 0x42);
    ROP3_FILL_HANDLERS(SPDSanaxn, 0x43);
    ROP3_FILL_HANDLERS(DPSnaon, 0x45);
    ROP3_FILL_HANDLERS(DSPDaox, 0x46);
    ROP3_FILL_HANDLERS(PSDPxaxn, 0x47);
    ROP3_FILL_HANDLERS(SDPxa, 0x48);
    ROP3_FILL_HANDLERS(PDSPDaoxxn, 0x49);
    ROP3_FILL_HANDLERS(DPSDoax, 0x4a);
    ROP3_FILL_HANDLERS(PDSnox, 0x4b);
    ROP3_FILL_HANDLERS(SDPana, 0x4c);
    ROP3_FILL_HANDLERS(SSPxDSxoxn, 0x4d);
    ROP3_FILL_HANDLERS(PDSPxox, 0x4e);
    ROP3_FILL_HANDLERS(PDSnoan, 0x4f);
    ROP3_FILL_HANDLERS(DSPnaon, 0x51);
    ROP3_FILL_HANDLERS(DPSDaox, 0x52);
    ROP3_FILL_HANDLERS(SPDSxaxn, 0x53);
    ROP3_FILL_HANDLERS(DPSonon, 0x54);
    ROP3_FILL_HANDLERS(DPSox, 0x56);
    ROP3_FILL_HANDLERS(DPSoan, 0x57);
    ROP3_FILL_HANDLERS(PDSPoax, 0x58);
    ROP3_FILL_HANDLERS(DPSnox, 0x59);
    ROP3_FILL_HANDLERS(DPSDonox, 0x5b);
    ROP3_FILL_HANDLERS(DPSDxox, 0x5c);
    ROP3_FILL_HANDLERS(DPSnoan, 0x5d);
    ROP3_FILL_HANDLERS(DPSDnaox, 0x5e);
    ROP3_FILL_HANDLERS(PDSxa, 0x60);
    ROP3_FILL_HANDLERS(DSPDSaoxxn, 0x61);
    ROP3_FILL_HANDLERS(DSPDoax, 0x62);
    ROP3_FILL_HANDLERS(SDPnox, 0x63);
    ROP3_FILL_HANDLERS(SDPSoax, 0x64);
    ROP3_FILL_HANDLERS(DSPnox, 0x65);
    ROP3_FILL_HANDLERS(SDPSonox, 0x67);
    ROP3_FILL_HANDLERS(DSPDSonoxxn, 0x68);
    ROP3_FILL_HANDLERS(PDSxxn, 0x69);
    ROP3_FILL_HANDLERS(DPSax, 0x6a);
    ROP3_FILL_HANDLERS(PSDPSoaxxn, 0x6b);
    ROP3_FILL_HANDLERS(SDPax, 0x6c);
    ROP3_FILL_HANDLERS(PDSPDoaxxn, 0x6d);
    ROP3_FILL_HANDLERS(SDPSnoax, 0x6e);
    ROP3_FILL_HANDLERS(PDSxnan, 0x6f);
    ROP3_FILL_HANDLERS(PDSana, 0x70);
    ROP3_FILL_HANDLERS(SSDxPDxaxn, 0x71);
    ROP3_FILL_HANDLERS(SDPSxox, 0x72);
    ROP3_FILL_HANDLERS(SDPnoan, 0x73);
    ROP3_FILL_HANDLERS(DSPDxox, 0x74);
    ROP3_FILL_HANDLERS(DSPnoan, 0x75);
    ROP3_FILL_HANDLERS(SDPSnaox, 0x76);
    ROP3_FILL_HANDLERS(PDSax, 0x78);
    ROP3_FILL_HANDLERS(DSPDSoaxxn, 0x79);
    ROP3_FILL_HANDLERS(DPSDnoax, 0x7a);
    ROP3_FILL_HANDLERS(SDPxnan, 0x7b);
    ROP3_FILL_HANDLERS(SPDSnoax, 0x7c);
    ROP3_FILL_HANDLERS(DPSxnan, 0x7d);
    ROP3_FILL_HANDLERS(SPxDSxo, 0x7e);
    ROP3_FILL_HANDLERS(DPSaan, 0x7f);
    ROP3_FILL_HANDLERS(DPSaa, 0x80);
    ROP3_FILL_HANDLERS(SPxDSxon, 0x81);
    ROP3_FILL_HANDLERS(DPSxna, 0x82);
    ROP3_FILL_HANDLERS(SPDSnoaxn, 0x83);
    ROP3_FILL_HANDLERS(SDPxna, 0x84);
    ROP3_FILL_HANDLERS(PDSPnoaxn, 0x85);
    ROP3_FILL_HANDLERS(DSPDSoaxx, 0x86);
    ROP3_FILL_HANDLERS(PDSaxn, 0x87);
    ROP3_FILL_HANDLERS(SDPSnaoxn, 0x89);
    ROP3_FILL_HANDLERS(DSPnoa, 0x8a);
    ROP3_FILL_HANDLERS(DSPDxoxn, 0x8b);
    ROP3_FILL_HANDLERS(SDPnoa, 0x8c);
    ROP3_FILL_HANDLERS(SDPSxoxn, 0x8d);
    ROP3_FILL_HANDLERS(SSDxPDxax, 0x8e);
    ROP3_FILL_HANDLERS(PDSanan, 0x8f);
    ROP3_FILL_HANDLERS(PDSxna, 0x90);
    ROP3_FILL_HANDLERS(SDPSnoaxn, 0x91);
    ROP3_FILL_HANDLERS(DPSDPoaxx, 0x92);
    ROP3_FILL_HANDLERS(SPDaxn, 0x93);
    ROP3_FILL_HANDLERS(PSDPSoaxx, 0x94);
    ROP3_FILL_HANDLERS(DPSaxn, 0x95);
    ROP3_FILL_HANDLERS(DPSxx, 0x96);
    ROP3_FILL_HANDLERS(PSDPSonoxx, 0x97);
    ROP3_FILL_HANDLERS(SDPSonoxn, 0x98);
    ROP3_FILL_HANDLERS(DPSnax, 0x9a);
    ROP3_FILL_HANDLERS(SDPSoaxn, 0x9b);
    ROP3_FILL_HANDLERS(SPDnax, 0x9c);
    ROP3_FILL_HANDLERS(DSPDoaxn, 0x9d);
    ROP3_FILL_HANDLERS(DSPDSaoxx, 0x9e);
    ROP3_FILL_HANDLERS(PDSxan, 0x9f);
    ROP3_FILL_HANDLERS(PDSPnaoxn, 0xa1);
    ROP3_FILL_HANDLERS(DPSnoa, 0xa2);
    ROP3_FILL_HANDLERS(DPSDxoxn, 0xa3);
    ROP3_FILL_HANDLERS(PDSPonoxn, 0xa4);
    ROP3_FILL_HANDLERS(DSPnax, 0xa6);
    ROP3_FILL_HANDLERS(PDSPoaxn, 0xa7);
    ROP3_FILL_HANDLERS(DPSoa, 0xa8);
    ROP3_FILL_HANDLERS(DPSoxn, 0xa9);
    ROP3_FILL_HANDLERS(DPSono, 0xab);
    ROP3_FILL_HANDLERS(SPDSxax, 0xac);
    ROP3_FILL_HANDLERS(DPSDaoxn, 0xad);
    ROP3_FILL_HANDLERS(DSPnao, 0xae);
    ROP3_FILL_HANDLERS(PDSnoa, 0xb0);
    ROP3_FILL_HANDLERS(PDSPxoxn, 0xb1);
    ROP3_FILL_HANDLERS(SSPxDSxox, 0xb2);
    ROP3_FILL_HANDLERS(SDPanan, 0xb3);
    ROP3_FILL_HANDLERS(PSDnax, 0xb4);
    ROP3_FILL_HANDLERS(DPSDoaxn, 0xb5);
    ROP3_FILL_HANDLERS(DPSDPaoxx, 0xb6);
    ROP3_FILL_HANDLERS(SDPxan, 0xb7);
    ROP3_FILL_HANDLERS(PSDPxax, 0xb8);
    ROP3_FILL_HANDLERS(DSPDaoxn, 0xb9);
    ROP3_FILL_HANDLERS(DPSnao, 0xba);
    ROP3_FILL_HANDLERS(SPDSanax, 0xbc);
    ROP3_FILL_HANDLERS(SDxPDxan, 0xbd);
    ROP3_FILL_HANDLERS(DPSxo, 0xbe);
    ROP3_FILL_HANDLERS(DPSano, 0xbf);
    ROP3_FILL_HANDLERS(SPDSnaoxn, 0xc1);
    ROP3_FILL_HANDLERS(SPDSonoxn, 0xc2);
    ROP3_FILL_HANDLERS(SPDnoa, 0xc4);
    ROP3_FILL_HANDLERS(SPDSxoxn, 0xc5);
    ROP3_FILL_HANDLERS(SDPnax, 0xc6);
    ROP3_FILL_HANDLERS(PSDPoaxn, 0xc7);
    ROP3_FILL_HANDLERS(SDPoa, 0xc8);
    ROP3_FILL_HANDLERS(SPDoxn, 0xc9);
    ROP3_FILL_HANDLERS(DPSDxax, 0xca);
    ROP3_FILL_HANDLERS(SPDSaoxn, 0xcb);
    ROP3_FILL_HANDLERS(SDPono, 0xcd);
    ROP3_FILL_HANDLERS(SDPnao, 0xce);
    ROP3_FILL_HANDLERS(PSDnoa, 0xd0);
    ROP3_FILL_HANDLERS(PSDPxoxn, 0xd1);
    ROP3_FILL_HANDLERS(PDSnax, 0xd2);
    ROP3_FILL_HANDLERS(SPDSoaxn, 0xd3);
    ROP3_FILL_HANDLERS(SSPxPDxax, 0xd4);
    ROP3_FILL_HANDLERS(DPSanan, 0xd5);
    ROP3_FILL_HANDLERS(PSDPSaoxx, 0xd6);
    ROP3_FILL_HANDLERS(DPSxan, 0xd7);
    ROP3_FILL_HANDLERS(PDSPxax, 0xd8);
    ROP3_FILL_HANDLERS(SDPSaoxn, 0xd9);
    ROP3_FILL_HANDLERS(DPSDanax, 0xda);
    ROP3_FILL_HANDLERS(SPxDSxan, 0xdb);
    ROP3_FILL_HANDLERS(SPDnao, 0xdc);
    ROP3_FILL_HANDLERS(SDPxo, 0xde);
    ROP3_FILL_HANDLERS(SDPano, 0xdf);
    ROP3_FILL_HANDLERS(PDSoa, 0xe0);
    ROP3_FILL_HANDLERS(PDSoxn, 0xe1);
    ROP3_FILL_HANDLERS(DSPDxax, 0xe2);
    ROP3_FILL_HANDLERS(PSDPaoxn, 0xe3);
    ROP3_FILL_HANDLERS(SDPSxax, 0xe4);
    ROP3_FILL_HANDLERS(PDSPaoxn, 0xe5);
    ROP3_FILL_HANDLERS(SDPSanax, 0xe6);
    ROP3_FILL_HANDLERS(SPxPDxan, 0xe7);
    ROP3_FILL_HANDLERS(SSPxDSxax, 0xe8);
    ROP3_FILL_HANDLERS(DSPDSanaxxn, 0xe9);
    ROP3_FILL_HANDLERS(DPSao, 0xea);
    ROP3_FILL_HANDLERS(DPSxno, 0xeb);
    ROP3_FILL_HANDLERS(SDPao, 0xec);
    ROP3_FILL_HANDLERS(SDPxno, 0xed);
    ROP3_FILL_HANDLERS(SDPnoo, 0xef);
    ROP3_FILL_HANDLERS(PDSono, 0xf1);
    ROP3_FILL_HANDLERS(PDSnao, 0xf2);
    ROP3_FILL_HANDLERS(PSDnao, 0xf4);
    ROP3_FILL_HANDLERS(PDSxo, 0xf6);
    ROP3_FILL_HANDLERS(PDSano, 0xf7);
    ROP3_FILL_HANDLERS(PDSao, 0xf8);
    ROP3_FILL_HANDLERS(PDSxno, 0xf9);
    ROP3_FILL_HANDLERS(DPSnoo, 0xfb);
    ROP3_FILL_HANDLERS(PSDnoo, 0xfd);
    ROP3_FILL_HANDLERS(DPSoo, 0xfe);

    for (i = 0; i < ROP3_NUM_OPS; i++) {
        rop3_test_handlers_32[i]();
        rop3_test_handlers_16[i]();
    }
}

void do_rop3_with_pattern(uint8_t rop3, pixman_image_t *d, pixman_image_t *s, SpicePoint *src_pos,
                          pixman_image_t *p, SpicePoint *pat_pos)
{
    int bpp;

    bpp = spice_pixman_image_get_bpp(d);
    spice_assert(bpp == spice_pixman_image_get_bpp(s));
    spice_assert(bpp == spice_pixman_image_get_bpp(p));

    if (bpp == 32) {
        rop3_with_pattern_handlers_32[rop3](d, s, src_pos, p, pat_pos);
    } else {
        rop3_with_pattern_handlers_16[rop3](d, s, src_pos, p, pat_pos);
    }
}

void do_rop3_with_color(uint8_t rop3, pixman_image_t *d, pixman_image_t *s, SpicePoint *src_pos,
                        uint32_t rgb)
{
    int bpp;

    bpp = spice_pixman_image_get_bpp(d);
    spice_assert(bpp == spice_pixman_image_get_bpp(s));

    if (bpp == 32) {
        rop3_with_color_handlers_32[rop3](d, s, src_pos, rgb);
    } else {
        rop3_with_color_handlers_16[rop3](d, s, src_pos, rgb);
    }
}
