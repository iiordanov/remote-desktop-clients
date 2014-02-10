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

#include <glib.h>
#include <gio/gio.h>
#include "desktop-integration.h"
#include "spice-session.h"
#include "spice-proxy.h"
#include "spice-gtk-session.h"
#include "spice-channel-cache.h"
#include "decode.h"

G_BEGIN_DECLS

#define IMAGES_CACHE_SIZE_DEFAULT (1024 * 1024 * 80)
#define MIN_GLZ_WINDOW_SIZE_DEFAULT (1024 * 1024 * 12)
#define MAX_GLZ_WINDOW_SIZE_DEFAULT MIN((LZ_MAX_WINDOW_SIZE * 4), 1024 * 1024 * 64)

struct _SpiceSessionPrivate {
    char              *host;
    char              *port;
    char              *tls_port;
    char              *password;
    char              *ca_file;
    char              *ciphers;
    GByteArray        *pubkey;
    GByteArray        *ca;
    char              *cert_subject;
    guint             verify;
    gboolean          read_only;
    SpiceProxy        *proxy;

    /* whether to enable audio */
    gboolean          audio;

    /* whether to enable smartcard event forwarding to the server */
    gboolean          smartcard;

    /* list of certificates to use for the software smartcard reader if
     * enabled. For now, it has to contain exactly 3 certificates for
     * the software reader to be functional
     */
    GStrv             smartcard_certificates;

    /* path to the local certificate database to use to lookup the
     * certificates stored in 'certificates'. If NULL, libcacard will
     * fallback to using a default database.
     */
    char *            smartcard_db;

    /* whether to enable USB redirection */
    gboolean          usbredir;

    /* Set when a usbredir channel has requested the keyboard grab to be
       temporarily released (because it is going to invoke policykit) */
    gboolean          inhibit_keyboard_grab;

    GStrv             disable_effects;
    GStrv             secure_channels;
    gint              color_depth;

    int               connection_id;
    int               protocol;
    SpiceChannel      *cmain; /* weak reference */
    Ring              channels;
    guint32           mm_time;
    gboolean          client_provided_sockets;
    guint64           mm_time_at_clock;
    SpiceSession      *migration;
    GList             *migration_left;
    SpiceSessionMigration migration_state;
    gboolean          full_migration; /* seamless migration indicator */
    gboolean          disconnecting;
    gboolean          migrate_wait_init;
    guint             after_main_init;
    gboolean          migration_copy;

    display_cache     *images;
    display_cache     *palettes;
    SpiceGlzDecoderWindow *glz_window;
    int               images_cache_size;
    int               glz_window_size;
    uint32_t          pci_ram_size;
    uint32_t          display_channels_count;
    guint8            uuid[16];
    gchar             *name;

    /* associated objects */
    SpiceAudio        *audio_manager;
    SpiceDesktopIntegration *desktop_integration;
    SpiceGtkSession   *gtk_session;
    SpiceUsbDeviceManager *usb_manager;
    SpicePlaybackChannel *playback_channel;
};

SpiceSession *spice_session_new_from_session(SpiceSession *session);

void spice_session_set_connection_id(SpiceSession *session, int id);
int spice_session_get_connection_id(SpiceSession *session);
gboolean spice_session_get_client_provided_socket(SpiceSession *session);

GSocketConnection* spice_session_channel_open_host(SpiceSession *session, SpiceChannel *channel,
                                                   gboolean *use_tls);
void spice_session_channel_new(SpiceSession *session, SpiceChannel *channel);
void spice_session_channel_destroy(SpiceSession *session, SpiceChannel *channel);
void spice_session_channel_migrate(SpiceSession *session, SpiceChannel *channel);

void spice_session_set_mm_time(SpiceSession *session, guint32 time);
guint32 spice_session_get_mm_time(SpiceSession *session);

void spice_session_switching_disconnect(SpiceSession *session);
void spice_session_set_migration(SpiceSession *session,
                                 SpiceSession *migration,
                                 gboolean full_migration);
void spice_session_abort_migration(SpiceSession *session);
void spice_session_set_migration_state(SpiceSession *session, SpiceSessionMigration state);

void spice_session_set_port(SpiceSession *session, int port, gboolean tls);
void spice_session_get_pubkey(SpiceSession *session, guint8 **pubkey, guint *size);
guint spice_session_get_verify(SpiceSession *session);
const gchar* spice_session_get_password(SpiceSession *session);
const gchar* spice_session_get_host(SpiceSession *session);
const gchar* spice_session_get_cert_subject(SpiceSession *session);
const gchar* spice_session_get_ciphers(SpiceSession *session);
const gchar* spice_session_get_ca_file(SpiceSession *session);
void spice_session_get_ca(SpiceSession *session, guint8 **ca, guint *size);

void spice_session_set_caches_hints(SpiceSession *session,
                                    uint32_t pci_ram_size,
                                    uint32_t display_channels_count);
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

G_END_DECLS

#endif /* __SPICE_CLIENT_SESSION_PRIV_H__ */
