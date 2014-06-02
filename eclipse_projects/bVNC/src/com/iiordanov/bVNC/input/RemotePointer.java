package com.iiordanov.bVNC.input;

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RfbConnectable;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

public abstract class RemotePointer {
    
    /**
     * Current and previous state of "mouse" buttons
     */
    protected int pointerMask = 0;
    protected int prevPointerMask = 0;

    protected RemoteCanvas vncCanvas;
    protected Handler handler;
    protected RfbConnectable rfb;

    /**
     * Use camera button as meta key for right mouse button
     */
    protected boolean cameraButtonDown = false;
    
    /**
     * Indicates where the mouse pointer is located.
     */
    public int mouseX, mouseY;

    /**
     * Indicates whether the device has a hardware menu key.
     */
    protected boolean hasMenuKey = false;
    
    public RemotePointer (RfbConnectable r, RemoteCanvas v, Handler h) {
        rfb = r;
        mouseX=rfb.framebufferWidth()/2;
        mouseY=rfb.framebufferHeight()/2;
        vncCanvas = v;
        handler = h;
        hasMenuKey = android.os.Build.VERSION.SDK_INT < 14 ||
                     ViewConfiguration.get(vncCanvas.getContext()).hasPermanentMenuKey();
    }
    
    abstract public int getX();
    abstract public int getY();
    abstract public void setX(int newX);
    abstract public void setY(int newY);
    abstract public void warpMouse(int x, int y);
    abstract public void mouseFollowPan();
    abstract boolean handleHardwareButtons(int keyCode, KeyEvent evt, int combinedMetastate);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, boolean useRightButton);
    abstract public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton);
    
    abstract public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton,
                                        boolean useMiddleButton, boolean useScrollButton, int direction);
}
