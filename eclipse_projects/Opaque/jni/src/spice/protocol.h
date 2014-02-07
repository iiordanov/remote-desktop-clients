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

#ifndef _H_SPICE_PROTOCOL
#define _H_SPICE_PROTOCOL

#include <spice/types.h>
#include <spice/enums.h>
#include <spice/start-packed.h>

#define SPICE_MAGIC (*(uint32_t*)"REDQ")
#define SPICE_VERSION_MAJOR 2
#define SPICE_VERSION_MINOR 2

// Encryption & Ticketing Parameters
#define SPICE_MAX_PASSWORD_LENGTH 60
#define SPICE_TICKET_KEY_PAIR_LENGTH 1024
#define SPICE_TICKET_PUBKEY_BYTES (SPICE_TICKET_KEY_PAIR_LENGTH / 8 + 34)

typedef struct SPICE_ATTR_PACKED SpiceLinkHeader {
    uint32_t magic;
    uint32_t major_version;
    uint32_t minor_version;
    uint32_t size;
} SpiceLinkHeader;

enum {
    SPICE_COMMON_CAP_PROTOCOL_AUTH_SELECTION,
    SPICE_COMMON_CAP_AUTH_SPICE,
    SPICE_COMMON_CAP_AUTH_SASL,
    SPICE_COMMON_CAP_MINI_HEADER,
};

typedef struct SPICE_ATTR_PACKED SpiceLinkMess {
    uint32_t connection_id;
    uint8_t channel_type;
    uint8_t channel_id;
    uint32_t num_common_caps;
    uint32_t num_channel_caps;
    uint32_t caps_offset;
} SpiceLinkMess;

typedef struct SPICE_ATTR_PACKED SpiceLinkReply {
    uint32_t error;
    uint8_t pub_key[SPICE_TICKET_PUBKEY_BYTES];
    uint32_t num_common_caps;
    uint32_t num_channel_caps;
    uint32_t caps_offset;
} SpiceLinkReply;

typedef struct SPICE_ATTR_PACKED SpiceLinkEncryptedTicket {
    uint8_t encrypted_data[SPICE_TICKET_KEY_PAIR_LENGTH / 8];
} SpiceLinkEncryptedTicket;

typedef struct SPICE_ATTR_PACKED SpiceLinkAuthMechanism {
    uint32_t auth_mechanism;
} SpiceLinkAuthMechanism;

typedef struct SPICE_ATTR_PACKED SpiceDataHeader {
    uint64_t serial;
    uint16_t type;
    uint32_t size;
    uint32_t sub_list; //offset to SpiceSubMessageList[]
} SpiceDataHeader;

typedef struct SPICE_ATTR_PACKED SpiceMiniDataHeader {
    uint16_t type;
    uint32_t size;
} SpiceMiniDataHeader;

typedef struct SPICE_ATTR_PACKED SpiceSubMessage {
    uint16_t type;
    uint32_t size;
} SpiceSubMessage;

typedef struct SPICE_ATTR_PACKED SpiceSubMessageList {
    uint16_t size;
    uint32_t sub_messages[0]; //offsets to SpicedSubMessage
} SpiceSubMessageList;

#define SPICE_INPUT_MOTION_ACK_BUNCH 4

enum {
    SPICE_PLAYBACK_CAP_CELT_0_5_1,
    SPICE_PLAYBACK_CAP_VOLUME,
    SPICE_PLAYBACK_CAP_LATENCY,
};

enum {
    SPICE_RECORD_CAP_CELT_0_5_1,
    SPICE_RECORD_CAP_VOLUME,
};

enum {
    SPICE_MAIN_CAP_SEMI_SEAMLESS_MIGRATE,
    SPICE_MAIN_CAP_NAME_AND_UUID,
    SPICE_MAIN_CAP_AGENT_CONNECTED_TOKENS,
    SPICE_MAIN_CAP_SEAMLESS_MIGRATE,
};

enum {
    SPICE_DISPLAY_CAP_SIZED_STREAM,
    SPICE_DISPLAY_CAP_MONITORS_CONFIG,
    SPICE_DISPLAY_CAP_COMPOSITE,
    SPICE_DISPLAY_CAP_A8_SURFACE,
    SPICE_DISPLAY_CAP_STREAM_REPORT,
};

enum {
    SPICE_INPUTS_CAP_KEY_SCANCODE,
};

enum {
    SPICE_PORT_EVENT_OPENED,
    SPICE_PORT_EVENT_CLOSED,
    SPICE_PORT_EVENT_BREAK,
};

#include <spice/end-packed.h>

#endif /* _H_SPICE_PROTOCOL */
