package com.iiordanov.bVNC.input;

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RfbConnectable;

import android.R.integer;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Handler;
import android.view.InputDevice;
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
    protected Context context;
    protected Handler handler;
    protected RfbConnectable rfb;
    
    /**
     * Indicates where the mouse pointer is located.
     */
    public int mouseX, mouseY;
    
    public RemotePointer (RfbConnectable r, RemoteCanvas v, Handler h) {
        rfb = r;
        mouseX=rfb.framebufferWidth()/2;
        mouseY=rfb.framebufferHeight()/2;
        vncCanvas = v;
        handler = h;
        context = v.getContext();
    }
    
    protected boolean shouldBeRightClick (KeyEvent e) {
        boolean result = false;
        int keyCode = e.getKeyCode();
        
        // If the camera button is pressed
        if (keyCode == KeyEvent.KEYCODE_CAMERA) {
            result = true;
        // Or the back button is pressed
        } else if (keyCode == KeyEvent.KEYCODE_BACK) {
            // Determine SDK
            boolean preGingerBread = android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD;
            // Whether the source is a mouse (getSource() is not available pre-Gingerbread)
            boolean mouseSource = (!preGingerBread && e.getSource() == InputDevice.SOURCE_MOUSE);
            // Whether the device has a qwerty keyboard
            boolean noQwertyKbd = (context.getResources().getConfiguration().keyboard != Configuration.KEYBOARD_QWERTY);
            // Whether the device is pre-Gingerbread or the event came from the "hard buttons"
            boolean fromVirtualHardKey = preGingerBread || (e.getFlags() & KeyEvent.FLAG_VIRTUAL_HARD_KEY) != 0;
            if (mouseSource || noQwertyKbd || fromVirtualHardKey) {
                result = true;
            }
        }
        
        return result;
    }
    
    abstract public int getX();
    abstract public int getY();
    abstract public void setX(int newX);
    abstract public void setY(int newY);
    abstract public void warpMouse(int x, int y);
    abstract public void mouseFollowPan();
    abstract boolean handleHardwareButtons(int keyCode, KeyEvent evt, int combinedMetastate);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton);
    abstract public boolean processPointerEvent(MotionEvent evt, boolean downEvent, boolean useRightButton);
    abstract public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton);
    
    abstract public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton,
                                        boolean useMiddleButton, boolean useScrollButton, int direction);
}
