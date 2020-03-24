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

void (*client_log_callback)(int8_t *);
int (*yes_no_callback)(int8_t *, int8_t *, int8_t *, int8_t *, int8_t *);

void client_log(const char *format, ...) {
    va_list args;
    char *message_buffer = malloc(128);
    va_start(args, format);
    vsnprintf(message_buffer, 127, format, args);
    client_log_callback((int8_t*)message_buffer);
    va_end(args);
    free(message_buffer);
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
