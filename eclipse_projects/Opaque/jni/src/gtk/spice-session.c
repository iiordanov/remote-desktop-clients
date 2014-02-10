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
#include <gio/gio.h>
#include <glib.h>

#include "common/ring.h"

#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"
#include "spice-util-priv.h"
#include "spice-session-priv.h"
#include "gio-coroutine.h"
#include "glib-compat.h"
#include "wocky-http-proxy.h"
#include "spice-proxy.h"
#include "channel-playback-priv.h"

struct channel {
    SpiceChannel      *channel;
    RingItem          link;
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
    PROP_SECURE_CHANNELS
};

/* signals */
enum {
    SPICE_SESSION_CHANNEL_NEW,
    SPICE_SESSION_CHANNEL_DESTROY,
    SPICE_SESSION_MM_TIME_RESET,
    SPICE_SESSION_LAST_SIGNAL,
};

static guint signals[SPICE_SESSION_LAST_SIGNAL];

struct SPICE_SESSION_MM_TIME_RESET {
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_SESSION_MM_TIME_RESET:
        g_signal_emit(object, signals[signum], 0);
        break;
    default:
        g_warn_if_reached();
    }
}

static void update_proxy(SpiceSession *self, const gchar *str)
{
    SpiceSessionPrivate *s = self->priv;
    SpiceProxy *proxy = NULL;
    GError *error = NULL;

    if (str == NULL)
        str = g_getenv("SPICE_PROXY");
    if (str == NULL || *str == 0) {
        g_clear_object(&s->proxy);
        return;
    }

    proxy = spice_proxy_new();
    if (!spice_proxy_parse(proxy, str, &error))
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
spice_session_dispose(GObject *gobject)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = session->priv;

    SPICE_DEBUG("session dispose");

    spice_session_disconnect(session);

    if (s->migration) {
        spice_session_disconnect(s->migration);
        g_object_unref(s->migration);
        s->migration = NULL;
    }

    if (s->migration_left) {
        g_list_free(s->migration_left);
        s->migration_left = NULL;
    }

    if (s->after_main_init) {
        g_source_remove(s->after_main_init);
        s->after_main_init = 0;
    }

    g_clear_object(&s->audio_manager);
    g_clear_object(&s->desktop_integration);
    g_clear_object(&s->gtk_session);
    g_clear_object(&s->usb_manager);
    g_clear_object(&s->proxy);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_session_parent_class)->dispose)
        G_OBJECT_CLASS(spice_session_parent_class)->dispose(gobject);
}

static void
spice_session_finalize(GObject *gobject)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    /* release stuff */
    g_free(s->host);
    g_free(s->port);
    g_free(s->tls_port);
    g_free(s->password);
    g_free(s->ca_file);
    g_free(s->ciphers);
    g_free(s->cert_subject);
    g_strfreev(s->smartcard_certificates);
    g_free(s->smartcard_db);
    g_strfreev(s->disable_effects);
    g_strfreev(s->secure_channels);

    g_clear_pointer(&s->images, cache_unref);
    glz_decoder_window_destroy(s->glz_window);

    g_clear_pointer(&s->pubkey, g_byte_array_unref);
    g_clear_pointer(&s->ca, g_byte_array_unref);

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_session_parent_class)->finalize)
        G_OBJECT_CLASS(spice_session_parent_class)->finalize(gobject);
}

#define URI_SCHEME_SPICE "spice://"
#define URI_QUERY_START ";?"
#define URI_QUERY_SEP   ";&"

static int spice_uri_create(SpiceSession *session, char *dest, int len)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    int pos = 0;

    if (s->host == NULL || (s->port == NULL && s->tls_port == NULL)) {
        return 0;
    }

    pos += snprintf(dest + pos, len-pos, "spice://%s?", s->host);
    if (s->port && strlen(s->port))
        pos += snprintf(dest + pos, len - pos, "port=%s;", s->port);
    if (s->tls_port && strlen(s->tls_port))
        pos += snprintf(dest + pos, len - pos, "tls-port=%s;", s->tls_port);
    return pos;
}

static int spice_uri_parse(SpiceSession *session, const char *original_uri)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    gchar *host = NULL, *port = NULL, *tls_port = NULL, *uri = NULL, *password = NULL;
    gchar *path = NULL;
    gchar *unescaped_path = NULL;
    gchar *authority = NULL;
    gchar *query = NULL;

    g_return_val_if_fail(original_uri != NULL, -1);

    uri = g_strdup(original_uri);

    /* Break up the URI into its various parts, scheme, authority,
     * path (ignored) and query
     */
    if (strncmp(uri, URI_SCHEME_SPICE, strlen(URI_SCHEME_SPICE)) != 0) {
        g_warning("Expected a URI scheme of '%s' in URI '%s'",
                  URI_SCHEME_SPICE, uri);
        goto fail;
    }
    authority = uri + strlen(URI_SCHEME_SPICE);
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
        gchar *tmp = strchr(authority, ']');
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
        gchar *tmp = strchr(authority, ':');
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

    while (query && query[0] != '\0') {
        gchar key[32], value[128];
        gchar **target_key;

        int len;
        if (sscanf(query, "%31[-a-zA-Z0-9]=%127[^;&]%n", key, value, &len) != 2) {
            g_warning("Failed to parse URI query '%s'", query);
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

    /* parsed ok -> apply */
    g_free(uri);
    g_free(unescaped_path);
    g_free(s->host);
    g_free(s->port);
    g_free(s->tls_port);
    g_free(s->password);
    s->host = host;
    s->port = port;
    s->tls_port = tls_port;
    s->password = password;
    return 0;

fail:
    g_free(uri);
    g_free(unescaped_path);
    g_free(host);
    g_free(port);
    g_free(tls_port);
    g_free(password);
    return -1;
}

static void spice_session_get_property(GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    SpiceSession *session = SPICE_SESSION(gobject);
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    char buf[256];
    int len;

    switch (prop_id) {
    case PROP_HOST:
        g_value_set_string(value, s->host);
	break;
    case PROP_PORT:
        g_value_set_string(value, s->port);
	break;
    case PROP_TLS_PORT:
        g_value_set_string(value, s->tls_port);
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
        len = spice_uri_create(session, buf, sizeof(buf));
        g_value_set_string(value, len ? buf : NULL);
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
        g_value_take_string(value, spice_proxy_to_string(s->proxy));
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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    const char *str;

    switch (prop_id) {
    case PROP_HOST:
        g_free(s->host);
        s->host = g_value_dup_string(value);
        break;
    case PROP_PORT:
        g_free(s->port);
        s->port = g_value_dup_string(value);
        break;
    case PROP_TLS_PORT:
        g_free(s->tls_port);
        s->tls_port = g_value_dup_string(value);
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
            spice_uri_parse(session, str);
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
        g_object_notify_main_context(gobject, "read-only");
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
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void spice_session_class_init(SpiceSessionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

#if GLIB_CHECK_VERSION(2, 26, 0)
    _wocky_http_proxy_get_type();
#endif

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
    SpiceSession *copy = SPICE_SESSION(g_object_new(SPICE_TYPE_SESSION,
                                                    "host", NULL,
                                                    "ca-file", NULL,
                                                    NULL));
    SpiceSessionPrivate *c = copy->priv, *s = session->priv;

    g_clear_object(&c->proxy);

    g_warn_if_fail(c->host == NULL);
    g_warn_if_fail(c->tls_port == NULL);
    g_warn_if_fail(c->password == NULL);
    g_warn_if_fail(c->ca_file == NULL);
    g_warn_if_fail(c->ciphers == NULL);
    g_warn_if_fail(c->cert_subject == NULL);
    g_warn_if_fail(c->pubkey == NULL);
    g_warn_if_fail(c->pubkey == NULL);
    g_warn_if_fail(c->proxy == NULL);

    g_object_get(session,
                 "host", &c->host,
                 "tls-port", &c->tls_port,
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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, FALSE);

    spice_session_disconnect(session);
    s->disconnecting = FALSE;

    s->client_provided_sockets = FALSE;

    g_warn_if_fail(s->cmain == NULL);
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
 * If @fd is -1, a valid fd will be requested later via the
 * SpiceChannel::open-fd signal.
 *
 * Returns:
 **/
gboolean spice_session_open_fd(SpiceSession *session, int fd)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, FALSE);
    g_return_val_if_fail(fd >= -1, FALSE);

    spice_session_disconnect(session);
    s->disconnecting = FALSE;

    s->client_provided_sockets = TRUE;

    g_warn_if_fail(s->cmain == NULL);
    s->cmain = spice_channel_new(session, SPICE_CHANNEL_MAIN, 0);

    glz_decoder_window_clear(s->glz_window);
    return spice_channel_open_fd(s->cmain, fd);
}

G_GNUC_INTERNAL
gboolean spice_session_get_client_provided_socket(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, FALSE);
    return s->client_provided_sockets;
}

static void cache_clear_all(SpiceSession *self)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(self);

    cache_clear(s->images);
    glz_decoder_window_clear(s->glz_window);
}

G_GNUC_INTERNAL
void spice_session_switching_disconnect(SpiceSession *self)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(self);
    struct channel *item;
    RingItem *ring, *next;

    g_return_if_fail(s != NULL);
    g_return_if_fail(s->cmain != NULL);

    /* disconnect/destroy all but main channel */

    for (ring = ring_get_head(&s->channels); ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        item = SPICE_CONTAINEROF(ring, struct channel, link);
        if (item->channel != s->cmain)
            spice_channel_destroy(item->channel); /* /!\ item and channel are destroy() after this call */
    }

    g_warn_if_fail(!ring_is_empty(&s->channels)); /* ring_get_length() == 1 */

    cache_clear_all(self);
}

G_GNUC_INTERNAL
void spice_session_set_migration(SpiceSession *session,
                                 SpiceSession *migration,
                                 gboolean full_migration)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    SpiceSessionPrivate *m = SPICE_SESSION_GET_PRIVATE(migration);
    gchar *tmp;

    g_return_if_fail(s != NULL);

    s->full_migration = full_migration;
    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_MIGRATING);

    g_warn_if_fail(s->migration == NULL);
    s->migration = g_object_ref(migration);

    tmp = s->host;
    s->host = m->host;
    m->host = tmp;

    tmp = s->port;
    s->port = m->port;
    m->port = tmp;

    tmp = s->tls_port;
    s->tls_port = m->tls_port;
    m->tls_port = tmp;

    g_warn_if_fail(ring_get_length(&s->channels) == ring_get_length(&m->channels));

    SPICE_DEBUG("migration channels left:%d (in migration:%d)",
                ring_get_length(&s->channels), ring_get_length(&m->channels));
    s->migration_left = spice_session_get_channels(session);
}

G_GNUC_INTERNAL
SpiceChannel* spice_session_lookup_channel(SpiceSession *session, gint id, gint type)
{
    RingItem *ring, *next;
    SpiceSessionPrivate *s = session->priv;
    struct channel *c;

    g_return_val_if_fail(s != NULL, NULL);

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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    RingItem *ring, *next;
    struct channel *c;

    g_return_if_fail(s != NULL);

    if (s->migration == NULL) {
        SPICE_DEBUG("no migration in progress");
        return;
    }

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

    g_list_free(s->migration_left);
    s->migration_left = NULL;
    spice_session_disconnect(s->migration);
    g_object_unref(s->migration);
    s->migration = NULL;

    spice_session_set_migration_state(session, SPICE_SESSION_MIGRATION_NONE);
}

G_GNUC_INTERNAL
void spice_session_channel_migrate(SpiceSession *session, SpiceChannel *channel)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    SpiceChannel *c;
    gint id, type;

    g_return_if_fail(s != NULL);
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
        CHANNEL_DEBUG(channel, "all channel migrated");
        spice_session_disconnect(s->migration);
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

/**
 * spice_session_disconnect:
 * @session:
 *
 * Disconnect the @session, and destroy all channels.
 **/
void spice_session_disconnect(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    struct channel *item;
    RingItem *ring, *next;

    g_return_if_fail(s != NULL);

    SPICE_DEBUG("session: disconnecting %d", s->disconnecting);
    if (s->disconnecting)
        return;

    s->disconnecting = TRUE;
    s->cmain = NULL;

    for (ring = ring_get_head(&s->channels); ring != NULL; ring = next) {
        next = ring_next(&s->channels, ring);
        item = SPICE_CONTAINEROF(ring, struct channel, link);
        spice_channel_destroy(item->channel); /* /!\ item and channel are destroy() after this call */
    }

    s->connection_id = 0;

    g_free(s->name);
    s->name = NULL;
    memset(s->uuid, 0, sizeof(s->uuid));

    /* we leave disconnecting = TRUE, so that spice_channel_destroy()
       is not called multiple times on channels that are in pending
       destroy state. */
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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    struct channel *item;
    GList *list = NULL;
    RingItem *ring;

    g_return_val_if_fail(s != NULL, NULL);

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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    struct channel *item;
    RingItem *ring;

    g_return_val_if_fail(s != NULL, FALSE);

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
    SpiceProxy *proxy;
    int port;
    GCancellable *cancellable;
    GError *error;
    GSocketConnection *connection;
    GSocketClient *client;
#if !GLIB_CHECK_VERSION(2,26,0)
    guint timeout_id;
#endif
};

static void socket_client_connect_ready(GObject *source_object, GAsyncResult *result,
                                        gpointer data)
{
    GSocketClient *client = G_SOCKET_CLIENT(source_object);
    spice_open_host *open_host = data;
    GSocketConnection *connection = NULL;

    SPICE_DEBUG("connect ready");
    connection = g_socket_client_connect_finish(client, result, &open_host->error);
    if (connection == NULL)
        goto end;

    open_host->connection = connection;

end:
    coroutine_yieldto(open_host->from, NULL);
}

/* main context */
static void open_host_connectable_connect(spice_open_host *open_host, GSocketConnectable *connectable)
{
    SPICE_DEBUG("connecting %p...", open_host);

    g_socket_client_connect_async(open_host->client, connectable,
                                  open_host->cancellable,
                                  socket_client_connect_ready, open_host);
}

#if GLIB_CHECK_VERSION(2,26,0)
/* main context */
static void proxy_lookup_ready(GObject *source_object, GAsyncResult *result,
                               gpointer data)
{
    spice_open_host *open_host = data;
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(open_host->session);
    GList *addresses = NULL, *it;
    GSocketAddress *address;

    SPICE_DEBUG("proxy lookup ready");
    addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(source_object),
                                                 result, &open_host->error);
    if (addresses == NULL || open_host->error) {
        coroutine_yieldto(open_host->from, NULL);
        return;
    }

    for (it = addresses; it != NULL; it = it->next) {
        address = g_proxy_address_new(G_INET_ADDRESS(it->data),
                                      spice_proxy_get_port(open_host->proxy), "http",
                                      s->host, open_host->port, NULL, NULL);
        if (address != NULL)
            break;
    }

    open_host_connectable_connect(open_host, G_SOCKET_CONNECTABLE(address));
    g_resolver_free_addresses(addresses);
}
#endif

/* main context */
static gboolean open_host_idle_cb(gpointer data)
{
    spice_open_host *open_host = data;
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(open_host->session);

    g_return_val_if_fail(open_host != NULL, FALSE);
    g_return_val_if_fail(open_host->connection == NULL, FALSE);

#if GLIB_CHECK_VERSION(2,26,0)
    open_host->proxy = s->proxy;
    if (open_host->error != NULL) {
        coroutine_yieldto(open_host->from, NULL);
        return FALSE;
    }

    if (open_host->proxy)
        g_resolver_lookup_by_name_async(g_resolver_get_default(),
                                        spice_proxy_get_hostname(open_host->proxy),
                                        open_host->cancellable,
                                        proxy_lookup_ready, open_host);
    else
#endif
    {
        GSocketConnectable *address;

        address = g_network_address_new(s->host, open_host->port);
        open_host_connectable_connect(open_host, address);
        g_object_unref(address);
    }

    SPICE_DEBUG("open host %s:%d", s->host, open_host->port);
    if (open_host->proxy != NULL) {
        gchar *str = spice_proxy_to_string(open_host->proxy);
        SPICE_DEBUG("(with proxy %s)", str);
        g_free(str);
    }

    return FALSE;
}

#define SOCKET_TIMEOUT 10

#if !GLIB_CHECK_VERSION(2,26,0)
static gboolean connect_timeout(gpointer data)
{
    spice_open_host *open_host = data;

    open_host->timeout_id = 0;
    coroutine_yieldto(open_host->from, NULL);

    return FALSE;
}
#endif

/* coroutine context */
G_GNUC_INTERNAL
GSocketConnection* spice_session_channel_open_host(SpiceSession *session, SpiceChannel *channel,
                                                   gboolean *use_tls)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
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

    port = *use_tls ? s->tls_port : s->port;
    if (port == NULL)
        return NULL;

    open_host.port = strtol(port, &endptr, 10);
    if (*port == '\0' || *endptr != '\0' ||
        open_host.port <= 0 || open_host.port > G_MAXUINT16) {
        g_warning("Invalid port value %s", port);
        return NULL;
    }

    open_host.client = g_socket_client_new();
#if GLIB_CHECK_VERSION(2,26,0)
    g_socket_client_set_timeout(open_host.client, SOCKET_TIMEOUT);
#else
    open_host.timeout_id =
        g_timeout_add_seconds(SOCKET_TIMEOUT, connect_timeout, &open_host);
#endif

    g_idle_add(open_host_idle_cb, &open_host);
    /* switch to main loop and wait for connection */
    coroutine_yield(NULL);

#if !GLIB_CHECK_VERSION(2,26,0)
    if (open_host.timeout_id == 0)
        open_host.error = g_error_new(SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                                      "connect timed out");
    else
        g_source_remove(open_host.timeout_id);
#endif

    if (open_host.error != NULL) {
        g_warning("%s", open_host.error->message);
        g_clear_error(&open_host.error);
    } else if (open_host.connection != NULL) {
        GSocket *socket;
        socket = g_socket_connection_get_socket(open_host.connection);
#if GLIB_CHECK_VERSION(2,26,0)
        g_socket_set_timeout(socket, 0);
#endif
        g_socket_set_blocking(socket, FALSE);
        g_socket_set_keepalive(socket, TRUE);
    }

    g_clear_object(&open_host.client);
    return open_host.connection;
}


G_GNUC_INTERNAL
void spice_session_channel_new(SpiceSession *session, SpiceChannel *channel)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    struct channel *item;

    g_return_if_fail(s != NULL);
    g_return_if_fail(channel != NULL);

    item = spice_new0(struct channel, 1);
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

G_GNUC_INTERNAL
void spice_session_channel_destroy(SpiceSession *session, SpiceChannel *channel)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    struct channel *item = NULL;
    RingItem *ring;

    g_return_if_fail(s != NULL);
    g_return_if_fail(channel != NULL);

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
}

G_GNUC_INTERNAL
void spice_session_set_connection_id(SpiceSession *session, int id)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);

    s->connection_id = id;
}

G_GNUC_INTERNAL
int spice_session_get_connection_id(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, -1);

    return s->connection_id;
}

G_GNUC_INTERNAL
guint32 spice_session_get_mm_time(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, 0);

    /* FIXME: we may want to estimate the drift of clocks, and well,
       do something better than this trivial approach */
    return s->mm_time + (g_get_monotonic_time() - s->mm_time_at_clock) / 1000;
}

#define MM_TIME_DIFF_RESET_THRESH 500 // 0.5 sec

G_GNUC_INTERNAL
void spice_session_set_mm_time(SpiceSession *session, guint32 time)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);
    guint32 old_time;

    g_return_if_fail(s != NULL);

    old_time = spice_session_get_mm_time(session);

    s->mm_time = time;
    s->mm_time_at_clock = g_get_monotonic_time();
    SPICE_DEBUG("set mm time: %u", spice_session_get_mm_time(session));
    if (time > old_time + MM_TIME_DIFF_RESET_THRESH ||
        time < old_time) {
        SPICE_DEBUG("%s: mm-time-reset, old %u, new %u", __FUNCTION__, old_time, s->mm_time);
        emit_main_context(session, SPICE_SESSION_MM_TIME_RESET);
    }
}

G_GNUC_INTERNAL
void spice_session_set_port(SpiceSession *session, int port, gboolean tls)
{
    const char *prop = tls ? "tls-port" : "port";
    char *tmp;

    g_return_if_fail(session != NULL);

    /* old spicec client doesn't accept port == 0, see Migrate::start */
    tmp = port > 0 ? g_strdup_printf("%d", port) : NULL;
    g_object_set(session, prop, tmp, NULL);
    g_free(tmp);
}

G_GNUC_INTERNAL
void spice_session_get_pubkey(SpiceSession *session, guint8 **pubkey, guint *size)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);
    g_return_if_fail(pubkey != NULL);
    g_return_if_fail(size != NULL);

    *pubkey = s->pubkey ? s->pubkey->data : NULL;
    *size = s->pubkey ? s->pubkey->len : 0;
}

G_GNUC_INTERNAL
void spice_session_get_ca(SpiceSession *session, guint8 **ca, guint *size)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);
    g_return_if_fail(ca != NULL);
    g_return_if_fail(size != NULL);

    *ca = s->ca ? s->ca->data : NULL;
    *size = s->ca ? s->ca->len : 0;
}

G_GNUC_INTERNAL
guint spice_session_get_verify(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, 0);
    return s->verify;
}

G_GNUC_INTERNAL
void spice_session_set_migration_state(SpiceSession *session, SpiceSessionMigration state)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);
    s->migration_state = state;
    g_object_notify_main_context(G_OBJECT(session), "migration-state");
}

G_GNUC_INTERNAL
const gchar* spice_session_get_password(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, NULL);
    return s->password;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_host(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, NULL);
    return s->host;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_cert_subject(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, NULL);
    return s->cert_subject;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_ciphers(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, NULL);
    return s->ciphers;
}

G_GNUC_INTERNAL
const gchar* spice_session_get_ca_file(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, NULL);
    return s->ca_file;
}

G_GNUC_INTERNAL
void spice_session_get_caches(SpiceSession *session,
                              display_cache **images,
                              SpiceGlzDecoderWindow **glz_window)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);

    if (images)
        *images = s->images;
    if (glz_window)
        *glz_window = s->glz_window;
}

G_GNUC_INTERNAL
void spice_session_set_caches_hints(SpiceSession *session,
                                    uint32_t pci_ram_size,
                                    uint32_t display_channels_count)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);

    s->pci_ram_size = pci_ram_size;
    s->display_channels_count = display_channels_count;

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
void spice_session_set_uuid(SpiceSession *session, guint8 uuid[16])
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);
    memcpy(s->uuid, uuid, sizeof(s->uuid));

    g_object_notify_main_context(G_OBJECT(session), "uuid");
}

G_GNUC_INTERNAL
void spice_session_set_name(SpiceSession *session, const gchar *name)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);
    g_free(s->name);
    s->name = g_strdup(name);

    g_object_notify_main_context(G_OBJECT(session), "name");
}

G_GNUC_INTERNAL
void spice_session_sync_playback_latency(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_if_fail(s != NULL);

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
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, FALSE);

    return (s->playback_channel &&
        spice_playback_channel_is_active(s->playback_channel));
}

G_GNUC_INTERNAL
guint32 spice_session_get_playback_latency(SpiceSession *session)
{
    SpiceSessionPrivate *s = SPICE_SESSION_GET_PRIVATE(session);

    g_return_val_if_fail(s != NULL, 0);

    if (s->playback_channel &&
        spice_playback_channel_is_active(s->playback_channel)) {
        return spice_playback_channel_get_latency(s->playback_channel);
    } else {
        SPICE_DEBUG("%s: not implemented when there isn't audio playback", __FUNCTION__);
        return 0;
    }
}
