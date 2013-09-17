/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2009 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

#include "ogl_ctx.h"
#include "spice_common.h"

enum {
    OGLCTX_TYPE_PBUF,
    OGLCTX_TYPE_PIXMAP,
};

struct OGLCtx {
    int type;
    Display *x_display;
    GLXContext glx_context;
    GLXDrawable drawable;
};

typedef struct OGLPixmapCtx {
    OGLCtx base;
    Pixmap pixmap;
} OGLPixmapCtx;



const char *oglctx_type_str(OGLCtx *ctx)
{
    static const char *pbuf_str = "pbuf";
    static const char *pixmap_str = "pixmap";
    static const char *invalid_str = "invalid";

    switch (ctx->type) {
    case OGLCTX_TYPE_PBUF:
        return pbuf_str;
    case OGLCTX_TYPE_PIXMAP:
        return pixmap_str;
    default:
        return invalid_str;
    }
}

void oglctx_make_current(OGLCtx *ctx)
{
    if (!glXMakeCurrent(ctx->x_display, ctx->drawable, ctx->glx_context)) {
        printf("%s: failed\n", __FUNCTION__);
    }
}

OGLCtx *pbuf_create(int width, int heigth)
{
    OGLCtx *ctx;
    Display *x_display;
    int num_configs;
    GLXFBConfig *fb_config;
    GLXPbuffer glx_pbuf;
    GLXContext glx_context;

    const int glx_attributes[] = {  GLX_RENDER_TYPE, GLX_RGBA_BIT,
                                    GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                                    GLX_RED_SIZE, 8,
                                    GLX_GREEN_SIZE, 8,
                                    GLX_BLUE_SIZE, 8,
                                    GLX_ALPHA_SIZE, 8,
                                    GLX_STENCIL_SIZE, 4,
                                    0 };

    int pbuf_attrib[] = { GLX_PRESERVED_CONTENTS, True,
                          GLX_PBUFFER_WIDTH, width,
                          GLX_PBUFFER_HEIGHT, heigth,
                          GLX_LARGEST_PBUFFER, False,
                          0, 0 };

    if (!(ctx = calloc(1, sizeof(*ctx)))) {
        printf("%s: alloc pbuf failed\n", __FUNCTION__);
        return NULL;
    }

    if (!(x_display = XOpenDisplay(NULL))) {
        printf("%s: open display failed\n", __FUNCTION__);
        goto error_1;
    }

    if (!(fb_config = glXChooseFBConfig(x_display, 0, glx_attributes, &num_configs)) ||
                                                                            !num_configs) {
        printf("%s: choose fb config failed\n", __FUNCTION__);
        goto error_2;
    }

    if (!(glx_pbuf = glXCreatePbuffer(x_display, fb_config[0], pbuf_attrib))) {
        goto error_3;
    }

    if (!(glx_context = glXCreateNewContext(x_display, fb_config[0], GLX_RGBA_TYPE, NULL, True))) {
        printf("%s: create context failed\n", __FUNCTION__);
        goto error_4;
    }

    XFree(fb_config);

    ctx->type = OGLCTX_TYPE_PBUF;
    ctx->drawable = glx_pbuf;
    ctx->glx_context = glx_context;
    ctx->x_display = x_display;

    return ctx;

error_4:
    glXDestroyPbuffer(x_display, glx_pbuf);

error_3:
    XFree(fb_config);

error_2:
    XCloseDisplay(x_display);

error_1:
    free(ctx);

    return NULL;
}

OGLCtx *pixmap_create(int width, int heigth)
{
    Display *x_display;
    int num_configs;
    GLXFBConfig *fb_config;
    GLXPixmap glx_pixmap;
    GLXContext glx_context;
    Pixmap pixmap;
    int screen;
    Window root_window;
    OGLPixmapCtx *pix;

    const int glx_attributes[] = {  GLX_RENDER_TYPE, GLX_RGBA_BIT,
                                    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
                                    GLX_RED_SIZE, 8,
                                    GLX_GREEN_SIZE, 8,
                                    GLX_BLUE_SIZE, 8,
                                    GLX_ALPHA_SIZE, 8,
                                    GLX_STENCIL_SIZE, 4,
                                    0 };

    if (!(pix = calloc(1, sizeof(*pix)))) {
        printf("%s: alloc pix failed\n", __FUNCTION__);
        return NULL;
    }

    if (!(x_display = XOpenDisplay(NULL))) {
        printf("%s: open display failed\n", __FUNCTION__);
        goto error_1;
    }

    screen = DefaultScreen(x_display);
    root_window = RootWindow(x_display, screen);

    if (!(fb_config = glXChooseFBConfig(x_display, 0, glx_attributes, &num_configs)) ||
                                                                              !num_configs) {
        printf("%s: choose fb config failed\n", __FUNCTION__);
        goto error_2;
    }

    if (!(pixmap = XCreatePixmap(x_display, root_window, width, heigth, 32 /*use fb config*/))) {
        printf("%s: create x pixmap failed\n", __FUNCTION__);
        goto error_3;
    }

    if (!(glx_pixmap = glXCreatePixmap(x_display, fb_config[0], pixmap, NULL))) {
        printf("%s: create glx pixmap failed\n", __FUNCTION__);
        goto error_4;
    }


    if (!(glx_context = glXCreateNewContext(x_display, fb_config[0], GLX_RGBA_TYPE, NULL, True))) {
        printf("%s: create context failed\n", __FUNCTION__);
        goto error_5;
    }

    XFree(fb_config);

    pix->base.type = OGLCTX_TYPE_PIXMAP;
    pix->base.x_display = x_display;
    pix->base.drawable = glx_pixmap;
    pix->base.glx_context = glx_context;
    pix->pixmap = pixmap;

    return &pix->base;

error_5:
    glXDestroyPixmap(x_display, glx_pixmap);

error_4:
    XFreePixmap(x_display, pixmap);

error_3:
    XFree(fb_config);

error_2:
    XCloseDisplay(x_display);

error_1:
    free(pix);

    return NULL;
}

void oglctx_destroy(OGLCtx *ctx)
{
    if (!ctx) {
        return;
    }
    // test is current ?

    glXDestroyContext(ctx->x_display, ctx->glx_context);
    switch (ctx->type) {
    case OGLCTX_TYPE_PBUF:
        glXDestroyPbuffer(ctx->x_display, ctx->drawable);
        break;
    case OGLCTX_TYPE_PIXMAP:
        glXDestroyPixmap(ctx->x_display, ctx->drawable);
        XFreePixmap(ctx->x_display, ((OGLPixmapCtx *)ctx)->pixmap);
        break;
    default:
        spice_error("invalid ogl ctx type");
    }

    XCloseDisplay(ctx->x_display);
    free(ctx);
}
