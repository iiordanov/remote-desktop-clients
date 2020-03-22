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
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
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
char *username = NULL;
char *password = NULL;

char *server_ip = NULL;
unsigned int server_ssh_port = 22;

char *local_listenip = NULL;
unsigned int local_listenport = 2222;

char *remote_desthost = NULL; /* resolved by the server */
unsigned int remote_destport = 22;

enum {
    AUTH_NONE = 0,
    AUTH_PASSWORD,
    AUTH_PUBLICKEY
};

void setupSshPortForward(void (*ssh_forward_success)(void),
                         void (*ssh_forward_failure)(void),
                         void (*cl_log_callback)(int8_t *),
                         char* host, char* port, char* user, char* password, char* local_ip,
                         char* local_port, char* remote_ip, char* remote_port) {
    client_log_callback = cl_log_callback;
    int argc = 9;
    char **argv = (char**)malloc(argc*sizeof(char*));
    int i = 0;
    
    char host_ip[256];
    if (resolve_host_to_ip(host , host_ip) != 0) {
        client_log("Unable to resolve %s to an IP\n", host);
        ssh_forward_failure();
        return;
    }
    
    for (i = 0; i <= argc; i++) {
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

    int res = startForwarding(argc, argv, ssh_forward_success);
    client_log ("Result of SSH forwarding: %d\n", res);
    ssh_forward_success();
}

int resolve_host_to_ip(char *hostname , char* ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, hostname, &(sa.sin_addr)) != 0) {
        // This is already an ip address
        client_log("Specified hostname %s is already an IP address\n", hostname);
        strcpy(ip, hostname);
        return 0;
    }
    
    if ((he = gethostbyname(hostname) ) == NULL) {
        client_log("Error calling gethostbyname for %s\n", hostname);
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    
    for(i = 0; addr_list[i] != NULL; i++) {
        strcpy(ip, inet_ntoa(*addr_list[i]));
        client_log("Successfully resolved hostname %s to IP %s\n", hostname, ip);
        return 0;
    }
    
    client_log("Unable to resolve hostname %s\n", hostname);
    return 1;
}

int startForwarding(int argc, char *argv[], void (*ssh_forward_success)(void))
{
    int rc, i, auth = AUTH_NONE;
    struct sockaddr_in sin;
    socklen_t sinlen;
    const char *fingerprint;
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
        fclient_log(stderr, "WSAStartup failed with error: %d\n", err);
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
        client_log("libssh2 initialization failed (%d)\n", rc);
        return 1;
    }

    /* Connect to SSH server */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef WIN32
    if(sock == INVALID_SOCKET) {
        client_log("failed to open socket!\n");
        return -1;
    }
#else
    if(sock == -1) {
        perror("socket");
        return -1;
    }
#endif

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(server_ip);
    if(INADDR_NONE == sin.sin_addr.s_addr) {
        perror("inet_addr");
        return -1;
    }
    sin.sin_port = htons(server_ssh_port);
    if(connect(sock, (struct sockaddr*)(&sin),
               sizeof(struct sockaddr_in)) != 0) {
        client_log("failed to connect!\n");
        return -1;
    }

    /* Create a session instance */
    session = libssh2_session_init();
    if(!session) {
        client_log("Could not initialize SSH session!\n");
        return -1;
    }

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);
    if(rc) {
        client_log("Error when starting up SSH session: %d\n", rc);
        return -1;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    client_log("Fingerprint: ");
    for(i = 0; i < 20; i++)
        client_log("%02X ", (unsigned char)fingerprint[i]);
    client_log("\n");

    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username, strlen(username));
    client_log("Authentication methods: %s\n", userauthlist);
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

    if(auth & AUTH_PASSWORD) {
        if(libssh2_userauth_password(session, username, password)) {
            client_log("Authentication by password failed.\n");
            goto shutdown;
        }
    }
    else if(auth & AUTH_PUBLICKEY) {
        if(libssh2_userauth_publickey_fromfile(session, username, keyfile1,
                                               keyfile2, password)) {
            client_log("\tAuthentication by public key failed!\n");
            goto shutdown;
        }
        client_log("\tAuthentication by public key succeeded.\n");
    }
    else {
        client_log("No supported authentication methods found!\n");
        goto shutdown;
    }

    listensock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef WIN32
    if(listensock == INVALID_SOCKET) {
        fclient_log(stderr, "failed to open listen socket!\n");
        return -1;
    }
#else
    if(listensock == -1) {
        perror("socket");
        return -1;
    }
#endif

    sin.sin_family = AF_INET;
    sin.sin_port = htons(local_listenport);
    sin.sin_addr.s_addr = inet_addr(local_listenip);
    if(INADDR_NONE == sin.sin_addr.s_addr) {
        perror("inet_addr");
        goto shutdown;
    }
    sockopt = 1;
    setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &sockopt,
               sizeof(sockopt));
    sinlen = sizeof(sin);
    if(-1 == bind(listensock, (struct sockaddr *)&sin, sinlen)) {
        perror("bind");
        goto shutdown;
    }
    if(-1 == listen(listensock, 2)) {
        perror("listen");
        goto shutdown;
    }

    ssh_forward_success();
    client_log("Waiting for TCP connection on %s:%d...\n",
        inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    forwardsock = accept(listensock, (struct sockaddr *)&sin, &sinlen);
#ifdef WIN32
    if(forwardsock == INVALID_SOCKET) {
        client_log("failed to accept forward socket!\n");
        goto shutdown;
    }
#else
    if(forwardsock == -1) {
        perror("accept");
        goto shutdown;
    }
#endif

    shost = inet_ntoa(sin.sin_addr);
    sport = ntohs(sin.sin_port);

    client_log("Forwarding connection from %s:%d here to remote %s:%d\n",
        shost, sport, remote_desthost, remote_destport);

    channel = libssh2_channel_direct_tcpip_ex(session, remote_desthost,
        remote_destport, shost, sport);
    if(!channel) {
        client_log("Could not open the direct-tcpip channel!\n"
                "(Note that this can be a problem at the server!"
                " Please review the server logs.)\n");
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

    return 0;
}
