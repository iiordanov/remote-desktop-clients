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
    
    public static final int POINTER_DOWN_MASK = 0x8000;
    
    /**
     * Current state of "mouse" buttons
     */
    protected int pointerMask = 0;
    protected int prevPointerMask = 0;
    
    protected RemoteCanvas canvas;
    protected Context context;
    protected Handler handler;
    protected RfbConnectable protocomm;
    MouseScroller scroller;
    
    public class MouseScroller implements Runnable {
        int delay = 100;
        public int direction = 0;

        @Override
        public void run() {
            if (direction == 0) {
                RemotePointer.this.scrollUp(pointerX, pointerY, 0);
            } else {
                RemotePointer.this.scrollDown(pointerX, pointerY, 0);
            }
            handler.postDelayed(this, delay);
            
        }
    } 

    /**
     * Indicates where the mouse pointer is located.
     */
    public int pointerX, pointerY;
    
    public RemotePointer (RfbConnectable protocomm, RemoteCanvas canvas, Handler handler) {
        this.protocomm = protocomm;
        this.canvas    = canvas;
        this.context   = canvas.getContext();
        this.handler   = handler;
        pointerX  = canvas.getImageWidth()/2;
        pointerY  = canvas.getImageHeight()/2;
        scroller = new MouseScroller();
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
    
    public int getX() {
        return pointerX;
    }

    public int getY() {
        return pointerY;
    }

    public void setX(int newX) {
        pointerX = newX;
    }

    public void setY(int newY) {
        pointerY = newY;
    }

    /**
     * Move mouse pointer to specified coordinates.
     */
    public void movePointer(int x, int y) {
        canvas.invalidateMousePosition();
        pointerX=x;
        pointerY=y;
        canvas.invalidateMousePosition();
        moveMouseButtonUp (x, y, 0);
    }
    
    /**
     * If necessary move the pointer to be visible.
     */
    public void movePointerToMakeVisible() {
        if (canvas.getMouseFollowPan()) {
            int absX = canvas.getAbsX();
            int absY = canvas.getAbsY();
            int vW = canvas.getVisibleWidth();
            int vH = canvas.getVisibleHeight();
            if (pointerX < absX || pointerX >= absX + vW ||
                pointerY < absY || pointerY >= absY + vH) {
                movePointer(absX + vW / 2, absY + vH / 2);
            }
        }
    }

    /**
     * Handles any hardware buttons designated to perform mouse events.
     */
    public boolean hardwareButtonsAsMouseEvents(int keyCode, KeyEvent e, int combinedMetastate) {
        boolean used = false;
        boolean down = (e.getAction() == KeyEvent.ACTION_DOWN) ||
                       (e.getAction() == KeyEvent.ACTION_MULTIPLE);
        if (down)
            pointerMask = POINTER_DOWN_MASK;
        else
            pointerMask = 0;
        
        if (shouldBeRightClick(e)) {
            rightButtonDown(getX(), getY(), combinedMetastate);
            used = true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
                scroller.direction = 0;
            } else {
                scroller.direction = 1;
            }
            
            if (e.getAction() == KeyEvent.ACTION_DOWN) {
                handler.post(scroller);
            } else {
                handler.removeCallbacks(scroller);
            }
            releaseButton(pointerX, pointerY, 0);
            used = true;
        }
        return used;
    }
    
    abstract public void leftButtonDown  (int x, int y, int metaState);
    abstract public void middleButtonDown(int x, int y, int metaState);
    abstract public void rightButtonDown (int x, int y, int metaState);
    abstract public void scrollUp        (int x, int y, int metaState);
    abstract public void scrollDown      (int x, int y, int metaState);
    abstract public void scrollLeft      (int x, int y, int metaState);
    abstract public void scrollRight     (int x, int y, int metaState);
    abstract public void releaseButton   (int x, int y, int metaState);
    abstract public void moveMouse       (int x, int y, int metaState);
    abstract public void moveMouseButtonDown (int x, int y, int metaState);
    abstract public void moveMouseButtonUp   (int x, int y, int metaState);
}
