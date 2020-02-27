//
//  VncBridge.c
//  bVNC
//
//  Created by iordan iordanov on 2019-12-26.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

#include "VncBridge.h"

char* HOST_AND_PORT = NULL;
char* USERNAME = NULL;
char* PASSWORD = NULL;
char* CA_PATH = NULL;
rfbClient *cl = NULL;
uint8_t* pixelBuffer = NULL;
int pixel_buffer_size = 0;
bool maintainConnection = true;
int BYTES_PER_PIXEL = 4;

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
    framebuffer_update_callback(cl->frameBuffer, cl->width, cl->height, x, y, w, h);
}

static rfbBool resize (rfbClient *cl) {
    rfbClientLog("Resize RFB Buffer, allocating buffer\n");
    static char first = TRUE;
    rfbClientLog("Width, height: %d, %d\n", cl->width, cl->height);
    
    if (first) {
        first = FALSE;
    } else {
        free(cl->frameBuffer);
    }
    pixel_buffer_size = BYTES_PER_PIXEL*cl->width*cl->height*sizeof(char);
    cl->frameBuffer = (uint8_t*)malloc(pixel_buffer_size);
    framebuffer_resize_callback(cl->width, cl->height);
    update(cl, 0, 0, cl->width, cl->height);
    return TRUE;
}

void disconnectVnc() {
    maintainConnection = false;
}

void connectVnc(void (*callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                void (*callback2)(int fbW, int fbH),
                char* addr, char* user, char* password, char* ca_path) {
    printf("Setting up connection");
    maintainConnection = true;
    
    HOST_AND_PORT = addr;
    USERNAME = user;
    PASSWORD = password;
    CA_PATH = ca_path;
    framebuffer_update_callback = callback;
    framebuffer_resize_callback = callback2;

    cl = NULL;

    int argc = 2;
    
    char **argv = (char**)malloc(argc*sizeof(char*));
    int i = 0;
    for (i = 0; i < argc; i++) {
        printf("%d\n", i);
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
    
    if (!rfbInitClient(cl, &argc, argv)) {
        cl = NULL; /* rfbInitClient has already freed the client struct */
        //cleanup(cl);
    }
    
    while (cl != NULL) {
        i = WaitForMessage(cl, 500);
        if (maintainConnection != true) {
            printf("Quitting because maintainConnection was set to false.\n");
            break;
        }
        if (i < 0) {
            printf("Quitting because WaitForMessage < 0\n\n");
            //cleanup(cl);
            break;
        }
        if (i) {
            printf("Handling RFB Server Message\n\n");
        }
        
        if (!HandleRFBServerMessage(cl)) {
            //cleanup(cl);
            break;
        }
    }
}
