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
#ifndef __SPICE_CLIENT_SESSION_PRIV_H__
#define __SPICE_CLIENT_SESSION_PRIV_H__

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#ifdef USE_PHODAV
#include <libphodav/phodav.h>
#else
typedef struct _PhodavServer PhodavServer;
#endif

#include "desktop-integration.h"
#include "spice-session.h"
#include "spice-gtk-session.h"
#include "spice-channel-cache.h"
#include "decode.h"

G_BEGIN_DECLS

#define WEBDAV_MAGIC_SIZE 16

SpiceSession *spice_session_new_from_session(SpiceSession *session);

void spice_session_set_connection_id(SpiceSession *session, int id);
int spice_session_get_connection_id(SpiceSession *session);
gboolean spice_session_get_client_provided_socket(SpiceSession *session);

GSocketConnection* spice_session_channel_open_host(SpiceSession *session, SpiceChannel *channel,
                                                   gboolean *use_tls, GError **error);
void spice_session_channel_new(SpiceSession *session, SpiceChannel *channel);
void spice_session_channel_migrate(SpiceSession *session, SpiceChannel *channel);

void spice_session_set_mm_time(SpiceSession *session, guint32 time);
guint32 spice_session_get_mm_time(SpiceSession *session);

void spice_session_switching_disconnect(SpiceSession *session);
void spice_session_start_migrating(SpiceSession *session,
                                   gboolean full_migration);
void spice_session_abort_migration(SpiceSession *session);
void spice_session_set_migration_state(SpiceSession *session, SpiceSessionMigration state);

void spice_session_set_port(SpiceSession *session, int port, gboolean tls);
void spice_session_get_pubkey(SpiceSession *session, guint8 **pubkey, guint *size);
guint spice_session_get_verify(SpiceSession *session);
const gchar* spice_session_get_username(SpiceSession *session);
const gchar* spice_session_get_password(SpiceSession *session);
const gchar* spice_session_get_host(SpiceSession *session);
const gchar* spice_session_get_cert_subject(SpiceSession *session);
const gchar* spice_session_get_ciphers(SpiceSession *session);
const gchar* spice_session_get_ca_file(SpiceSession *session);
void spice_session_get_ca(SpiceSession *session, guint8 **ca, guint *size);

void spice_session_set_caches_hints(SpiceSession *session,
                                    uint32_t pci_ram_size,
                                    uint32_t n_display_channels);
void spice_session_get_caches(SpiceSession *session,
                              display_cache **images,
                              SpiceGlzDecoderWindow **glz_window);
void spice_session_palettes_clear(SpiceSession *session);
void spice_session_images_clear(SpiceSession *session);
void spice_session_migrate_end(SpiceSession *session);
gboolean spice_session_migrate_after_main_init(SpiceSession *session);
SpiceChannel* spice_session_lookup_channel(SpiceSession *session, gint id, gint type);
void spice_session_set_uuid(SpiceSession *session, guint8 uuid[16]);
void spice_session_set_name(SpiceSession *session, const gchar *name);
gboolean spice_session_is_playback_active(SpiceSession *session);
guint32 spice_session_get_playback_latency(SpiceSession *session);
void spice_session_sync_playback_latency(SpiceSession *session);
const gchar* spice_session_get_shared_dir(SpiceSession *session);
void spice_session_set_shared_dir(SpiceSession *session, const gchar *dir);
gboolean spice_session_get_audio_enabled(SpiceSession *session);
gboolean spice_session_get_smartcard_enabled(SpiceSession *session);
gboolean spice_session_get_usbredir_enabled(SpiceSession *session);

const guint8* spice_session_get_webdav_magic(SpiceSession *session);
PhodavServer *spice_session_get_webdav_server(SpiceSession *session);
PhodavServer* channel_webdav_server_new(SpiceSession *session);
guint spice_session_get_n_display_channels(SpiceSession *session);
void spice_session_set_main_channel(SpiceSession *session, SpiceChannel *channel);
gboolean spice_session_set_migration_session(SpiceSession *session, SpiceSession *mig_session);
SpiceAudio *spice_audio_get(SpiceSession *session, GMainContext *context);
G_END_DECLS

#endif /* __SPICE_CLIENT_SESSION_PRIV_H__ */
