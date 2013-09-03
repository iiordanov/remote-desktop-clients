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

#ifndef _H_VD_AGENT
#define _H_VD_AGENT

#include <spice/types.h>

#include <spice/start-packed.h>

enum {
    VDP_CLIENT_PORT = 1,
    VDP_SERVER_PORT,
    VDP_END_PORT
};

typedef struct SPICE_ATTR_PACKED VDIChunkHeader {
    uint32_t port;
    uint32_t size;
} VDIChunkHeader;

typedef struct SPICE_ATTR_PACKED VDAgentMessage {
    uint32_t protocol;
    uint32_t type;
    uint64_t opaque;
    uint32_t size;
    uint8_t data[0];
} VDAgentMessage;

#define VD_AGENT_PROTOCOL 1
#define VD_AGENT_MAX_DATA_SIZE 2048
#define VD_AGENT_CLIPBOARD_MAX_SIZE_DEFAULT 1024
#define VD_AGENT_CLIPBOARD_MAX_SIZE_ENV "SPICE_CLIPBOARD_MAX_SIZE"

enum {
    VD_AGENT_MOUSE_STATE = 1,
    VD_AGENT_MONITORS_CONFIG,
    VD_AGENT_REPLY,
    VD_AGENT_CLIPBOARD,
    VD_AGENT_DISPLAY_CONFIG,
    VD_AGENT_ANNOUNCE_CAPABILITIES,
    VD_AGENT_CLIPBOARD_GRAB,
    VD_AGENT_CLIPBOARD_REQUEST,
    VD_AGENT_CLIPBOARD_RELEASE,
    VD_AGENT_FILE_XFER_START,
    VD_AGENT_FILE_XFER_STATUS,
    VD_AGENT_FILE_XFER_DATA,
    VD_AGENT_CLIENT_DISCONNECTED,
    VD_AGENT_END_MESSAGE,
};

enum {
    VD_AGENT_FILE_XFER_STATUS_CAN_SEND_DATA,
    VD_AGENT_FILE_XFER_STATUS_CANCELLED,
    VD_AGENT_FILE_XFER_STATUS_ERROR,
    VD_AGENT_FILE_XFER_STATUS_SUCCESS,
};

typedef struct SPICE_ATTR_PACKED VDAgentFileXferStatusMessage {
   uint32_t id;
   uint32_t result;
} VDAgentFileXferStatusMessage;

typedef struct SPICE_ATTR_PACKED VDAgentFileXferStartMessage {
   uint32_t id;
   uint8_t data[0];
} VDAgentFileXferStartMessage;

typedef struct SPICE_ATTR_PACKED VDAgentFileXferDataMessage {
   uint32_t id;
   uint64_t size;
   uint8_t data[0];
} VDAgentFileXferDataMessage;

typedef struct SPICE_ATTR_PACKED VDAgentMonConfig {
    /*
     * Note a width and height of 0 can be used to indicate a disabled
     * monitor, this may only be used with agents with the
     * VD_AGENT_CAP_SPARSE_MONITORS_CONFIG capability.
     */
    uint32_t height;
    uint32_t width;
    uint32_t depth;
    int32_t x;
    int32_t y;
} VDAgentMonConfig;

enum {
    VD_AGENT_CONFIG_MONITORS_FLAG_USE_POS = (1 << 0),
};

typedef struct SPICE_ATTR_PACKED VDAgentMonitorsConfig {
    uint32_t num_of_monitors;
    uint32_t flags;
    VDAgentMonConfig monitors[0];
} VDAgentMonitorsConfig;

enum {
    VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_WALLPAPER = (1 << 0),
    VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_FONT_SMOOTH = (1 << 1),
    VD_AGENT_DISPLAY_CONFIG_FLAG_DISABLE_ANIMATION = (1 << 2),
    VD_AGENT_DISPLAY_CONFIG_FLAG_SET_COLOR_DEPTH = (1 << 3),
};

typedef struct SPICE_ATTR_PACKED VDAgentDisplayConfig {
    uint32_t flags;
    uint32_t depth;
} VDAgentDisplayConfig;

#define VD_AGENT_LBUTTON_MASK (1 << 1)
#define VD_AGENT_MBUTTON_MASK (1 << 2)
#define VD_AGENT_RBUTTON_MASK (1 << 3)
#define VD_AGENT_UBUTTON_MASK (1 << 4)
#define VD_AGENT_DBUTTON_MASK (1 << 5)

typedef struct SPICE_ATTR_PACKED VDAgentMouseState {
    uint32_t x;
    uint32_t y;
    uint32_t buttons;
    uint8_t display_id;
} VDAgentMouseState;

typedef struct SPICE_ATTR_PACKED VDAgentReply {
    uint32_t type;
    uint32_t error;
} VDAgentReply;

enum {
    VD_AGENT_SUCCESS = 1,
    VD_AGENT_ERROR,
};

typedef struct SPICE_ATTR_PACKED VDAgentClipboard {
#if 0 /* VD_AGENT_CAP_CLIPBOARD_SELECTION */
    uint8_t selection;
    uint8_t __reserved[sizeof(uint32_t) - 1 * sizeof(uint8_t)];
#endif
    uint32_t type;
    uint8_t data[0];
} VDAgentClipboard;

enum {
    VD_AGENT_CLIPBOARD_NONE = 0,
    VD_AGENT_CLIPBOARD_UTF8_TEXT,
    VD_AGENT_CLIPBOARD_IMAGE_PNG,  /* All clients with image support should support this one */
    VD_AGENT_CLIPBOARD_IMAGE_BMP,  /* optional */
    VD_AGENT_CLIPBOARD_IMAGE_TIFF, /* optional */
    VD_AGENT_CLIPBOARD_IMAGE_JPG,  /* optional */
};

typedef struct SPICE_ATTR_PACKED VDAgentClipboardGrab {
#if 0 /* VD_AGENT_CAP_CLIPBOARD_SELECTION */
    uint8_t selection;
    uint8_t __reserved[sizeof(uint32_t) - 1 * sizeof(uint8_t)];
#endif
    uint32_t types[0];
} VDAgentClipboardGrab;

typedef struct SPICE_ATTR_PACKED VDAgentClipboardRequest {
#if 0 /* VD_AGENT_CAP_CLIPBOARD_SELECTION */
    uint8_t selection;
    uint8_t __reserved[sizeof(uint32_t) - 1 * sizeof(uint8_t)];
#endif
    uint32_t type;
} VDAgentClipboardRequest;

typedef struct SPICE_ATTR_PACKED VDAgentClipboardRelease {
#if 0 /* VD_AGENT_CAP_CLIPBOARD_SELECTION */
    uint8_t selection;
    uint8_t __reserved[sizeof(uint32_t) - 1 * sizeof(uint8_t)];
#endif
} VDAgentClipboardRelease;

enum {
    VD_AGENT_CAP_MOUSE_STATE = 0,
    VD_AGENT_CAP_MONITORS_CONFIG,
    VD_AGENT_CAP_REPLY,
    VD_AGENT_CAP_CLIPBOARD,
    VD_AGENT_CAP_DISPLAY_CONFIG,
    VD_AGENT_CAP_CLIPBOARD_BY_DEMAND,
    VD_AGENT_CAP_CLIPBOARD_SELECTION,
    VD_AGENT_CAP_SPARSE_MONITORS_CONFIG,
    VD_AGENT_CAP_GUEST_LINEEND_LF,
    VD_AGENT_CAP_GUEST_LINEEND_CRLF,
    VD_AGENT_END_CAP,
};

enum {
    VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD = 0,
    VD_AGENT_CLIPBOARD_SELECTION_PRIMARY,
    VD_AGENT_CLIPBOARD_SELECTION_SECONDARY,
};

typedef struct SPICE_ATTR_PACKED VDAgentAnnounceCapabilities {
    uint32_t  request;
    uint32_t caps[0];
} VDAgentAnnounceCapabilities;

#define VD_AGENT_CAPS_SIZE_FROM_MSG_SIZE(msg_size) \
    (((msg_size) - sizeof(VDAgentAnnounceCapabilities)) / sizeof(uint32_t))

#define VD_AGENT_CAPS_SIZE ((VD_AGENT_END_CAP + 31) / 32)

#define VD_AGENT_CAPS_BYTES (((VD_AGENT_END_CAP + 31) / 8) & ~3)

#define VD_AGENT_HAS_CAPABILITY(caps, caps_size, index) \
    ((index) < (caps_size * 32) && ((caps)[(index) / 32] & (1 << ((index) % 32))))

#define VD_AGENT_SET_CAPABILITY(caps, index) \
    { (caps)[(index) / 32] |= (1 << ((index) % 32)); }

#include <spice/end-packed.h>

#endif /* _H_VD_AGENT */
