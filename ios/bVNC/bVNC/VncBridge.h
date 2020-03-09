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

bool getMaintainConnection(void);
void connectVnc(void (*callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                void (*callback2)(int fbW, int fbH),
                void (*callback3)(void), char*, char*, char*, char*);
void disconnectVnc(void);
void (*framebuffer_update_callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h);
void (*framebuffer_resize_callback)(int fbW, int fbH);
void (*failure_callback)(void);
void sendKeyEvent(const unsigned char *c);
bool sendKeyEventInt(int c);
void sendKeyEventWithKeySym(int c);
void sendPointerEventToServer(int totalX, int totalY, int x, int y, bool firstDown, bool secondDown, bool thirdDown, bool scrollUp, bool scrollDown);
void checkForError(rfbBool res);
void cleanup(char* message, rfbClient *client);

#endif /* VncBridge_h */
