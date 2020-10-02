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
#include <stdbool.h>

bool getMaintainConnection(void *c);
void *initializeVnc(int instance,
                   bool (*fb_update_callback)(int instance, uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                   void (*fb_resize_callback)(int instance, void *, int fbW, int fbH),
                   void (*fail_callback)(int instance, uint8_t *),
                   void (*cl_log_callback)(int8_t *),
                   void (*lock_wrt_tls_callback)(int instance),
                   void (*unlock_wrt_tls_callback)(int instance),
                   int (*y_n_callback)(int instance, int8_t *, int8_t *, int8_t *, int8_t *, int8_t *, int),
                   char* addr, char* user, char* password);
void connectVnc(void *c);

void disconnectVnc(void *c);
bool (*framebuffer_update_callback)(int, uint8_t *, int fbW, int fbH, int x, int y, int w, int h);
void (*framebuffer_resize_callback)(int, void *, int fbW, int fbH);
void (*failure_callback)(int, uint8_t *);
void sendKeyEvent(void *c, const char *character);
void sendUniDirectionalKeyEvent(void *c, const char *characters, bool down);
bool sendKeyEventInt(void *c, int character);
void sendKeyEventWithKeySym(void *c, int character);
void sendUniDirectionalKeyEventWithKeySym(void *c, int sym, bool down);
void sendPointerEventToServer(void *c, float totalX, float totalY, float x, float y, bool firstDown, bool secondDown, bool thirdDown, bool scrollUp, bool scrollDown);
void checkForError(rfbClient *cl, rfbBool res);
void cleanup(rfbClient *cl, char* message);
void rfb_client_cleanup(rfbClient *cl);
void (*lock_write_tls_callback)(int instance);
void (*unlock_write_tls_callback)(int instance);
void sendWholeScreenUpdateRequest(void *c, bool fullScreenUpdate);
void setMaintainConnection(void *c, int state);
void keepSessionFresh(void *c);
void signal_handler(int signal, siginfo_t *info, void *reserved);

#endif /* VncBridge_h */
