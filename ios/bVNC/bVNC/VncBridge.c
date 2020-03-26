//
//  VncBridge.c
//  bVNC
//
//  Created by iordan iordanov on 2019-12-26.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

#include <pthread/pthread.h>
#include "VncBridge.h"
#include "ucs2xkeysym.h"
#include "SshPortForwarder.h"
#include "Utility.h"

char* HOST_AND_PORT = NULL;
char* USERNAME = NULL;
char* PASSWORD = NULL;
char* CA_PATH = NULL;
rfbClient *cl = NULL;
int pixel_buffer_size = 0;
bool maintainConnection = true;
int BYTES_PER_PIXEL = 4;
int fbW = 0;
int fbH = 0;

bool getMaintainConnection() {
    return maintainConnection;
}

static rfbCredential* get_credential(rfbClient* cl, int credentialType){
    rfbClientLog("VeNCrypt authentication callback called\n\n");
    rfbCredential *c = malloc(sizeof(rfbCredential));
    
    if(credentialType == rfbCredentialTypeUser) {
        rfbClientLog("Username and password requested for authentication, initializing now\n");
        c->userCredential.username = malloc(RFB_BUF_SIZE);
        c->userCredential.password = malloc(RFB_BUF_SIZE);
        strcpy(c->userCredential.username, USERNAME);
        strcpy(c->userCredential.password, PASSWORD);
        /* remove trailing newlines */
        c->userCredential.username[strcspn(c->userCredential.username, "\n")] = 0;
        c->userCredential.password[strcspn(c->userCredential.password, "\n")] = 0;
    } else if (credentialType == rfbCredentialTypeX509) {
        rfbClientLog("x509 certificates requested for authentication, initializing now\n");
        c->x509Credential.x509CACertFile = malloc(strlen(CA_PATH));
        strcpy(c->x509Credential.x509CACertFile, CA_PATH);
        c->x509Credential.x509CrlVerifyMode = rfbX509CrlVerifyNone;
        c->x509Credential.x509CACrlFile = false;
        c->x509Credential.x509ClientKeyFile = false;
        c->x509Credential.x509ClientCertFile = false;
    }
    
    return c;
}

static char* get_password(rfbClient* cl){
    rfbClientLog("VNC password authentication callback called\n\n");
    char *p = malloc(RFB_BUF_SIZE);
    
    rfbClientLog("Password requested for authentication\n");
    strcpy(p, PASSWORD);
    
    /* remove trailing newlines */
    return p;
}

static void update (rfbClient *cl, int x, int y, int w, int h) {
    //rfbClientLog("Update received\n");
    framebuffer_update_callback(cl->frameBuffer, fbW, fbH, x, y, w, h);
}

static rfbBool resize (rfbClient *cl) {
    rfbClientLog("Resize RFB Buffer, allocating buffer\n");
    fbW = cl->width;
    fbH = cl->height;
    rfbClientLog("Width, height: %d, %d\n", fbW, fbH);
    
    uint8_t* oldFrameBuffer = cl->frameBuffer;
    pixel_buffer_size = BYTES_PER_PIXEL*fbW*fbH*sizeof(char);
    cl->frameBuffer = (uint8_t*)malloc(pixel_buffer_size);
    framebuffer_resize_callback(fbW, fbH);
    update(cl, 0, 0, fbW, fbH);
    if (oldFrameBuffer != NULL) {
        free(oldFrameBuffer);
    }
    return TRUE;
}

void disconnectVnc() {
    printf("Setting maintainConnection to false\n");
    maintainConnection = false;
    // Force force some communication with server in order to wake up the
    // background thread waiting for server messages.
    SendFramebufferUpdateRequest(cl, 0, 0, 1, 1, FALSE);
}

int ssl_certificate_verification_callback(rfbClient* client, char* issuer, char* common_name,
char* fingerprint_sha256, char* fingerprint_sha512, int pday, int psec) {
    char user_message[8192];

    char validity[8];
    snprintf(validity, 8, "Invalid");
    if (pday >= 0 && psec > 0) {
        snprintf(validity, 8, "Valid");
    }

    snprintf(user_message, 8191,
            "Issuer: %s\n\nCommon name: %s\n\nSHA256 Fingerprint: %s\n\nSHA512 Fingerprint: %s\n\n%s for %d days and %d seconds.\n",
            issuer, common_name, fingerprint_sha256, fingerprint_sha512, validity, pday, psec);
    
    int response = yes_no_callback((int8_t *)"Please verify VNC server certificate", (int8_t *)user_message,
                                   (int8_t *)fingerprint_sha256, (int8_t *)fingerprint_sha512, (int8_t *)"X509");

    return response;
}

rfbBool lockWriteToTLS(rfbClient* client) {
    lock_write_tls_callback();
    return TRUE;
}

rfbBool unlockWriteToTLS(rfbClient* client) {
    unlock_write_tls_callback();
    return TRUE;
}

void connectVnc(void (*fb_update_callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                void (*fb_resize_callback)(int fbW, int fbH),
                void (*fail_callback)(int8_t *),
                void (*cl_log_callback)(int8_t *),
                void (*lock_wrt_tls_callback)(void),
                void (*unlock_wrt_tls_callback)(void),
                int (*y_n_callback)(int8_t *, int8_t *, int8_t *, int8_t *, int8_t *),
                char* addr, char* user, char* password, char* ca_path) {
    rfbClientLog("Setting up connection.\n");
    maintainConnection = true;
    
    HOST_AND_PORT = addr;
    USERNAME = user;
    PASSWORD = password;
    CA_PATH = ca_path;
    framebuffer_update_callback = fb_update_callback;
    framebuffer_resize_callback = fb_resize_callback;
    failure_callback = fail_callback;
    client_log_callback = cl_log_callback;
    yes_no_callback = y_n_callback;
    lock_write_tls_callback = lock_wrt_tls_callback;
    unlock_write_tls_callback = unlock_wrt_tls_callback;

    rfbClientLog = rfbClientErr = client_log;
    
    if(cl != NULL) {
        rfb_client_cleanup();
    }
    
    int argc = 2;
    char **argv = (char**)malloc(argc*sizeof(char*));
    int i = 0;
    for (i = 0; i < argc; i++) {
        //rfbClientLog("%d\n", i);
        argv[i] = (char*)malloc(256*sizeof(char));
    }
    strcpy(argv[0], "dummy");
    strcpy(argv[1], HOST_AND_PORT);
    
    /* 16-bit: cl=rfbGetClient(5,3,2); */
    cl=rfbGetClient(8,3,BYTES_PER_PIXEL);
    cl->MallocFrameBuffer=resize;
    cl->canHandleNewFBSize = TRUE;
    cl->GotFrameBufferUpdate=update;
    //cl->HandleKeyboardLedState=kbd_leds;
    //cl->HandleTextChat=text_chat;
    //cl->GotXCutText = got_selection;
    cl->GetCredential = get_credential;
    cl->GetPassword = get_password;
    //cl->listenPort = LISTEN_PORT_OFFSET;
    //cl->listen6Port = LISTEN_PORT_OFFSET;
    cl->SslCertificateVerifyCallback = ssl_certificate_verification_callback;
    cl->LockWriteToTLS = lockWriteToTLS;
    cl->UnlockWriteToTLS = unlockWriteToTLS;
    
    if (!rfbInitClient(cl, &argc, argv)) {
        cl = NULL; /* rfbInitClient has already freed the client struct */
        cleanup("Failed to connect to server\n", cl);
    }
    
    while (cl != NULL) {
        i = WaitForMessage(cl, 500);
        if (maintainConnection != true) {
            cleanup(NULL, cl);
            break;
        }
        if (i < 0) {
            cleanup("Connection to server failed\n", cl);
            break;
        }
        if (i) {
            rfbClientLog("Handling RFB Server Message\n");
        }
        
        if (!HandleRFBServerMessage(cl)) {
            cleanup("Connection to server failed\n", cl);
            break;
        }
    }
    rfbClientLog("Background thread exiting connectVnc function.\n\n");
}

void rfb_client_cleanup() {
    if (cl != NULL) {
        if (cl->frameBuffer != NULL) {
            free(cl->frameBuffer);
        }
        rfbClientCleanup(cl);
        cl = NULL;
    }
}

void cleanup(char *message, rfbClient *client) {
    maintainConnection = false;
    rfbClientLog("%s", message);
    failure_callback((int8_t*)message);
}

// TODO: Replace with real conversion table
struct { char mask; int bits_stored; } utf8Mapping[] = {
        {0b00111111, 6},
        {0b01111111, 7},
        {0b00011111, 5},
        {0b00001111, 4},
        {0b00000111, 3},
        {0,0}
};

/* UTF-8 decoding is from https://rosettacode.org/wiki/UTF-8_encode_and_decode which is under GFDL 1.2 */
static rfbKeySym utf8char2rfbKeySym(const char chr[4]) {
        int bytes = (int)strlen(chr);
        //rfbClientLog("Number of bytes in %s: %d\n", chr, bytes);

        int shift = utf8Mapping[0].bits_stored * (bytes - 1);
        rfbKeySym codep = (*chr++ & utf8Mapping[bytes].mask) << shift;
        int i;
        for(i = 1; i < bytes; ++i, ++chr) {
                shift -= utf8Mapping[0].bits_stored;
                codep |= ((char)*chr & utf8Mapping[0].mask) << shift;
        }
        //rfbClientLog("%s converted to %#06x\n", chr, codep);
        return codep;
}

void sendUniDirectionalKeyEvent(const char *c, bool down) {
    if (!maintainConnection) {
        return;
    }
    rfbKeySym sym = utf8char2rfbKeySym(c);
    //rfbClientLog("sendKeyEvent converted %#06x to xkeysym: %#06x\n", (int)*c, sym);
    sendUniDirectionalKeyEventWithKeySym(sym, down);
}

void sendKeyEvent(const char *c) {
    if (!maintainConnection) {
        return;
    }
    rfbKeySym sym = utf8char2rfbKeySym(c);
    //rfbClientLog("sendKeyEvent converted %#06x to xkeysym: %#06x\n", (int)*c, sym);
    sendKeyEventWithKeySym(sym);
}

bool sendKeyEventInt(int c) {
    if (!maintainConnection) {
        return false;
    }
    rfbKeySym sym = ucs2keysym(c);
    if (sym == -1) {
        return false;
    }
    //rfbClientLog("sendKeyEventInt converted %#06x to xkeysym: %#06x\n", c, sym);
    sendKeyEventWithKeySym(sym);
    return true;
}

void sendKeyEventWithKeySym(int sym) {
    if (!maintainConnection) {
        return;
    }
    if (cl != NULL) {
        //rfbClientLog("Sending xkeysym: %#06x\n", sym);
        checkForError(SendKeyEvent(cl, sym, TRUE));
        checkForError(SendKeyEvent(cl, sym, FALSE));
    } else {
        rfbClientLog("RFB Client object is NULL, need to quit!");
        checkForError(false);
    }
}

void sendUniDirectionalKeyEventWithKeySym(int sym, bool down) {
    if (!maintainConnection) {
        return;
    }
    if (cl != NULL) {
        //rfbClientLog("Sending xkeysym: %#06x\n", sym);
        checkForError(SendKeyEvent(cl, sym, down));
    } else {
        rfbClientLog("RFB Client object is NULL, need to quit!");
        checkForError(false);
    }
}

void sendPointerEventToServer(int totalX, int totalY, int x, int y, bool firstDown, bool secondDown, bool thirdDown,
                              bool scrollUp, bool scrollDown) {
    if (!maintainConnection) {
        return;
    }
    int buttonMask = 0;
    if (firstDown) {
        buttonMask = buttonMask | rfbButton1Mask;
    }
    if (secondDown) {
        buttonMask = buttonMask | rfbButton2Mask;
    }
    if (thirdDown) {
        buttonMask = buttonMask | rfbButton3Mask;
    }
    if (scrollUp) {
        buttonMask = buttonMask | rfbButton4Mask;
    }
    if (scrollDown) {
        buttonMask = buttonMask | rfbButton5Mask;
    }
    if (cl != NULL) {
        int remoteX = (double)fbW * (double)x / (double)totalX;
        int remoteY = (double)fbH * (double)y / (double)totalY;
        printf("Sending pointer event at %d, %d, with mask %d\n", remoteX, remoteY, buttonMask);
        checkForError(SendPointerEvent(cl, remoteX, remoteY, buttonMask));
    } else {
        rfbClientLog("RFB Client object is NULL, will quit now.\n");
        checkForError(false);
    }
}

void checkForError(rfbBool res) {
    if (cl == NULL) {
        cleanup("Unexpectedly, RFB client object is null\n", cl);
    } else if (!res) {
        cleanup("Failed to send message to server\n", cl);
    }
}
