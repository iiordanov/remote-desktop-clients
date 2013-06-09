/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
        dictionary compression for images based on fastlz (http://www.fastlz.org/)
        (Distributed under MIT license).
*/
#ifndef __LZ_H
#define __LZ_H

#include <spice/macros.h>

#include "lz_common.h"
#include "lz_config.h"
#include "draw.h"
#include "macros.h"

SPICE_BEGIN_DECLS

typedef void *LzContext;

typedef struct LzUsrContext LzUsrContext;
struct LzUsrContext {
    SPICE_ATTR_PRINTF(2, 3) void (*error)(LzUsrContext *usr, const char *fmt, ...);
    SPICE_ATTR_PRINTF(2, 3) void (*warn)(LzUsrContext *usr, const char *fmt, ...);
    SPICE_ATTR_PRINTF(2, 3) void (*info)(LzUsrContext *usr, const char *fmt, ...);
    void    *(*malloc)(LzUsrContext *usr, int size);
    void (*free)(LzUsrContext *usr, void *ptr);
    int (*more_space)(LzUsrContext *usr, uint8_t **io_ptr);     // get the next chunk of the
                                                                // compressed buffer. return
                                                                // number of bytes in the chunk.
    int (*more_lines)(LzUsrContext *usr, uint8_t **lines);      // get the next chunk of the
                                                                // original image. If the image
                                                                // is down to top, return it from
                                                                // the last line to the first one
                                                                // (stride should always be
                                                                // positive)
};

/*
        assumes width is in pixels and stride is in bytes
        return: the number of bytes in the compressed data

        TODO :	determine size limit for the first segment and each chunk. check validity
                        of the segment or go to literal copy.
        TODO :	currently support only rgb images in which width*bytes_per_pixel = stride OR
                        palette images in which stride equals the min number of bytes to
                        hold a line. stride is not necessary for now. just for sanity check.
                        stride should be > 0
*/
int lz_encode(LzContext *lz, LzImageType type, int width, int height, int top_down,
              uint8_t *lines, unsigned int num_lines, int stride,
              uint8_t *io_ptr, unsigned int num_io_bytes);

/*
        prepare encoder and read lz magic.
        out_n_pixels number of compressed pixels. May differ from Width*height in plt1/4.
        Use it for allocation the decompressed buffer.

*/
void lz_decode_begin(LzContext *lz, uint8_t *io_ptr, unsigned int num_io_bytes,
                     LzImageType *out_type, int *out_width, int *out_height,
                     int *out_n_pixels, int *out_top_down, const SpicePalette *palette);

/*
        to_type = the image output type.
        We assume the buffer is consecutive. i.e. width = stride

        Important: if the image is plt1/4 and to_type is rgb32, the image
        will decompressed including the last bits in each line. This means buffer should be
        larger than width*height if needed and you should use stride to fix it.
        Note: If the image is down to top, set the stride in the sw surface to negative.
        use alloc_lz_image_surface create the surface.
*/
void lz_decode(LzContext *lz, LzImageType to_type, uint8_t *buf);

LzContext *lz_create(LzUsrContext *usr);

void lz_destroy(LzContext *lz);

SPICE_END_DECLS

#endif  // __LZ_H
