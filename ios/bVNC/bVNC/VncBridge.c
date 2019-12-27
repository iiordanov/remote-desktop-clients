//
//  VncBridge.c
//  bVNC
//
//  Created by iordan iordanov on 2019-12-26.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

#include "VncBridge.h"

char* HOST_AND_PORT;
char* USERNAME;
char* PASSWORD;

static rfbCredential* get_credential(rfbClient* cl, int credentialType){
    rfbClientLog("Authenticating with credentials\n");
    rfbCredential *c = malloc(sizeof(rfbCredential));
    c->userCredential.username = malloc(RFB_BUF_SIZE);
    c->userCredential.password = malloc(RFB_BUF_SIZE);
    
    if(credentialType != rfbCredentialTypeUser) {
        rfbClientErr("Something else than username and password required for authentication\n");
        return NULL;
    }
    
    rfbClientLog("username and password required for authentication!\n");
    strcpy(c->userCredential.username, USERNAME);
    strcpy(c->userCredential.password, PASSWORD);
    
    /* remove trailing newlines */
    c->userCredential.username[strcspn(c->userCredential.username, "\n")] = 0;
    c->userCredential.password[strcspn(c->userCredential.password, "\n")] = 0;
    
    return c;
}

static char* get_password(rfbClient* cl){
    rfbClientLog("Doing password auth\n\n");
    char *p = malloc(RFB_BUF_SIZE);
    
    rfbClientLog("Password required for authentication!\n");
    strcpy(p, PASSWORD);
    
    /* remove trailing newlines */
    return p;
}

static void update (rfbClient *cl, int x, int y, int w, int h) {
    //rfbClientLog("Update received\n");
    callback_from_swift(cl->frameBuffer, cl->width, cl->height, x, y, w, h);
}

static rfbBool resize (rfbClient *cl) {
    rfbClientLog("Resize RFB Buffer, allocating buffer\n");
    static char first = TRUE;
    rfbClientLog("%d %d", cl->width, cl->height);
    
    if (first) {
        first = FALSE;
    } else {
        free(cl->frameBuffer);
    }
    cl->frameBuffer = (uint8_t*)malloc(4*cl->width*cl->height*sizeof(char));
    update(cl, 0, 0, cl->width, cl->height);
    return TRUE;
}

void connectVnc(void (*callback)(uint8_t *, int fbW, int fbH, int x, int y, int w, int h),
                char* addr, char* user, char* password) {
    printf("Setting up connection");
    
    HOST_AND_PORT = addr;
    USERNAME = user;
    PASSWORD = password;
    callback_from_swift = callback;
    
    rfbClient *cl = NULL;
    
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
    cl=rfbGetClient(8,3,4);
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
