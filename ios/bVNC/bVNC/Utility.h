/**
 * Copyright (C) 2021- Morpheusly Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

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
