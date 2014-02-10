/*
   Copyright (C) 2009 Red Hat, Inc.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in
         the documentation and/or other materials provided with the
         distribution.
       * Neither the name of the copyright holder nor the names of its
         contributors may be used to endorse or promote products derived
         from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef _H_QXL_DEV
#define _H_QXL_DEV

#include <spice/types.h>
#include <spice/barrier.h>
#include <spice/ipc_ring.h>
#include <spice/enums.h>

#include <spice/start-packed.h>

#define REDHAT_PCI_VENDOR_ID 0x1b36

/* 0x100-0x11f reserved for spice, 0x1ff used for unstable work */
#define QXL_DEVICE_ID_STABLE 0x0100

enum {
    QXL_REVISION_STABLE_V04=0x01,
    QXL_REVISION_STABLE_V06=0x02,
    QXL_REVISION_STABLE_V10=0x03,
    QXL_REVISION_STABLE_V12=0x04,
};

#define QXL_DEVICE_ID_DEVEL 0x01ff
#define QXL_REVISION_DEVEL 0x01

#define QXL_ROM_MAGIC (*(const uint32_t*)"QXRO")
#define QXL_RAM_MAGIC (*(const uint32_t*)"QXRA")

enum {
    QXL_RAM_RANGE_INDEX,
    QXL_VRAM_RANGE_INDEX,
    QXL_ROM_RANGE_INDEX,
    QXL_IO_RANGE_INDEX,

    QXL_PCI_RANGES
};

/* qxl-1 compat: append only */
enum {
    QXL_IO_NOTIFY_CMD,
    QXL_IO_NOTIFY_CURSOR,
    QXL_IO_UPDATE_AREA,
    QXL_IO_UPDATE_IRQ,
    QXL_IO_NOTIFY_OOM,
    QXL_IO_RESET,
    QXL_IO_SET_MODE,                  /* qxl-1 */
    QXL_IO_LOG,
    /* appended for qxl-2 */
    QXL_IO_MEMSLOT_ADD,
    QXL_IO_MEMSLOT_DEL,
    QXL_IO_DETACH_PRIMARY,
    QXL_IO_ATTACH_PRIMARY,
    QXL_IO_CREATE_PRIMARY,
    QXL_IO_DESTROY_PRIMARY,
    QXL_IO_DESTROY_SURFACE_WAIT,
    QXL_IO_DESTROY_ALL_SURFACES,
    /* appended for qxl-3 */
    QXL_IO_UPDATE_AREA_ASYNC,
    QXL_IO_MEMSLOT_ADD_ASYNC,
    QXL_IO_CREATE_PRIMARY_ASYNC,
    QXL_IO_DESTROY_PRIMARY_ASYNC,
    QXL_IO_DESTROY_SURFACE_ASYNC,
    QXL_IO_DESTROY_ALL_SURFACES_ASYNC,
    QXL_IO_FLUSH_SURFACES_ASYNC,
    QXL_IO_FLUSH_RELEASE,
    /* appended for qxl-4 */
    QXL_IO_MONITORS_CONFIG_ASYNC,

    QXL_IO_RANGE_SIZE
};

typedef uint64_t QXLPHYSICAL;
typedef int32_t QXLFIXED; //fixed 28.4

typedef struct SPICE_ATTR_PACKED QXLPointFix {
    QXLFIXED x;
    QXLFIXED y;
} QXLPointFix;

typedef struct SPICE_ATTR_PACKED QXLPoint {
    int32_t x;
    int32_t y;
} QXLPoint;

typedef struct SPICE_ATTR_PACKED QXLPoint16 {
    int16_t x;
    int16_t y;
} QXLPoint16;

typedef struct SPICE_ATTR_PACKED QXLRect {
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
} QXLRect;

typedef struct SPICE_ATTR_PACKED QXLURect {
    uint32_t top;
    uint32_t left;
    uint32_t bottom;
    uint32_t right;
} QXLURect;

/* qxl-1 compat: append only */
typedef struct SPICE_ATTR_PACKED QXLRom {
    uint32_t magic;
    uint32_t id;
    uint32_t update_id;
    uint32_t compression_level;
    uint32_t log_level;
    uint32_t mode;                    /* qxl-1 */
    uint32_t modes_offset;
    uint32_t num_pages;
    uint32_t pages_offset;            /* qxl-1 */
    uint32_t draw_area_offset;        /* qxl-1 */
    uint32_t surface0_area_size;      /* qxl-1 name: draw_area_size */
    uint32_t ram_header_offset;
    uint32_t mm_clock;
    /* appended for qxl-2 */
    uint32_t n_surfaces;
    uint64_t flags;
    uint8_t slots_start;
    uint8_t slots_end;
    uint8_t slot_gen_bits;
    uint8_t slot_id_bits;
    uint8_t slot_generation;
    /* appended for qxl-4 */
    uint8_t client_present;
    uint8_t client_capabilities[58];
    uint32_t client_monitors_config_crc;
    struct {
        uint16_t count;
        uint16_t padding;
        QXLURect heads[64];
    } client_monitors_config;
} QXLRom;

#define CLIENT_MONITORS_CONFIG_CRC32_POLY 0xedb88320

/* qxl-1 compat: fixed */
typedef struct SPICE_ATTR_PACKED QXLMode {
    uint32_t id;
    uint32_t x_res;
    uint32_t y_res;
    uint32_t bits;
    uint32_t stride;
    uint32_t x_mili;
    uint32_t y_mili;
    uint32_t orientation;
} QXLMode;

/* qxl-1 compat: fixed */
typedef struct SPICE_ATTR_PACKED QXLModes {
    uint32_t n_modes;
    QXLMode modes[0];
} QXLModes;

/* qxl-1 compat: append only */
typedef enum QXLCmdType {
    QXL_CMD_NOP,
    QXL_CMD_DRAW,
    QXL_CMD_UPDATE,
    QXL_CMD_CURSOR,
    QXL_CMD_MESSAGE,
    QXL_CMD_SURFACE,
} QXLCmdType;

/* qxl-1 compat: fixed */
typedef struct SPICE_ATTR_PACKED QXLCommand {
    QXLPHYSICAL data;
    uint32_t type;
    uint32_t padding;
} QXLCommand;

#define QXL_COMMAND_FLAG_COMPAT          (1<<0)
#define QXL_COMMAND_FLAG_COMPAT_16BPP    (2<<0)

typedef struct SPICE_ATTR_PACKED QXLCommandExt {
    QXLCommand cmd;
    uint32_t group_id;
    uint32_t flags;
} QXLCommandExt;

typedef struct SPICE_ATTR_PACKED QXLMemSlot {
    uint64_t mem_start;
    uint64_t mem_end;
} QXLMemSlot;

#define QXL_SURF_TYPE_PRIMARY      0

#define QXL_SURF_FLAG_KEEP_DATA    (1 << 0)

typedef struct SPICE_ATTR_PACKED QXLSurfaceCreate {
    uint32_t width;
    uint32_t height;
    int32_t stride;
    uint32_t format;
    uint32_t position;
    uint32_t mouse_mode;
    uint32_t flags;
    uint32_t type;
    QXLPHYSICAL mem;
} QXLSurfaceCreate;

#define QXL_COMMAND_RING_SIZE 32
#define QXL_CURSOR_RING_SIZE 32
#define QXL_RELEASE_RING_SIZE 8

SPICE_RING_DECLARE(QXLCommandRing, QXLCommand, QXL_COMMAND_RING_SIZE);
SPICE_RING_DECLARE(QXLCursorRing, QXLCommand, QXL_CURSOR_RING_SIZE);

SPICE_RING_DECLARE(QXLReleaseRing, uint64_t, QXL_RELEASE_RING_SIZE);

#define QXL_LOG_BUF_SIZE 4096

#define QXL_INTERRUPT_DISPLAY (1 << 0)
#define QXL_INTERRUPT_CURSOR (1 << 1)
#define QXL_INTERRUPT_IO_CMD (1 << 2)
#define QXL_INTERRUPT_ERROR  (1 << 3)
#define QXL_INTERRUPT_CLIENT (1 << 4)
#define QXL_INTERRUPT_CLIENT_MONITORS_CONFIG  (1 << 5)

/* qxl-1 compat: append only */
typedef struct SPICE_ATTR_PACKED QXLRam {
    uint32_t magic;
    uint32_t int_pending;
    uint32_t int_mask;
    uint8_t log_buf[QXL_LOG_BUF_SIZE];
    QXLCommandRing cmd_ring;
    QXLCursorRing cursor_ring;
    QXLReleaseRing release_ring;
    QXLRect update_area;
    /* appended for qxl-2 */
    uint32_t update_surface;
    QXLMemSlot mem_slot;
    QXLSurfaceCreate create_surface;
    uint64_t flags;

    /* appended for qxl-4 */

    /* used by QXL_IO_MONITORS_CONFIG_ASYNC */
    QXLPHYSICAL monitors_config;

} QXLRam;

typedef union QXLReleaseInfo {
    uint64_t id;      // in
    uint64_t next;    // out
} QXLReleaseInfo;

typedef struct QXLReleaseInfoExt {
    QXLReleaseInfo *info;
    uint32_t group_id;
} QXLReleaseInfoExt;

typedef struct  SPICE_ATTR_PACKED QXLDataChunk {
    uint32_t data_size;
    QXLPHYSICAL prev_chunk;
    QXLPHYSICAL next_chunk;
    uint8_t data[0];
} QXLDataChunk;

typedef struct SPICE_ATTR_PACKED QXLMessage {
    QXLReleaseInfo release_info;
    uint8_t data[0];
} QXLMessage;

typedef struct SPICE_ATTR_PACKED QXLCompatUpdateCmd {
    QXLReleaseInfo release_info;
    QXLRect area;
    uint32_t update_id;
} QXLCompatUpdateCmd;

typedef struct SPICE_ATTR_PACKED QXLUpdateCmd {
    QXLReleaseInfo release_info;
    QXLRect area;
    uint32_t update_id;
    uint32_t surface_id;
} QXLUpdateCmd;

typedef struct SPICE_ATTR_PACKED QXLCursorHeader {
    uint64_t unique;
    uint16_t type;
    uint16_t width;
    uint16_t height;
    uint16_t hot_spot_x;
    uint16_t hot_spot_y;
} QXLCursorHeader;

typedef struct SPICE_ATTR_PACKED QXLCursor {
    QXLCursorHeader header;
    uint32_t data_size;
    QXLDataChunk chunk;
} QXLCursor;

enum {
    QXL_CURSOR_SET,
    QXL_CURSOR_MOVE,
    QXL_CURSOR_HIDE,
    QXL_CURSOR_TRAIL,
};

#define QXL_CURSUR_DEVICE_DATA_SIZE 128

typedef struct SPICE_ATTR_PACKED QXLCursorCmd {
    QXLReleaseInfo release_info;
    uint8_t type;
    union {
        struct SPICE_ATTR_PACKED {
            QXLPoint16 position;
            uint8_t visible;
            QXLPHYSICAL shape;
        } set;
        struct SPICE_ATTR_PACKED {
            uint16_t length;
            uint16_t frequency;
        } trail;
        QXLPoint16 position;
    } u;
    uint8_t device_data[QXL_CURSUR_DEVICE_DATA_SIZE]; //todo: dynamic size from rom
} QXLCursorCmd;

enum {
    QXL_DRAW_NOP,
    QXL_DRAW_FILL,
    QXL_DRAW_OPAQUE,
    QXL_DRAW_COPY,
    QXL_COPY_BITS,
    QXL_DRAW_BLEND,
    QXL_DRAW_BLACKNESS,
    QXL_DRAW_WHITENESS,
    QXL_DRAW_INVERS,
    QXL_DRAW_ROP3,
    QXL_DRAW_STROKE,
    QXL_DRAW_TEXT,
    QXL_DRAW_TRANSPARENT,
    QXL_DRAW_ALPHA_BLEND,
    QXL_DRAW_COMPOSITE
};

typedef struct SPICE_ATTR_PACKED QXLRasterGlyph {
    QXLPoint render_pos;
    QXLPoint glyph_origin;
    uint16_t width;
    uint16_t height;
    uint8_t data[0];
} QXLRasterGlyph;

typedef struct SPICE_ATTR_PACKED QXLString {
    uint32_t data_size;
    uint16_t length;
    uint16_t flags;
    QXLDataChunk chunk;
} QXLString;

typedef struct SPICE_ATTR_PACKED QXLCopyBits {
    QXLPoint src_pos;
} QXLCopyBits;

typedef enum QXLEffectType
{
    QXL_EFFECT_BLEND = 0,
    QXL_EFFECT_OPAQUE = 1,
    QXL_EFFECT_REVERT_ON_DUP = 2,
    QXL_EFFECT_BLACKNESS_ON_DUP = 3,
    QXL_EFFECT_WHITENESS_ON_DUP = 4,
    QXL_EFFECT_NOP_ON_DUP = 5,
    QXL_EFFECT_NOP = 6,
    QXL_EFFECT_OPAQUE_BRUSH = 7
} QXLEffectType;

typedef struct SPICE_ATTR_PACKED QXLPattern {
    QXLPHYSICAL pat;
    QXLPoint pos;
} QXLPattern;

typedef struct SPICE_ATTR_PACKED QXLBrush {
    uint32_t type;
    union {
        uint32_t color;
        QXLPattern pattern;
    } u;
} QXLBrush;

typedef struct SPICE_ATTR_PACKED QXLQMask {
    uint8_t flags;
    QXLPoint pos;
    QXLPHYSICAL bitmap;
} QXLQMask;

typedef struct SPICE_ATTR_PACKED QXLFill {
    QXLBrush brush;
    uint16_t rop_descriptor;
    QXLQMask mask;
} QXLFill;

typedef struct SPICE_ATTR_PACKED QXLOpaque {
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
    QXLBrush brush;
    uint16_t rop_descriptor;
    uint8_t scale_mode;
    QXLQMask mask;
} QXLOpaque;

typedef struct SPICE_ATTR_PACKED QXLCopy {
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
    uint16_t rop_descriptor;
    uint8_t scale_mode;
    QXLQMask mask;
} QXLCopy, QXLBlend;

typedef struct SPICE_ATTR_PACKED QXLTransparent {
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
    uint32_t src_color;
    uint32_t true_color;
} QXLTransparent;

typedef struct SPICE_ATTR_PACKED QXLAlphaBlend {
    uint16_t alpha_flags;
    uint8_t alpha;
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
} QXLAlphaBlend;

typedef struct SPICE_ATTR_PACKED QXLCompatAlphaBlend {
    uint8_t alpha;
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
} QXLCompatAlphaBlend;

typedef struct SPICE_ATTR_PACKED QXLRop3 {
    QXLPHYSICAL src_bitmap;
    QXLRect src_area;
    QXLBrush brush;
    uint8_t rop3;
    uint8_t scale_mode;
    QXLQMask mask;
} QXLRop3;

typedef struct SPICE_ATTR_PACKED QXLLineAttr {
    uint8_t flags;
    uint8_t join_style;
    uint8_t end_style;
    uint8_t style_nseg;
    QXLFIXED width;
    QXLFIXED miter_limit;
    QXLPHYSICAL style;
} QXLLineAttr;

typedef struct SPICE_ATTR_PACKED QXLStroke {
    QXLPHYSICAL path;
    QXLLineAttr attr;
    QXLBrush brush;
    uint16_t fore_mode;
    uint16_t back_mode;
} QXLStroke;

typedef struct SPICE_ATTR_PACKED QXLText {
    QXLPHYSICAL str;
    QXLRect back_area;
    QXLBrush fore_brush;
    QXLBrush back_brush;
    uint16_t fore_mode;
    uint16_t back_mode;
} QXLText;

typedef struct SPICE_ATTR_PACKED QXLBlackness {
    QXLQMask mask;
} QXLBlackness, QXLInvers, QXLWhiteness;

typedef struct SPICE_ATTR_PACKED QXLClip {
    uint32_t type;
    QXLPHYSICAL data;
} QXLClip;

typedef enum {
    QXL_OP_CLEAR                     = 0x00,
    QXL_OP_SOURCE		     = 0x01,
    QXL_OP_DST                       = 0x02,
    QXL_OP_OVER                      = 0x03,
    QXL_OP_OVER_REVERSE              = 0x04,
    QXL_OP_IN                        = 0x05,
    QXL_OP_IN_REVERSE                = 0x06,
    QXL_OP_OUT                       = 0x07,
    QXL_OP_OUT_REVERSE               = 0x08,
    QXL_OP_ATOP                      = 0x09,
    QXL_OP_ATOP_REVERSE              = 0x0a,
    QXL_OP_XOR                       = 0x0b,
    QXL_OP_ADD                       = 0x0c,
    QXL_OP_SATURATE                  = 0x0d,
    /* Note the jump here from 0x0d to 0x30 */
    QXL_OP_MULTIPLY                  = 0x30,
    QXL_OP_SCREEN                    = 0x31,
    QXL_OP_OVERLAY                   = 0x32,
    QXL_OP_DARKEN                    = 0x33,
    QXL_OP_LIGHTEN                   = 0x34,
    QXL_OP_COLOR_DODGE               = 0x35,
    QXL_OP_COLOR_BURN                = 0x36,
    QXL_OP_HARD_LIGHT                = 0x37,
    QXL_OP_SOFT_LIGHT                = 0x38,
    QXL_OP_DIFFERENCE                = 0x39,
    QXL_OP_EXCLUSION                 = 0x3a,
    QXL_OP_HSL_HUE                   = 0x3b,
    QXL_OP_HSL_SATURATION            = 0x3c,
    QXL_OP_HSL_COLOR                 = 0x3d,
    QXL_OP_HSL_LUMINOSITY            = 0x3e
} QXLOperator;

typedef struct {
    uint32_t	t00;
    uint32_t	t01;
    uint32_t	t02;
    uint32_t	t10;
    uint32_t	t11;
    uint32_t	t12;
} QXLTransform;

/* The flags field has the following bit fields:
 *
 *     operator:		[  0 -  7 ]
 *     src_filter:		[  8 - 10 ]
 *     mask_filter:		[ 11 - 13 ]
 *     src_repeat:		[ 14 - 15 ]
 *     mask_repeat:		[ 16 - 17 ]
 *     component_alpha:		[ 18 - 18 ]
 *     reserved:		[ 19 - 31 ]
 *
 * The repeat and filter values are those of pixman:
 *		REPEAT_NONE =		0
 *              REPEAT_NORMAL =		1
 *		REPEAT_PAD =		2
 *		REPEAT_REFLECT =	3
 *
 * The filter values are:
 *		FILTER_NEAREST =	0
 *		FILTER_BILINEAR	=	1
 */
typedef struct SPICE_ATTR_PACKED QXLComposite {
    uint32_t		flags;

    QXLPHYSICAL		src;
    QXLPHYSICAL		src_transform;		/* May be NULL */
    QXLPHYSICAL		mask;			/* May be NULL */
    QXLPHYSICAL		mask_transform;		/* May be NULL */
    QXLPoint16		src_origin;
    QXLPoint16		mask_origin;
} QXLComposite;

typedef struct SPICE_ATTR_PACKED QXLCompatDrawable {
    QXLReleaseInfo release_info;
    uint8_t effect;
    uint8_t type;
    uint16_t bitmap_offset;
    QXLRect bitmap_area;
    QXLRect bbox;
    QXLClip clip;
    uint32_t mm_time;
    union {
        QXLFill fill;
        QXLOpaque opaque;
        QXLCopy copy;
        QXLTransparent transparent;
        QXLCompatAlphaBlend alpha_blend;
        QXLCopyBits copy_bits;
        QXLBlend blend;
        QXLRop3 rop3;
        QXLStroke stroke;
        QXLText text;
        QXLBlackness blackness;
        QXLInvers invers;
        QXLWhiteness whiteness;
    } u;
} QXLCompatDrawable;

typedef struct SPICE_ATTR_PACKED QXLDrawable {
    QXLReleaseInfo release_info;
    uint32_t surface_id;
    uint8_t effect;
    uint8_t type;
    uint8_t self_bitmap;
    QXLRect self_bitmap_area;
    QXLRect bbox;
    QXLClip clip;
    uint32_t mm_time;
    int32_t surfaces_dest[3];
    QXLRect surfaces_rects[3];
    union {
        QXLFill fill;
        QXLOpaque opaque;
        QXLCopy copy;
        QXLTransparent transparent;
        QXLAlphaBlend alpha_blend;
        QXLCopyBits copy_bits;
        QXLBlend blend;
        QXLRop3 rop3;
        QXLStroke stroke;
        QXLText text;
        QXLBlackness blackness;
        QXLInvers invers;
        QXLWhiteness whiteness;
	QXLComposite composite;
    } u;
} QXLDrawable;

typedef enum QXLSurfaceCmdType {
    QXL_SURFACE_CMD_CREATE,
    QXL_SURFACE_CMD_DESTROY,
} QXLSurfaceCmdType;

typedef struct SPICE_ATTR_PACKED QXLSurface {
    uint32_t format;
    uint32_t width;
    uint32_t height;
    int32_t stride;
    QXLPHYSICAL data;
} QXLSurface;

typedef struct SPICE_ATTR_PACKED QXLSurfaceCmd {
    QXLReleaseInfo release_info;
    uint32_t surface_id;
    uint8_t type;
    uint32_t flags;
    union {
        QXLSurface surface_create;
    } u;
} QXLSurfaceCmd;

typedef struct SPICE_ATTR_PACKED QXLClipRects {
    uint32_t num_rects;
    QXLDataChunk chunk;
} QXLClipRects;

enum {
    QXL_PATH_BEGIN = (1 << 0),
    QXL_PATH_END = (1 << 1),
    QXL_PATH_CLOSE = (1 << 3),
    QXL_PATH_BEZIER = (1 << 4),
};

typedef struct SPICE_ATTR_PACKED QXLPathSeg {
    uint32_t flags;
    uint32_t count;
    QXLPointFix points[0];
} QXLPathSeg;

typedef struct SPICE_ATTR_PACKED QXLPath {
    uint32_t data_size;
    QXLDataChunk chunk;
} QXLPath;

enum {
    QXL_IMAGE_GROUP_DRIVER,
    QXL_IMAGE_GROUP_DEVICE,
    QXL_IMAGE_GROUP_RED,
    QXL_IMAGE_GROUP_DRIVER_DONT_CACHE,
};

typedef struct SPICE_ATTR_PACKED QXLImageID {
    uint32_t group;
    uint32_t unique;
} QXLImageID;

typedef union {
  QXLImageID id;
  uint64_t value;
} QXLImageIDUnion;

typedef enum QXLImageFlags {
    QXL_IMAGE_CACHE = (1 << 0),
    QXL_IMAGE_HIGH_BITS_SET = (1 << 1),
} QXLImageFlags;

typedef enum QXLBitmapFlags {
    QXL_BITMAP_DIRECT = (1 << 0),
    QXL_BITMAP_UNSTABLE = (1 << 1),
    QXL_BITMAP_TOP_DOWN = (1 << 2), // == SPICE_BITMAP_FLAGS_TOP_DOWN
} QXLBitmapFlags;

#define QXL_SET_IMAGE_ID(image, _group, _unique) {              \
    (image)->descriptor.id = (((uint64_t)_unique) << 32) | _group;	\
}

typedef struct SPICE_ATTR_PACKED QXLImageDescriptor {
    uint64_t id;
    uint8_t type;
    uint8_t flags;
    uint32_t width;
    uint32_t height;
} QXLImageDescriptor;

typedef struct SPICE_ATTR_PACKED QXLPalette {
    uint64_t unique;
    uint16_t num_ents;
    uint32_t ents[0];
} QXLPalette;

typedef struct SPICE_ATTR_PACKED QXLBitmap {
    uint8_t format;
    uint8_t flags;
    uint32_t x;
    uint32_t y;
    uint32_t stride;
    QXLPHYSICAL palette;
    QXLPHYSICAL data; //data[0] ?
} QXLBitmap;

typedef struct SPICE_ATTR_PACKED QXLSurfaceId {
    uint32_t surface_id;
} QXLSurfaceId;

typedef struct SPICE_ATTR_PACKED QXLQUICData {
    uint32_t data_size;
    uint8_t data[0];
} QXLQUICData, QXLLZRGBData, QXLJPEGData;

typedef struct SPICE_ATTR_PACKED QXLImage {
    QXLImageDescriptor descriptor;
    union { // variable length
        QXLBitmap bitmap;
        QXLQUICData quic;
        QXLSurfaceId surface_image;
    };
} QXLImage;

/* A QXLHead is a single monitor output backed by a QXLSurface.
 * x and y offsets are unsigned since they are used in relation to
 * the given surface, not the same as the x, y coordinates in the guest
 * screen reference frame. */
typedef struct SPICE_ATTR_PACKED QXLHead {
    uint32_t id;
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t flags;
} QXLHead;

typedef struct SPICE_ATTR_PACKED QXLMonitorsConfig {
    uint16_t count;
    uint16_t max_allowed; /* If it is 0 no fixed limit is given by the driver */
    QXLHead heads[0];
} QXLMonitorsConfig;

#include <spice/end-packed.h>

#endif /* _H_QXL_DEV */
