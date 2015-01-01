package com.iiordanov.bVNC.input;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.RemoteCanvas;

public class RemoteVncPointer extends RemotePointer {
    private static final String TAG = "RemotePointer";
    
    public static final int MOUSE_BUTTON_NONE = 0;
    public static final int MOUSE_BUTTON_LEFT = 1;
    public static final int MOUSE_BUTTON_MIDDLE = 2;
    public static final int MOUSE_BUTTON_RIGHT = 4;
    public static final int MOUSE_BUTTON_SCROLL_UP = 8;
    public static final int MOUSE_BUTTON_SCROLL_DOWN = 16;
    public static final int MOUSE_BUTTON_SCROLL_LEFT = 32;
    public static final int MOUSE_BUTTON_SCROLL_RIGHT = 64;

    public MouseScrollRunnable scrollRunnable;

    public RemoteVncPointer (RfbConnectable r, RemoteCanvas v, Handler h) {
        super(r,v,h);
        scrollRunnable = new MouseScrollRunnable();
    }

    public int getX() {
        return mouseX;
    }

    public int getY() {
        return mouseY;
    }

    public void setX(int newX) {
        mouseX = newX;
    }

    public void setY(int newY) {
        mouseY = newY;
    }

    /**
     * Warp the mouse to x, y in the RFB coordinates
     * 
     * @param x
     * @param y
     */
    public void warpMouse(int x, int y)
    {
        vncCanvas.invalidateMousePosition();
        mouseX=x;
        mouseY=y;
        vncCanvas.invalidateMousePosition();
        //android.util.Log.i(TAG, "warp mouse to " + x + "," + y);
        rfb.writePointerEvent(x, y, 0, MOUSE_BUTTON_NONE);
    }
    
    public void mouseFollowPan()
    {
        if (vncCanvas.getMouseFollowPan())
        {
            int scrollx = vncCanvas.getAbsoluteX();
            int scrolly = vncCanvas.getAbsoluteY();
            int width = vncCanvas.getVisibleWidth();
            int height = vncCanvas.getVisibleHeight();
            //Log.i(TAG,"scrollx " + scrollx + " scrolly " + scrolly + " mouseX " + mouseX +" Y " + mouseY + " w " + width + " h " + height);
            if (mouseX < scrollx || mouseX >= scrollx + width || mouseY < scrolly || mouseY >= scrolly + height) {
                warpMouse(scrollx + width/2, scrolly + height / 2);
            }
        }
    }
    
    /**
     * Moves the scroll while the volume key is held down
     * 
     * @author Michael A. MacDonald
     */
    public class MouseScrollRunnable implements Runnable
    {
        int delay = 100;
        
        public int scrollButton = 0;
        
        /* (non-Javadoc)
         * @see java.lang.Runnable#run()
         */
        @Override
        public void run() {
            if (rfb != null && rfb.isInNormalProtocol()) {
                rfb.writePointerEvent(mouseX, mouseY, 0, scrollButton);
                rfb.writePointerEvent(mouseX, mouseY, 0, 0);                
                handler.postDelayed(this, delay);
            }
        }        
    }

    public boolean handleHardwareButtons(int keyCode, KeyEvent evt, int combinedMetastate) {
        boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
                   (evt.getAction() == KeyEvent.ACTION_MULTIPLE);

        int mouseChange = keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ? RemoteVncPointer.MOUSE_BUTTON_SCROLL_DOWN : RemoteVncPointer.MOUSE_BUTTON_SCROLL_UP;
        if (shouldBeRightClick (evt)) {
            if (down)
                pointerMask = RemoteVncPointer.MOUSE_BUTTON_RIGHT;
            else
                pointerMask = 0;
            rfb.writePointerEvent(getX(), getY(), combinedMetastate, pointerMask);
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            if (evt.getAction() == KeyEvent.ACTION_DOWN) {
                // If not auto-repeat
                if (scrollRunnable.scrollButton != mouseChange) {
                    pointerMask |= mouseChange;
                    scrollRunnable.scrollButton = mouseChange;
                    handler.postDelayed(scrollRunnable, 200);
                }
            } else {
                handler.removeCallbacks(scrollRunnable);
                scrollRunnable.scrollButton = 0;
                pointerMask &= ~mouseChange;
            }
            rfb.writePointerEvent(getX(), getY(), combinedMetastate, pointerMask);
            return true;
        }
        return false;
    }
    
    /**
     *  Overloaded processPointerEvent method which supports mouse scroll button.
     * @param evt motion event; x and y must already have been converted from screen coordinates
     * to remote frame buffer coordinates.
     * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
     * @param useRightButton If true, event is interpreted as happening with right mouse button
     * @param useMiddleButton If true, event is interpreted as click happening with middle mouse button
     * @param useScrollButton If true, event is interpreted as click happening with mouse scroll button
     * @param direction Indicates the direction of the scroll event: 0 for up, 1 for down, 2 for left, 3 for right.
     * @return true if event was actually sent
     */
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction) {
        return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getActionMasked(), 
                                    evt.getMetaState(), downEvent, useRightButton, useMiddleButton, useScrollButton, direction);
    }
    
    /**
     *  Overloaded processPointerEvent method which supports middle mouse button.
     * @param evt motion event; x and y must already have been converted from screen coordinates
     * to remote frame buffer coordinates.
     * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
     * @param useRightButton If true, event is interpreted as happening with right mouse button
     * @param useMiddleButton If true, event is interpreted as click happening with middle mouse button
     * @return true if event was actually sent
     */
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton) {
        return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getActionMasked(), 
                                    evt.getMetaState(), downEvent, useRightButton, useMiddleButton, false, -1);
    }

    /**
     * Convert a motion event to a format suitable for sending over the wire
     * @param evt motion event; x and y must already have been converted from screen coordinates
     * to remote frame buffer coordinates.
     * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
     * @param useRightButton If true, event is interpreted as happening with right mouse button
     * @return true if event was actually sent
     */
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, boolean useRightButton) {
        return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getAction(), 
                                    evt.getMetaState(), downEvent, useRightButton, false, false, -1);
    }

    /**
     *  Overloaded processPointerEvent method which supports right mouse button.
     * @param evt motion event; x and y must already have been converted from screen coordinates
     * to remote frame buffer coordinates.
     * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
     * @param useRightButton If true, event is interpreted as happening with right mouse button
     * @return true if event was actually sent
     */
    public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton) {
        return processPointerEvent(x, y, action, modifiers, mouseIsDown, useRightButton, false, false, -1);
    }
    
    public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton,
                                        boolean useMiddleButton, boolean useScrollButton, int direction) {
        
        if (rfb != null && rfb.isInNormalProtocol()) {
            if (mouseIsDown && useRightButton) {
                //Log.i(TAG,"Right mouse button mask set");
                pointerMask = MOUSE_BUTTON_RIGHT;
            } else if (mouseIsDown && useMiddleButton) {
                //Log.i(TAG,"Middle mouse button mask set");
                pointerMask = MOUSE_BUTTON_MIDDLE;
            } else if (mouseIsDown && useScrollButton) {
                //Log.d(TAG, "Sending a Mouse Scroll event: " + direction);
                if        ( direction == 0 ) {
                    pointerMask = MOUSE_BUTTON_SCROLL_UP;
                } else if ( direction == 1 ) {
                    pointerMask = MOUSE_BUTTON_SCROLL_DOWN;
                } else if ( direction == 2 ) {
                    pointerMask = MOUSE_BUTTON_SCROLL_LEFT;
                } else if ( direction == 3 ) {
                    pointerMask = MOUSE_BUTTON_SCROLL_RIGHT;
                }
            } else if (mouseIsDown && (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE)) {
                //Log.i(TAG,"Left mouse button mask set");
                pointerMask = MOUSE_BUTTON_LEFT;
            } else {
                //Log.i(TAG,"Mouse button mask cleared");
                // If none of the conditions are satisfied, clear the pointer mask.
                pointerMask = 0;
            }
                        
            vncCanvas.invalidateMousePosition();
            mouseX = x;
            mouseY = y;
            if ( mouseX < 0) mouseX=0;
            else if ( mouseX >= rfb.framebufferWidth())  mouseX = rfb.framebufferWidth() - 1;
            if ( mouseY < 0) mouseY=0;
            else if ( mouseY >= rfb.framebufferHeight()) mouseY = rfb.framebufferHeight() - 1;
            vncCanvas.invalidateMousePosition();

            rfb.writePointerEvent(mouseX, mouseY, modifiers|vncCanvas.getKeyboard().getMetaState(), pointerMask);
            return true;
        }
        return false;        
    }
    
}
