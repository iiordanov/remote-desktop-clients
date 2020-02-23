//
//  VncBridge.h
//  bVNC
//
//  Created by iordan iordanov on 2019-12-26.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

#ifndef VncBridge_h
#define VncBridge_h

#include <stdio.h>
#include "rfb/rfbclient.h"

void connectVnc(void (*callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h), char*, char*, char*, char*);
void (*callback_from_swift)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h);

#endif /* VncBridge_h */
