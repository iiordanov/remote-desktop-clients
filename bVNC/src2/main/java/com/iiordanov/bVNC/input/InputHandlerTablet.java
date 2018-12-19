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

import android.os.Vibrator;
import android.util.Log;
import android.view.MotionEvent;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.R;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;

public class InputHandlerTablet extends InputHandlerGeneric {
	static final String TAG = "InputHandlerDirectDragPan";
	public static final String ID = "TOUCH_ZOOM_MODE_DRAG_PAN";

	/**
	 * @param c
	 */
	public InputHandlerTablet(RemoteCanvasActivity activity, RemoteCanvas canvas, Vibrator myVibrator) {
		super(activity, canvas, myVibrator);
	}

	/*
	 * (non-Javadoc)
	 * @see com.iiordanov.bVNC.input.InputHandler#getDescription()
	 */
	@Override
	public String getDescription() {
		return canvas.getResources().getString(R.string.input_method_direct_drag_pan_description);
	}

	/*
	 * (non-Javadoc)
	 * @see com.iiordanov.bVNC.input.InputHandler#getId()
	 */
	@Override
	public String getId() {
		return ID;
	}
	
	/*
	 * (non-Javadoc)
	 * @see com.iiordanov.bVNC.input.InputHandlerGeneric#onLongPress(android.view.MotionEvent)
	 */
	@Override
	public void onLongPress(MotionEvent e) {

		// If we've performed a right/middle-click and the gesture is not over yet, do not start drag mode.
		if (secondPointerWasDown || thirdPointerWasDown)
			return;


		RemotePointer p = canvas.getPointer();
		p.rightButtonDown(getX(e), getY(e), e.getMetaState());
		// canvas.displayShortToastMessage("Right Click");
		endDragModesAndScrolling();
	}

	/*
	 * (non-Javadoc)
	 * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent, android.view.MotionEvent, float, float)
	 */
	@Override
	public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        RemotePointer p = canvas.getPointer();
        //Log.d("tablet", "onScroll called."+" inSwiping: "+inSwiping+" dragMode: "+dragMode);
        //Log.d("tablet", "distX: "+distanceX+" distY"+distanceY);
        inSwiping = true;
        swipeSpeed = 1;
		scrollDown  = false;
		scrollUp    = false;
		scrollRight = false;
		scrollLeft  = false;
        if(distanceX > 0 && -1 < distanceY && distanceY < 1) scrollRight = true;
        else if (distanceX < 0 && -1 < distanceY && distanceY < 1) scrollLeft = true;
        else if (-1 < distanceX && distanceX < 1 && distanceY > 0) scrollDown = true;
        else if (-1 < distanceX && distanceX < 1 && distanceY < 0) scrollUp = true;
        return true;
        // If we are scaling, allow panning around by moving two fingers around the screen
//        if (inScaling) {
//    		float scale = canvas.getZoomFactor();
//    		activity.showToolbar();
//    		canvas.relativePan((int)(distanceX*scale), (int)(distanceY*scale));
//        } else {
//			// onScroll called while scaling/swiping gesture is in effect. We ignore the event and pretend it was
//			// consumed. This prevents the mouse pointer from flailing around while we are scaling.
//			// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android
//			// to stick a spiteful onScroll with a MASSIVE delta here.
//			// This would cause the mouse pointer to jump to another place suddenly.
//			// Hence, we ignore onScroll after scaling until we lift all pointers up.
//			boolean twoFingers = false;
//			if (e1 != null)
//				twoFingers = (e1.getPointerCount() > 1);
//			if (e2 != null)
//				twoFingers = twoFingers || (e2.getPointerCount() > 1);
//
//			if (twoFingers||inSwiping) {
//				return true;
//			}
//			activity.showToolbar();
//
//			if (!dragMode) {
//				Log.d("tablet","left Button will be put down");
//				dragMode = true;
//			    p.leftButtonDown(getX(e1), getY(e1), e1.getMetaState());
//			} else {
//				p.moveMouseButtonDown(getX(e2), getY(e2), e2.getMetaState());
//			}
//		}
//        canvas.movePanToMakePointerVisible();
//		return true;
	}

	@Override
	public boolean onTouchEvent(MotionEvent e) {
		final int action     = e.getActionMasked();
		final int index      = e.getActionIndex();
		final int pointerID  = e.getPointerId(index);
		final int meta       = e.getMetaState();
		RemotePointer p = canvas.getPointer();
		float f = e.getPressure();
		if (f > 2.f)
			f = f / 50.f;
		if (f > .92f) {
			disregardNextOnFling = true;
		}

		if (android.os.Build.VERSION.SDK_INT >= 14) {
			// Handle and consume actions performed by a (e.g. USB or bluetooth) mouse.
			if (handleMouseActions (e))
				return true;
		}

		if (action == MotionEvent.ACTION_UP) {
			// Turn filtering back on and invalidate to make things pretty.
			canvas.myDrawable.paint.setFilterBitmap(true);
			canvas.invalidate();
		}

		switch (pointerID) {

			case 0:
				switch (action) {
					case MotionEvent.ACTION_DOWN:
						disregardNextOnFling = false;
						singleHandedJustEnded = false;
						// We have put down first pointer on the screen, so we can reset the state of all click-state variables.
						// Permit sending mouse-down event on long-tap again.
						secondPointerWasDown = false;
						// Permit right-clicking again.
						thirdPointerWasDown = false;
						// Cancel any effect of scaling having "just finished" (e.g. ignoring scrolling).
						scalingJustFinished = false;
						// Cancel drag modes and scrolling.
						if (!singleHandedGesture)
							endDragModesAndScrolling();
						canvas.cursorBeingMoved = true;
						// If we are manipulating the desktop, turn off bitmap filtering for faster response.
						canvas.myDrawable.paint.setFilterBitmap(false);
						// Indicate where we start dragging from.
						dragX = e.getX();
						dragY = e.getY();

						// Detect whether this is potentially the start of a gesture to show the nav bar.
						detectImmersiveSwipe(dragY);
						break;
					case MotionEvent.ACTION_UP:
						singleHandedGesture = false;
						singleHandedJustEnded = true;

						// If this is the end of a swipe that showed the nav bar, consume.
						if (immersiveSwipe && Math.abs(dragY - e.getY()) > immersiveSwipeDistance) {
							endDragModesAndScrolling();
							return true;
						}

						// If any drag modes were going on, end them and send a mouse up event.
						if (endDragModesAndScrolling()) {
							p.releaseButton(getX(e), getY(e), meta);
							return true;
						}
						break;
					case MotionEvent.ACTION_MOVE:
						// Send scroll up/down events if swiping is happening.
						if (panMode) {
							float scale = canvas.getZoomFactor();
							canvas.relativePan(-(int)((e.getX() - dragX)*scale), -(int)((e.getY() - dragY)*scale));
							dragX = e.getX();
							dragY = e.getY();
							return true;
						} else if (dragMode || rightDragMode || middleDragMode) {
							canvas.movePanToMakePointerVisible();
							p.moveMouseButtonDown(getX(e), getY(e), meta);
							return true;
						} else if (inSwiping) {
							// Save the coordinates and restore them afterward.
							float x = e.getX();
							float y = e.getY();
							// Set the coordinates to where the swipe began (i.e. where scaling started).
							Log.d("tablet", "sending scroll event - scrollUp:"+scrollUp + " scrollDown: "+scrollDown + " e.X: " + getX(e) + " e.Y:" + getY(e) + " swipeSpeed: " + swipeSpeed);
							sendScrollEvents (getX(e), getY(e), meta);
							// Restore the coordinates so that onScale doesn't get all muddled up.
						} else if (immersiveSwipe) {
							// If this is part of swipe that shows the nav bar, consume.
							return true;
						}
				}
				break;

			case 1:
				switch (action) {
					case MotionEvent.ACTION_POINTER_DOWN:
						// We re-calculate the initial focal point to be between the 1st and 2nd pointer index.
						xInitialFocus = 0.5f * (dragX + e.getX(pointerID));
						yInitialFocus = 0.5f * (dragY + e.getY(pointerID));
						// Here we only prepare for the second click, which we perform on ACTION_POINTER_UP for pointerID==1.
						endDragModesAndScrolling();
						// Permit sending mouse-down event on long-tap again.
						secondPointerWasDown = true;
						// Permit right-clicking again.
						thirdPointerWasDown  = false;
						break;
					case MotionEvent.ACTION_POINTER_UP:
						if (!inSwiping && !inScaling && !thirdPointerWasDown) {
							// If user taps with a second finger while first finger is down, then we treat this as
							// a right mouse click, but we only effect the click when the second pointer goes up.
							// If the user taps with a second and third finger while the first
							// finger is down, we treat it as a middle mouse click. We ignore the lifting of the
							// second index when the third index has gone down (using the thirdPointerWasDown variable)
							// to prevent inadvertent right-clicks when a middle click has been performed.
							p.rightButtonDown(getX(e), getY(e), meta);
							// Enter right-drag mode.
							rightDragMode = true;
							// Now the event must be passed on to the parent class in order to
							// end scaling as it was certainly started when the second pointer went down.
						}
						break;
				}
				break;

			case 2:
				switch (action) {
					case MotionEvent.ACTION_POINTER_DOWN:
						if (!inScaling) {
							// This boolean prevents the right-click from firing simultaneously as a middle button click.
							thirdPointerWasDown = true;
							p.middleButtonDown(getX(e), getY(e), meta);
							// Enter middle-drag mode.
							middleDragMode      = true;
						}
				}
				break;
		}

		scalingGestureDetector.onTouchEvent(e);
		return gestureDetector.onTouchEvent(e);
	}
}

