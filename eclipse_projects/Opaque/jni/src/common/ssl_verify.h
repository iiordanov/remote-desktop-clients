/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

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

#ifndef SSL_VERIFY_H
#define SSL_VERIFY_H

#if defined(WIN32) && !defined(__MINGW32__)
#include <windows.h>
#include <wincrypt.h>
#endif

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef X509_NAME
/* wincrypt.h has already a different define... */
#undef X509_NAME
#endif
#include <openssl/x509v3.h>

#include <spice/macros.h>

SPICE_BEGIN_DECLS

typedef enum {
  SPICE_SSL_VERIFY_OP_NONE     = 0,
  SPICE_SSL_VERIFY_OP_PUBKEY   = (1 << 0),
  SPICE_SSL_VERIFY_OP_HOSTNAME = (1 << 1),
  SPICE_SSL_VERIFY_OP_SUBJECT  = (1 << 2),
} SPICE_SSL_VERIFY_OP;

typedef struct {
    SSL                 *ssl;
    SPICE_SSL_VERIFY_OP verifyop;
    int                 all_preverify_ok;
    char                *hostname;
    char                *pubkey;
    size_t              pubkey_size;
    char                *subject;
    X509_NAME           *in_subject;
} SpiceOpenSSLVerify;

SpiceOpenSSLVerify* spice_openssl_verify_new(SSL *ssl, SPICE_SSL_VERIFY_OP verifyop,
                                             const char *hostname,
                                             const char *pubkey, size_t pubkey_size,
                                             const char *subject);
void spice_openssl_verify_free(SpiceOpenSSLVerify* verify);

SPICE_END_DECLS

#endif // SSL_VERIFY_H
