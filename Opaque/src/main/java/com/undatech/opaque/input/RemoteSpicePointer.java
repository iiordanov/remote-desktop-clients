/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque.input;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Handler;
import android.view.InputDevice;
import android.view.KeyEvent;

import com.undatech.opaque.RemoteCanvas;
import com.undatech.opaque.SpiceCommunicator;

public class RemoteSpicePointer implements RemotePointer {
    private static final String TAG = "RemoteSpicePointer";

    public static final int SPICE_MOUSE_BUTTON_MOVE   = 0;
    public static final int SPICE_MOUSE_BUTTON_LEFT   = 1;
    public static final int SPICE_MOUSE_BUTTON_MIDDLE = 2;
    public static final int SPICE_MOUSE_BUTTON_RIGHT  = 3;
    public static final int SPICE_MOUSE_BUTTON_UP     = 4;
    public static final int SPICE_MOUSE_BUTTON_DOWN   = 5;

    public static final int POINTER_DOWN_MASK         = 0x8000;
    
    private int prevPointerMask = 0;
    
    /**
     * Current state of "mouse" buttons
     */
    private int pointerMask = 0;
    
    private RemoteCanvas canvas;
    private Context context;
    private Handler handler;
    private SpiceCommunicator spicecomm;
    
    
    /**
     * Indicates where the mouse pointer is located.
     */
    public int pointerX, pointerY;
    
    public RemoteSpicePointer (SpiceCommunicator spicecomm, RemoteCanvas canvas, Handler handler) {
        this.spicecomm = spicecomm;
        this.canvas    = canvas;
        this.context   = canvas.getContext();
        this.handler   = handler;
        pointerX  = canvas.getDesktopWidth() /2;
        pointerY  = canvas.getDesktopHeight()/2;
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
        canvas.reDrawRemotePointer(x, y);
        pointerX=x;
        pointerY=y;
        canvas.reDrawRemotePointer(x, y);
        moveMouseButtonUp (x, y, 0);
    }
    
    /**
     * If necessary move the pointer to be visible.
     */
    public void movePointerToMakeVisible() {
        if (canvas.getMouseFollowPan()) {
            int absX = canvas.getAbsX();
            int absY = canvas.getAbsY();
            int vW = canvas.getVisibleDesktopWidth();
            int vH = canvas.getVisibleDesktopHeight();
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
            pointerMask |= RemoteSpicePointer.SPICE_MOUSE_BUTTON_RIGHT;
            spicecomm.sendPointerEvent(getX(), getY(), combinedMetastate, pointerMask);
            used = true;
        }
        return used;
    }
    
    @Override
    public void leftButtonDown(int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_LEFT | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void middleButtonDown(int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_MIDDLE | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void rightButtonDown(int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_RIGHT | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollUp(int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_UP | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollDown(int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_DOWN | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollLeft(int x, int y, int metaState) {
        // TODO: Protocol does not support scrolling left/right yet.
    }
    
    @Override
    public void scrollRight(int x, int y, int metaState) {
        // TODO: Protocol does not support scrolling left/right yet.
    }

    @Override
    public void moveMouse (int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_MOVE;
        sendPointerEvent (x, y, metaState, true);
    }

    @Override
    public void moveMouseButtonDown (int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_MOVE | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, true);
    }
    
    @Override
    public void moveMouseButtonUp (int x, int y, int metaState) {
        pointerMask = SPICE_MOUSE_BUTTON_MOVE;
        sendPointerEvent (x, y, metaState, true);
    }
    
    @Override
    public void releaseButton(int x, int y, int metaState) {
        pointerMask = prevPointerMask & ~POINTER_DOWN_MASK;
        prevPointerMask = 0;
        sendPointerEvent (x, y, metaState, false);
    }
    
    /**
     * Sends a pointer event to the server.
     * @param x
     * @param y
     * @param metaState
     * @param isMoving
     */
    private void sendPointerEvent(int x, int y, int metaState, boolean isMoving) {
        
        int combinedMetaState = metaState|canvas.getKeyboard().getMetaState();
        
        // Save the previous pointer mask other than action_move, so we can
        // send it with the pointer flag "not down" to clear the action.
        if (!isMoving) {
            // If this is a new mouse down event, release previous button pressed to avoid confusing the remote OS.
            if (prevPointerMask != 0 && prevPointerMask != pointerMask) {
                spicecomm.sendPointerEvent(pointerX, pointerY,
                                            combinedMetaState,
                                            prevPointerMask & ~POINTER_DOWN_MASK);
            }
            prevPointerMask = pointerMask;
        }

        canvas.reDrawRemotePointer(x, y);
        pointerX = x;
        pointerY = y;

        // Do not let mouse pointer leave the bounds of the desktop.
        if ( pointerX < 0) {
            pointerX = 0;
        } else if ( pointerX >= canvas.getDesktopWidth()) {
            pointerX = spicecomm.framebufferWidth()  - 1;
        }
        if ( pointerY < 0) { 
            pointerY=0;
        } else if ( pointerY >= canvas.getDesktopHeight()) {
            pointerY = spicecomm.framebufferHeight() - 1;
        }
        canvas.reDrawRemotePointer(x, y);
        
        spicecomm.sendPointerEvent(pointerX, pointerY, combinedMetaState, pointerMask);
    }
}
