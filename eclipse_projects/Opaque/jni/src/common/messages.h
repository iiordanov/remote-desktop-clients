/*
   Copyright (C) 2009-2010 Red Hat, Inc.

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

#ifndef _H_MESSAGES
#define _H_MESSAGES

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <spice/protocol.h>
#include <spice/macros.h>

#ifdef USE_SMARTCARD
#include <vscard_common.h>
#endif

#include "draw.h"

SPICE_BEGIN_DECLS

typedef struct SpiceMsgData {
    uint32_t data_size;
    uint8_t data[0];
} SpiceMsgData;

typedef struct SpiceMsgEmpty {
    uint8_t padding;
} SpiceMsgEmpty;

typedef struct SpiceMsgInputsInit {
    uint32_t keyboard_modifiers;
} SpiceMsgInputsInit;

typedef struct SpiceMsgInputsKeyModifiers {
    uint32_t modifiers;
} SpiceMsgInputsKeyModifiers;

typedef struct SpiceMsgMainMultiMediaTime {
    uint32_t time;
} SpiceMsgMainMultiMediaTime;

typedef struct SpiceMigrationDstInfo {
    uint16_t port;
    uint16_t sport;
    uint32_t host_size;
    uint8_t *host_data;
    uint16_t pub_key_type;
    uint32_t pub_key_size;
    uint8_t *pub_key_data;
    uint32_t cert_subject_size;
    uint8_t *cert_subject_data;
} SpiceMigrationDstInfo;

typedef struct SpiceMsgMainMigrationBegin {
    SpiceMigrationDstInfo dst_info;
} SpiceMsgMainMigrationBegin;

typedef struct SpiceMsgMainMigrateBeginSeamless {
    SpiceMigrationDstInfo dst_info;
    uint32_t src_mig_version;
} SpiceMsgMainMigrateBeginSeamless;

typedef struct SpiceMsgcMainMigrateDstDoSeamless {
    uint32_t src_version;
} SpiceMsgcMainMigrateDstDoSeamless;

typedef struct SpiceMsgMainMigrationSwitchHost {
    uint16_t port;
    uint16_t sport;
    uint32_t host_size;
    uint8_t *host_data;
    uint32_t cert_subject_size;
    uint8_t *cert_subject_data;
} SpiceMsgMainMigrationSwitchHost;


typedef struct SpiceMsgMigrate {
    uint32_t flags;
} SpiceMsgMigrate;

typedef struct SpiceResourceID {
    uint8_t type;
    uint64_t id;
} SpiceResourceID;

typedef struct SpiceResourceList {
    uint16_t count;
    SpiceResourceID resources[0];
} SpiceResourceList;

typedef struct SpiceMsgSetAck {
    uint32_t generation;
    uint32_t window;
} SpiceMsgSetAck;

typedef struct SpiceMsgcAckSync {
  uint32_t generation;
} SpiceMsgcAckSync;

typedef struct SpiceWaitForChannel {
    uint8_t channel_type;
    uint8_t channel_id;
    uint64_t message_serial;
} SpiceWaitForChannel;

typedef struct SpiceMsgWaitForChannels {
    uint8_t wait_count;
    SpiceWaitForChannel wait_list[0];
} SpiceMsgWaitForChannels;

typedef struct SpiceChannelId {
    uint8_t type;
    uint8_t id;
} SpiceChannelId;

typedef struct SpiceMsgMainInit {
    uint32_t session_id;
    uint32_t display_channels_hint;
    uint32_t supported_mouse_modes;
    uint32_t current_mouse_mode;
    uint32_t agent_connected;
    uint32_t agent_tokens;
    uint32_t multi_media_time;
    uint32_t ram_hint;
} SpiceMsgMainInit;

typedef struct SpiceMsgDisconnect {
    uint64_t time_stamp;
    uint32_t reason; // SPICE_ERR_?
} SpiceMsgDisconnect;

typedef struct SpiceMsgNotify {
    uint64_t time_stamp;
    uint32_t severity;
    uint32_t visibilty;
    uint32_t what;
    uint32_t message_len;
    uint8_t message[0];
} SpiceMsgNotify;

typedef struct SpiceMsgChannels {
    uint32_t num_of_channels;
    SpiceChannelId channels[0];
} SpiceMsgChannels;

typedef struct SpiceMsgMainName {
    uint32_t name_len;
    uint8_t name[0];
} SpiceMsgMainName;

typedef struct SpiceMsgMainUuid {
    uint8_t uuid[16];
} SpiceMsgMainUuid;

typedef struct SpiceMsgMainMouseMode {
    uint32_t supported_modes;
    uint32_t current_mode;
} SpiceMsgMainMouseMode;

typedef struct SpiceMsgPing {
    uint32_t id;
    uint64_t timestamp;
    void *data;
    uint32_t data_len;
} SpiceMsgPing;

typedef struct SpiceMsgMainAgentDisconnect {
    uint32_t error_code; // SPICE_ERR_?
} SpiceMsgMainAgentDisconnect;

#define SPICE_AGENT_MAX_DATA_SIZE 2048

typedef struct SpiceMsgMainAgentTokens {
    uint32_t num_tokens;
} SpiceMsgMainAgentTokens, SpiceMsgcMainAgentTokens, SpiceMsgcMainAgentStart;

typedef struct SpiceMsgMainAgentTokens SpiceMsgMainAgentConnectedTokens;

typedef struct SpiceMsgcClientInfo {
    uint64_t cache_size;
} SpiceMsgcClientInfo;

typedef struct SpiceMsgcMainMouseModeRequest {
    uint32_t mode;
} SpiceMsgcMainMouseModeRequest;

typedef struct SpiceCursor {
    uint32_t flags;
    SpiceCursorHeader header;
    uint32_t data_size;
    uint8_t *data;
} SpiceCursor;

typedef struct SpiceMsgDisplayMode {
    uint32_t x_res;
    uint32_t y_res;
    uint32_t bits;
} SpiceMsgDisplayMode;

typedef struct SpiceMsgSurfaceCreate {
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
} SpiceMsgSurfaceCreate;

typedef struct SpiceMsgSurfaceDestroy {
    uint32_t surface_id;
} SpiceMsgSurfaceDestroy;

typedef struct SpiceMsgDisplayBase {
    uint32_t surface_id;
    SpiceRect box;
    SpiceClip clip;
} SpiceMsgDisplayBase;

typedef struct SpiceMsgDisplayDrawFill {
    SpiceMsgDisplayBase base;
    SpiceFill data;
} SpiceMsgDisplayDrawFill;

typedef struct SpiceMsgDisplayDrawOpaque {
    SpiceMsgDisplayBase base;
    SpiceOpaque data;
} SpiceMsgDisplayDrawOpaque;

typedef struct SpiceMsgDisplayDrawCopy {
    SpiceMsgDisplayBase base;
    SpiceCopy data;
} SpiceMsgDisplayDrawCopy;

typedef struct SpiceMsgDisplayDrawTransparent {
    SpiceMsgDisplayBase base;
    SpiceTransparent data;
} SpiceMsgDisplayDrawTransparent;

typedef struct SpiceMsgDisplayDrawAlphaBlend {
    SpiceMsgDisplayBase base;
    SpiceAlphaBlend data;
} SpiceMsgDisplayDrawAlphaBlend;

typedef struct SpiceMsgDisplayDrawComposite {
    SpiceMsgDisplayBase base;
    SpiceComposite data;
} SpiceMsgDisplayDrawComposite;

typedef struct SpiceMsgDisplayCopyBits {
    SpiceMsgDisplayBase base;
    SpicePoint src_pos;
} SpiceMsgDisplayCopyBits;

typedef SpiceMsgDisplayDrawCopy SpiceMsgDisplayDrawBlend;

typedef struct SpiceMsgDisplayDrawRop3 {
    SpiceMsgDisplayBase base;
    SpiceRop3 data;
} SpiceMsgDisplayDrawRop3;

typedef struct SpiceMsgDisplayDrawBlackness {
    SpiceMsgDisplayBase base;
    SpiceBlackness data;
} SpiceMsgDisplayDrawBlackness;

typedef struct SpiceMsgDisplayDrawWhiteness {
    SpiceMsgDisplayBase base;
    SpiceWhiteness data;
} SpiceMsgDisplayDrawWhiteness;

typedef struct SpiceMsgDisplayDrawInvers {
    SpiceMsgDisplayBase base;
    SpiceInvers data;
} SpiceMsgDisplayDrawInvers;

typedef struct SpiceMsgDisplayDrawStroke {
    SpiceMsgDisplayBase base;
    SpiceStroke data;
} SpiceMsgDisplayDrawStroke;

typedef struct SpiceMsgDisplayDrawText {
    SpiceMsgDisplayBase base;
    SpiceText data;
} SpiceMsgDisplayDrawText;

typedef struct SpiceMsgDisplayInvalOne {
    uint64_t id;
} SpiceMsgDisplayInvalOne;

typedef struct SpiceMsgDisplayStreamCreate {
    uint32_t surface_id;
    uint32_t id;
    uint32_t flags;
    uint32_t codec_type;
    uint64_t stamp;
    uint32_t stream_width;
    uint32_t stream_height;
    uint32_t src_width;
    uint32_t src_height;
    SpiceRect dest;
    SpiceClip clip;
} SpiceMsgDisplayStreamCreate;

typedef struct SpiceStreamDataHeader {
    uint32_t id;
    uint32_t multi_media_time;
} SpiceStreamDataHeader;

typedef struct SpiceMsgDisplayStreamData {
    SpiceStreamDataHeader base;
    uint32_t data_size;
    uint8_t data[0];
} SpiceMsgDisplayStreamData;

typedef struct SpiceMsgDisplayStreamDataSized {
    SpiceStreamDataHeader base;
    uint32_t width;
    uint32_t height;
    SpiceRect dest;
    uint32_t data_size;
    uint8_t data[0];
} SpiceMsgDisplayStreamDataSized;

typedef struct SpiceMsgDisplayStreamClip {
    uint32_t id;
    SpiceClip clip;
} SpiceMsgDisplayStreamClip;

typedef struct SpiceMsgDisplayStreamDestroy {
    uint32_t id;
} SpiceMsgDisplayStreamDestroy;

typedef struct SpiceMsgDisplayStreamActivateReport {
    uint32_t stream_id;
    uint32_t unique_id;
    uint32_t max_window_size;
    uint32_t timeout_ms;
} SpiceMsgDisplayStreamActivateReport;

typedef struct SpiceMsgcDisplayStreamReport {
    uint32_t stream_id;
    uint32_t unique_id;
    uint32_t start_frame_mm_time;
    uint32_t end_frame_mm_time;
    uint32_t num_frames;
    uint32_t num_drops;
    int32_t last_frame_delay;
    uint32_t audio_delay;
} SpiceMsgcDisplayStreamReport;

typedef struct SpiceMsgCursorInit {
    SpicePoint16 position;
    uint16_t trail_length;
    uint16_t trail_frequency;
    uint8_t visible;
    SpiceCursor cursor;
} SpiceMsgCursorInit;

typedef struct SpiceMsgCursorSet {
    SpicePoint16 position;
    uint8_t visible;
    SpiceCursor cursor;
} SpiceMsgCursorSet;

typedef struct SpiceMsgCursorMove {
    SpicePoint16 position;
} SpiceMsgCursorMove;

typedef struct SpiceMsgCursorTrail {
    uint16_t length;
    uint16_t frequency;
} SpiceMsgCursorTrail;

typedef struct SpiceMsgcDisplayInit {
    uint8_t pixmap_cache_id;
    int64_t pixmap_cache_size; //in pixels
    uint8_t glz_dictionary_id;
    int32_t glz_dictionary_window_size;       // in pixels
} SpiceMsgcDisplayInit;

typedef struct SpiceMsgcKeyDown {
    uint32_t code;
} SpiceMsgcKeyDown;

typedef struct SpiceMsgcKeyUp {
    uint32_t code;
} SpiceMsgcKeyUp;

typedef struct SpiceMsgcKeyModifiers {
    uint32_t modifiers;
} SpiceMsgcKeyModifiers;

typedef struct SpiceMsgcMouseMotion {
    int32_t dx;
    int32_t dy;
    uint32_t buttons_state;
} SpiceMsgcMouseMotion;

typedef struct SpiceMsgcMousePosition {
    uint32_t x;
    uint32_t y;
    uint32_t buttons_state;
    uint8_t display_id;
} SpiceMsgcMousePosition;

typedef struct SpiceMsgcMousePress {
    int32_t button;
    int32_t buttons_state;
} SpiceMsgcMousePress;

typedef struct SpiceMsgcMouseRelease {
    int32_t button;
    int32_t buttons_state;
} SpiceMsgcMouseRelease;

typedef struct SpiceMsgAudioVolume {
    uint8_t nchannels;
    uint16_t volume[0];
} SpiceMsgAudioVolume;

typedef struct SpiceMsgAudioMute {
    uint8_t mute;
} SpiceMsgAudioMute;

typedef struct SpiceMsgPlaybackMode {
    uint32_t time;
    uint32_t mode; //SPICE_AUDIO_DATA_MODE_?
    uint8_t *data;
    uint32_t data_size;
} SpiceMsgPlaybackMode, SpiceMsgcRecordMode;

typedef struct SpiceMsgPlaybackStart {
    uint32_t channels;
    uint32_t format; //SPICE_AUDIO_FMT_?
    uint32_t frequency;
    uint32_t time;
} SpiceMsgPlaybackStart;

typedef struct SpiceMsgPlaybackPacket {
    uint32_t time;
    uint8_t *data;
    uint32_t data_size;
} SpiceMsgPlaybackPacket, SpiceMsgcRecordPacket;

typedef struct SpiceMsgPlaybackLatency {
    uint32_t latency_ms;
} SpiceMsgPlaybackLatency;

typedef struct SpiceMsgRecordStart {
    uint32_t channels;
    uint32_t format; //SPICE_AUDIO_FMT_?
    uint32_t frequency;
} SpiceMsgRecordStart;

typedef struct SpiceMsgcRecordStartMark {
    uint32_t time;
} SpiceMsgcRecordStartMark;

typedef struct SpiceMsgTunnelInit {
    uint16_t max_num_of_sockets;
    uint32_t max_socket_data_size;
} SpiceMsgTunnelInit;

typedef uint8_t SpiceTunnelIPv4[4];

typedef struct SpiceMsgTunnelIpInfo {
    uint16_t type;
    union {
      SpiceTunnelIPv4 ipv4;
    } u;
    uint8_t data[0];
} SpiceMsgTunnelIpInfo;

typedef struct SpiceMsgTunnelServiceIpMap {
    uint32_t service_id;
    SpiceMsgTunnelIpInfo virtual_ip;
} SpiceMsgTunnelServiceIpMap;

typedef struct SpiceMsgTunnelSocketOpen {
    uint16_t connection_id;
    uint32_t service_id;
    uint32_t tokens;
} SpiceMsgTunnelSocketOpen;

/* connection id must be the first field in msgs directed to a specific connection */

typedef struct SpiceMsgTunnelSocketFin {
    uint16_t connection_id;
} SpiceMsgTunnelSocketFin;

typedef struct SpiceMsgTunnelSocketClose {
    uint16_t connection_id;
} SpiceMsgTunnelSocketClose;

typedef struct SpiceMsgTunnelSocketData {
    uint16_t connection_id;
    uint8_t data[0];
} SpiceMsgTunnelSocketData;

typedef struct SpiceMsgTunnelSocketTokens {
    uint16_t connection_id;
    uint32_t num_tokens;
} SpiceMsgTunnelSocketTokens;

typedef struct SpiceMsgTunnelSocketClosedAck {
    uint16_t connection_id;
} SpiceMsgTunnelSocketClosedAck;

typedef struct SpiceMsgcTunnelAddGenericService {
    uint32_t type;
    uint32_t id;
    uint32_t group;
    uint32_t port;
    uint64_t name;
    uint64_t description;
    union {
        SpiceMsgTunnelIpInfo ip;
    } u;
} SpiceMsgcTunnelAddGenericService;

typedef struct SpiceMsgcTunnelRemoveService {
    uint32_t id;
} SpiceMsgcTunnelRemoveService;

/* connection id must be the first field in msgs directed to a specific connection */

typedef struct SpiceMsgcTunnelSocketOpenAck {
    uint16_t connection_id;
    uint32_t tokens;
} SpiceMsgcTunnelSocketOpenAck;

typedef struct SpiceMsgcTunnelSocketOpenNack {
    uint16_t connection_id;
} SpiceMsgcTunnelSocketOpenNack;

typedef struct SpiceMsgcTunnelSocketData {
    uint16_t connection_id;
    uint8_t data[0];
} SpiceMsgcTunnelSocketData;

typedef struct SpiceMsgcTunnelSocketFin {
    uint16_t connection_id;
} SpiceMsgcTunnelSocketFin;

typedef struct SpiceMsgcTunnelSocketClosed {
    uint16_t connection_id;
} SpiceMsgcTunnelSocketClosed;

typedef struct SpiceMsgcTunnelSocketClosedAck {
    uint16_t connection_id;
} SpiceMsgcTunnelSocketClosedAck;

typedef struct SpiceMsgcTunnelSocketTokens {
    uint16_t connection_id;
    uint32_t num_tokens;
} SpiceMsgcTunnelSocketTokens;

#ifdef USE_SMARTCARD
typedef struct SpiceMsgSmartcard {
    VSCMsgType type;
    uint32_t length;
    uint32_t reader_id;
    uint8_t data[0];
} SpiceMsgSmartcard;

typedef struct SpiceMsgcSmartcard {
    VSCMsgHeader header;
    union {
        VSCMsgError error;
        VSCMsgATR atr_data;
        VSCMsgReaderAdd add;
    };
} SpiceMsgcSmartcard;
#endif

typedef struct SpiceMsgDisplayHead {
    uint32_t id;
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t flags;
} SpiceHead;

typedef struct SpiceMsgDisplayMonitorsConfig {
    uint16_t count;
    uint16_t max_allowed;
    SpiceHead heads[0];
} SpiceMsgDisplayMonitorsConfig;

typedef struct SpiceMsgPortInit {
    uint32_t name_size;
    uint8_t *name;
    uint8_t opened;
} SpiceMsgPortInit;

typedef struct SpiceMsgPortEvent {
    uint8_t event;
} SpiceMsgPortEvent;

typedef struct SpiceMsgcPortEvent {
    uint8_t event;
} SpiceMsgcPortEvent;

SPICE_END_DECLS

#endif /* _H_SPICE_PROTOCOL */
