#ifndef _H_USBCLERK
#define _H_USBCLERK

#include <windows.h>

#define USB_CLERK_PIPE_NAME     TEXT("\\\\.\\pipe\\usbclerkpipe")
#define USB_CLERK_MAGIC         0xDADA
#define USB_CLERK_VERSION       0x0003

typedef struct USBClerkHeader {
    UINT16 magic;
    UINT16 version;
    UINT16 type;
    UINT16 size;
} USBClerkHeader;

enum {
    USB_CLERK_DRIVER_INSTALL = 1,
    USB_CLERK_DRIVER_REMOVE,
    USB_CLERK_REPLY,
    USB_CLERK_DRIVER_SESSION_INSTALL,
    USB_CLERK_END_MESSAGE,
};

typedef struct USBClerkDriverOp {
    USBClerkHeader hdr;
    UINT16 vid;
    UINT16 pid;
} USBClerkDriverOp;

typedef struct USBClerkReply {
    USBClerkHeader hdr;
    UINT32 status;
} USBClerkReply;

#endif
