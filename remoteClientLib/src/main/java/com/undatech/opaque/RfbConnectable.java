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

package com.undatech.opaque;

import com.undatech.opaque.input.RemoteKeyboardState;
import java.util.HashMap;
import java.util.Map;

public abstract class RfbConnectable {
    protected Map<Integer, Integer> modifierMap = new HashMap<>();
    public RemoteKeyboardState remoteKeyboardState = null;
    protected boolean debugLogging = false;
    protected int metaState = 0;

    public RfbConnectable(boolean debugLogging) {
        this.debugLogging = debugLogging;
        this.remoteKeyboardState = new RemoteKeyboardState(debugLogging);
    }

    public abstract int framebufferWidth ();
    public abstract int framebufferHeight ();
    public abstract String desktopName ();
    public abstract void requestUpdate (boolean incremental);
    public abstract void requestResolution (int x, int y) throws Exception;
    public abstract void writeClientCutText (String text);
    public abstract void setIsInNormalProtocol (boolean state);
    public abstract boolean isInNormalProtocol();
    public abstract String getEncoding ();
    public abstract void writePointerEvent(int x, int y, int metaState, int pointerMask, boolean relative);
    public abstract void writeKeyEvent(int key, int metaState, boolean down);
    public abstract void writeSetPixelFormat(int bitsPerPixel, int depth, boolean bigEndian,
                                             boolean trueColour, int redMax, int greenMax, int blueMax,
                                             int redShift, int greenShift, int blueShift, boolean fGreyScale);

    public abstract void writeFramebufferUpdateRequest(int x, int y, int w, int h,    boolean b);
    public abstract void close();
    public abstract boolean isCertificateAccepted();
    public abstract void setCertificateAccepted(boolean certificateAccepted);
}
