/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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

#ifndef _H_MARSHALLERS
#define _H_MARSHALLERS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <spice/protocol.h>

#include "marshaller.h"
#include "messages.h"

SPICE_BEGIN_DECLS

typedef struct {
    void (*msg_SpiceMsgEmpty)(SpiceMarshaller *m, SpiceMsgEmpty *msg);
    void (*msg_SpiceMsgData)(SpiceMarshaller *m, SpiceMsgData *msg);
    void (*msg_SpiceMsgAudioVolume)(SpiceMarshaller *m, SpiceMsgAudioVolume *msg);
    void (*msg_SpiceMsgAudioMute)(SpiceMarshaller *m, SpiceMsgAudioMute *msg);
    void (*msgc_ack_sync)(SpiceMarshaller *m, SpiceMsgcAckSync *msg);
    void (*msgc_pong)(SpiceMarshaller *m, SpiceMsgPing *msg);
    void (*msgc_disconnecting)(SpiceMarshaller *m, SpiceMsgDisconnect *msg);
    void (*msgc_main_client_info)(SpiceMarshaller *m, SpiceMsgcClientInfo *msg);
    void (*msgc_main_mouse_mode_request)(SpiceMarshaller *m, SpiceMsgcMainMouseModeRequest *msg);
    void (*msgc_main_agent_start)(SpiceMarshaller *m, SpiceMsgcMainAgentStart *msg);
    void (*msgc_main_agent_token)(SpiceMarshaller *m, SpiceMsgcMainAgentTokens *msg);
    void (*msgc_main_migrate_dst_do_seamless)(SpiceMarshaller *m, SpiceMsgcMainMigrateDstDoSeamless *msg);
    void (*msgc_display_init)(SpiceMarshaller *m, SpiceMsgcDisplayInit *msg);
    void (*msgc_display_stream_report)(SpiceMarshaller *m, SpiceMsgcDisplayStreamReport *msg);
    void (*msgc_inputs_key_down)(SpiceMarshaller *m, SpiceMsgcKeyDown *msg);
    void (*msgc_inputs_key_up)(SpiceMarshaller *m, SpiceMsgcKeyUp *msg);
    void (*msgc_inputs_key_modifiers)(SpiceMarshaller *m, SpiceMsgcKeyModifiers *msg);
    void (*msgc_inputs_mouse_motion)(SpiceMarshaller *m, SpiceMsgcMouseMotion *msg);
    void (*msgc_inputs_mouse_position)(SpiceMarshaller *m, SpiceMsgcMousePosition *msg);
    void (*msgc_inputs_mouse_press)(SpiceMarshaller *m, SpiceMsgcMousePress *msg);
    void (*msgc_inputs_mouse_release)(SpiceMarshaller *m, SpiceMsgcMouseRelease *msg);
    void (*msgc_record_data)(SpiceMarshaller *m, SpiceMsgcRecordPacket *msg);
    void (*msgc_record_mode)(SpiceMarshaller *m, SpiceMsgcRecordMode *msg);
    void (*msgc_record_start_mark)(SpiceMarshaller *m, SpiceMsgcRecordStartMark *msg);
    void (*msgc_tunnel_service_add)(SpiceMarshaller *m, SpiceMsgcTunnelAddGenericService *msg, SpiceMarshaller **name_out, SpiceMarshaller **description_out);
    void (*msgc_tunnel_service_remove)(SpiceMarshaller *m, SpiceMsgcTunnelRemoveService *msg);
    void (*msgc_tunnel_socket_open_ack)(SpiceMarshaller *m, SpiceMsgcTunnelSocketOpenAck *msg);
    void (*msgc_tunnel_socket_open_nack)(SpiceMarshaller *m, SpiceMsgcTunnelSocketOpenNack *msg);
    void (*msgc_tunnel_socket_fin)(SpiceMarshaller *m, SpiceMsgcTunnelSocketFin *msg);
    void (*msgc_tunnel_socket_closed)(SpiceMarshaller *m, SpiceMsgcTunnelSocketClosed *msg);
    void (*msgc_tunnel_socket_closed_ack)(SpiceMarshaller *m, SpiceMsgcTunnelSocketClosedAck *msg);
    void (*msgc_tunnel_socket_data)(SpiceMarshaller *m, SpiceMsgcTunnelSocketData *msg);
    void (*msgc_tunnel_socket_token)(SpiceMarshaller *m, SpiceMsgcTunnelSocketTokens *msg);
#ifdef USE_SMARTCARD
    void (*msgc_smartcard_atr)(SpiceMarshaller *m, VSCMsgATR *msg);
    void (*msgc_smartcard_error)(SpiceMarshaller *m, VSCMsgError *msg);
    void (*msgc_smartcard_header)(SpiceMarshaller *m, VSCMsgHeader *msg);
    void (*msgc_smartcard_data)(SpiceMarshaller *m, SpiceMsgcSmartcard *msg, SpiceMarshaller **reader_name_out);
    void (*msgc_smartcard_reader_add)(SpiceMarshaller *m, VSCMsgReaderAdd *msg);
#endif
    void (*msgc_port_event)(SpiceMarshaller *m, SpiceMsgcPortEvent *msg);
} SpiceMessageMarshallers;

SpiceMessageMarshallers *spice_message_marshallers_get(void);
SpiceMessageMarshallers *spice_message_marshallers_get1(void);

SPICE_END_DECLS

#endif
