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
#include "config.h"

#include <string.h>
#include <glib.h>

#include "spice-util.h"
#include "bio-gio.h"

typedef struct bio_gsocket_method {
    BIO_METHOD method;
    GIOStream *stream;
} bio_gsocket_method;

#define BIO_GET_GSOCKET(bio)  (((bio_gsocket_method*)bio->method)->gsocket)
#define BIO_GET_ISTREAM(bio)  (g_io_stream_get_input_stream(((bio_gsocket_method*)bio->method)->stream))
#define BIO_GET_OSTREAM(bio)  (g_io_stream_get_output_stream(((bio_gsocket_method*)bio->method)->stream))

static int bio_gio_write(BIO *bio, const char *in, int inl)
{
    gssize ret;
    GError *error = NULL;

    ret = g_pollable_output_stream_write_nonblocking(G_POLLABLE_OUTPUT_STREAM(BIO_GET_OSTREAM(bio)),
                                                     in, inl, NULL, &error);
    BIO_clear_retry_flags(bio);

    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        BIO_set_retry_write(bio);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_clear_error(&error);
    }

    return ret;
}

static int bio_gio_read(BIO *bio, char *out, int outl)
{
    gssize ret;
    GError *error = NULL;

    ret = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(BIO_GET_ISTREAM(bio)),
                                                   out, outl, NULL, &error);
    BIO_clear_retry_flags(bio);

    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        BIO_set_retry_read(bio);
    else if (error != NULL)
        g_warning("%s", error->message);

    g_clear_error(&error);

    return ret;
}

static int bio_gio_destroy(BIO *bio)
{
    if (bio == NULL || bio->method == NULL)
        return 0;

    SPICE_DEBUG("bio gsocket destroy");
    g_free(bio->method);
    bio->method = NULL;;

    return 1;
}

static int bio_gio_puts(BIO *bio, const char *str)
{
    int n, ret;

    n = strlen(str);
    ret = bio_gio_write(bio, str, n);

    return ret;
}

G_GNUC_INTERNAL
BIO* bio_new_giostream(GIOStream *stream)
{
    // TODO: make an actual new BIO type, or just switch to GTls already...
    BIO *bio = BIO_new_socket(-1, BIO_NOCLOSE);

    bio_gsocket_method *bio_method = g_new(bio_gsocket_method, 1);
    bio_method->method = *bio->method;
    bio_method->stream = stream;

    bio->method->destroy(bio);
    bio->method = (BIO_METHOD*)bio_method;

    bio->method->bwrite = bio_gio_write;
    bio->method->bread = bio_gio_read;
    bio->method->bputs = bio_gio_puts;
    bio->method->destroy = bio_gio_destroy;

    return bio;
}
