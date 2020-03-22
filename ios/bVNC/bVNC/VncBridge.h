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
#include "SshPortForwarder.h"

bool getMaintainConnection(void);
void connectVnc(void (*fb_update_callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                void (*fb_resize_callback)(int fbW, int fbH),
                void (*fail_callback)(int8_t *),
                void (*cl_log_callback)(int8_t *),
                char* addr, char* user, char* password, char* ca_path);

void disconnectVnc(void);
void (*framebuffer_update_callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h);
void (*framebuffer_resize_callback)(int fbW, int fbH);
void (*failure_callback)(int8_t *);
void sendKeyEvent(const unsigned char *c);
void sendUniDirectionalKeyEvent(const unsigned char *c, bool down);
bool sendKeyEventInt(int c);
void sendKeyEventWithKeySym(int c);
void sendUniDirectionalKeyEventWithKeySym(int sym, bool down);
void sendPointerEventToServer(int totalX, int totalY, int x, int y, bool firstDown, bool secondDown, bool thirdDown, bool scrollUp, bool scrollDown);
void checkForError(rfbBool res);
void cleanup(char* message, rfbClient *client);

#endif /* VncBridge_h */
