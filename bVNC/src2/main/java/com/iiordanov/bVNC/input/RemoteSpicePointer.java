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

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RfbConnectable;

public class RemoteSpicePointer extends RemotePointer {
	private static final String TAG = "RemoteSpicePointer";
    public static final int SPICE_MOUSE_BUTTON_MOVE   = 0;
    public static final int SPICE_MOUSE_BUTTON_LEFT   = 1;
    public static final int SPICE_MOUSE_BUTTON_MIDDLE = 2;
    public static final int SPICE_MOUSE_BUTTON_RIGHT  = 3;
    public static final int SPICE_MOUSE_BUTTON_UP     = 4;
    public static final int SPICE_MOUSE_BUTTON_DOWN   = 5;
    
    public RemoteSpicePointer (RfbConnectable spicecomm, RemoteCanvas canvas, Handler handler) {
        super(spicecomm, canvas, handler);
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
	    if ( pointerX < 0) {
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
}
