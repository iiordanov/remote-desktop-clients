//
//  SshPortForwarder.c
//  bVNC
//
//  Created by iordan iordanov on 2020-03-14.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

#include "SshPortForwarder.h"
#include "Utility.h"

#include <netdb.h>
#include <libssh2.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

const char *keyfile1 = NULL;
const char *keyfile2 = NULL;
char *privKeyPassphrase = NULL;
char *privKeyData = NULL;
char *username = NULL;
char *password = NULL;

char *server_ip = NULL;
unsigned int server_ssh_port = 22;

char *local_listenip = NULL;
unsigned int local_listenport = 22222;

char *remote_desthost = NULL; /* resolved by the server */
unsigned int remote_destport = 22;

enum {
    AUTH_NONE = 0,
    AUTH_PASSWORD,
    AUTH_PUBLICKEY
};


int ssh_certificate_verification_callback(int instance, char* fingerprint_sha1, char* fingerprint_sha256) {
    char user_message[1024];

    snprintf(user_message, 1023,
            "SHA1: %s\n\nSHA256: %s\n\n\n",
            fingerprint_sha1, fingerprint_sha256);
    
    int response = yes_no_callback(instance,
                                   (int8_t *)"PLEASE_VERIFY_SSH_CERT_TITLE", (int8_t *)user_message,
                                   (int8_t *)fingerprint_sha256, (int8_t *)fingerprint_sha256, (int8_t *)"SSH", 1);

    return response;
}

void setupSshPortForward(int instance,
                         void (*ssh_forward_success)(void),
                         void (*ssh_forward_failure)(void),
                         void (*cl_log_callback)(int8_t *),
                         int  (*y_n_callback)(int instance, int8_t *, int8_t *, int8_t *, int8_t *, int8_t *, int),
                         char* host, char* port, char* user, char* password, char* privKeyP, char* privKeyD,
                         char* local_ip, char* local_port, char* remote_ip, char* remote_port) {
    client_log_callback = cl_log_callback;
    yes_no_callback = y_n_callback;
    
    int argc = 10;
    char **argv = (char**)malloc(argc*sizeof(char*));
    int i = 0;
    
    char *host_ip = calloc(256, sizeof(char));
    if (resolve_host_to_ip(host, host_ip) != 0) {
        client_log("SSH Unable to resolve %s to an IP\n", host);
        ssh_forward_failure();
        return;
    }
    
    for (i = 0; i < argc; i++) {
        argv[i] = (char*)malloc(256*sizeof(char));
    }
    strcpy(argv[0], "dummy");
    strcpy(argv[1], host_ip);
    strcpy(argv[2], port);
    strcpy(argv[3], user);
    strcpy(argv[4], password);
    strcpy(argv[5], local_ip);
    strcpy(argv[6], local_port);
    strcpy(argv[7], remote_ip);
    strcpy(argv[8], remote_port);
    
    privKeyPassphrase = (char*)malloc(1024*sizeof(char));
    strncpy(privKeyPassphrase, privKeyP, 1023);
    privKeyData = (char*)malloc(16384*sizeof(char));
    strncpy(privKeyData, privKeyD, 16383);

    int res = startForwarding(instance, argc, argv, ssh_forward_success);
    client_log ("SSH Result of SSH forwarding: %d\n", res);
}

int startForwarding(int instance, int argc, char *argv[], void (*ssh_forward_success)(void))
{
    int rc, auth = AUTH_NONE;
    int return_code = 0;
    long i;
    struct sockaddr_in sin;
    struct sockaddr_in6 addr;
    socklen_t sinlen;
    const char *fingerprint_sha1;
    const char *fingerprint_sha256;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel = NULL;
    const char *shost;
    unsigned int sport;
    fd_set fds;
    struct timeval tv;
    ssize_t len, wr;
    char buf[16384];

#ifdef WIN32
    char sockopt;
    SOCKET sock = INVALID_SOCKET;
    SOCKET listensock = INVALID_SOCKET, forwardsock = INVALID_SOCKET;
    WSADATA wsadata;
    int err;

    err = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if(err != 0) {
        client_log("SSH WSAStartup failed with error: %d\n", err);
        return 1;
    }
#else
    int sockopt, sock = -1;
    int listensock = -1, forwardsock = -1;
#endif
    server_ip = malloc(256*sizeof(char));
    username = malloc(256*sizeof(char));
    password = malloc(256*sizeof(char));
    local_listenip = malloc(256*sizeof(char));
    remote_desthost = malloc(256*sizeof(char));

    if(argc > 1)
        strcpy(server_ip, argv[1]);
    if(argc > 2)
        server_ssh_port = atoi(argv[2]);
    if(argc > 3)
        strcpy(username, argv[3]);
    if(argc > 4)
        strcpy(password, argv[4]);
    if(argc > 5)
        strcpy(local_listenip, argv[5]);
    if(argc > 6)
        local_listenport = atoi(argv[6]);
    if(argc > 7)
        strcpy(remote_desthost, argv[7]);
    if(argc > 8)
        remote_destport = atoi(argv[8]);

    rc = libssh2_init(0);
    if(rc) {
        client_log("SSH libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
    
    /* Connect to SSH server */
    
    int is_ipv6 = is_address_ipv6(server_ip);
    if (is_ipv6 == 1) {
        client_log("SSH Address is ipv6, will try to connect over ipv6!\n");
        sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    } else if (is_ipv6 == 0) {
        client_log("SSH Address is ipv4, will try to connect over ipv4!\n");
        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    } else {
        client_log("SSH Unknown address format.!\n");
        return -1;
    }
    
#ifdef WIN32
    if(sock == INVALID_SOCKET) {
        client_log("SSH Failed to open socket!\n");
        return -1;
    }
#else
    if(sock == -1) {
        perror("socket");
        client_log("SSH Error %s open socket!\n", strerror(errno));
        return -1;
    }
#endif
    
    sockopt = 1;
    client_log("SSH Setting socket options SO_NOSIGPIPE, TCP_NODELAY\n");
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &sockopt, sizeof(sockopt));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &sockopt, sizeof(sockopt));

    if (is_ipv6 == 1) {
        client_log("SSH Attempting ipv6 connection\n");
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(server_ssh_port);
        inet_pton(AF_INET6, server_ip, &addr.sin6_addr);
        addr.sin6_port = htons(server_ssh_port);
        if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            client_log("SSH Failed to connect over ipv6!\n");
            return -1;
        }
    } else {
        client_log("SSH Attempting ipv4 connection\n");
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(server_ip);
        if(INADDR_NONE == sin.sin_addr.s_addr) {
            perror("inet_addr");
            return -1;
        }
        sin.sin_port = htons(server_ssh_port);
        if(connect(sock, (struct sockaddr*)(&sin),
                   sizeof(struct sockaddr_in)) != 0) {
            client_log("SSH Failed to connect over ipv4!\n");
            return -1;
        }
	}

    /* Create a session instance */
    client_log("SSH Creating a session instance\n");
    session = libssh2_session_init();
    if(!session) {
        client_log("SSH Could not initialize SSH session!\n");
        return -1;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    client_log("SSH Session handshake\n");
    rc = libssh2_session_handshake(session, sock);
    if(rc) {
        client_log("SSH Error when starting up SSH session: %d\n", rc);
        return -1;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint_sha1 = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    char *fingerprint_sha1_str = get_human_readable_fingerprint((uint8_t *)fingerprint_sha1, 20);
    client_log("SHA1 Fingerprint: %s\n", fingerprint_sha1_str);
    fingerprint_sha256 = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    char *fingerprint_sha256_str = get_human_readable_fingerprint((uint8_t *)fingerprint_sha256, 32);
    client_log("SHA256 Fingerprint: %s\n", fingerprint_sha256_str);
    if (!ssh_certificate_verification_callback(instance, fingerprint_sha1_str, fingerprint_sha256_str)) {
        client_log("SSH User did not accept SSH server certificate.\n");
        goto shutdown;
    }

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username, (uint)strlen(username));
    client_log("SSH Authentication methods: %s\n", userauthlist);
    if(strstr(userauthlist, "password"))
        auth |= AUTH_PASSWORD;
    if(strstr(userauthlist, "publickey"))
        auth |= AUTH_PUBLICKEY;

    /* check for options */
    if(argc > 8) {
        if((auth & AUTH_PASSWORD) && !strcasecmp(argv[8], "-p"))
            auth = AUTH_PASSWORD;
        if((auth & AUTH_PUBLICKEY) && !strcasecmp(argv[8], "-k"))
            auth = AUTH_PUBLICKEY;
    }

    if(auth & AUTH_PASSWORD && strcmp(password, "") != 0) {
        if(libssh2_userauth_password(session, username, password)) {
            client_log("SSH Authentication by password failed.\n");
            return_code = -2;
            goto shutdown;
        }
    }
    else if(auth & AUTH_PUBLICKEY && strcmp(privKeyData, "") != 0) {
        if(libssh2_userauth_publickey_frommemory(session, username, strlen(username), NULL, 0, privKeyData, strlen(privKeyData), privKeyPassphrase)) {
            client_log("SSH Authentication by public key failed!\n");
            return_code = -3;
            goto shutdown;
        }
        client_log("SSH Authentication by public key succeeded.\n");
    }
    else {
        if (strcmp(password, "") == 0 && strcmp(privKeyData, "") == 0) {
            client_log("SSH Both password and private key are empty!\n");
        }
        client_log("SSH No supported authentication methods found!\n");
        return_code = -4;
        goto shutdown;
    }

    listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef WIN32
    if(listensock == INVALID_SOCKET) {
        client_log("SSH Failed to open listen socket!\n");
        return -1;
    }
#else
    if(listensock == -1) {
        perror("socket");
        client_log("SSH Error %s opening listen socket!\n", strerror(errno));
        return -1;
    }
#endif
    sinlen = sizeof(sin);
    memset(&sin, 0, sinlen);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(local_listenport);
    sin.sin_addr.s_addr = inet_addr(local_listenip);
    if(INADDR_NONE == sin.sin_addr.s_addr) {
        perror("inet_addr");
        client_log("SSH Error %s initializing local_listenip %s or local_listenport %d\n",
                   strerror(errno), local_listenip, local_listenport);
        return_code = -5;
        goto shutdown;
    }
    sockopt = 1;
    setsockopt(listensock, SOL_SOCKET, (SO_REUSEADDR|SO_NOSIGPIPE), &sockopt, sizeof(sockopt));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &sockopt, sizeof(sockopt));
    if(-1 == bind(listensock, (struct sockaddr *)&sin, sinlen)) {
        client_log("SSH Error %s binding listensock with local_listenip %s or local_listenport %d\n",
                   strerror(errno), local_listenip, local_listenport);
        perror("bind");
        return_code = -6;
        goto shutdown;
    }
    if(-1 == listen(listensock, 2)) {
        perror("listen");
        client_log("SSH Error %s listening on local_listenip %s or local_listenport %d\n",
                   strerror(errno), local_listenip, local_listenport);
        return_code = -7;
        goto shutdown;
    }

    ssh_forward_success();
    client_log("SSH Waiting for TCP connection on %s:%d...\n",
                inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    forwardsock = accept(listensock, (struct sockaddr *)&sin, &sinlen);
#ifdef WIN32
    if(forwardsock == INVALID_SOCKET) {
        client_log("SSH Failed to accept forward socket!\n");
        return_code = -8;
        goto shutdown;
    }
#else
    if(forwardsock == -1) {
        perror("accept");
        client_log("SSH Error %s accepting forward socket!\n", strerror(errno));
        return_code = -9;
        goto shutdown;
    }
#endif

    shost = inet_ntoa(sin.sin_addr);
    sport = ntohs(sin.sin_port);

    client_log("SSH Forwarding connection from %s:%d here to remote %s:%d\n",
        shost, sport, remote_desthost, remote_destport);

    channel = libssh2_channel_direct_tcpip_ex(session, remote_desthost,
        remote_destport, shost, sport);
    if(!channel) {
        client_log("SSH Could not open the direct-tcpip channel!\n"
                   "(Note that this can be a problem at the server!"
                   "Please review the server logs.)\n");
        return_code = -10;
        goto shutdown;
    }

    /* Must use non-blocking IO hereafter due to the current libssh2 API */
    libssh2_session_set_blocking(session, 0);

    while(1) {
        FD_ZERO(&fds);
        FD_SET(forwardsock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        rc = select(forwardsock + 1, &fds, NULL, NULL, &tv);
        if(-1 == rc) {
            perror("select");
            goto shutdown;
        }
        if(rc && FD_ISSET(forwardsock, &fds)) {
            len = recv(forwardsock, buf, sizeof(buf), 0);
            if(len < 0) {
                perror("read");
                goto shutdown;
            }
            else if(0 == len) {
                client_log("The client at %s:%d disconnected!\n", shost,
                    sport);
                goto shutdown;
            }
            wr = 0;
            while(wr < len) {
                i = libssh2_channel_write(channel, buf + wr, len - wr);
                if(LIBSSH2_ERROR_EAGAIN == i) {
                    continue;
                }
                if(i < 0) {
                    client_log("libssh2_channel_write: %d\n", i);
                    goto shutdown;
                }
                wr += i;
            }
        }
        while(1) {
            len = libssh2_channel_read(channel, buf, sizeof(buf));
            if(LIBSSH2_ERROR_EAGAIN == len)
                break;
            else if(len < 0) {
                client_log("libssh2_channel_read: %d", (int)len);
                goto shutdown;
            }
            wr = 0;
            while(wr < len) {
                i = send(forwardsock, buf + wr, len - wr, 0);
                if(i <= 0) {
                    perror("write");
                    goto shutdown;
                }
                wr += i;
            }
            if(libssh2_channel_eof(channel)) {
                client_log("The server at %s:%d disconnected!\n",
                    remote_desthost, remote_destport);
                goto shutdown;
            }
        }
    }

shutdown:
#ifdef WIN32
    closesocket(forwardsock);
    closesocket(listensock);
#else
    close(forwardsock);
    close(listensock);
#endif
    if(channel)
        libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "Client disconnecting normally");
    libssh2_session_free(session);

#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    libssh2_exit();

    return return_code;
}
