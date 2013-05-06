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

#include "mem.h"
#include "ssl_verify.h"

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <ctype.h>

#ifndef SPICE_DEBUG
# define SPICE_DEBUG(format, ...)
#endif

#ifdef WIN32
static int inet_aton(const char* ip, struct in_addr* in_addr)
{
    unsigned long addr = inet_addr(ip);

    if (addr == INADDR_NONE) {
        return 0;
    }
    in_addr->S_un.S_addr = addr;
    return 1;
}
#endif

static int verify_pubkey(X509* cert, const char *key, size_t key_size)
{
    EVP_PKEY* cert_pubkey = NULL;
    EVP_PKEY* orig_pubkey = NULL;
    BIO* bio = NULL;
    int ret = 0;

    if (!key || key_size == 0)
        return 0;

    if (!cert) {
        SPICE_DEBUG("warning: no cert!");
        return 0;
    }

    cert_pubkey = X509_get_pubkey(cert);
    if (!cert_pubkey) {
        SPICE_DEBUG("warning: reading public key from certificate failed");
        goto finish;
    }

    bio = BIO_new_mem_buf((void*)key, key_size);
    if (!bio) {
        SPICE_DEBUG("creating BIO failed");
        goto finish;
    }

    orig_pubkey = d2i_PUBKEY_bio(bio, NULL);
    if (!orig_pubkey) {
        SPICE_DEBUG("reading pubkey from bio failed");
        goto finish;
    }

    ret = EVP_PKEY_cmp(orig_pubkey, cert_pubkey);

    if (ret == 1)
        SPICE_DEBUG("public keys match");
    else if (ret == 0)
        SPICE_DEBUG("public keys mismatch");
    else
        SPICE_DEBUG("public keys types mismatch");

finish:
    if (bio)
        BIO_free(bio);

    if (orig_pubkey)
        EVP_PKEY_free(orig_pubkey);

    if (cert_pubkey)
        EVP_PKEY_free(cert_pubkey);

    return ret;
}

/* from gnutls
 * compare hostname against certificate, taking account of wildcards
 * return 1 on success or 0 on error
 *
 * note: certnamesize is required as X509 certs can contain embedded NULs in
 * the strings such as CN or subjectAltName
 */
static int _gnutls_hostname_compare(const char *certname,
                                    size_t certnamesize, const char *hostname)
{
    /* find the first different character */
    for (; *certname && *hostname && toupper (*certname) == toupper (*hostname);
         certname++, hostname++, certnamesize--)
        ;

    /* the strings are the same */
    if (certnamesize == 0 && *hostname == '\0')
        return 1;

    if (*certname == '*')
        {
            /* a wildcard certificate */

            certname++;
            certnamesize--;

            while (1)
                {
                    /* Use a recursive call to allow multiple wildcards */
                    if (_gnutls_hostname_compare (certname, certnamesize, hostname))
                        return 1;

                    /* wildcards are only allowed to match a single domain
                       component or component fragment */
                    if (*hostname == '\0' || *hostname == '.')
                        break;
                    hostname++;
                }

            return 0;
        }

    return 0;
}

/**
 * From gnutls and spice red_peer.c
 * TODO: switch to gnutls and get rid of this
 *
 * This function will check if the given certificate's subject matches
 * the given hostname.  This is a basic implementation of the matching
 * described in RFC2818 (HTTPS), which takes into account wildcards,
 * and the DNSName/IPAddress subject alternative name PKIX extension.
 *
 * Returns: 1 for a successful match, and 0 on failure.
 **/
static int verify_hostname(X509* cert, const char *hostname)
{
    GENERAL_NAMES* subject_alt_names;
    int found_dns_name = 0;
    struct in_addr addr;
    int addr_len = 0;
    int cn_match = 0;

    if (!cert) {
        SPICE_DEBUG("warning: no cert!");
        return 0;
    }

    // only IpV4 supported
    if (inet_aton(hostname, &addr)) {
        addr_len = sizeof(struct in_addr);
    }

    /* try matching against:
     *  1) a DNS name as an alternative name (subjectAltName) extension
     *     in the certificate
     *  2) the common name (CN) in the certificate
     *
     *  either of these may be of the form: *.domain.tld
     *
     *  only try (2) if there is no subjectAltName extension of
     *  type dNSName
     */

    /* Check through all included subjectAltName extensions, comparing
     * against all those of type dNSName.
     */
    subject_alt_names = (GENERAL_NAMES*)X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);

    if (subject_alt_names) {
        int num_alts = sk_GENERAL_NAME_num(subject_alt_names);
        int i;
        for (i = 0; i < num_alts; i++) {
            const GENERAL_NAME* name = sk_GENERAL_NAME_value(subject_alt_names, i);
            if (name->type == GEN_DNS) {
                found_dns_name = 1;
                if (_gnutls_hostname_compare((char *)ASN1_STRING_data(name->d.dNSName),
                                             ASN1_STRING_length(name->d.dNSName),
                                             hostname)) {
                    SPICE_DEBUG("alt name match=%s", ASN1_STRING_data(name->d.dNSName));
                    GENERAL_NAMES_free(subject_alt_names);
                    return 1;
                }
            } else if (name->type == GEN_IPADD) {
                int alt_ip_len = ASN1_STRING_length(name->d.iPAddress);
                found_dns_name = 1;
                if ((addr_len == alt_ip_len)&&
                    !memcmp(ASN1_STRING_data(name->d.iPAddress), &addr, addr_len)) {
                    SPICE_DEBUG("alt name IP match=%s",
                                inet_ntoa(*((struct in_addr*)ASN1_STRING_data(name->d.dNSName))));
                    GENERAL_NAMES_free(subject_alt_names);
                    return 1;
                }
            }
        }
        GENERAL_NAMES_free(subject_alt_names);
    }

    if (found_dns_name) {
        SPICE_DEBUG("warning: SubjectAltName mismatch");
        return 0;
    }

    /* extracting commonNames */
    X509_NAME* subject = X509_get_subject_name(cert);
    if (subject) {
        int pos = -1;
        X509_NAME_ENTRY* cn_entry;
        ASN1_STRING* cn_asn1;

        while ((pos = X509_NAME_get_index_by_NID(subject, NID_commonName, pos)) != -1) {
            cn_entry = X509_NAME_get_entry(subject, pos);
            if (!cn_entry) {
                continue;
            }
            cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry);
            if (!cn_asn1) {
                continue;
            }

            if (_gnutls_hostname_compare((char*)ASN1_STRING_data(cn_asn1),
                                         ASN1_STRING_length(cn_asn1),
                                         hostname)) {
                SPICE_DEBUG("common name match=%s", (char*)ASN1_STRING_data(cn_asn1));
                cn_match = 1;
                break;
            }
        }
    }

    if (!cn_match)
        SPICE_DEBUG("warning: common name mismatch");

    return cn_match;
}

X509_NAME* subject_to_x509_name(const char *subject, int *nentries)
{
    X509_NAME* in_subject;
    const char *p;
    char *key, *val, *k, *v = NULL;
    int escape;
    enum {
        KEY,
        VALUE
    } state;

    key = (char*)alloca(strlen(subject));
    val = (char*)alloca(strlen(subject));
    in_subject = X509_NAME_new();

    if (!in_subject || !key || !val) {
        SPICE_DEBUG("failed to allocate");
        return NULL;
    }

    *nentries = 0;

    k = key;
    state = KEY;
    for (p = subject;; ++p) {
        escape = 0;
        if (*p == '\\') {
            ++p;
            if (*p != '\\' && *p != ',') {
                SPICE_DEBUG("Invalid character after \\");
                goto fail;
            }
            escape = 1;
        }

        switch (state) {
        case KEY:
            if (*p == 0) {
                if (k == key) /* empty key */
                    goto success;
                goto fail;
            } else if (*p == '=' && !escape) {
                state = VALUE;
                *k = 0;
                v = val;
            } else
                *k++ = *p;
            break;
        case VALUE:
            if (*p == 0 || (*p == ',' && !escape)) {
                if (v == val) /* empty value */
                    goto fail;

                *v = 0;

                if (!X509_NAME_add_entry_by_txt(in_subject, key,
                                                MBSTRING_UTF8,
                                                (const unsigned char*)val,
                                                strlen(val), -1, 0)) {
                    SPICE_DEBUG("warning: failed to add entry %s=%s to X509_NAME",
                                key, val);
                    goto fail;
                }
                *nentries += 1;

                if (*p == 0)
                    goto success;

                state = KEY;
                k = key;
            } else
                *v++ = *p;
        }
    }

success:
    return in_subject;

fail:
    if (in_subject)
        X509_NAME_free(in_subject);

    return NULL;
}

int verify_subject(X509* cert, SpiceOpenSSLVerify* verify)
{
    X509_NAME *cert_subject = NULL;
    int ret;
    int in_entries;

    if (!cert) {
        SPICE_DEBUG("warning: no cert!");
        return 0;
    }

    cert_subject = X509_get_subject_name(cert);
    if (!cert_subject) {
        SPICE_DEBUG("warning: reading certificate subject failed");
        return 0;
    }

    if (!verify->in_subject) {
        verify->in_subject = subject_to_x509_name(verify->subject, &in_entries);
        if (!verify->in_subject) {
            SPICE_DEBUG("warning: no in_subject!");
            return 0;
        }
    }

    /* Note: this check is redundant with the pre-condition in X509_NAME_cmp */
    if (X509_NAME_entry_count(cert_subject) != in_entries) {
        SPICE_DEBUG("subject mismatch: #entries cert=%d, input=%d",
            X509_NAME_entry_count(cert_subject), in_entries);
        return 0;
    }

    ret = X509_NAME_cmp(cert_subject, verify->in_subject);

    if (ret == 0)
        SPICE_DEBUG("subjects match");
    else
        SPICE_DEBUG("subjects mismatch");

    return !ret;
}

static int openssl_verify(int preverify_ok, X509_STORE_CTX *ctx)
{
    int depth;
    SpiceOpenSSLVerify *v;
    SSL *ssl;
    X509* cert;

    ssl = (SSL*)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    v = (SpiceOpenSSLVerify*)SSL_get_app_data(ssl);

    depth = X509_STORE_CTX_get_error_depth(ctx);
    if (depth > 0) {
        if (!preverify_ok) {
            SPICE_DEBUG("openssl verify failed at depth=%d", depth);
            v->all_preverify_ok = 0;
            return 0;
        } else
            return 1;
    }

    /* depth == 0 */
    cert = X509_STORE_CTX_get_current_cert(ctx);
    if (!cert) {
        SPICE_DEBUG("failed to get server certificate");
        return 0;
    }

    if (v->verifyop & SPICE_SSL_VERIFY_OP_PUBKEY &&
        verify_pubkey(cert, v->pubkey, v->pubkey_size))
        return 1;

    if (!v->all_preverify_ok || !preverify_ok)
        return 0;

    if (v->verifyop & SPICE_SSL_VERIFY_OP_HOSTNAME &&
        verify_hostname(cert, v->hostname))
        return 1;

    if (v->verifyop & SPICE_SSL_VERIFY_OP_SUBJECT &&
        verify_subject(cert, v))
        return 1;

    return 0;
}

SpiceOpenSSLVerify* spice_openssl_verify_new(SSL *ssl, SPICE_SSL_VERIFY_OP verifyop,
                                             const char *hostname,
                                             const char *pubkey, size_t pubkey_size,
                                             const char *subject)
{
    SpiceOpenSSLVerify *v;

    if (!verifyop)
        return NULL;

    v = spice_new0(SpiceOpenSSLVerify, 1);

    v->ssl              = ssl;
    v->verifyop         = verifyop;
    v->hostname         = spice_strdup(hostname);
    v->pubkey           = (char*)spice_memdup(pubkey, pubkey_size);
    v->pubkey_size      = pubkey_size;
    v->subject          = spice_strdup(subject);

    v->all_preverify_ok = 1;

    SSL_set_app_data(ssl, v);
    SSL_set_verify(ssl,
                   SSL_VERIFY_PEER, openssl_verify);

    return v;
}

void spice_openssl_verify_free(SpiceOpenSSLVerify* verify)
{
    if (!verify)
        return;

    free(verify->pubkey);
    free(verify->subject);
    free(verify->hostname);

    if (verify->in_subject)
        X509_NAME_free(verify->in_subject);

    if (verify->ssl)
        SSL_set_app_data(verify->ssl, NULL);
    free(verify);
}
