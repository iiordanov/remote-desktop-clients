/**
 * Copyright (C) 2012 Iordan Iordanov
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.bVNC;

import java.io.IOException;

public interface RfbConnectable {
    int framebufferWidth ();
    int framebufferHeight ();
    String desktopName ();
    void requestUpdate (boolean incremental);
    void requestResolution (int x, int y);
    void writeClientCutText (String text);
    public void setIsInNormalProtocol (boolean state);
    boolean isInNormalProtocol();
    String getEncoding ();
    void writePointerEvent(int x, int y, int metaState, int pointerMask);
    void writeKeyEvent(int key, int metaState, boolean down);
    void writeSetPixelFormat(int bitsPerPixel, int depth, boolean bigEndian,
               boolean trueColour, int redMax, int greenMax, int blueMax,
               int redShift, int greenShift, int blueShift, boolean fGreyScale);
    void writeFramebufferUpdateRequest(int x, int y, int w, int h,    boolean b);
    void close();
}
