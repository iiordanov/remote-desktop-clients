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
#include "config.h"

#include <gio/gio.h>
#include <glib.h>
#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif
#include "common/ring.h"

#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"
#include "spice-util-priv.h"
#include "spice-session-priv.h"
#include "gio-coroutine.h"
#include "glib-compat.h"
#include "wocky-http-proxy.h"
#include "spice-uri-priv.h"
#include "channel-playback-priv.h"
#include "spice-audio.h"

struct channel {
    SpiceChannel      *channel;
    RingItem          link;
};

#define IMAGES_CACHE_SIZE_DEFAULT (1024 * 1024 * 80)
#define MIN_GLZ_WINDOW_SIZE_DEFAULT (1024 * 1024 * 12)
#define MAX_GLZ_WINDOW_SIZE_DEFAULT MIN((LZ_MAX_WINDOW_SIZE * 4), 1024 * 1024 * 64)

struct _SpiceSessionPrivate {
    char              *host;
    char              *unix_path;
    char              *port;
    char              *tls_port;
    char              *username;
    char              *password;
    char              *ca_file;
    char              *ciphers;
    GByteArray        *pubkey;
    GByteArray        *ca;
    char              *cert_subject;
    guint             verify;
    gboolean          read_only;
    SpiceURI          *proxy;
    gchar             *shared_dir;
    gboolean          share_dir_ro;

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
    guint             disconnecting;
    gboolean          migrate_wait_init;
    guint             after_main_init;
    gboolean          for_migration;

    display_cache     *images;
    display_cache     *palettes;
    SpiceGlzDecoderWindow *glz_window;
    int               images_cache_size;
    int               glz_window_size;
    uint32_t          pci_ram_size;
    uint32_t          n_display_channels;
    guint8            uuid[16];
    gchar             *name;

    /* associated objects */
    SpiceAudio        *audio_manager;
    SpiceUsbDeviceManager *usb_manager;
    SpicePlaybackChannel *playback_channel;
    PhodavServer      *webdav;
};


/**
 * SECTION:spice-session
 * @short_description: handles connection details, and active channels
 * @title: Spice Session
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: spice-session.h
 *
 * The #SpiceSession class handles all the #SpiceChannel connections.
 * It's also the class that contains connections informations, such as
 * #SpiceSession:host and #SpiceSession:port.
 *
 * You can simply set the property #SpiceSession:uri to something like
 * "spice://127.0.0.1?port=5930" to configure your connection details.
 *
 * You may want to connect to #SpiceSession::channel-new signal, to be
 * informed of the availability of channels and to interact with
 * them.
 *
 * For example, when the #SpiceInputsChannel is available and get the
 * event #SPICE_CHANNEL_OPENED, you can send key events with
 * spice_inputs_key_press(). When the #SpiceMainChannel is available,
 * you can start sharing the clipboard...  .
 *
 *
 * Once #SpiceSession properties set, you can call
 * spice_session_connect() to start connecting and communicating with
 * a Spice server.
 */

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_SESSION_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_SESSION, SpiceSessionPrivate))

G_DEFINE_TYPE (SpiceSession, spice_session, G_TYPE_OBJECT);

/* Properties */
enum {
    PROP_0,
    PROP_HOST,
    PROP_PORT,
    PROP_TLS_PORT,
    PROP_PASSWORD,
    PROP_CA_FILE,
    PROP_CIPHERS,
    PROP_IPV4,
    PROP_IPV6,
    PROP_PROTOCOL,
    PROP_URI,
    PROP_CLIENT_SOCKETS,
    PROP_PUBKEY,
    PROP_CERT_SUBJECT,
    PROP_VERIFY,
    PROP_MIGRATION_STATE,
    PROP_AUDIO,
    PROP_SMARTCARD,
    PROP_SMARTCARD_CERTIFICATES,
    PROP_SMARTCARD_DB,
    PROP_USBREDIR,
    PROP_INHIBIT_KEYBOARD_GRAB,
    PROP_DISABLE_EFFECTS,
    PROP_COLOR_DEPTH,
    PROP_READ_ONLY,
    PROP_CACHE_SIZE,
    PROP_GLZ_WINDOW_SIZE,
    PROP_UUID,
    PROP_NAME,
    PROP_CA,
    PROP_PROXY,
    PROP_SECURE_CHANNELS,
    PROP_SHARED_DIR,
    PROP_SHARE_DIR_RO,
    PROP_USERNAME,
    PROP_UNIX_PATH,
};

/* signals */
enum {
    SPICE_SESSION_CHANNEL_NEW,
    SPICE_SESSION_CHANNEL_DESTROY,
    SPICE_SESSION_MM_TIME_RESET,
    SPICE_SESSION_LAST_SIGNAL,
};

static guint signals[SPICE_SESSION_LAST_SIGNAL];

static void spice_session_channel_destroy(SpiceSession *session, SpiceChannel *channel);

static void update_proxy(SpiceSession *self, const gchar *str)
{
    SpiceSessionPrivate *s = self->priv;
    SpiceURI *proxy = NULL;
    GError *error = NULL;

    if (str == NULL)
        str = g_getenv("SPICE_PROXY");
    if (str == NULL || *str == 0) {
        g_clear_object(&s->proxy);
        return;
    }

    proxy = spice_uri_new();
    if (!spice_uri_parse(proxy, str, &error))
        g_clear_object(&proxy);
    if (error) {
        g_warning("%s", error->message);
        g_clear_error(&error);
    }

    if (proxy != NULL) {
        g_clear_object(&s->proxy);
        s->proxy = proxy;
    }
}

static void spice_session_init(SpiceSession *session)
{
    SpiceSessionPrivate *s;
    gchar *channels;

    SPICE_DEBUG("New session (compiled from package " PACKAGE_STRING ")");
    s = session->priv = SPICE_SESSION_GET_PRIVATE(session);

    channels = spice_channel_supported_string();
    SPICE_DEBUG("Supported channels: %s", channels);
    g_free(channels);

    ring_init(&s->channels);
    s->images = cache_new((GDestroyNotify)pixman_image_unref);
    s->glz_window = glz_decoder_window_new();
    update_proxy(session, NULL);
}

static void
session_disconnect(SpiceSession *self, gboolean keep_main)
{
    SpiceSessionPrivate *s;
    struct channel *item;
    RingItem *ring, *next;

    s = self->priv;

    for (ring = ring_get_head(&s->channels); ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        item = SPICE_CONTAINEROF(ring, struct channel, link);

        if (keep_main && item->channel == s->cmain) {
            spice_channel_disconnect(item->channel, SPICE_CHANNEL_NONE);
        } else {
            spice_session_channel_destroy(self, item->channel);
        }
    }

    s->connection_id = 0;

    g_free(s->name);
    s->name = NULL;
    memset(s->uuid, 0, sizeof(s->uuid));

    spice_session_abort_migration(self);
}

static void
spice_session_dispose(GObject *gobject)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = session->priv;

    SPICE_DEBUG("session dispose");

    session_disconnect(session, FALSE);

    g_warn_if_fail(s->migration == NULL);
    g_warn_if_fail(s->migration_left == NULL);
    g_warn_if_fail(s->after_main_init == 0);
    g_warn_if_fail(s->disconnecting == 0);

    g_clear_object(&s->audio_manager);
    g_clear_object(&s->usb_manager);
    g_clear_object(&s->proxy);
    g_clear_object(&s->webdav);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_session_parent_class)->dispose)
        G_OBJECT_CLASS(spice_session_parent_class)->dispose(gobject);
}

static void
spice_session_finalize(GObject *gobject)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = session->priv;

    /* release stuff */
    g_free(s->unix_path);
    g_free(s->host);
    g_free(s->port);
    g_free(s->tls_port);
    g_free(s->username);
    g_free(s->password);
    g_free(s->ca_file);
    g_free(s->ciphers);
    g_free(s->cert_subject);
    g_strfreev(s->smartcard_certificates);
    g_free(s->smartcard_db);
    g_strfreev(s->disable_effects);
    g_strfreev(s->secure_channels);
    g_free(s->shared_dir);

    g_clear_pointer(&s->images, cache_unref);
    glz_decoder_window_destroy(s->glz_window);

    g_clear_pointer(&s->pubkey, g_byte_array_unref);
    g_clear_pointer(&s->ca, g_byte_array_unref);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_session_parent_class)->finalize)
        G_OBJECT_CLASS(spice_session_parent_class)->finalize(gobject);
}

#define URI_SCHEME_SPICE "spice://"
#define URI_SCHEME_SPICE_UNIX "spice+unix://"
#define URI_QUERY_START ";?"
#define URI_QUERY_SEP   ";&"

static gchar* spice_uri_create(SpiceSession *session)
{
    SpiceSessionPrivate *s = session->priv;

    if (s->unix_path != NULL) {
        return g_strdup_printf(URI_SCHEME_SPICE_UNIX "%s", s->unix_path);
    } else if (s->host != NULL) {
        g_return_val_if_fail(s->port != NULL || s->tls_port != NULL, NULL);

        GString *str = g_string_new(URI_SCHEME_SPICE);

        g_string_append(str, s->host);
        g_string_append(str, "?");
        if (s->port != NULL) {
            g_string_append_printf(str, "port=%s&", s->port);
        }
        if (s->tls_port != NULL) {
            g_string_append_printf(str, "tls-port=%s", s->tls_port);
        }
        return g_string_free(str, FALSE);
    }

    g_return_val_if_reached(NULL);
}

static int spice_parse_uri(SpiceSession *session, const char *original_uri)
{
    SpiceSessionPrivate *s = session->priv;
    gchar *host = NULL, *port = NULL, *tls_port = NULL, *uri = NULL, *username = NULL, *password = NULL;
    gchar *path = NULL;
    gchar *unescaped_path = NULL;
    gchar *authority = NULL;
    gchar *query = NULL;
    gchar *tmp = NULL;

    g_return_val_if_fail(original_uri != NULL, -1);

    uri = g_strdup(original_uri);

    if (g_str_has_prefix(uri, URI_SCHEME_SPICE_UNIX)) {
        path = uri + strlen(URI_SCHEME_SPICE_UNIX);
        goto end;
    }

    /* Break up the URI into its various parts, scheme, authority,
     * path (ignored) and query
     */
    if (!g_str_has_prefix(uri, URI_SCHEME_SPICE)) {
        g_warning("Expected a URI scheme of '%s' in URI '%s'",
                  URI_SCHEME_SPICE, uri);
        goto fail;
    }
    authority = uri + strlen(URI_SCHEME_SPICE);

    tmp = strchr(authority, '@');
    if (tmp) {
        tmp[0] = '\0';
        username = g_uri_unescape_string(authority, NULL);
        authority = ++tmp;
        tmp = NULL;
    }

    path = strchr(authority, '/');
    if (path) {
        path[0] = '\0';
        path++;
    }

    if (path) {
        size_t prefix = strcspn(path, URI_QUERY_START);
        query = path + prefix;
    } else {
        size_t prefix = strcspn(authority, URI_QUERY_START);
        query = authority + prefix;
    }

    if (query && query[0]) {
        query[0] = '\0';
        query++;
    }

    /* Now process the individual parts */

    if (authority[0] == '[') {
        tmp = strchr(authority, ']');
        if (!tmp) {
            g_warning("Missing closing ']' in authority for URI '%s'", uri);
            goto fail;
        }
        tmp[0] = '\0';
        tmp++;
        host = g_strdup(authority + 1);
        if (tmp[0] == ':')
            port = g_strdup(tmp + 1);
    } else {
        tmp = strchr(authority, ':');
        if (tmp) {
            *tmp = '\0';
            tmp++;
            port = g_strdup(tmp);
        }
        host = g_uri_unescape_string(authority, NULL);
    }

    if (path && !(g_str_equal(path, "") ||
                  g_str_equal(path, "/"))) {
        g_warning("Unexpected path data '%s' for URI '%s'", path, uri);
        /* don't fail, just ignore */
    }
    unescaped_path = g_uri_unescape_string(path, NULL);
    path = NULL;

    while (query && query[0] != '\0') {
        gchar key[32], value[128];
        gchar **target_key;

        int len;
        if (sscanf(query, "%31[-a-zA-Z0-9]=%n", key, &len) != 1) {
            spice_warning("Failed to parse key in URI '%s'", query);
            goto fail;
        }

        query += len;
        if (*query == '\0') {
            spice_warning ("key '%s' without value", key);
            break;
        } else if (*query == ';' || *query == '&') {
            /* another argument */
            query++;
            continue;
        }

        if (sscanf(query, "%127[^;&]%n", value, &len) != 1) {
            spice_warning("Failed to parse value of key '%s' in URI '%s'", key, query);
            goto fail;
        }

        query += len;
        if (*query)
            query++;

        target_key = NULL;
        if (g_str_equal(key, "port")) {
            target_key = &port;
        } else if (g_str_equal(key, "tls-port")) {
            target_key = &tls_port;
        } else if (g_str_equal(key, "password")) {
            target_key = &password;
            g_warning("password may be visible in process listings");
        } else {
            g_warning("unknown key in spice URI parsing: '%s'", key);
            goto fail;
        }
        if (target_key) {
            if (*target_key) {
                g_warning("Double set of '%s' in URI '%s'", key, uri);
                goto fail;
            }
            *target_key = g_uri_unescape_string(value, NULL);
        }
    }

    if (port == NULL && tls_port == NULL) {
        g_warning("Missing port or tls-port in spice URI '%s'", uri);
        goto fail;
    }

end:
    /* parsed ok -> apply */
    g_free(uri);
    g_free(unescaped_path);
    g_free(s->unix_path);
    g_free(s->host);
    g_free(s->port);
    g_free(s->tls_port);
    g_free(s->username);
    g_free(s->password);
    s->unix_path = g_strdup(path);
    s->host = host;
    s->port = port;
    s->tls_port = tls_port;
    s->username = username;
    s->password = password;
    return 0;

fail:
    g_free(uri);
    g_free(unescaped_path);
    g_free(host);
    g_free(port);
    g_free(tls_port);
    g_free(username);
    g_free(password);
    return -1;
}

static void spice_session_get_property(GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = session->priv;

    switch (prop_id) {
    case PROP_HOST:
        g_value_set_string(value, s->host);
	break;
    case PROP_UNIX_PATH:
        g_value_set_string(value, s->unix_path);
        break;
    case PROP_PORT:
        g_value_set_string(value, s->port);
	break;
    case PROP_TLS_PORT:
        g_value_set_string(value, s->tls_port);
	break;
    case PROP_USERNAME:
        g_value_set_string(value, s->username);
	break;
    case PROP_PASSWORD:
        g_value_set_string(value, s->password);
	break;
    case PROP_CA_FILE:
        g_value_set_string(value, s->ca_file);
	break;
    case PROP_CIPHERS:
        g_value_set_string(value, s->ciphers);
	break;
    case PROP_PROTOCOL:
        g_value_set_int(value, s->protocol);
	break;
    case PROP_URI:
        g_value_take_string(value, spice_uri_create(session));
        break;
    case PROP_CLIENT_SOCKETS:
        g_value_set_boolean(value, s->client_provided_sockets);
	break;
    case PROP_PUBKEY:
        g_value_set_boxed(value, s->pubkey);
	break;
    case PROP_CA:
        g_value_set_boxed(value, s->ca);
	break;
    case PROP_CERT_SUBJECT:
        g_value_set_string(value, s->cert_subject);
	break;
    case PROP_VERIFY:
        g_value_set_flags(value, s->verify);
        break;
    case PROP_MIGRATION_STATE:
        g_value_set_enum(value, s->migration_state);
        break;
    case PROP_SMARTCARD:
        g_value_set_boolean(value, s->smartcard);
        break;
    case PROP_SMARTCARD_CERTIFICATES:
        g_value_set_boxed(value, s->smartcard_certificates);
        break;
    case PROP_SMARTCARD_DB:
        g_value_set_string(value, s->smartcard_db);
        break;
    case PROP_USBREDIR:
        g_value_set_boolean(value, s->usbredir);
        break;
    case PROP_INHIBIT_KEYBOARD_GRAB:
        g_value_set_boolean(value, s->inhibit_keyboard_grab);
        break;
    case PROP_DISABLE_EFFECTS:
        g_value_set_boxed(value, s->disable_effects);
        break;
    case PROP_SECURE_CHANNELS:
        g_value_set_boxed(value, s->secure_channels);
        break;
    case PROP_COLOR_DEPTH:
        g_value_set_int(value, s->color_depth);
        break;
    case PROP_AUDIO:
        g_value_set_boolean(value, s->audio);
        break;
    case PROP_READ_ONLY:
        g_value_set_boolean(value, s->read_only);
        break;
    case PROP_CACHE_SIZE:
        g_value_set_int(value, s->images_cache_size);
        break;
    case PROP_GLZ_WINDOW_SIZE:
        g_value_set_int(value, s->glz_window_size);
        break;
    case PROP_NAME:
        g_value_set_string(value, s->name);
	break;
    case PROP_UUID:
        g_value_set_pointer(value, s->uuid);
	break;
    case PROP_PROXY:
        g_value_take_string(value, spice_uri_to_string(s->proxy));
	break;
    case PROP_SHARED_DIR:
        g_value_set_string(value, spice_session_get_shared_dir(session));
        break;
    case PROP_SHARE_DIR_RO:
        g_value_set_boolean(value, s->share_dir_ro);
        break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
	break;
    }
}

static void spice_session_set_property(GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = session->priv;
    const char *str;

    switch (prop_id) {
    case PROP_HOST:
        g_free(s->host);
        s->host = g_value_dup_string(value);
        break;
    case PROP_UNIX_PATH:
        g_free(s->unix_path);
        s->unix_path = g_value_dup_string(value);
        break;
    case PROP_PORT:
        g_free(s->port);
        s->port = g_value_dup_string(value);
        break;
    case PROP_TLS_PORT:
        g_free(s->tls_port);
        s->tls_port = g_value_dup_string(value);
        break;
    case PROP_USERNAME:
        g_free(s->username);
        s->username = g_value_dup_string(value);
        break;
    case PROP_PASSWORD:
        g_free(s->password);
        s->password = g_value_dup_string(value);
        break;
    case PROP_CA_FILE:
        g_free(s->ca_file);
        s->ca_file = g_value_dup_string(value);
        break;
    case PROP_CIPHERS:
        g_free(s->ciphers);
        s->ciphers = g_value_dup_string(value);
        break;
    case PROP_PROTOCOL:
        s->protocol = g_value_get_int(value);
        break;
    case PROP_URI:
        str = g_value_get_string(value);
        if (str != NULL)
            spice_parse_uri(session, str);
        break;
    case PROP_CLIENT_SOCKETS:
        s->client_provided_sockets = g_value_get_boolean(value);
        break;
    case PROP_PUBKEY:
        if (s->pubkey)
            g_byte_array_unref(s->pubkey);
        s->pubkey = g_value_dup_boxed(value);
        if (s->pubkey)
            s->verify |= SPICE_SESSION_VERIFY_PUBKEY;
        else
            s->verify &= ~SPICE_SESSION_VERIFY_PUBKEY;
	break;
    case PROP_CERT_SUBJECT:
        g_free(s->cert_subject);
        s->cert_subject = g_value_dup_string(value);
        if (s->cert_subject)
            s->verify |= SPICE_SESSION_VERIFY_SUBJECT;
        else
            s->verify &= ~SPICE_SESSION_VERIFY_SUBJECT;
        break;
    case PROP_VERIFY:
        s->verify = g_value_get_flags(value);
        break;
    case PROP_MIGRATION_STATE:
        s->migration_state = g_value_get_enum(value);
        break;
    case PROP_SMARTCARD:
        s->smartcard = g_value_get_boolean(value);
        break;
    case PROP_SMARTCARD_CERTIFICATES:
        g_strfreev(s->smartcard_certificates);
        s->smartcard_certificates = g_value_dup_boxed(value);
        break;
    case PROP_SMARTCARD_DB:
        g_free(s->smartcard_db);
        s->smartcard_db = g_value_dup_string(value);
        break;
    case PROP_USBREDIR:
        s->usbredir = g_value_get_boolean(value);
        break;
    case PROP_INHIBIT_KEYBOARD_GRAB:
        s->inhibit_keyboard_grab = g_value_get_boolean(value);
        break;
    case PROP_DISABLE_EFFECTS:
        g_strfreev(s->disable_effects);
        s->disable_effects = g_value_dup_boxed(value);
        break;
    case PROP_SECURE_CHANNELS:
        g_strfreev(s->secure_channels);
        s->secure_channels = g_value_dup_boxed(value);
        break;
    case PROP_COLOR_DEPTH:
        s->color_depth = g_value_get_int(value);
        break;
    case PROP_AUDIO:
        s->audio = g_value_get_boolean(value);
        break;
    case PROP_READ_ONLY:
        s->read_only = g_value_get_boolean(value);
        g_coroutine_object_notify(gobject, "read-only");
        break;
    case PROP_CACHE_SIZE:
        s->images_cache_size = g_value_get_int(value);
        break;
    case PROP_GLZ_WINDOW_SIZE:
        s->glz_window_size = g_value_get_int(value);
        break;
    case PROP_CA:
        g_clear_pointer(&s->ca, g_byte_array_unref);
        s->ca = g_value_dup_boxed(value);
        break;
    case PROP_PROXY:
        update_proxy(session, g_value_get_string(value));
        break;
    case PROP_SHARED_DIR:
        spice_session_set_shared_dir(session, g_value_get_string(value));
        break;
    case PROP_SHARE_DIR_RO:
        s->share_dir_ro = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_session_class_init(SpiceSessionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    _wocky_http_proxy_get_type();
    _wocky_https_proxy_get_type();

    gobject_class->dispose      = spice_session_dispose;
    gobject_class->finalize     = spice_session_finalize;
    gobject_class->get_property = spice_session_get_property;
    gobject_class->set_property = spice_session_set_property;

    /**
     * SpiceSession:host:
     *
     * URL of the SPICE host to connect to
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_HOST,
         g_param_spec_string("host",
                             "Host",
                             "Remote host",
                             "localhost",
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:unix-path:
     *
     * Path of the Unix socket to connect to
     *
     * Since: 0.28
     **/
    g_object_class_install_property
        (gobject_class, PROP_UNIX_PATH,
         g_param_spec_string("unix-path",
                             "Unix path",
                             "Unix path",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:port:
     *
     * Port to connect to for unencrypted sessions
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_PORT,
         g_param_spec_string("port",
                             "Port",
                             "Remote port (plaintext)",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:tls-port:
     *
     * Port to connect to for TLS sessions
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_TLS_PORT,
         g_param_spec_string("tls-port",
                             "TLS port",
                             "Remote port (encrypted)",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:username:
     *
     * Username to use
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_USERNAME,
         g_param_spec_string("username",
                             "Username",
                             "Username used for SASL connections",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:password:
     *
     * TLS password to use
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_PASSWORD,
         g_param_spec_string("password",
                             "Password",
                             "",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:ca-file:
     *
     * File holding the CA certificates for the host the client is
     * connecting to
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_CA_FILE,
         g_param_spec_string("ca-file",
                             "CA file",
                             "File holding the CA certificates",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:ciphers:
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_CIPHERS,
         g_param_spec_string("ciphers",
                             "Ciphers",
                             "SSL cipher list",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:protocol:
     *
     * Version of the SPICE protocol to use
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_PROTOCOL,
         g_param_spec_int("protocol",
                          "Protocol",
                          "Spice protocol major version",
                          1, 2, 2,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:uri:
     *
     * URI of the SPICE host to connect to. The URI is of the form
     * spice://hostname?port=XXX or spice://hostname?tls_port=XXX
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_URI,
         g_param_spec_string("uri",
                             "URI",
                             "Spice connection URI",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:client-sockets:
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_CLIENT_SOCKETS,
         g_param_spec_boolean("client-sockets",
                          "Client sockets",
                          "Sockets are provided by the client",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:pubkey:
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_PUBKEY,
         g_param_spec_boxed("pubkey",
                            "Pub Key",
                            "Public key to check",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:cert-subject:
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_CERT_SUBJECT,
         g_param_spec_string("cert-subject",
                             "Cert Subject",
                             "Certificate subject to check",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:verify:
     *
     * #SpiceSessionVerify bit field indicating which parts of the peer
     * certificate should be checked
     **/
    g_object_class_install_property
        (gobject_class, PROP_VERIFY,
         g_param_spec_flags("verify",
                            "Verify",
                            "Certificate verification parameters",
                            SPICE_TYPE_SESSION_VERIFY,
                            SPICE_SESSION_VERIFY_HOSTNAME,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT |
                            G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:migration-state:
     *
     * #SpiceSessionMigration bit field indicating if a migration is in
     * progress
     *
     **/
    g_object_class_install_property
        (gobject_class, PROP_MIGRATION_STATE,
         g_param_spec_enum("migration-state",
                           "Migration state",
                           "Migration state",
                           SPICE_TYPE_SESSION_MIGRATION,
                           SPICE_SESSION_MIGRATION_NONE,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:disable-effects:
     *
     * A string array of effects to disable. The settings will
     * be applied on new display channels. The following effets can be
     * disabled "wallpaper", "font-smooth", "animation", and "all",
     * which will disable all the effects. If NULL, don't apply changes.
     *
     * Since: 0.7
     **/
    g_object_class_install_property
        (gobject_class, PROP_DISABLE_EFFECTS,
         g_param_spec_boxed ("disable-effects",
                             "Disable effects",
                             "Comma-separated effects to disable",
                             G_TYPE_STRV,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:color-depth:
     *
     * Display color depth to set on new display channels. If 0, don't set.
     *
     * Since: 0.7
     **/
    g_object_class_install_property
        (gobject_class, PROP_COLOR_DEPTH,
         g_param_spec_int("color-depth",
                          "Color depth",
                          "Display channel color depth",
                          0, 32, 0,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:enable-smartcard:
     *
     * If set to TRUE, the smartcard channel will be enabled and smartcard
     * events will be forwarded to the guest
     *
     * Since: 0.7
     **/
    g_object_class_install_property
        (gobject_class, PROP_SMARTCARD,
         g_param_spec_boolean("enable-smartcard",
                          "Enable smartcard event forwarding",
                          "Forward smartcard events to the SPICE server",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:enable-audio:
     *
     * If set to TRUE, the audio channels will be enabled for
     * playback and recording.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_AUDIO,
         g_param_spec_boolean("enable-audio",
                          "Enable audio channels",
                          "Enable audio channels",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:smartcard-certificates:
     *
     * This property is used when one wants to simulate a smartcard with no
     * hardware smartcard reader. If it's set to a NULL-terminated string
     * array containing the names of 3 valid certificates, these will be
     * used to simulate a smartcard in the guest
     * See also spice_smartcard_manager_insert_card()
     *
     * Since: 0.7
     **/
    g_object_class_install_property
        (gobject_class, PROP_SMARTCARD_CERTIFICATES,
         g_param_spec_boxed("smartcard-certificates",
                            "Smartcard certificates",
                            "Smartcard certificates for software-based smartcards",
                            G_TYPE_STRV,
                            G_PARAM_READABLE |
                            G_PARAM_WRITABLE |
                            G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:smartcard-db:
     *
     * Path to the NSS certificate database containing the certificates to
     * use to simulate a software smartcard
     *
     * Since: 0.7
     **/
    g_object_class_install_property
        (gobject_class, PROP_SMARTCARD_DB,
         g_param_spec_string("smartcard-db",
                              "Smartcard certificate database",
                              "Path to the database for smartcard certificates",
                              NULL,
                              G_PARAM_READABLE |
                              G_PARAM_WRITABLE |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:enable-usbredir:
     *
     * If set to TRUE, the usbredir channel will be enabled and USB devices
     * can be redirected to the guest
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_USBREDIR,
         g_param_spec_boolean("enable-usbredir",
                          "Enable USB device redirection",
                          "Forward USB devices to the SPICE server",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession::inhibit-keyboard-grab:
     *
     * This boolean is set by the usbredir channel to indicate to #SpiceDisplay
     * that the keyboard grab should be temporarily released, because it is
     * going to invoke policykit. It will get reset when the usbredir channel
     * is done with polickit.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_INHIBIT_KEYBOARD_GRAB,
         g_param_spec_boolean("inhibit-keyboard-grab",
                        "Inhibit Keyboard Grab",
                        "Request that SpiceDisplays don't grab the keyboard",
                        FALSE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:ca:
     *
     * CA certificates in PEM format. The text data can contain
     * several CA certificates identified by:
     *
     *  -----BEGIN CERTIFICATE-----
     *  ... (CA certificate in base64 encoding) ...
     *  -----END CERTIFICATE-----
     *
     * Since: 0.15
     **/
    g_object_class_install_property
        (gobject_class, PROP_CA,
         g_param_spec_boxed("ca",
                            "CA",
                            "The CA certificates data",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:secure-channels:
     *
     * A string array of channel types to be secured.
     *
     * Since: 0.20
     **/
    g_object_class_install_property
        (gobject_class, PROP_SECURE_CHANNELS,
         g_param_spec_boxed ("secure-channels",
                             "Secure channels",
                             "Array of channel type to secure",
                             G_TYPE_STRV,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));


    /**
     * SpiceSession::channel-new:
     * @session: the session that emitted the signal
     * @channel: the new #SpiceChannel
     *
     * The #SpiceSession::channel-new signal is emitted each time a #SpiceChannel is created.
     **/
    signals[SPICE_SESSION_CHANNEL_NEW] =
        g_signal_new("channel-new",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSessionClass, channel_new),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_CHANNEL);

    /**
     * SpiceSession::channel-destroy:
     * @session: the session that emitted the signal
     * @channel: the destroyed #SpiceChannel
     *
     * The #SpiceSession::channel-destroy signal is emitted each time a #SpiceChannel is destroyed.
     **/
    signals[SPICE_SESSION_CHANNEL_DESTROY] =
        g_signal_new("channel-destroy",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSessionClass, channel_destroy),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_CHANNEL);

    /**
     * SpiceSession::mm-time-reset:
     * @session: the session that emitted the signal
     *
     * The #SpiceSession::mm-time-reset is emitted when we identify discontinuity in mm-time
     *
     * Since 0.20
     **/
    signals[SPICE_SESSION_MM_TIME_RESET] =
        g_signal_new("mm-time-reset",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    /**
     * SpiceSession:read-only:
     *
     * Whether this connection is read-only mode.
     *
     * Since: 0.8
     **/
    g_object_class_install_property
        (gobject_class, PROP_READ_ONLY,
         g_param_spec_boolean("read-only", "Read-only",
                              "Whether this connection is read-only mode",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:cache-size:
     *
     * Images cache size. If 0, don't set.
     *
     * Since: 0.9
     **/
    g_object_class_install_property
        (gobject_class, PROP_CACHE_SIZE,
         g_param_spec_int("cache-size",
                          "Cache size",
                          "Images cache size (bytes)",
                          0, G_MAXINT, 0,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:glz-window-size:
     *
     * Glz window size. If 0, don't set.
     *
     * Since: 0.9
     **/
    g_object_class_install_property
        (gobject_class, PROP_GLZ_WINDOW_SIZE,
         g_param_spec_int("glz-window-size",
                          "Glz window size",
                          "Glz window size (bytes)",
                          0, LZ_MAX_WINDOW_SIZE * 4, 0,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:name:
     *
     * Spice server name.
     *
     * Since: 0.11
     **/
    g_object_class_install_property
        (gobject_class, PROP_NAME,
         g_param_spec_string("name",
                             "Name",
                             "Spice server name",
                             NULL,
                             G_PARAM_READABLE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:uuid:
     *
     * Spice server uuid.
     *
     * Since: 0.11
     **/
    g_object_class_install_property
        (gobject_class, PROP_UUID,
         g_param_spec_pointer("uuid",
                              "UUID",
                              "Spice server uuid",
                              G_PARAM_READABLE |
                              G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:proxy:
     *
     * URI to the proxy server to use when doing network connection.
     * of the form <![CDATA[ [protocol://]<host>[:port] ]]>
     *
     * Since: 0.17
     **/
    g_object_class_install_property
        (gobject_class, PROP_PROXY,
         g_param_spec_string("proxy",
                             "Proxy",
                             "The proxy server",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:shared-dir:
     *
     * Location of the shared directory
     *
     * Since: 0.24
     **/
    g_object_class_install_property
        (gobject_class, PROP_SHARED_DIR,
         g_param_spec_string("shared-dir",
                             "Shared directory",
                             "Shared directory",
                             g_get_user_special_dir(G_USER_DIRECTORY_PUBLIC_SHARE),
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT |
                             G_PARAM_STATIC_STRINGS));

    /**
     * SpiceSession:share-dir-ro:
     *
     * Whether to share the directory read-only.
     *
     * Since: 0.28
     **/
    g_object_class_install_property
        (gobject_class, PROP_SHARE_DIR_RO,
         g_param_spec_boolean("share-dir-ro",
                              "Share directory read-only",
                              "Share directory read-only",
                              FALSE,
                              G_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT |
                              G_PARAM_STATIC_STRINGS));

    g_type_class_add_private(klass, sizeof(SpiceSessionPrivate));
}

/* ------------------------------------------------------------------ */
/* public functions                                                   */

/**
 * spice_session_new:
 *
 * Creates a new Spice session.
 *
 * Returns: a new #SpiceSession
 **/
SpiceSession *spice_session_new(void)
{
    return SPICE_SESSION(g_object_new(SPICE_TYPE_SESSION, NULL));
}

G_GNUC_INTERNAL
SpiceSession *spice_session_new_from_session(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;
    SpiceSession *copy;
    SpiceSessionPrivate *c;

    if (s->client_provided_sockets) {
        g_warning("migration with client provided fd is not supported yet");
        return NULL;
    }

    copy = SPICE_SESSION(g_object_new(SPICE_TYPE_SESSION,
                                      "host", NULL,
                                      "ca-file", NULL,
                                      NULL));
    c = copy->priv;
    g_clear_object(&c->proxy);

    g_warn_if_fail(c->host == NULL);
    g_warn_if_fail(c->unix_path == NULL);
    g_warn_if_fail(c->tls_port == NULL);
    g_warn_if_fail(c->username == NULL);
    g_warn_if_fail(c->password == NULL);
    g_warn_if_fail(c->ca_file == NULL);
    g_warn_if_fail(c->ciphers == NULL);
    g_warn_if_fail(c->cert_subject == NULL);
    g_warn_if_fail(c->pubkey == NULL);
    g_warn_if_fail(c->pubkey == NULL);
    g_warn_if_fail(c->proxy == NULL);

    g_object_get(session,
                 "host", &c->host,
                 "unix-path", &c->unix_path,
                 "tls-port", &c->tls_port,
                 "username", &c->username,
                 "password", &c->password,
                 "ca-file", &c->ca_file,
                 "ciphers", &c->ciphers,
                 "cert-subject", &c->cert_subject,
                 "pubkey", &c->pubkey,
                 "verify", &c->verify,
                 "smartcard-certificates", &c->smartcard_certificates,
                 "smartcard-db", &c->smartcard_db,
                 "enable-smartcard", &c->smartcard,
                 "enable-audio", &c->audio,
                 "enable-usbredir", &c->usbredir,
                 "ca", &c->ca,
                 NULL);

    c->client_provided_sockets = s->client_provided_sockets;
    c->protocol = s->protocol;
    c->connection_id = s->connection_id;
    if (s->proxy)
        c->proxy = g_object_ref(s->proxy);

    return copy;
}

/**
 * spice_session_connect:
 * @session:
 *
 * Open the session using the #SpiceSession:host and
 * #SpiceSession:port.
 *
 * Returns: %FALSE if the connection failed.
 **/
gboolean spice_session_connect(SpiceSession *session)
{
    SpiceSessionPrivate *s;

    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    s = session->priv;
    g_return_val_if_fail(!s->disconnecting, FALSE);

    session_disconnect(session, TRUE);

    s->client_provided_sockets = FALSE;

    if (s->cmain == NULL)
        s->cmain = spice_channel_new(session, SPICE_CHANNEL_MAIN, 0);

    glz_decoder_window_clear(s->glz_window);
    return spice_channel_connect(s->cmain);
}

/**
 * spice_session_open_fd:
 * @session:
 * @fd: a file descriptor (socket) or -1
 *
 * Open the session using the provided @fd socket file
 * descriptor. This is useful if you create the fd yourself, for
 * example to setup a SSH tunnel.
 *
 * Note however that additional sockets will be needed by all the channels
 * created for @session so users of this API should hook into
 * SpiceChannel::open-fd signal for each channel they are interested in, and
 * create and pass a new socket to the channel using #spice_channel_open_fd, in
 * the signal callback.
 *
 * If @fd is -1, a valid fd will be requested later via the
 * SpiceChannel::open-fd signal. Typically, you would want to just pass -1 as
 * @fd this call since you will have to hook to SpiceChannel::open-fd signal
 * anyway.
 *
 * Returns:
 **/
gboolean spice_session_open_fd(SpiceSession *session, int fd)
{
    SpiceSessionPrivate *s;

    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);
    g_return_val_if_fail(fd >= -1, FALSE);

    s = session->priv;
    g_return_val_if_fail(!s->disconnecting, FALSE);

    session_disconnect(session, TRUE);

    s->client_provided_sockets = TRUE;

    if (s->cmain == NULL)
        s->cmain = spice_channel_new(session, SPICE_CHANNEL_MAIN, 0);

    glz_decoder_window_clear(s->glz_window);
    return spice_channel_open_fd(s->cmain, fd);
}

G_GNUC_INTERNAL
gboolean spice_session_get_client_provided_socket(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    SpiceSessionPrivate *s = session->priv;

    return s->client_provided_sockets;
}

static void cache_clear_all(SpiceSession *self)
{
    SpiceSessionPrivate *s = self->priv;

    cache_clear(s->images);
    glz_decoder_window_clear(s->glz_window);
}

G_GNUC_INTERNAL
void spice_session_switching_disconnect(SpiceSession *self)
{
    g_return_if_fail(SPICE_IS_SESSION(self));

    SpiceSessionPrivate *s = self->priv;
    struct channel *item;
    RingItem *ring, *next;

    g_return_if_fail(s->cmain != NULL);

    /* disconnect/destroy all but main channel */

    for (ring = ring_get_head(&s->channels); ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        item = SPICE_CONTAINEROF(ring, struct channel, link);

        if (item->channel == s->cmain)
            continue;
        spice_session_channel_destroy(self, item->channel);
    }

    g_warn_if_fail(!ring_is_empty(&s->channels)); /* ring_get_length() == 1 */

    cache_clear_all(self);
    s->connection_id = 0;
}

#define SWAP_STR(x, y) G_STMT_START { \
    const gchar *tmp;                 \
    const gchar *a = x;               \
    const gchar *b = y;               \
    tmp = a;                          \
    a = b;                            \
    b = tmp;                          \
} G_STMT_END

G_GNUC_INTERNAL
void spice_session_start_migrating(SpiceSession *session,
                                   gboolean full_migration)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;
    SpiceSessionPrivate *m;

    g_return_if_fail(s->migration != NULL);
    m = s->migration->priv;
    g_return_if_fail(m->migration_state == SPICE_SESSION_MIGRATION_CONNECTING);


    s->full_migration = full_migration;
    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_MIGRATING);

    /* swapping connection details happens after MIGRATION_CONNECTING state */
    SWAP_STR(s->host, m->host);
    SWAP_STR(s->port, m->port);
    SWAP_STR(s->tls_port, m->tls_port);
    SWAP_STR(s->unix_path, m->unix_path);

    g_warn_if_fail(ring_get_length(&s->channels) == ring_get_length(&m->channels));

    SPICE_DEBUG("migration channels left:%d (in migration:%d)",
                ring_get_length(&s->channels), ring_get_length(&m->channels));
    s->migration_left = spice_session_get_channels(session);
}
#undef SWAP_STR

G_GNUC_INTERNAL
SpiceChannel* spice_session_lookup_channel(SpiceSession *session, gint id, gint type)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    RingItem *ring, *next;
    SpiceSessionPrivate *s = session->priv;
    struct channel *c;

    for (ring = ring_get_head(&s->channels);
         ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        c = SPICE_CONTAINEROF(ring, struct channel, link);
        if (c == NULL || c->channel == NULL) {
            g_warn_if_reached();
            continue;
        }

        if (id == spice_channel_get_channel_id(c->channel) &&
            type == spice_channel_get_channel_type(c->channel))
            break;
    }
    g_return_val_if_fail(ring != NULL, NULL);

    return c->channel;
}

G_GNUC_INTERNAL
void spice_session_abort_migration(SpiceSession *session)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;
    RingItem *ring, *next;
    struct channel *c;

    if (s->migration == NULL) {
        SPICE_DEBUG("no migration in progress");
        return;
    }

    SPICE_DEBUG("migration: abort");
    if (s->migration_state != SPICE_SESSION_MIGRATION_MIGRATING)
        goto end;

    for (ring = ring_get_head(&s->channels);
         ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        c = SPICE_CONTAINEROF(ring, struct channel, link);

        if (g_list_find(s->migration_left, c->channel))
            continue;

        spice_channel_swap(c->channel,
            spice_session_lookup_channel(s->migration,
                                         spice_channel_get_channel_id(c->channel),
                                         spice_channel_get_channel_type(c->channel)),
                                         !s->full_migration);
    }

end:
    g_list_free(s->migration_left);
    s->migration_left = NULL;
    session_disconnect(s->migration, FALSE);
    g_object_unref(s->migration);
    s->migration = NULL;

    s->migrate_wait_init = FALSE;
    if (s->after_main_init) {
        g_source_remove(s->after_main_init);
        s->after_main_init = 0;
    }

    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_NONE);
}

G_GNUC_INTERNAL
void spice_session_channel_migrate(SpiceSession *session, SpiceChannel *channel)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;
    SpiceChannel *c;
    gint id, type;

    g_return_if_fail(s->migration != NULL);
    g_return_if_fail(SPICE_IS_CHANNEL(channel));

    id = spice_channel_get_channel_id(channel);
    type = spice_channel_get_channel_type(channel);
    CHANNEL_DEBUG(channel, "migrating channel id:%d type:%d", id, type);

    c = spice_session_lookup_channel(s->migration, id, type);
    g_return_if_fail(c != NULL);

    if (!g_queue_is_empty(&c->priv->xmit_queue) && s->full_migration) {
        CHANNEL_DEBUG(channel, "mig channel xmit queue is not empty. type %s", c->priv->name);
    }
    spice_channel_swap(channel, c, !s->full_migration);
    s->migration_left = g_list_remove(s->migration_left, channel);

    if (g_list_length(s->migration_left) == 0) {
        CHANNEL_DEBUG(channel, "migration: all channel migrated, success");
        session_disconnect(s->migration, FALSE);
        g_object_unref(s->migration);
        s->migration = NULL;
        spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_NONE);
    }
}

/* main context */
static gboolean after_main_init(gpointer data)
{
    SpiceSession *self = data;
    SpiceSessionPrivate *s = self->priv;
    GList *l;

    for (l = s->migration_left; l != NULL; ) {
        SpiceChannel *channel = l->data;
        l = l->next;

        spice_session_channel_migrate(self, channel);
        channel->priv->state = SPICE_CHANNEL_STATE_READY;
        spice_channel_up(channel);
    }

    s->after_main_init = 0;
    return FALSE;
}

/* coroutine context */
G_GNUC_INTERNAL
gboolean spice_session_migrate_after_main_init(SpiceSession *self)
{
    g_return_val_if_fail(SPICE_IS_SESSION(self), FALSE);

    SpiceSessionPrivate *s = self->priv;

    if (!s->migrate_wait_init)
        return FALSE;

    g_return_val_if_fail(g_list_length(s->migration_left) != 0, FALSE);
    g_return_val_if_fail(s->after_main_init == 0, FALSE);

    s->migrate_wait_init = FALSE;
    s->after_main_init = g_idle_add(after_main_init, self);

    return TRUE;
}

/* main context */
G_GNUC_INTERNAL
void spice_session_migrate_end(SpiceSession *self)
{
    g_return_if_fail(SPICE_IS_SESSION(self));

    SpiceSessionPrivate *s = self->priv;
    SpiceMsgOut *out;
    GList *l;

    g_return_if_fail(s->migration);
    g_return_if_fail(s->migration->priv->cmain);
    g_return_if_fail(g_list_length(s->migration_left) != 0);

    /* disconnect and reset all channels */
    for (l = s->migration_left; l != NULL; ) {
        SpiceChannel *channel = l->data;
        l = l->next;

        if (!SPICE_IS_MAIN_CHANNEL(channel)) {
            /* freeze other channels */
            channel->priv->state = SPICE_CHANNEL_STATE_MIGRATING;
        }

        /* reset for migration, disconnect */
        spice_channel_reset(channel, TRUE);

        if (SPICE_IS_MAIN_CHANNEL(channel)) {
            /* migrate main to target, so we can start talking */
            spice_session_channel_migrate(self, channel);
        }
    }

    cache_clear_all(self);

    /* send MIGRATE_END to target */
    out = spice_msg_out_new(s->cmain, SPICE_MSGC_MAIN_MIGRATE_END);
    spice_msg_out_send(out);

    /* now wait after main init for the rest of channels migration */
    s->migrate_wait_init = TRUE;
}

/**
 * spice_session_get_read_only:
 * @session: a #SpiceSession
 *
 * Returns: wether the @session is in read-only mode.
 **/
gboolean spice_session_get_read_only(SpiceSession *self)
{
    g_return_val_if_fail(SPICE_IS_SESSION(self), FALSE);

    return self->priv->read_only;
}

static gboolean session_disconnect_idle(SpiceSession *self)
{
    SpiceSessionPrivate *s = self->priv;

    session_disconnect(self, FALSE);
    s->disconnecting = 0;

    g_object_unref(self);

    return FALSE;
}

/**
 * spice_session_disconnect:
 * @session:
 *
 * Disconnect the @session, and destroy all channels.
 **/
void spice_session_disconnect(SpiceSession *session)
{
    SpiceSessionPrivate *s;

    g_return_if_fail(SPICE_IS_SESSION(session));

    s = session->priv;

    SPICE_DEBUG("session: disconnecting %d", s->disconnecting);
    if (s->disconnecting != 0)
        return;

    g_object_ref(session);
    s->disconnecting = g_idle_add((GSourceFunc)session_disconnect_idle, session);
}

/**
 * spice_session_get_channels:
 * @session: a #SpiceSession
 *
 * Get the list of current channels associated with this @session.
 *
 * Returns: (element-type SpiceChannel) (transfer container): a #GList
 *          of unowned #SpiceChannel channels.
 **/
GList *spice_session_get_channels(SpiceSession *session)
{
    SpiceSessionPrivate *s;
    struct channel *item;
    GList *list = NULL;
    RingItem *ring;

    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);
    g_return_val_if_fail(session->priv != NULL, NULL);

    s = session->priv;

    for (ring = ring_get_head(&s->channels);
         ring != NULL;
         ring = ring_next(&s->channels, ring)) {
        item = SPICE_CONTAINEROF(ring, struct channel, link);
        list = g_list_append(list, item->channel);
    }
    return list;
}

/**
 * spice_session_has_channel_type:
 * @session: a #SpiceSession
 *
 * See if there is a @type channel in the channels associated with this
 * @session.
 *
 * Returns: TRUE if a @type channel is available otherwise FALSE.
 **/
gboolean spice_session_has_channel_type(SpiceSession *session, gint type)
{
    SpiceSessionPrivate *s;
    struct channel *item;
    RingItem *ring;

    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);
    g_return_val_if_fail(session->priv != NULL, FALSE);

    s = session->priv;

    for (ring = ring_get_head(&s->channels);
         ring != NULL;
         ring = ring_next(&s->channels, ring)) {
        item = SPICE_CONTAINEROF(ring, struct channel, link);
        if (spice_channel_get_channel_type(item->channel) == type) {
            return TRUE;
        }
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* private functions                                                  */

typedef struct spice_open_host spice_open_host;

struct spice_open_host {
    struct coroutine *from;
    SpiceSession *session;
    SpiceChannel *channel;
    SpiceURI *proxy;
    int port;
    GCancellable *cancellable;
    GError *error;
    GSocketConnection *connection;
    GSocketClient *client;
};

static void socket_client_connect_ready(GObject *source_object, GAsyncResult *result,
                                        gpointer data)
{
    GSocketClient *client = G_SOCKET_CLIENT(source_object);
    spice_open_host *open_host = data;
    GSocketConnection *connection = NULL;

    CHANNEL_DEBUG(open_host->channel, "connect ready");
    connection = g_socket_client_connect_finish(client, result, &open_host->error);
    if (connection == NULL) {
        g_warn_if_fail(open_host->error != NULL);
        goto end;
    }

    open_host->connection = connection;

end:
    coroutine_yieldto(open_host->from, NULL);
}

/* main context */
static void open_host_connectable_connect(spice_open_host *open_host, GSocketConnectable *connectable)
{
    CHANNEL_DEBUG(open_host->channel, "connecting %p...", open_host);

    g_socket_client_connect_async(open_host->client, connectable,
                                  open_host->cancellable,
                                  socket_client_connect_ready, open_host);
}

/* main context */
static void proxy_lookup_ready(GObject *source_object, GAsyncResult *result,
                               gpointer data)
{
    spice_open_host *open_host = data;
    SpiceSession *session = open_host->session;
    SpiceSessionPrivate *s = session->priv;
    GList *addresses = NULL, *it;
    GSocketAddress *address;

    SPICE_DEBUG("proxy lookup ready");
    addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(source_object),
                                                 result, &open_host->error);
    if (addresses == NULL || open_host->error) {
        g_prefix_error(&open_host->error, "SPICE proxy: ");
        coroutine_yieldto(open_host->from, NULL);
        return;
    }

    for (it = addresses; it != NULL; it = it->next) {
        address = g_proxy_address_new(G_INET_ADDRESS(it->data),
                                      spice_uri_get_port(open_host->proxy),
                                      spice_uri_get_scheme(open_host->proxy),
                                      s->host, open_host->port,
                                      spice_uri_get_user(open_host->proxy),
                                      spice_uri_get_password(open_host->proxy));
        if (address != NULL)
            break;
    }

    open_host_connectable_connect(open_host, G_SOCKET_CONNECTABLE(address));
    g_resolver_free_addresses(addresses);
    g_object_unref(address);
}

/* main context */
static gboolean open_host_idle_cb(gpointer data)
{
    spice_open_host *open_host = data;
    SpiceSessionPrivate *s;

    g_return_val_if_fail(open_host != NULL, FALSE);
    g_return_val_if_fail(open_host->connection == NULL, FALSE);

    if (spice_channel_get_session(open_host->channel) != open_host->session)
        return FALSE;

    s = open_host->session->priv;
    open_host->proxy = s->proxy;
    if (open_host->error != NULL) {
        coroutine_yieldto(open_host->from, NULL);
        return FALSE;
    }

    if (open_host->proxy) {
        g_resolver_lookup_by_name_async(g_resolver_get_default(),
                                        spice_uri_get_hostname(open_host->proxy),
                                        open_host->cancellable,
                                        proxy_lookup_ready, open_host);
    } else {
        GSocketConnectable *address = NULL;

        if (s->unix_path) {
            SPICE_DEBUG("open unix path %s", s->unix_path);
#ifdef G_OS_UNIX
            address = G_SOCKET_CONNECTABLE(g_unix_socket_address_new(s->unix_path));
#else
            g_set_error_literal(&open_host->error, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                                "Unix path unsupported on this platform");
#endif
        } else {
            SPICE_DEBUG("open host %s:%d", s->host, open_host->port);
            address = g_network_address_new(s->host, open_host->port);
        }

        if (address == NULL || open_host->error != NULL) {
            coroutine_yieldto(open_host->from, NULL);
            return FALSE;
        }

        open_host_connectable_connect(open_host, address);
        g_object_unref(address);
    }

    if (open_host->proxy != NULL) {
        gchar *str = spice_uri_to_string(open_host->proxy);
        SPICE_DEBUG("(with proxy %s)", str);
        g_free(str);
    }

    return FALSE;
}

#define SOCKET_TIMEOUT 10

/* coroutine context */
G_GNUC_INTERNAL
GSocketConnection* spice_session_channel_open_host(SpiceSession *session, SpiceChannel *channel,
                                                   gboolean *use_tls, GError **error)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;
    SpiceChannelPrivate *c = channel->priv;
    spice_open_host open_host = { 0, };
    gchar *port, *endptr;

    // FIXME: make open_host() cancellable
    open_host.from = coroutine_self();
    open_host.session = session;
    open_host.channel = channel;

    const char *name = spice_channel_type_to_string(c->channel_type);
    if (spice_strv_contains(s->secure_channels, "all") ||
        spice_strv_contains(s->secure_channels, name))
        *use_tls = TRUE;

    if (s->unix_path) {
        if (*use_tls) {
            CHANNEL_DEBUG(channel, "No TLS for Unix sockets");
            return NULL;
        }
    } else {
        port = *use_tls ? s->tls_port : s->port;
        if (port == NULL) {
            g_debug("Missing port value, not attempting %s connection.",
                    *use_tls?"TLS":"unencrypted");
            return NULL;
        }

        open_host.port = strtol(port, &endptr, 10);
        if (*port == '\0' || *endptr != '\0' ||
            open_host.port <= 0 || open_host.port > G_MAXUINT16) {
            g_warning("Invalid port value %s", port);
            return NULL;
        }
    }
    if (*use_tls) {
        CHANNEL_DEBUG(channel, "Using TLS, port %d", open_host.port);
    } else {
        CHANNEL_DEBUG(channel, "Using plain text, port %d", open_host.port);
    }

    open_host.client = g_socket_client_new();
    g_socket_client_set_enable_proxy(open_host.client, s->proxy != NULL);
    g_socket_client_set_timeout(open_host.client, SOCKET_TIMEOUT);

    g_idle_add(open_host_idle_cb, &open_host);
    /* switch to main loop and wait for connection */
    coroutine_yield(NULL);

    if (open_host.error != NULL) {
        CHANNEL_DEBUG(channel, "open host: %s", open_host.error->message);
        g_propagate_error(error, open_host.error);
    } else if (open_host.connection != NULL) {
        GSocket *socket;
        socket = g_socket_connection_get_socket(open_host.connection);
        g_socket_set_timeout(socket, 0);
        g_socket_set_blocking(socket, FALSE);
        g_socket_set_keepalive(socket, TRUE);
    }

    g_clear_object(&open_host.client);
    return open_host.connection;
}


G_GNUC_INTERNAL
void spice_session_channel_new(SpiceSession *session, SpiceChannel *channel)
{
    g_return_if_fail(SPICE_IS_SESSION(session));
    g_return_if_fail(SPICE_IS_CHANNEL(channel));

    SpiceSessionPrivate *s = session->priv;
    struct channel *item;


    item = g_new0(struct channel, 1);
    item->channel = channel;
    ring_add(&s->channels, &item->link);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        gboolean all = spice_strv_contains(s->disable_effects, "all");

        g_object_set(channel,
                     "disable-wallpaper", all || spice_strv_contains(s->disable_effects, "wallpaper"),
                     "disable-font-smooth", all || spice_strv_contains(s->disable_effects, "font-smooth"),
                     "disable-animation", all || spice_strv_contains(s->disable_effects, "animation"),
                     NULL);
        if (s->color_depth != 0)
            g_object_set(channel, "color-depth", s->color_depth, NULL);

        CHANNEL_DEBUG(channel, "new main channel, switching");
        s->cmain = channel;
    } else if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        g_warn_if_fail(s->playback_channel == NULL);
        s->playback_channel = SPICE_PLAYBACK_CHANNEL(channel);
    }

    g_signal_emit(session, signals[SPICE_SESSION_CHANNEL_NEW], 0, channel);
}

static void spice_session_channel_destroy(SpiceSession *session, SpiceChannel *channel)
{
    g_return_if_fail(SPICE_IS_SESSION(session));
    g_return_if_fail(SPICE_IS_CHANNEL(channel));

    SpiceSessionPrivate *s = session->priv;
    struct channel *item = NULL;
    RingItem *ring;

    if (s->migration_left)
        s->migration_left = g_list_remove(s->migration_left, channel);

    for (ring = ring_get_head(&s->channels); ring != NULL;
         ring = ring_next(&s->channels, ring)) {
        item = SPICE_CONTAINEROF(ring, struct channel, link);
        if (item->channel == channel)
            break;
    }

    g_return_if_fail(ring != NULL);

    if (channel == s->cmain) {
        CHANNEL_DEBUG(channel, "the session lost the main channel");
        s->cmain = NULL;
    }

    ring_remove(&item->link);
    free(item);

    g_signal_emit(session, signals[SPICE_SESSION_CHANNEL_DESTROY], 0, channel);

    g_clear_object(&channel->priv->session);
    spice_channel_disconnect(channel, SPICE_CHANNEL_NONE);
    g_object_unref(channel);
}

G_GNUC_INTERNAL
void spice_session_set_connection_id(SpiceSession *session, int id)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    s->connection_id = id;
}

G_GNUC_INTERNAL
int spice_session_get_connection_id(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), -1);

    SpiceSessionPrivate *s = session->priv;

    return s->connection_id;
}

G_GNUC_INTERNAL
guint32 spice_session_get_mm_time(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), 0);

    SpiceSessionPrivate *s = session->priv;

    /* FIXME: we may want to estimate the drift of clocks, and well,
       do something better than this trivial approach */
    return s->mm_time + (g_get_monotonic_time() - s->mm_time_at_clock) / 1000;
}

#define MM_TIME_DIFF_RESET_THRESH 500 // 0.5 sec

G_GNUC_INTERNAL
void spice_session_set_mm_time(SpiceSession *session, guint32 time)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;
    guint32 old_time;

    old_time = spice_session_get_mm_time(session);

    s->mm_time = time;
    s->mm_time_at_clock = g_get_monotonic_time();
    SPICE_DEBUG("set mm time: %u", spice_session_get_mm_time(session));
    if (time > old_time + MM_TIME_DIFF_RESET_THRESH ||
        time < old_time) {
        SPICE_DEBUG("%s: mm-time-reset, old %u, new %u", __FUNCTION__, old_time, s->mm_time);
        g_coroutine_signal_emit(session, signals[SPICE_SESSION_MM_TIME_RESET], 0);
    }
}

G_GNUC_INTERNAL
void spice_session_set_port(SpiceSession *session, int port, gboolean tls)
{
    const char *prop = tls ? "tls-port" : "port";
    char *tmp;

    g_return_if_fail(SPICE_IS_SESSION(session));

    /* old spicec client doesn't accept port == 0, see Migrate::start */
    tmp = port > 0 ? g_strdup_printf("%d", port) : NULL;
    g_object_set(session, prop, tmp, NULL);
    g_free(tmp);
}

G_GNUC_INTERNAL
void spice_session_get_pubkey(SpiceSession *session, guint8 **pubkey, guint *size)
{
    g_return_if_fail(SPICE_IS_SESSION(session));
    g_return_if_fail(pubkey != NULL);
    g_return_if_fail(size != NULL);

    SpiceSessionPrivate *s = session->priv;

    *pubkey = s->pubkey ? s->pubkey->data : NULL;
    *size = s->pubkey ? s->pubkey->len : 0;
}

G_GNUC_INTERNAL
void spice_session_get_ca(SpiceSession *session, guint8 **ca, guint *size)
{
    g_return_if_fail(SPICE_IS_SESSION(session));
    g_return_if_fail(ca != NULL);
    g_return_if_fail(size != NULL);

    SpiceSessionPrivate *s = session->priv;

    *ca = s->ca ? s->ca->data : NULL;
    *size = s->ca ? s->ca->len : 0;
}

G_GNUC_INTERNAL
guint spice_session_get_verify(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), 0);

    SpiceSessionPrivate *s = session->priv;

    return s->verify;
}

G_GNUC_INTERNAL
void spice_session_set_migration_state(SpiceSession *session, SpiceSessionMigration state)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    if (state == SPICE_SESSION_MIGRATION_CONNECTING)
        s->for_migration = true;

    s->migration_state = state;
    g_coroutine_object_notify(G_OBJECT(session), "migration-state");
}

G_GNUC_INTERNAL
const gchar* spice_session_get_username(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->username;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_password(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->password;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_host(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->host;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_cert_subject(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->cert_subject;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_ciphers(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->ciphers;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_ca_file(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->ca_file;
}

G_GNUC_INTERNAL
void spice_session_get_caches(SpiceSession *session,
                              display_cache **images,
                              SpiceGlzDecoderWindow **glz_window)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    if (images)
        *images = s->images;
    if (glz_window)
        *glz_window = s->glz_window;
}

G_GNUC_INTERNAL
void spice_session_set_caches_hints(SpiceSession *session,
                                    uint32_t pci_ram_size,
                                    uint32_t n_display_channels)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    s->pci_ram_size = pci_ram_size;
    s->n_display_channels = n_display_channels;

    /* TODO: when setting cache and window size, we should consider the client's
     *       available memory and the number of display channels */
    if (s->images_cache_size == 0) {
        s->images_cache_size = IMAGES_CACHE_SIZE_DEFAULT;
    }

    if (s->glz_window_size == 0) {
        s->glz_window_size = MIN(MAX_GLZ_WINDOW_SIZE_DEFAULT, pci_ram_size / 2);
        s->glz_window_size = MAX(MIN_GLZ_WINDOW_SIZE_DEFAULT, s->glz_window_size);
    }
}

G_GNUC_INTERNAL
guint spice_session_get_n_display_channels(SpiceSession *session)
{
    g_return_val_if_fail(session != NULL, 0);

    return session->priv->n_display_channels;
}

G_GNUC_INTERNAL
void spice_session_set_uuid(SpiceSession *session, guint8 uuid[16])
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    memcpy(s->uuid, uuid, sizeof(s->uuid));

    g_coroutine_object_notify(G_OBJECT(session), "uuid");
}

G_GNUC_INTERNAL
void spice_session_set_name(SpiceSession *session, const gchar *name)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    g_free(s->name);
    s->name = g_strdup(name);

    g_coroutine_object_notify(G_OBJECT(session), "name");
}

G_GNUC_INTERNAL
void spice_session_sync_playback_latency(SpiceSession *session)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    if (s->playback_channel &&
        spice_playback_channel_is_active(s->playback_channel)) {
        spice_playback_channel_sync_latency(s->playback_channel);
    } else {
        SPICE_DEBUG("%s: not implemented when there isn't audio playback", __FUNCTION__);
    }
}

G_GNUC_INTERNAL
gboolean spice_session_is_playback_active(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    SpiceSessionPrivate *s = session->priv;

    return (s->playback_channel &&
        spice_playback_channel_is_active(s->playback_channel));
}

G_GNUC_INTERNAL
guint32 spice_session_get_playback_latency(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), 0);

    SpiceSessionPrivate *s = session->priv;

    if (s->playback_channel &&
        spice_playback_channel_is_active(s->playback_channel)) {
        return spice_playback_channel_get_latency(s->playback_channel);
    } else {
        SPICE_DEBUG("%s: not implemented when there isn't audio playback", __FUNCTION__);
        return 0;
    }
}

G_GNUC_INTERNAL
const gchar* spice_session_get_shared_dir(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    SpiceSessionPrivate *s = session->priv;

    return s->shared_dir;
}

G_GNUC_INTERNAL
void spice_session_set_shared_dir(SpiceSession *session, const gchar *dir)
{
    g_return_if_fail(SPICE_IS_SESSION(session));

    SpiceSessionPrivate *s = session->priv;

    g_free(s->shared_dir);
    s->shared_dir = g_strdup(dir);
}

/**
 * spice_session_get_proxy_uri:
 * @session: a #SpiceSession
 *
 * Returns: (transfer none): the session proxy #SpiceURI or %NULL.
 * Since: 0.24
 **/
SpiceURI *spice_session_get_proxy_uri(SpiceSession *session)
{
    SpiceSessionPrivate *s;

    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);
    g_return_val_if_fail(session->priv != NULL, NULL);

    s = session->priv;

    return s->proxy;
}

/**
 * spice_audio_get:
 * @session: the #SpiceSession to connect to
 * @context: (allow-none): a #GMainContext to attach to (or %NULL for default).
 *
 * Gets the #SpiceAudio associated with the passed in #SpiceSession.
 * A new #SpiceAudio instance will be created the first time this
 * function is called for a certain #SpiceSession.
 *
 * Note that this function returns a weak reference, which should not be used
 * after the #SpiceSession itself has been unref-ed by the caller.
 *
 * Returns: (transfer none): a weak reference to a #SpiceAudio
 * instance or %NULL if failed.
 **/
SpiceAudio *spice_audio_get(SpiceSession *session, GMainContext *context)
{
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    SpiceAudio *self;

    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);

    g_static_mutex_lock(&mutex);
    self = session->priv->audio_manager;
    if (self == NULL) {
        self = spice_audio_new(session, context, NULL);
        session->priv->audio_manager = self;
    }
    g_static_mutex_unlock(&mutex);

    return self;
}

/**
 * spice_usb_device_manager_get:
 * @session: #SpiceSession for which to get the #SpiceUsbDeviceManager
 *
 * Gets the #SpiceUsbDeviceManager associated with the passed in #SpiceSession.
 * A new #SpiceUsbDeviceManager instance will be created the first time this
 * function is called for a certain #SpiceSession.
 *
 * Note that this function returns a weak reference, which should not be used
 * after the #SpiceSession itself has been unref-ed by the caller.
 *
 * Returns: (transfer none): a weak reference to the #SpiceUsbDeviceManager associated with the passed in #SpiceSession
 */
SpiceUsbDeviceManager *spice_usb_device_manager_get(SpiceSession *session,
                                                    GError **err)
{
    SpiceUsbDeviceManager *self;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);
    g_return_val_if_fail(err == NULL || *err == NULL, NULL);

    g_static_mutex_lock(&mutex);
    self = session->priv->usb_manager;
    if (self == NULL) {
        self = g_initable_new(SPICE_TYPE_USB_DEVICE_MANAGER, NULL, err,
                              "session", session, NULL);
        session->priv->usb_manager = self;
    }
    g_static_mutex_unlock(&mutex);

    return self;
}

G_GNUC_INTERNAL
gboolean spice_session_get_audio_enabled(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    return session->priv->audio;
}

G_GNUC_INTERNAL
gboolean spice_session_get_usbredir_enabled(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    return session->priv->usbredir;
}

G_GNUC_INTERNAL
gboolean spice_session_get_smartcard_enabled(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    return session->priv->smartcard;
}

G_GNUC_INTERNAL
PhodavServer* spice_session_get_webdav_server(SpiceSession *session)
{
    SpiceSessionPrivate *priv;

    g_return_val_if_fail(SPICE_IS_SESSION(session), NULL);
    priv = session->priv;

#ifdef USE_PHODAV
    static GMutex mutex;

    const gchar *shared_dir = spice_session_get_shared_dir(session);
    if (shared_dir == NULL) {
        g_debug("No shared dir set, not creating webdav server");
        return NULL;
    }

    g_mutex_lock(&mutex);

    if (priv->webdav)
        goto end;

    priv->webdav = phodav_server_new(shared_dir);
    g_object_bind_property(session,  "share-dir-ro",
                           priv->webdav, "read-only",
                           G_BINDING_SYNC_CREATE|G_BINDING_BIDIRECTIONAL);
    g_object_bind_property(session,  "shared-dir",
                           priv->webdav, "root",
                           G_BINDING_SYNC_CREATE|G_BINDING_BIDIRECTIONAL);

end:
    g_mutex_unlock(&mutex);
#endif

    return priv->webdav;
}

/**
 * spice_session_is_for_migration:
 * @session: a Spice session
 *
 * During seamless migration, channels may be created to establish a
 * connection with the target, but they are temporary and should only
 * handle migration steps. In order to avoid other interactions with
 * the client, channels should check this value.
 *
 * Returns: %TRUE if the session is a copy created during migration
 * Since: 0.27
 **/
gboolean spice_session_is_for_migration(SpiceSession *session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    return session->priv->for_migration;
}

G_GNUC_INTERNAL
void spice_session_set_main_channel(SpiceSession *session, SpiceChannel *channel)
{
    g_return_if_fail(SPICE_IS_SESSION(session));
    g_return_if_fail(SPICE_IS_CHANNEL(channel));
    g_return_if_fail(session->priv->cmain == NULL);

    session->priv->cmain = channel;
}

G_GNUC_INTERNAL
gboolean spice_session_set_migration_session(SpiceSession *session, SpiceSession *mig_session)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);
    g_return_val_if_fail(SPICE_IS_SESSION(mig_session), FALSE);
    g_return_val_if_fail(session->priv->migration == NULL, FALSE);

    session->priv->migration = mig_session;

    return TRUE;
}
