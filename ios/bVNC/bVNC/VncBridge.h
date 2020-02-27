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

bool getMaintainConnection();
void connectVnc(void (*callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                void (*callback2)(int fbW, int fbH), char*, char*, char*, char*);
void disconnectVnc();
void (*framebuffer_update_callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h);
void (*framebuffer_resize_callback)(int fbW, int fbH);
void freeBuffer(uint8_t*);

#endif /* VncBridge_h */
