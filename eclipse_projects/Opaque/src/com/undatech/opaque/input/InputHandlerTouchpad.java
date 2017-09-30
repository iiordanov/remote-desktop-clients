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

import android.view.MotionEvent;
import android.os.Vibrator;

import com.undatech.opaque.R;
import com.undatech.opaque.RemoteCanvas;
import com.undatech.opaque.RemoteCanvasActivity;
import com.undatech.opaque.input.RemotePointer;

public class InputHandlerTouchpad extends InputHandlerGeneric {
	static final String TAG = "InputHandlerTouchpad";
	public static final String ID = "Touchpad";
	float sensitivity = 0;
	boolean acceleration = false;

	public InputHandlerTouchpad(RemoteCanvasActivity activity, RemoteCanvas canvas, Vibrator myVibrator) {
		super(activity, canvas, myVibrator);
		acceleration = activity.getAccelerationEnabled();
		sensitivity = activity.getSensitivity();
	}

	/*
	 * (non-Javadoc)
	 * @see com.undatech.opaque.input.InputHandler#getDescription()
	 */
	@Override
	public String getDescription() {
		return canvas.getResources().getString(R.string.input_method_touchpad_description);
	}

	/*
	 * (non-Javadoc)
	 * @see com.undatech.opaque.input.InputHandler#getId()
	 */
	@Override
	public String getId() {
		return ID;
	}
	
	/*
	 * (non-Javadoc)
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent, android.view.MotionEvent, float, float)
	 */
	@Override
	public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        RemotePointer p = canvas.getPointer();
        final int action = e2.getActionMasked();
        final int meta   = e2.getMetaState();
        
        // If we are scaling, allow panning around by moving two fingers around the screen
        if (inScaling) {
    		float scale = canvas.getZoomFactor();
    		activity.showToolbar();
    		canvas.relativePan((int)(distanceX*scale), (int)(distanceY*scale));
        } else {
	        // TODO: This is a workaround for Android 4.2
			boolean twoFingers = false;
			if (e1 != null)
				twoFingers = (e1.getPointerCount() > 1);
			if (e2 != null)
				twoFingers = twoFingers || (e2.getPointerCount() > 1);
	
			// onScroll called while scaling/swiping gesture is in effect. We ignore the event and pretend it was
			// consumed. This prevents the mouse pointer from flailing around while we are scaling.
			// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
			// to stick a spiteful onScroll with a MASSIVE delta here. 
			// This would cause the mouse pointer to jump to another place suddenly.
			// Hence, we ignore onScroll after scaling until we lift all pointers up.
			if (twoFingers||inSwiping||scalingJustFinished)
				return true;
	
			activity.showToolbar();
	
	        // If the gesture has just began, then don't allow a big delta to prevent
	        // pointer jumps at the start of scrolling.
	        if (!inScrolling) {
	            inScrolling = true;
	            distanceX = getSign(distanceX);
	            distanceY = getSign(distanceY);
	            distXQueue.clear();
	            distYQueue.clear();
	        }
	        
	        distXQueue.add(distanceX);
	        distYQueue.add(distanceY);

	        // Only after the first two events have arrived do we start using distanceX and Y
	        // In order to effectively discard the last two events (which are typically unreliable
	        // because of the finger lifting off).
	        if (distXQueue.size() > 2) {
	            distanceX = distXQueue.poll();
	            distanceY = distYQueue.poll();
	        } else {
	            return true;
	        }

	        // Make distanceX/Y display density independent.
	        distanceX = sensitivity * distanceX / displayDensity;
	        distanceY = sensitivity * distanceY / displayDensity;
	        
			// Compute the absolute new mouse position.
			int newX = (int) (p.getX() + getDelta(-distanceX));
			int newY = (int) (p.getY() + getDelta(-distanceY));
	        p.moveMouseButtonUp(newX, newY, meta);
        }
    	canvas.movePanToMakePointerVisible();
    	return true;
	}

	/*
	 * (non-Javadoc)
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onDown(android.view.MotionEvent)
	 */
	@Override
	public boolean onDown(MotionEvent e) {
		panRepeater.stop();
		return true;
	}
	
	/*
	 * (non-Javadoc)
	 * @see com.undatech.opaque.input.InputHandlerGeneric#getX(android.view.MotionEvent)
	 */
	protected int getX (MotionEvent e) {
        RemotePointer p = canvas.getPointer();
		if (dragMode || rightDragMode || middleDragMode) {
			float distanceX = e.getX() - dragX;
			dragX = e.getX();
			// Compute the absolute new X coordinate.
			return (int) (p.getX() + getDelta(distanceX));
		}
		dragX = e.getX();
		return p.getX();
	}

	/*
	 * (non-Javadoc)
	 * @see com.undatech.opaque.input.InputHandlerGeneric#getY(android.view.MotionEvent)
	 */
	protected int getY (MotionEvent e) {
        RemotePointer p = canvas.getPointer();
		if (dragMode || rightDragMode || middleDragMode) {
			float distanceY = e.getY() - dragY;
			dragY = e.getY();
			// Compute the absolute new Y coordinate.
			return (int) (p.getY() + getDelta(distanceY));
		}
		dragY = e.getY();
		return p.getY();
	}

	/**
	 * Computes how far the pointer will move.
	 * @param distance
	 * @return
	 */
	private float getDelta(float distance) {
		float delta = (float) (distance * Math.cbrt(canvas.getZoomFactor()));
		return computeAcceleration(delta);
	}

	/**
	 * Computes the acceleration depending on the size of the supplied delta.
	 * @param delta
	 * @return
	 */
	private float computeAcceleration (float delta) {
		float origSign = getSign(delta);
		delta = Math.abs(delta);
		if (delta <= 15) {
			delta = delta * 0.75f;
		} else if (acceleration && delta <= 70.0f ) {
			delta = delta * delta / 20.0f;
		} else if (acceleration) {
			delta = delta * 4.5f;
		}
		return origSign * delta;
	}
}
