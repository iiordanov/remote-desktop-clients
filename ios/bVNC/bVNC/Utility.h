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
extern void (*client_log_callback)(int8_t *);
extern int (*yes_no_callback)(int8_t *, int8_t *, int8_t *, int8_t *);

#endif /* Utility_h */
