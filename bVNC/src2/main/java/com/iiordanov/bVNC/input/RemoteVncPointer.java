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

    public RemoteVncPointer (RfbConnectable r, RemoteCanvas v, Handler h) {
        super(r,v,h);
    }

    @Override
    public void leftButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_LEFT;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void middleButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MIDDLE;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void rightButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_RIGHT;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollUp(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_SCROLL_UP | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_SCROLL_DOWN | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollLeft(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_SCROLL_LEFT | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void scrollRight(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_SCROLL_RIGHT | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }

    @Override
    public void moveMouse (int x, int y, int metaState) {
        pointerMask = prevPointerMask;
        sendPointerEvent (x, y, metaState, true);
    }

    @Override
    public void moveMouseButtonDown (int x, int y, int metaState) {
        pointerMask = prevPointerMask | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, true);
    }
    
    @Override
    public void moveMouseButtonUp (int x, int y, int metaState) {
        pointerMask = 0;
        sendPointerEvent (x, y, metaState, true);
    }

    @Override
    public void releaseButton(int x, int y, int metaState) {
        pointerMask = 0;
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
                protocomm.writePointerEvent(pointerX, pointerY, 
                                            combinedMetaState,
                                            prevPointerMask & ~POINTER_DOWN_MASK);
            }
            prevPointerMask = pointerMask;
        }
        
        canvas.invalidateMousePosition();
        pointerX = x;
        pointerY = y;
        
        // Do not let mouse pointer leave the bounds of the desktop.
        if (pointerX < 0) {
            pointerX = 0;
        } else if ( pointerX >= canvas.getImageWidth()) {
            pointerX = canvas.getImageWidth() - 1;
        }
        if ( pointerY < 0) { 
            pointerY = 0;
        } else if ( pointerY >= canvas.getImageHeight()) {
            pointerY = canvas.getImageHeight() - 1;
        }
        canvas.invalidateMousePosition();
        
        protocomm.writePointerEvent(pointerX, pointerY, combinedMetaState, pointerMask);
    }
    
    public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton,
            boolean useMiddleButton, boolean useScrollButton, int direction) {

        if (protocomm != null && protocomm.isInNormalProtocol()) {
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

            canvas.invalidateMousePosition();
            pointerX = x;
            pointerY = y;
            
            // Do not let mouse pointer leave the bounds of the desktop.
            if (pointerX < 0) {
                pointerX = 0;
            } else if ( pointerX >= canvas.getImageWidth()) {
                pointerX = canvas.getImageWidth() - 1;
            }
            if ( pointerY < 0) { 
                pointerY = 0;
            } else if ( pointerY >= canvas.getImageHeight()) {
                pointerY = canvas.getImageHeight() - 1;
            }
            canvas.invalidateMousePosition();
            
            protocomm.writePointerEvent(pointerX, pointerY, modifiers|canvas.getKeyboard().getMetaState(), pointerMask);
            return true;
        }
        return false;
    }
}
