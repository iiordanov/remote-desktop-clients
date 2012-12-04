/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
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

package com.iiordanov.bVNC;

import android.os.SystemClock;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.android.bc.IBCScaleGestureDetector;
import com.iiordanov.android.bc.OnScaleGestureListener;
import com.iiordanov.bVNC.input.RemotePointer;

/**
 * An AbstractInputHandler that uses GestureDetector to detect standard gestures in touch events
 * 
 * @author Michael A. MacDonald
 */
abstract class AbstractGestureInputHandler extends GestureDetector.SimpleOnGestureListener implements AbstractInputHandler, OnScaleGestureListener {
	protected GestureDetector gestures;
	protected IBCScaleGestureDetector scaleGestures;
	/**
	 * Handles to the VncCanvas view and VncCanvasActivity activity.
	 */
	protected VncCanvas vncCanvas;
	protected VncCanvasActivity activity;
	
	/**
	 * Key handler delegate that handles DPad-based mouse motion
	 */
	protected DPadMouseKeyHandler keyHandler;
	
	// This is the initial "focal point" of the gesture (between the two fingers).
	float xInitialFocus;
	float yInitialFocus;
	
	// This is the final "focal point" of the gesture (between the two fingers).
	float xCurrentFocus;
	float yCurrentFocus;
	float xPreviousFocus;
	float yPreviousFocus;
	
	// These variables record whether there was a two-finger swipe performed up or down.
	boolean inSwiping           = false;
	boolean twoFingerSwipeUp    = false;
	boolean twoFingerSwipeDown  = false;
	boolean twoFingerSwipeLeft  = false;
	boolean twoFingerSwipeRight = false;
	
	// These variables indicate whether the dpad should be used as arrow keys
	// and whether it should be rotated.
	boolean useDpadAsArrows    = false;
	boolean rotateDpad         = false;
	boolean trackballButtonDown;
	
	// The variable which indicates how many scroll events to send per swipe event.
	long    swipeSpeed = 1;
	// If swipe events are registered once every baseSwipeTime miliseconds, then
	// swipeSpeed will be one. If more often, swipe-speed goes up, if less, down.
	final long    baseSwipeTime = 600;
	// This is how far the swipe has to travel before a swipe event is generated.
	final float   baseSwipeDist = 40.f;
	
	boolean inScaling           = false;
	boolean scalingJustFinished = false;
	// The minimum distance a scale event has to traverse the FIRST time before scaling starts.
	final double  minScaleFactor = 0.1;
	
	// What action was previously performed by a mouse or stylus.
	int prevMouseOrStylusAction = 0;
	
	/**
	 * In drag and rightDrag mode (entered with long press) you process mouse events
	 * without sending them through the gesture detector
	 */
	protected boolean dragMode       = false;
	protected boolean rightDragMode  = false;
	protected boolean middleDragMode = false;
	protected float   dragX, dragY;

	/**
	 * These variables keep track of which pointers have seen ACTION_DOWN events.
	 */
	protected boolean secondPointerWasDown = false;
	protected boolean thirdPointerWasDown  = false;

	private static final String TAG = "AbstractGestureInputHandler";
	
	AbstractGestureInputHandler(VncCanvasActivity c, VncCanvas v)
	{
		activity = c;
		vncCanvas = v;
		gestures=BCFactory.getInstance().getBCGestureDetector().createGestureDetector(c, this);
		gestures.setOnDoubleTapListener(this);
		scaleGestures=BCFactory.getInstance().getScaleGestureDetector(c, this);
		useDpadAsArrows = activity.getUseDpadAsArrows();
		rotateDpad      = activity.getRotateDpad();
		keyHandler = new DPadMouseKeyHandler(activity, vncCanvas.handler, useDpadAsArrows, rotateDpad);
	}

	
	@Override
	public boolean onTouchEvent(MotionEvent evt) {
		scaleGestures.onTouchEvent(evt);
		return gestures.onTouchEvent(evt);
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.android.bc.OnScaleGestureListener#onScale(com.iiordanov.android.bc.IBCScaleGestureDetector)
	 */
	@Override
	public boolean onScale(IBCScaleGestureDetector detector) {

		boolean consumed = true;

		// Get the current focus.
		xCurrentFocus = detector.getFocusX();
		yCurrentFocus = detector.getFocusY();
		
		// If we haven't started scaling yet, we check whether a swipe is being performed.
		// The arbitrary fudge factor may not be the best way to set a tolerance...
		if (!inScaling) {
			
			// Start swiping mode only after we've moved away from the initial focal point some distance.
			if (!inSwiping) {
				if ( (yCurrentFocus < (yInitialFocus - baseSwipeDist)) ||
			         (yCurrentFocus > (yInitialFocus + baseSwipeDist)) ||
			         (xCurrentFocus < (xInitialFocus - baseSwipeDist)) ||
			         (xCurrentFocus > (xInitialFocus + baseSwipeDist)) ) {
					inSwiping      = true;
					xPreviousFocus = xInitialFocus;
					yPreviousFocus = yInitialFocus;
				}
			}
			
			// If in swiping mode, indicate a swipe at regular intervals.
			if (inSwiping) {
				twoFingerSwipeUp    = false;					
				twoFingerSwipeDown  = false;
				twoFingerSwipeLeft  = false;					
				twoFingerSwipeRight = false;
				if        (yCurrentFocus < (yPreviousFocus - baseSwipeDist)) {
					twoFingerSwipeDown   = true;
					xPreviousFocus = xCurrentFocus;
					yPreviousFocus = yCurrentFocus;
				} else if (yCurrentFocus > (yPreviousFocus + baseSwipeDist)) {
					twoFingerSwipeUp     = true;
					xPreviousFocus = xCurrentFocus;
					yPreviousFocus = yCurrentFocus;
				} else if (xCurrentFocus < (xPreviousFocus - baseSwipeDist)) {
					twoFingerSwipeRight  = true;
					xPreviousFocus = xCurrentFocus;
					yPreviousFocus = yCurrentFocus;
				} else if (xCurrentFocus > (xPreviousFocus + baseSwipeDist)) {
					twoFingerSwipeLeft   = true;
					xPreviousFocus = xCurrentFocus;
					yPreviousFocus = yCurrentFocus;
				} else {
					consumed           = false;
				}
				// The faster we swipe, the faster we traverse the screen, and hence, the 
				// smaller the time-delta between consumed events. We take the reciprocal
				// obtain swipeSpeed. If it goes to zero, we set it to at least one.
				long elapsedTime = detector.getTimeDelta();
				if (elapsedTime < 10) elapsedTime = 10;
				
				swipeSpeed = baseSwipeTime/elapsedTime;
				if (swipeSpeed == 0)  swipeSpeed = 1;
				//if (consumed)        Log.d(TAG,"Current swipe speed: " + swipeSpeed);
			}
		}
		
		if (!inSwiping) {
			if ( !inScaling && Math.abs(1.0 - detector.getScaleFactor()) < minScaleFactor ) {
				//Log.i(TAG,"Not scaling due to small scale factor.");
				consumed = false;
			}

			if (consumed)
			{
				inScaling = true;
				//Log.i(TAG,"Adjust scaling " + detector.getScaleFactor());
				if (activity.vncCanvas != null && activity.vncCanvas.scaling != null)
					activity.vncCanvas.scaling.adjust(activity, detector.getScaleFactor(), xCurrentFocus, yCurrentFocus);
			}
		}
		return consumed;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.android.bc.OnScaleGestureListener#onScaleBegin(com.iiordanov.android.bc.IBCScaleGestureDetector)
	 */
	@Override
	public boolean onScaleBegin(IBCScaleGestureDetector detector) {

		xInitialFocus = detector.getFocusX();
		yInitialFocus = detector.getFocusY();
		inScaling           = false;
		scalingJustFinished = false;
		// Cancel any swipes that may have been registered last time.
		inSwiping           = false;
		twoFingerSwipeUp    = false;					
		twoFingerSwipeDown  = false;
		twoFingerSwipeLeft  = false;					
		twoFingerSwipeRight = false;
		//Log.i(TAG,"scale begin ("+xInitialFocus+","+yInitialFocus+")");
		return true;
	}

	/* (non-Javadoc)
	 * @see com.iiordanov.android.bc.OnScaleGestureListener#onScaleEnd(com.iiordanov.android.bc.IBCScaleGestureDetector)
	 */
	@Override
	public void onScaleEnd(IBCScaleGestureDetector detector) {
		//Log.i(TAG,"scale end");
		inScaling = false;
		inSwiping = false;
		scalingJustFinished = true;
	}
	
	private static int convertTrackballDelta(double delta) {
		return (int) Math.pow(Math.abs(delta) * 6.01, 2.5)
				* (delta < 0.0 ? -1 : 1);
	}

	boolean trackballMouse(MotionEvent evt) {
		
		int dx = convertTrackballDelta(evt.getX());
		int dy = convertTrackballDelta(evt.getY());

		switch (evt.getAction()) {
			case MotionEvent.ACTION_DOWN:
				trackballButtonDown = true;
				break;
			case MotionEvent.ACTION_UP:
				trackballButtonDown = false;
				break;
		}
		
		evt.offsetLocation(vncCanvas.pointer.mouseX + dx - evt.getX(),
							vncCanvas.pointer.mouseY + dy - evt.getY());

		if (vncCanvas.pointer.processPointerEvent(evt, trackballButtonDown))
			return true;
		
		return activity.onTouchEvent(evt);
	}
	
	/**
	 * Apply scroll offset and scaling to convert touch-space coordinates to the corresponding
	 * point on the full frame.
	 * @param e MotionEvent with the original, touch space coordinates.  This event is altered in place.
	 * @return e -- The same event passed in, with the coordinates mapped
	 */
	MotionEvent changeTouchCoordinatesToFullFrame(MotionEvent e)
	{
		//Log.v(TAG, String.format("tap at %f,%f", e.getX(), e.getY()));
		float scale = vncCanvas.getScale();
		
		// Adjust coordinates for Android notification bar.
		e.offsetLocation(0, -1f * vncCanvas.getTop());
		e.setLocation(vncCanvas.getAbsoluteX() + e.getX() / scale, vncCanvas.getAbsoluteY() + e.getY() / scale);
		return e;
	}
	
	protected boolean endDragModesAndScrolling () {
    	vncCanvas.inScrolling = false;
    	if (dragMode || rightDragMode || middleDragMode) {
    		dragMode              = false;
			rightDragMode         = false;
			middleDragMode        = false;
			return true;
    	} else
    		return false;
	}
	
	/**
	 * Handles actions performed by a mouse.
	 * @param e touch or generic motion event
	 * @return
	 */
	protected boolean handleMouseActions (MotionEvent e) {
        final int action     = e.getActionMasked();
		final int bstate     = e.getButtonState();
        RemotePointer p = vncCanvas.getPointer();

		switch (action) {
		// If a mouse button was pressed.
		case MotionEvent.ACTION_DOWN:
			switch (bstate) {
			case MotionEvent.BUTTON_PRIMARY:
				changeTouchCoordinatesToFullFrame(e);
				return p.processPointerEvent(e, true);
			case MotionEvent.BUTTON_SECONDARY:
				changeTouchCoordinatesToFullFrame(e);
	        	return p.processPointerEvent(e, true, true, false);		
			case MotionEvent.BUTTON_TERTIARY:
				changeTouchCoordinatesToFullFrame(e);
	        	return p.processPointerEvent(e, true, false, true);
			}
			break;
		// If a mouse button was released.
		case MotionEvent.ACTION_UP:
			switch (bstate) {
			case MotionEvent.BUTTON_PRIMARY:
			case MotionEvent.BUTTON_SECONDARY:
			case MotionEvent.BUTTON_TERTIARY:
				changeTouchCoordinatesToFullFrame(e);
				return p.processPointerEvent(e, false);
			}
			break;
		// If the mouse was moved between button down and button up.
		case MotionEvent.ACTION_MOVE:
			switch (bstate) {
			case MotionEvent.BUTTON_PRIMARY:
				changeTouchCoordinatesToFullFrame(e);
				return p.processPointerEvent(e, true);
			case MotionEvent.BUTTON_SECONDARY:
				changeTouchCoordinatesToFullFrame(e);
	        	return p.processPointerEvent(e, true, true, false);		
			case MotionEvent.BUTTON_TERTIARY:
				changeTouchCoordinatesToFullFrame(e);
	        	return p.processPointerEvent(e, true, false, true);
			}
		// If the mouse wheel was scrolled.
		case MotionEvent.ACTION_SCROLL:
			float vscroll = e.getAxisValue(MotionEvent.AXIS_VSCROLL);
			float hscroll = e.getAxisValue(MotionEvent.AXIS_HSCROLL);
			int swipeSpeed = 0, direction = 0;
			if (vscroll < 0) {
				swipeSpeed = (int)(-1*vscroll);
				direction = 1;
			} else if (vscroll > 0) {
				swipeSpeed = (int)vscroll;
				direction = 0;
			} else if (hscroll < 0) {
				swipeSpeed = (int)(-1*hscroll);
				direction = 3;
			} else if (hscroll > 0) {
				swipeSpeed = (int)hscroll;
				direction = 2;				
			} else
				return false;
				
			changeTouchCoordinatesToFullFrame(e);   	
        	int numEvents = 0;
        	while (numEvents < swipeSpeed) {
        		p.processPointerEvent(e, false, false, false, true, direction);
        		p.processPointerEvent(e, false);
        		numEvents++;
        	}
			break;
		// If the mouse was moved.
		case MotionEvent.ACTION_HOVER_MOVE:
			changeTouchCoordinatesToFullFrame(e);
			return p.processPointerEvent(e, false, false, false);
		// If a stylus enters hover right after exiting hover, then a stylus tap was
		// performed with its button depressed. We trigger a right-click.
		case MotionEvent.ACTION_HOVER_ENTER:
			if (prevMouseOrStylusAction == MotionEvent.ACTION_HOVER_EXIT) {
	        	changeTouchCoordinatesToFullFrame(e);
	        	p.processPointerEvent(e, true, true, false);
				SystemClock.sleep(50);
	        	// Offset the pointer by one pixel to prevent accidental click and disappearing context menu.
	        	setEventCoordinates(e, p.getX() - 1, p.getY());
	        	p.processPointerEvent(e, false, true, false);
	        	// Put the pointer where it was before the 1px offset.
	        	p.setX(p.getX() + 1);
			}
			break;
		}
		
		prevMouseOrStylusAction = action;
		return false;
	}
	
	
	/**
	 * Modify the event so that the mouse goes where we specify.
	 * @param e event to be modified.
	 * @param x new x coordinate.
	 * @param y new y coordinate.
	 */
	protected void setEventCoordinates(MotionEvent e, int x, int y) {
		e.setLocation((float)x, (float)y);
	}
	
	/**
	 * Modify the event so that the mouse goes where we specify.
	 * @param e event to be modified.
	 * @param x new x coordinate.
	 * @param y new y coordinate.
	 */
	protected void setEventCoordinates(MotionEvent e, float x, float y) {
		e.setLocation(x, y);
	}
}
