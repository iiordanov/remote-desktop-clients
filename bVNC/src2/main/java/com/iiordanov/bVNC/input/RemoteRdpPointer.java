package com.iiordanov.bVNC.input;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.RemoteCanvas;

public class RemoteRdpPointer extends RemotePointer {
    private static final String TAG = "RemoteRdpPointer";

    private final static int PTRFLAGS_WHEEL          = 0x0200;
    private final static int PTRFLAGS_WHEEL_NEGATIVE = 0x0100;
    //private final static int PTRFLAGS_DOWN           = 0x8000;
    
    private final static int MOUSE_BUTTON_NONE       = 0x0000;
    private final static int MOUSE_BUTTON_MOVE       = 0x0800;
    private final static int MOUSE_BUTTON_LEFT       = 0x1000;
    private final static int MOUSE_BUTTON_RIGHT      = 0x2000;

    private static final int MOUSE_BUTTON_MIDDLE      = 0x4000;
    private static final int MOUSE_BUTTON_SCROLL_UP   = PTRFLAGS_WHEEL|0x0078;
    private static final int MOUSE_BUTTON_SCROLL_DOWN = PTRFLAGS_WHEEL|PTRFLAGS_WHEEL_NEGATIVE|0x0088;
    
    public RemoteRdpPointer (RfbConnectable spicecomm, RemoteCanvas canvas, Handler handler) {
        super(spicecomm, canvas, handler);
    }
    
    @Override
    public void leftButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_LEFT | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void middleButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MIDDLE | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, false);
    }
    
    @Override
    public void rightButtonDown(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_RIGHT | POINTER_DOWN_MASK;
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
        // TODO: Protocol does not support scrolling left/right yet.
    }
    
    @Override
    public void scrollRight(int x, int y, int metaState) {
        // TODO: Protocol does not support scrolling left/right yet.
    }

    @Override
    public void moveMouse (int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MOVE | prevPointerMask;
        sendPointerEvent (x, y, metaState, true);
    }

    @Override
    public void moveMouseButtonDown (int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MOVE | POINTER_DOWN_MASK;
        sendPointerEvent (x, y, metaState, true);
    }
    
    @Override
    public void moveMouseButtonUp (int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MOVE;
        sendPointerEvent (x, y, metaState, true);
    }
    
    @Override
    public void releaseButton(int x, int y, int metaState) {
        pointerMask = MOUSE_BUTTON_MOVE;
        sendPointerEvent (x, y, metaState, false);
        prevPointerMask = 0;
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
        if (pointerY < 0) { 
            pointerY = 0;
        } else if ( pointerY >= canvas.getImageHeight()) {
            pointerY = canvas.getImageHeight() - 1;
        }
        canvas.invalidateMousePosition();
        
        protocomm.writePointerEvent(pointerX, pointerY, combinedMetaState, pointerMask);
    }

}
