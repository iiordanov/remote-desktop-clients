package com.iiordanov.bVNC.input;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.iiordanov.bVNC.RfbConnectable;
import com.iiordanov.bVNC.VncCanvas;

public class RemoteSpicePointer implements RemotePointer {
	private static final String TAG = "RemoteSpicePointer";

	public static final int SPICE_MOUSE_BUTTON_MOVE   = 0;
	public static final int SPICE_MOUSE_BUTTON_LEFT   = 1;
	public static final int SPICE_MOUSE_BUTTON_MIDDLE = 2;
	public static final int SPICE_MOUSE_BUTTON_RIGHT  = 3;
	public static final int SPICE_MOUSE_BUTTON_UP     = 4;
	public static final int SPICE_MOUSE_BUTTON_DOWN   = 5;

	public static final int PTRFLAGS_DOWN             = 0x8000;
	
	private int prevPointerMask = 0;
	
	/**
	 * Current state of "mouse" buttons
	 */
	private int pointerMask = 0;

	private VncCanvas vncCanvas;
	private Handler handler;
	private RfbConnectable rfb;
	public MouseScrollRunnable scrollRunnable;

	/**
	 * Use camera button as meta key for right mouse button
	 */
	boolean cameraButtonDown = false;
	
	/**
	 * Indicates where the mouse pointer is located.
	 */
	public int mouseX, mouseY;


	public RemoteSpicePointer (RfbConnectable r, VncCanvas v, Handler h) {
		rfb = r;
		mouseX=rfb.framebufferWidth()/2;
		mouseY=rfb.framebufferHeight()/2;
		vncCanvas = v;
		handler = h;
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
		//processPointerEvent(getX(), getY(), MotionEvent.ACTION_MOVE, 0, false, false, false, false, 0);
		rfb.writePointerEvent(x, y, 0, SPICE_MOUSE_BUTTON_MOVE);
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
				rfb.writePointerEvent(mouseX, mouseY, 0, scrollButton|PTRFLAGS_DOWN);
				try {Thread.sleep(2);} catch (InterruptedException e) {}
				rfb.writePointerEvent(mouseX, mouseY, 0, scrollButton);				
				handler.postDelayed(this, delay);
			}
		}		
	}

	public boolean handleHardwareButtons(int keyCode, KeyEvent evt, int combinedMetastate) {
		boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
						(evt.getAction() == KeyEvent.ACTION_MULTIPLE);
		if (down)
			pointerMask = PTRFLAGS_DOWN;
		else
			pointerMask = 0;

		int mouseChange = 0;
		//int direction = 0;
		if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
			mouseChange = RemoteRdpPointer.MOUSE_BUTTON_SCROLL_DOWN;
			//direction = 1;
		} else {
			mouseChange = RemoteRdpPointer.MOUSE_BUTTON_SCROLL_UP;
			//direction = 0;
		}
		
		if (keyCode == KeyEvent.KEYCODE_CAMERA) {
			cameraButtonDown = down;
			pointerMask |= RemoteRdpPointer.MOUSE_BUTTON_RIGHT;
			//processPointerEvent(getX(), getY(), evt.getAction(), combinedMetastate, down, true, false, false, direction);
			rfb.writePointerEvent(getX(), getY(), combinedMetastate, pointerMask);
			return true;
		} else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
			pointerMask |= mouseChange;
			if (down) {
				// If not auto-repeat
				if (scrollRunnable.scrollButton != mouseChange) {
					scrollRunnable.scrollButton = mouseChange;
					handler.postDelayed(scrollRunnable, 200);
				}
			} else {
				handler.removeCallbacks(scrollRunnable);
				scrollRunnable.scrollButton = 0;
			}
			//processPointerEvent(getX(), getY(), evt.getAction(), combinedMetastate, down, false, false, true, direction);
			rfb.writePointerEvent(getX(), getY(), combinedMetastate, pointerMask);
			return true;
		}
		return false;
	}
	
	
	/**
	 * Convert a motion event to a format suitable for sending over the wire
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.  cameraButton flag is interpreted as second mouse
	 * button
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @return true if event was actually sent
	 */
	public boolean processPointerEvent(MotionEvent evt, boolean downEvent)
	{
		return processPointerEvent(evt, downEvent, cameraButtonDown);
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
	 * Overloaded processPointerEvent method which supports right mouse button.
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
			if (useRightButton) {
				//android.util.Log.e("", "Mouse button right");
				pointerMask = SPICE_MOUSE_BUTTON_RIGHT;
			} else if (useMiddleButton) {
				//android.util.Log.e("", "Mouse button middle");
				pointerMask = SPICE_MOUSE_BUTTON_MIDDLE;
			} else if (action == MotionEvent.ACTION_DOWN) {
				//android.util.Log.e("", "Mouse button left");
				pointerMask = SPICE_MOUSE_BUTTON_LEFT;
			} else if (useScrollButton) {
				if        ( direction == 0 ) {
					//android.util.Log.e("", "Scrolling up");
					pointerMask = SPICE_MOUSE_BUTTON_UP;
				} else if ( direction == 1 ) {
					//android.util.Log.e("", "Scrolling down");
					pointerMask = SPICE_MOUSE_BUTTON_DOWN;
				}
		    } else if (action == MotionEvent.ACTION_MOVE) {
				//android.util.Log.e("", "Mouse moving");
				pointerMask = SPICE_MOUSE_BUTTON_MOVE;
			} else {
				//android.util.Log.e("", "Setting previous mouse action with mouse not down.");
				// If none of the conditions are satisfied, then set the pointer mask to
				// the previous mask so we can unpress any pressed buttons.
		        pointerMask = prevPointerMask;
		    }
			
			// Save the previous pointer mask other than action_move, so we can
			// send it with the pointer flag "not down" to clear the action.
			if (pointerMask != SPICE_MOUSE_BUTTON_MOVE) {
				// If this is a new mouse down event, release previous button pressed to avoid confusing the remote OS.
				if (prevPointerMask != 0 && prevPointerMask != pointerMask) {
					rfb.writePointerEvent(mouseX, mouseY, modifiers|vncCanvas.getKeyboard().getMetaState(), prevPointerMask & ~PTRFLAGS_DOWN);
				}
				prevPointerMask = pointerMask;
			}
			
			if (mouseIsDown /*&& pointerMask != MOUSE_BUTTON_MOVE*/) {
				//android.util.Log.e("", "Mouse pointer is down");
				pointerMask = pointerMask | PTRFLAGS_DOWN;
			} else {
				//android.util.Log.e("", "Mouse pointer is up");
				prevPointerMask = 0;
			}
						
			vncCanvas.invalidateMousePosition();
		    mouseX = x;
		    mouseY = y;
		    if ( mouseX < 0) mouseX=0;
		    else if ( mouseX >= rfb.framebufferWidth())  mouseX = rfb.framebufferWidth()  - 1;
		    if ( mouseY < 0) mouseY=0;
		    else if ( mouseY >= rfb.framebufferHeight()) mouseY = rfb.framebufferHeight() - 1;
		    vncCanvas.invalidateMousePosition();
		    
		    rfb.writePointerEvent(mouseX, mouseY, modifiers|vncCanvas.getKeyboard().getMetaState(), pointerMask);
			return true;
		}
		return false;
	}
}
