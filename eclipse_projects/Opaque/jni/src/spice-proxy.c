/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

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

#include <stdlib.h>
#include <string.h>

#include "spice-client.h"
#include "spice-proxy.h"

struct _SpiceProxyPrivate {
    gchar *protocol;
    gchar *hostname;
    guint port;
};

#define SPICE_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), SPICE_TYPE_PROXY, SpiceProxyPrivate))

G_DEFINE_TYPE(SpiceProxy, spice_proxy, G_TYPE_OBJECT);

enum  {
    SPICE_PROXY_DUMMY_PROPERTY,
    SPICE_PROXY_PROTOCOL,
    SPICE_PROXY_HOSTNAME,
    SPICE_PROXY_PORT
};

G_GNUC_INTERNAL
SpiceProxy* spice_proxy_new(void)
{
    SpiceProxy * self = NULL;
    self = (SpiceProxy*)g_object_new(SPICE_TYPE_PROXY, NULL);
    return self;
}

G_GNUC_INTERNAL
gboolean spice_proxy_parse(SpiceProxy *self, const gchar *proxyuri, GError **error)
{
    gchar *dup, *uri;
    gboolean success = FALSE;
    size_t len;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(proxyuri != NULL, FALSE);

    uri = dup = g_strdup(proxyuri);
    /* FIXME: use GUri when it is ready... only support http atm */
    /* the code is voluntarily not parsing thoroughly the uri */
    if (g_ascii_strncasecmp("http://", uri, 7) == 0)
        uri += 7;

    /* remove trailing slash */
    len = strlen(uri);
    for (; len > 0; len--)
        if (uri[len-1] == '/')
            uri[len-1] = '\0';
        else
            break;

    spice_proxy_set_protocol(self, "http");
    spice_proxy_set_port(self, 3128);

    gchar **proxyv = g_strsplit(uri, ":", 0);
    const gchar *proxy_port = NULL;

    if (proxyv[0] == NULL || strlen(proxyv[0]) == 0) {
        g_set_error(error, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                    "Invalid hostname in proxy address");
        goto end;
    }

    spice_proxy_set_hostname(self, proxyv[0]);
    if (proxyv[0] != NULL)
        proxy_port = proxyv[1];

    if (proxy_port != NULL) {
        char *endptr;
        guint port = strtoul(proxy_port, &endptr, 10);
        if (*endptr != '\0') {
            g_set_error(error, SPICE_CLIENT_ERROR, SPICE_CLIENT_ERROR_FAILED,
                        "Invalid proxy port: %s", proxy_port);
            goto end;
        }
        spice_proxy_set_port(self, port);
    }

    success = TRUE;

end:
    g_free(dup);
    g_strfreev(proxyv);
    return success;
}

G_GNUC_INTERNAL
const gchar* spice_proxy_get_protocol(SpiceProxy *self)
{
    g_return_val_if_fail(SPICE_IS_PROXY(self), NULL);
    return self->priv->protocol;
}

G_GNUC_INTERNAL
void spice_proxy_set_protocol(SpiceProxy *self, const gchar *value)
{
    g_return_if_fail(SPICE_IS_PROXY(self));

    g_free(self->priv->protocol);
    self->priv->protocol = g_strdup(value);
    g_object_notify((GObject *)self, "protocol");
}

G_GNUC_INTERNAL
const gchar* spice_proxy_get_hostname(SpiceProxy *self)
{
    g_return_val_if_fail(SPICE_IS_PROXY(self), NULL);
    return self->priv->hostname;
}


G_GNUC_INTERNAL
void spice_proxy_set_hostname(SpiceProxy *self, const gchar *value)
{
    g_return_if_fail(SPICE_IS_PROXY(self));

    g_free(self->priv->hostname);
    self->priv->hostname = g_strdup(value);
    g_object_notify((GObject *)self, "hostname");
}

G_GNUC_INTERNAL
guint spice_proxy_get_port(SpiceProxy *self)
{
    g_return_val_if_fail(SPICE_IS_PROXY(self), 0);
    return self->priv->port;
}

G_GNUC_INTERNAL
void spice_proxy_set_port(SpiceProxy *self, guint port)
{
    g_return_if_fail(SPICE_IS_PROXY(self));
    self->priv->port = port;
    g_object_notify((GObject *)self, "port");
}

static void spice_proxy_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
    SpiceProxy *self;
    self = G_TYPE_CHECK_INSTANCE_CAST(object, SPICE_TYPE_PROXY, SpiceProxy);

    switch (property_id) {
    case SPICE_PROXY_PROTOCOL:
        g_value_set_string(value, spice_proxy_get_protocol(self));
        break;
    case SPICE_PROXY_HOSTNAME:
        g_value_set_string(value, spice_proxy_get_hostname(self));
        break;
    case SPICE_PROXY_PORT:
        g_value_set_uint(value, spice_proxy_get_port(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}


static void spice_proxy_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
    SpiceProxy * self;
    self = G_TYPE_CHECK_INSTANCE_CAST(object, SPICE_TYPE_PROXY, SpiceProxy);

    switch (property_id) {
    case SPICE_PROXY_PROTOCOL:
        spice_proxy_set_protocol(self, g_value_get_string(value));
        break;
    case SPICE_PROXY_HOSTNAME:
        spice_proxy_set_hostname(self, g_value_get_string(value));
        break;
    case SPICE_PROXY_PORT:
        spice_proxy_set_port(self, g_value_get_uint(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void spice_proxy_finalize(GObject* obj)
{
    SpiceProxy *self;

    self = G_TYPE_CHECK_INSTANCE_CAST(obj, SPICE_TYPE_PROXY, SpiceProxy);
    g_free(self->priv->protocol);
    g_free(self->priv->hostname);

    G_OBJECT_CLASS (spice_proxy_parent_class)->finalize (obj);
}

static void spice_proxy_init (SpiceProxy *self)
{
    self->priv = SPICE_PROXY_GET_PRIVATE(self);
}


static void spice_proxy_class_init(SpiceProxyClass *klass)
{
    spice_proxy_parent_class = g_type_class_peek_parent (klass);
    g_type_class_add_private(klass, sizeof(SpiceProxyPrivate));

    G_OBJECT_CLASS (klass)->get_property = spice_proxy_get_property;
    G_OBJECT_CLASS (klass)->set_property = spice_proxy_set_property;
    G_OBJECT_CLASS (klass)->finalize = spice_proxy_finalize;

    g_object_class_install_property(G_OBJECT_CLASS (klass),
                                    SPICE_PROXY_PROTOCOL,
                                    g_param_spec_string ("protocol",
                                                         "protocol",
                                                         "protocol",
                                                         NULL,
                                                         G_PARAM_STATIC_STRINGS |
                                                         G_PARAM_READWRITE));

    g_object_class_install_property(G_OBJECT_CLASS (klass),
                                    SPICE_PROXY_HOSTNAME,
                                    g_param_spec_string ("hostname",
                                                         "hostname",
                                                         "hostname",
                                                         NULL,
                                                         G_PARAM_STATIC_STRINGS |
                                                         G_PARAM_READWRITE));

    g_object_class_install_property(G_OBJECT_CLASS (klass),
                                    SPICE_PROXY_PORT,
                                    g_param_spec_uint ("port",
                                                       "port",
                                                       "port",
                                                       0, G_MAXUINT, 0,
                                                       G_PARAM_STATIC_STRINGS |
                                                       G_PARAM_READWRITE));
}

G_GNUC_INTERNAL
gchar* spice_proxy_to_string(SpiceProxy* self)
{
    SpiceProxyPrivate *p;

    g_return_val_if_fail(SPICE_IS_PROXY(self), NULL);
    p = self->priv;

    if (p->protocol == NULL || p->hostname == NULL)
        return NULL;

    return g_strdup_printf("%s://%s:%u", p->protocol, p->hostname, p->port);
}
