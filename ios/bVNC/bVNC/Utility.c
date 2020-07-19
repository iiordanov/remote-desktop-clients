//
//  Utility.c
//  bVNC
//
//  Created by iordan iordanov on 2020-03-22.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>


void (*client_log_callback)(int8_t *);
int (*yes_no_callback)(int instance, int8_t *, int8_t *, int8_t *, int8_t *, int8_t *, int);

void client_log(const char *format, ...) {
    va_list args;
    static char message_buffer[128];
    va_start(args, format);
    vsnprintf(message_buffer, 127, format, args);
    client_log_callback((int8_t*)message_buffer);
    va_end(args);
}

char *get_human_readable_fingerprint(uint8_t *raw_fingerprint, uint32_t len) {
    uint32_t buflen = len*4;
    char *fingerprint_string = malloc(buflen);
    int pos = 0, i;

    for (i = 0; i < len; ++i) {
        if (i > 0) {
            pos += snprintf(fingerprint_string + pos, buflen - pos, ":");
        }
        pos += snprintf(fingerprint_string + pos, buflen - pos, "%02X", raw_fingerprint[i]);
    }
    return fingerprint_string;
}

int is_address_ipv6(char * ip_address) {
    struct addrinfo hint, *res = NULL;
    int ret, result;
    
    memset(&hint, '\0', sizeof hint);
    
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;
    
    ret = getaddrinfo(ip_address, NULL, &hint, &res);
    
    if (ret != 0) {
        puts("Invalid address");
        puts(gai_strerror(ret));
        result = -1;
    }
    
    if(res->ai_family == AF_INET) {
        result = 0;
    } else if (res->ai_family == AF_INET6) {
        result = 1;
    } else {
        result = -2;
    }
    freeaddrinfo(res);
    return result;
}

int resolve_host_to_ip(char *hostname , char* ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    if (inet_pton(AF_INET, hostname, &(sa.sin_addr)) != 0 || inet_pton(AF_INET6, hostname, &(sa6.sin6_addr)) != 0) {
        // This is already an ip address
        client_log("Specified hostname %s is already an IP address\n", hostname);
        strncpy(ip, hostname, 255);
        return 0;
    }
    
    if ((he = gethostbyname(hostname) ) == NULL) {
        client_log("Error calling gethostbyname for %s\n", hostname);
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    
    if (addr_list[0] != NULL) {
        strncpy(ip, inet_ntoa(*addr_list[0]), 255);
        client_log("Successfully resolved hostname %s to IP %s\n", hostname, ip);
        return 0;
    }
    
    client_log("Unable to resolve hostname %s\n", hostname);
    return 1;
}
