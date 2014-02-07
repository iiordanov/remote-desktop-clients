#ifndef _H_QXL_WINDOWS
#define _H_QXL_WINDOWS

#include <spice/types.h>

#include <spice/start-packed.h>

enum {
    QXL_ESCAPE_SET_CUSTOM_DISPLAY = 0x10001,
};

typedef struct SPICE_ATTR_PACKED QXLEscapeSetCustomDisplay {
    uint32_t xres;
    uint32_t yres;
    uint32_t bpp;
} QXLEscapeSetCustomDisplay;

#include <spice/end-packed.h>

#endif /* _H_QXL_WINDOWS */
