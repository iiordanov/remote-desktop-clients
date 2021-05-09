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
