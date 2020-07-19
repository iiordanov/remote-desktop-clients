//
//  Utility.h
//  bVNC
//
//  Created by iordan iordanov on 2020-03-22.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

#ifndef Utility_h
#define Utility_h

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void client_log(const char *format, ...);
char *get_human_readable_fingerprint(uint8_t *raw_fingerprint, uint32_t len);
extern void (*client_log_callback)(int8_t *);
extern int (*yes_no_callback)(int instance, int8_t *, int8_t *, int8_t *, int8_t *, int8_t *, int);
int is_address_ipv6(char * ip_address);
int resolve_host_to_ip(char *hostname , char* ip);

#endif /* Utility_h */
