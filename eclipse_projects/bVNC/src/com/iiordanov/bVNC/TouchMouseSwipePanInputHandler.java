package com.iiordanov.bVNC;

import android.graphics.PointF;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.input.RemotePointer;

/**
 * @author Michael A. MacDonald
 */
class TouchMouseSwipePanInputHandler extends AbstractGestureInputHandler {
	static final String TAG = "SimulatedTouchpadInputHandler";
	static final String TOUCH_ZOOM_MODE = "TOUCH_ZOOM_MODE";

	/**
	 * @param c
	 */
	TouchMouseSwipePanInputHandler(VncCanvasActivity va, VncCanvas v) {
		super(va, v);
		activity = va;
		vncCanvas = v;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
	 */
	@Override
	public CharSequence getHandlerDescription() {
		return vncCanvas.getResources().getString(
				R.string.input_mode_touch_pan_zoom_mouse);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
	 */
	@Override
	public String getName() {
		return TOUCH_ZOOM_MODE;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.VncCanvasActivity.ZoomInputHandler#onKeyDown(int,
	 *      android.view.KeyEvent)
	 */
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent evt) {
		return keyHandler.onKeyDown(keyCode, evt);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.VncCanvasActivity.ZoomInputHandler#onKeyUp(int,
	 *      android.view.KeyEvent)
	 */
	@Override
	public boolean onKeyUp(int keyCode, KeyEvent evt) {
		return keyHandler.onKeyUp(keyCode, evt);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.AbstractInputHandler#onTrackballEvent(android.view.MotionEvent)
	 */
	@Override
	public boolean onTrackballEvent(MotionEvent evt) {
		return trackballMouse(evt);
	}
	
	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onDown(android.view.MotionEvent)
	 */
	@Override
	public boolean onDown(MotionEvent e) {
		activity.stopPanner();
		return true;
	}

	/**
	 * Divide stated fling velocity by this amount to get initial velocity
	 * per pan interval
	 */
	static final float FLING_FACTOR = 8;

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onFling(android.view.MotionEvent,
	 *      android.view.MotionEvent, float, float)
	 */
	@Override
	public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX,
			float velocityY) {

		// onFling called while scaling/swiping gesture is in effect. We ignore the event and pretend it was
		// consumed. This prevents the mouse pointer from flailing around while we are scaling.
		// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
		// to stick a spiteful onScroll with a MASSIVE delta here. 
		// This would cause the mouse pointer to jump to another place suddenly.
		// Hence, we ignore onScroll after scaling until we lift all pointers up.
		boolean twoFingers = false;
		if (e1 != null)
			twoFingers = (e1.getPointerCount() > 1);
		if (e2 != null)
			twoFingers = twoFingers || (e2.getPointerCount() > 1);

		if (twoFingers||inSwiping||inScaling||scalingJustFinished)
			return true;

		activity.showZoomer(false);
		activity.panner.start(-(velocityX / FLING_FACTOR),
				-(velocityY / FLING_FACTOR), new Panner.VelocityUpdater() {

					/*
					 * (non-Javadoc)
					 * 
					 * @see com.iiordanov.bVNC.Panner.VelocityUpdater#updateVelocity(android.graphics.Point,
					 *      long)
					 */
					@Override
					public boolean updateVelocity(PointF p, long interval) {
						double scale = Math.pow(0.8, interval / 50.0);
						p.x *= scale;
						p.y *= scale;
						return (Math.abs(p.x) > 0.5 || Math.abs(p.y) > 0.5);
					}
				});
		return true;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.iiordanov.bVNC.AbstractGestureInputHandler#onTouchEvent(android.view.MotionEvent)
	 */
	@Override
	public boolean onTouchEvent(MotionEvent e) {
		final int pointerCnt = e.getPointerCount();
        final int action     = e.getActionMasked();
        final int index      = e.getActionIndex();
        final int pointerID  = e.getPointerId(index);
        RemotePointer p = vncCanvas.getPointer();

        if (android.os.Build.VERSION.SDK_INT >= 14) {
	        // Handle actions performed by a (e.g. USB or bluetooth) mouse.
	        if (handleMouseActions (e))
	        	return true;
        }

        // We have put down first pointer on the screen, so we can reset the state of all click-state variables.
        if (pointerID == 0 && action == MotionEvent.ACTION_DOWN) {
        	// Permit sending mouse-down event on long-tap again.
        	secondPointerWasDown = false;
        	// Permit right-clicking again.
        	thirdPointerWasDown = false;
        	// Cancel right-drag mode.
        	rightDragMode = false;
        	// Cancel any effect of scaling having "just finished" (e.g. ignoring scrolling).
			scalingJustFinished = false;
			dragX = e.getX();
			dragY = e.getY();
        } else if (pointerID == 0 && action == MotionEvent.ACTION_UP)
        	vncCanvas.inScrolling = false;

    	// Here we only prepare for the second click, which we perform on ACTION_POINTER_UP for pointerID==1.
        if (pointerID == 1 && action == MotionEvent.ACTION_POINTER_DOWN) {
        	// If drag mode is on then stop it and indicate button was released.
        	if (dragMode) {
				dragMode = false;
				changeTouchCoordinatesToFullFrame(e);
				p.processPointerEvent(e, false, false, false);       		
        	}
        	// Permit sending mouse-down event on long-tap again.
        	secondPointerWasDown = true;
        	// Permit right-clicking again.
        	thirdPointerWasDown = false;
        }
        
        // Send scroll up/down events if swiping is happening.
        if (inSwiping) {
        	// Save the coordinates and restore them afterward.
        	float x = e.getX();
        	float y = e.getY();
        	// Set the coordinates to where the swipe began (i.e. where scaling started).
        	setEventCoordinates(e, xInitialFocus, yInitialFocus);
        	changeTouchCoordinatesToFullFrame(e);
        	int numEvents = 0;
        	while (numEvents < swipeSpeed) {
        		if        (twoFingerSwipeUp)   {
        			p.processPointerEvent(e, false, false, false, true, 0);
        			p.processPointerEvent(e, false);
        		} else if (twoFingerSwipeDown) {
        			p.processPointerEvent(e, false, false, false, true, 1);
        			p.processPointerEvent(e, false);
        		} else if (twoFingerSwipeLeft)   {
        			p.processPointerEvent(e, false, false, false, true, 2);
        			p.processPointerEvent(e, false);
        		} else if (twoFingerSwipeRight) {
        			p.processPointerEvent(e, false, false, false, true, 3);
        			p.processPointerEvent(e, false);
        		}
        		numEvents++;
        	}
        	// Restore the coordinates so that onScale doesn't get all muddled up.
        	setEventCoordinates(e, x, y);
    		return super.onTouchEvent(e);

    	// If user taps with a second finger while first finger is down, then we treat this as
        // a right mouse click, but we only effect the click when the second pointer goes up.
        // If the user taps with a second and third finger while the first 
        // finger is down, we treat it as a middle mouse click. We ignore the lifting of the
        // second index when the third index has gone down (using the thirdPointerWasDown variable)
        // to prevent inadvertent right-clicks when a middle click has been performed.
        } else if (!inScaling && !thirdPointerWasDown && pointerID == 1 && action == MotionEvent.ACTION_POINTER_UP) {
        	// Enter right-drag mode so we can move the pointer to an entry in the context menu
        	// without clicking.
        	rightDragMode = true;
        	changeTouchCoordinatesToFullFrame(e);
        	p.processPointerEvent(e, true, true, false);
			SystemClock.sleep(50);
        	// Offset the pointer by one pixel to prevent accidental click and disappearing context menu.
			setEventCoordinates(e, p.getX() - 1, p.getY());
        	p.processPointerEvent(e, false, true, false);
        	// Put the pointer where it was before the 1px offset.
        	p.setX(p.getX() + 1);
			// Pass this event on to the parent class in order to end scaling as it was certainly
			// started when the second pointer went down.
			return super.onTouchEvent(e);
        } else if (!inScaling && pointerID == 2 && action == MotionEvent.ACTION_POINTER_DOWN) {
        	// This boolean prevents the right-click from firing simultaneously as a middle button click.
        	thirdPointerWasDown = true;
        	changeTouchCoordinatesToFullFrame(e);
        	p.processPointerEvent(e, true, false, true);
			SystemClock.sleep(50);
			return p.processPointerEvent(e, false, false, true);
        } else if (pointerCnt == 1 && pointerID == 0 && rightDragMode) {
			changeTouchCoordinatesToFullFrame(e);
			return p.processPointerEvent(e, false, false, false);
        } else if (pointerCnt == 1 && pointerID == 0 && dragMode) {
			changeTouchCoordinatesToFullFrame(e);
			if (action == MotionEvent.ACTION_UP) {
				dragMode = false;
				return p.processPointerEvent(e, false, false, false);
			}
			return p.processPointerEvent(e, true, false, false);
		} else
			return super.onTouchEvent(e);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onLongPress(android.view.MotionEvent)
	 */
	@Override
	public void onLongPress(MotionEvent e) {
        RemotePointer p = vncCanvas.getPointer();

		// If we've performed a right/middle-click and the gesture is not over yet, do not start drag mode.
		if (secondPointerWasDown || thirdPointerWasDown)
			return;
		
		activity.showZoomer(true);
		BCFactory.getInstance().getBCHaptic().performLongPressHaptic(vncCanvas);
		dragMode = true;
		
		p.processPointerEvent(changeTouchCoordinatesToFullFrame(e), true);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent,
	 *      android.view.MotionEvent, float, float)
	 */
	@Override
	public boolean onScroll(MotionEvent e1, MotionEvent e2,
			float distanceX, float distanceY) {

		// onScroll called while scaling/swiping gesture is in effect. We ignore the event and pretend it was
		// consumed. This prevents the mouse pointer from flailing around while we are scaling.
		// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
		// to stick a spiteful onScroll with a MASSIVE delta here. 
		// This would cause the mouse pointer to jump to another place suddenly.
		// Hence, we ignore onScroll after scaling until we lift all pointers up.
		boolean twoFingers = false;
		if (e1 != null)
			twoFingers = (e1.getPointerCount() > 1);
		if (e2 != null)
			twoFingers = twoFingers || (e2.getPointerCount() > 1);

		if (twoFingers||inSwiping||inScaling||scalingJustFinished)
			return true;

		activity.showZoomer(false);
		return vncCanvas.pan((int) distanceX, (int) distanceY);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onSingleTapConfirmed(android.view.MotionEvent)
	 */
	@Override
	public boolean onSingleTapConfirmed(MotionEvent e) {
        RemotePointer p = vncCanvas.getPointer();

		activity.showZoomer(true);
		changeTouchCoordinatesToFullFrame(e);
		p.processPointerEvent(e, true);
		SystemClock.sleep(50);
		return p.processPointerEvent(e, false);
	}
	
	/*
	 * (non-Javadoc)
	 * 
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onDoubleTap(android.view.MotionEvent)
	 */
	@Override
	public boolean onDoubleTap (MotionEvent e) {
        RemotePointer p = vncCanvas.getPointer();

		changeTouchCoordinatesToFullFrame(e);
		p.processPointerEvent(e, true);
		SystemClock.sleep(50);
		p.processPointerEvent(e, false);
		SystemClock.sleep(50);
		p.processPointerEvent(e, true);
		SystemClock.sleep(50);
		return p.processPointerEvent(e, false);
	}
}

