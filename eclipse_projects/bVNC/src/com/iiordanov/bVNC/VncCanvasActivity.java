/* 
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
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

//
// CanvasView is the Activity for showing VNC Desktop.
//
package com.iiordanov.bVNC;

import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.List;

import com.iiordanov.android.bc.BCFactory;

import com.iiordanov.android.zoomer.ZoomControls;

import android.app.Activity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.ContentValues;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.DialogInterface.OnDismissListener;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.PointF;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.Button;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;


public class VncCanvasActivity extends Activity {
	
	private static boolean secondPointerWasDown = false;
	private static boolean thirdPointerWasDown = false;

	
	/**
	 * @author Michael A. MacDonald
	 */
	class ZoomInputHandler extends AbstractGestureInputHandler {

		/**
		 * In drag mode (entered with long press) you process mouse events
		 * without sending them through the gesture detector
		 */
		private boolean dragMode = false;
		
		/**
		 * In right-drag mode (entered when a right-click occurs) you process mouse events
		 * without sending them through the gesture detector, and only send the location of
		 * the pointer to the remote machine.
		 */
		private boolean rightDragMode = false;

		/**
		 * These variables hold the coordinates of the last double-tap down event.
		 */
		float doubleTapX, doubleTapY;
		
		/**
		 * Key handler delegate that handles DPad-based mouse motion
		 */
		private DPadMouseKeyHandler keyHandler;

		/**
		 * @param c
		 */
		ZoomInputHandler() {
			super(VncCanvasActivity.this);
			keyHandler = new DPadMouseKeyHandler(VncCanvasActivity.this,vncCanvas.handler);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getString(
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
			panner.stop();
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

			// onFling called while onScale gesture is in effect. We ignore the event and pretend it was
			// consumed. This prevents the mouse pointer from flailing around while we are scaling.
			// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
			// to stick a spiteful onFling with a MASSIVE delta here. 
			// This would cause the mouse pointer to jump to another place suddenly.
			// Hence, we ignore onScroll after scaling until we lift all pointers up.
			if (inScaling||scalingJustFinished)
				return true;
			
			showZoomer(false);
			panner.start(-(velocityX / FLING_FACTOR),
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
		
		/**
		 * Modify the event so that it does not move the mouse on the
		 * remote server.
		 * @param e
		 */
		private void remoteMouseStayPut(MotionEvent e) {
			//Log.i(TAG, "Setting pointer location in remoteMouseStayPut");
			e.setLocation(vncCanvas.mouseX, vncCanvas.mouseY);	
		}
		
		/**
		 * Modify the event so that the mouse goes where we specify.
		 * @param e
		 */
		private void remoteMouseSetCoordinates(MotionEvent e, float x, float y) {
			//Log.i(TAG, "Setting pointer location in remoteMouseSetCoordinates");
			e.setLocation(x, y);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractGestureInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent e) {
			final int pointerCnt   = e.getPointerCount();
	        final int action       = e.getActionMasked();
	        final int index        = e.getActionIndex();
	        final int pointerID    = e.getPointerId(index);
	        
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
	        }

        	// Here we only prepare for the second click, which we perform on ACTION_POINTER_UP for pointerID==1.
	        if (pointerID == 1 && action == MotionEvent.ACTION_POINTER_DOWN) {
	        	// If drag mode is on then stop it and indicate button was released.
	        	if (dragMode) {
					dragMode = false;
					vncCanvas.changeTouchCoordinatesToFullFrame(e);
					vncCanvas.processPointerEvent(e, false, false, false);       		
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
        		remoteMouseSetCoordinates(e, xInitialFocus, yInitialFocus);
	        	vncCanvas.changeTouchCoordinatesToFullFrame(e);
	        	int numEvents = 0;
	        	while (numEvents < swipeSpeed) {
	        		if        (twoFingerSwipeUp)   {
	        			vncCanvas.processPointerEvent(e, false, false, false, true, 0);
	        			vncCanvas.processPointerEvent(e, false);
	        		} else if (twoFingerSwipeDown) {
	        			vncCanvas.processPointerEvent(e, false, false, false, true, 1);
	        			vncCanvas.processPointerEvent(e, false);
	        		}
	        		numEvents++;
	        	}
	        	// Restore the coordinates so that onScale doesn't get all muddled up.
	        	remoteMouseSetCoordinates(e, x, y);
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
	        	vncCanvas.changeTouchCoordinatesToFullFrame(e);
	        	vncCanvas.processPointerEvent(e, true, true, false);
				SystemClock.sleep(50);
	        	// Offset the pointer by one pixel to prevent accidental click and disappearing context menu.
	        	remoteMouseSetCoordinates(e, vncCanvas.mouseX - 1.f, vncCanvas.mouseY);
	        	vncCanvas.processPointerEvent(e, false, true, false);
	        	// Put the pointer where it was before the 1px offset.
	        	remoteMouseSetCoordinates(e, vncCanvas.mouseX + 1.f, vncCanvas.mouseY);
				// Pass this event on to the parent class in order to end scaling as it was certainly
				// started when the second pointer went down.
				return super.onTouchEvent(e);
	        } else if (!inScaling && pointerID == 2 && action == MotionEvent.ACTION_POINTER_DOWN ) {
	        	// This boolean prevents the right-click from firing simultaneously as a middle button click.
	        	thirdPointerWasDown = true;
	        	vncCanvas.changeTouchCoordinatesToFullFrame(e);
	        	vncCanvas.processPointerEvent(e, true, false, true);
				SystemClock.sleep(50);
				return vncCanvas.processPointerEvent(e, false, false, true);
	        } else if (pointerCnt == 1 && pointerID == 0 && rightDragMode) {
				vncCanvas.changeTouchCoordinatesToFullFrame(e);
				return vncCanvas.processPointerEvent(e, false, false, false);
	        } else if (pointerCnt == 1 && pointerID == 0 && dragMode) {
				vncCanvas.changeTouchCoordinatesToFullFrame(e);
				if (action == MotionEvent.ACTION_UP) {
					dragMode = false;
					return vncCanvas.processPointerEvent(e, false, false, false);
				}
				return vncCanvas.processPointerEvent(e, true, false, false);
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
			
			// If we've performed a right/middle-click and the gesture is not over yet, do not start drag mode.
			if (secondPointerWasDown || thirdPointerWasDown)
				return;
			
			showZoomer(true);
			BCFactory.getInstance().getBCHaptic().performLongPressHaptic(vncCanvas);
			dragMode = true;
			
			vncCanvas.processPointerEvent(vncCanvas.changeTouchCoordinatesToFullFrame(e), true);
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

			// onScroll called while onScale gesture is in effect. We ignore the event and pretend it was
			// consumed. This prevents the mouse pointer from flailing around while we are scaling.
			// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
			// to stick a spiteful onScroll with a MASSIVE delta here. 
			// This would cause the mouse pointer to jump to another place suddenly.
			// Hence, we ignore onScroll after scaling until we lift all pointers up.
			if (inScaling||scalingJustFinished)
				return true;
			
			showZoomer(false);
			return vncCanvas.pan((int) distanceX, (int) distanceY);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onSingleTapConfirmed(android.view.MotionEvent)
		 */
		@Override
		public boolean onSingleTapConfirmed(MotionEvent e) {
			showZoomer(true);
			vncCanvas.changeTouchCoordinatesToFullFrame(e);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			return vncCanvas.processPointerEvent(e, false);
		}
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onDoubleTap(android.view.MotionEvent)
		 */
		@Override
		public boolean onDoubleTap (MotionEvent e) {
			vncCanvas.changeTouchCoordinatesToFullFrame(e);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			vncCanvas.processPointerEvent(e, false);
			SystemClock.sleep(50);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			return vncCanvas.processPointerEvent(e, false);
		}
	}

	public class TouchpadInputHandler extends AbstractGestureInputHandler {
		/**
		 * In drag mode (entered with long press) you process mouse events
		 * without sending them through the gesture detector
		 */
		private boolean dragMode;
		
		/**
		 * In right drag mode, entered after right-click, we process mouse events
		 * without sending them through the gesture detector
		 */
		private boolean rightDragMode = false;
		
		float dragX, dragY;
		
		/**
		 * Key handler delegate that handles DPad-based mouse motion
		 */
		private DPadMouseKeyHandler keyHandler;

		TouchpadInputHandler() {
			super(VncCanvasActivity.this);
			keyHandler = new DPadMouseKeyHandler(VncCanvasActivity.this,vncCanvas.handler); 
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getString(
					R.string.input_mode_touchpad);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return TOUCHPAD_MODE;
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

		/**
		 * scale down delta when it is small. This will allow finer control
		 * when user is making a small movement on touch screen.
		 * Scale up delta when delta is big. This allows fast mouse movement when
		 * user is flinging.
		 * @param deltaX
		 * @return
		 */
		private float fineCtrlScale(float delta) {
			float sign = (delta>0) ? 1 : -1;
			delta = Math.abs(delta);
			if (delta>=1 && delta <=3) {
				delta = 1;
			}else if (delta <= 10) {
				delta *= 0.34;
			} else if (delta <= 30 ) {
				delta *= delta/30;
			} else if (delta <= 90) {
				delta *=  (delta/30);
			} else {
				delta *= 3.0;
			}
			return sign * delta;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onLongPress(android.view.MotionEvent)
		 */
		@Override
		public void onLongPress(MotionEvent e) {
			
			// If we've performed a right/middle-click and the gesture is not over yet, do not start drag mode.
			if (thirdPointerWasDown || secondPointerWasDown)
				return;
			
			showZoomer(true);
			BCFactory.getInstance().getBCHaptic().performLongPressHaptic(vncCanvas);			
			dragMode = true;
			dragX = e.getX();
			dragY = e.getY();
			
			remoteMouseStayPut(e);
			vncCanvas.processPointerEvent(e, true);
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

			// onScroll called while onScale gesture is in effect. We ignore the event and pretend it was
			// consumed. This prevents the mouse pointer from flailing around while we are scaling.
			// Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
			// to stick a spiteful onScroll with a MASSIVE delta here. 
			// This would cause the mouse pointer to jump to another place suddenly.
			// Hence, we ignore onScroll after scaling until we lift all pointers up.
			if (inScaling||scalingJustFinished)
				return true;

			showZoomer(true);

			// Compute the relative movement offset on the remote screen.
			float deltaX = -distanceX *vncCanvas.getScale();
			float deltaY = -distanceY *vncCanvas.getScale();
			deltaX = fineCtrlScale(deltaX);
			deltaY = fineCtrlScale(deltaY);
			
			// Compute the absolute new mouse position on the remote site.
			float newRemoteX = vncCanvas.mouseX + deltaX;
			float newRemoteY = vncCanvas.mouseY + deltaY;
			
			if (dragMode) {
				if (e2.getAction() == MotionEvent.ACTION_UP)
					dragMode = false;
				
				dragX = e2.getX();
				dragY = e2.getY();
				e2.setLocation(newRemoteX, newRemoteY);
				return vncCanvas.processPointerEvent(e2, true);
			} else {
				e2.setLocation(newRemoteX, newRemoteY);
				vncCanvas.processPointerEvent(e2, false);
			}
			return true;
		}

		/**
		 * Modify the event so that the mouse goes where we specify.
		 * @param e
		 */
		private void remoteMouseSetCoordinates(MotionEvent e, float x, float y) {
			//Log.i(TAG, "Setting pointer location in remoteMouseSetCoordinates");
			e.setLocation(x, y);
		}
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractGestureInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent e) {
			final int pointerCnt   = e.getPointerCount();
	        final int action       = e.getActionMasked();
	        final int index        = e.getActionIndex();
	        final int pointerID    = e.getPointerId(index);

	        // We have put down first pointer on the screen, so we can reset the state of all click-state variables.
	        if (pointerID == 0 && action == MotionEvent.ACTION_DOWN) {
	        	// Permit sending mouse-down event on long-tap again.
	        	secondPointerWasDown = false;
	        	// Permit right-clicking again.
	        	thirdPointerWasDown = false;
	        	// Cancel any effect of scaling having "just finished" (e.g. ignoring scrolling).
				scalingJustFinished = false;
	        }

        	// Here we only prepare for the second click, which we perform on ACTION_POINTER_UP for pointerID==1.
	        if (pointerID == 1 && action == MotionEvent.ACTION_POINTER_DOWN) {
	        	// If drag mode is on then stop it and indicate button was released.
	        	if (dragMode) {
					dragMode = false;
		        	remoteMouseStayPut(e);
					vncCanvas.processPointerEvent(e, false, false, false);       		
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
	        	remoteMouseStayPut(e);
	        	int numEvents = 0;
	        	while (numEvents < swipeSpeed) {
	        		if        (twoFingerSwipeUp)   {
	        			vncCanvas.processPointerEvent(e, false, false, false, true, 0);
	        			vncCanvas.processPointerEvent(e, false);
	        		} else if (twoFingerSwipeDown) {
	        			vncCanvas.processPointerEvent(e, false, false, false, true, 1);
	        			vncCanvas.processPointerEvent(e, false);
	        		}
	        		numEvents++;
	        	}
	        	// Restore the coordinates so that onScale doesn't get all muddled up.
        		remoteMouseSetCoordinates(e, x, y);
        		return super.onTouchEvent(e);

        	// If user taps with a second finger while first finger is down, then we treat this as
	        // a right mouse click, but we only effect the click when the second pointer goes up.
	        // If the user taps with a second and third finger while the first 
	        // finger is down, we treat it as a middle mouse click. We ignore the lifting of the
	        // second index when the third index has gone down (using the thirdPointerWasDown variable)
	        // to prevent inadvertent right-clicks when a middle click has been performed.
	        } else if (!inScaling && !thirdPointerWasDown && pointerID == 1 && action == MotionEvent.ACTION_POINTER_UP) {     	
	        	remoteMouseStayPut(e);
	        	// We offset the click down and up by one pixel to workaround for Firefox (and any other application)
	        	// where if the two events are in the same spot the context menu may disappear.
	        	vncCanvas.processPointerEvent(e, true, true, false);
				SystemClock.sleep(50);
	        	// Offset the pointer by one pixel to prevent accidental click and disappearing context menu.
	        	remoteMouseSetCoordinates(e, vncCanvas.mouseX - 1.f, vncCanvas.mouseY);
	        	vncCanvas.processPointerEvent(e, false, true, false);
	        	// Put the pointer where it was before the 1px offset.
	        	remoteMouseSetCoordinates(e, vncCanvas.mouseX + 1.f, vncCanvas.mouseY);
	        	vncCanvas.mouseX = vncCanvas.mouseX + 1;
				// Pass this event on to the parent class in order to end scaling as it was certainly
				// started when the second pointer went down.
				return super.onTouchEvent(e);
	        } else if (!inScaling && pointerID == 2 && action == MotionEvent.ACTION_POINTER_DOWN) {
	        	// This boolean prevents the right-click from firing simultaneously as a middle button click.
	        	thirdPointerWasDown = true;
	        	remoteMouseStayPut(e);
	        	vncCanvas.processPointerEvent(e, true, false, true);
				SystemClock.sleep(50);
				return vncCanvas.processPointerEvent(e, false, false, true);			
			} else if (pointerCnt == 1 && pointerID == 0 && dragMode) {	

	        	// Compute the relative movement offset on the remote screen.
				float deltaX = (e.getX() - dragX) *vncCanvas.getScale();
				float deltaY = (e.getY() - dragY) *vncCanvas.getScale();
				dragX = e.getX();
				dragY = e.getY();
				deltaX = fineCtrlScale(deltaX);
				deltaY = fineCtrlScale(deltaY);
	
				// Compute the absolute new mouse position on the remote site.
				float newRemoteX = vncCanvas.mouseX + deltaX;
				float newRemoteY = vncCanvas.mouseY + deltaY;
				
				e.setLocation(newRemoteX, newRemoteY);

				if (action == MotionEvent.ACTION_UP) {
					dragMode = false;
					return vncCanvas.processPointerEvent(e, false, false, false);
				}
				return vncCanvas.processPointerEvent(e, true, false, false);
			} else
				return super.onTouchEvent(e);
		}		
		
		/**
		 * Modify the event so that it does not move the mouse on the
		 * remote server.
		 * @param e
		 */
		private void remoteMouseStayPut(MotionEvent e) {
			//Log.i(TAG, "Setting pointer location in remoteMouseStayPut");
			e.setLocation(vncCanvas.mouseX, vncCanvas.mouseY);	
		}
		
		/*
		 * (non-Javadoc)
		 * confirmed single tap: do a single mouse click on remote without moving the mouse.
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onSingleTapConfirmed(android.view.MotionEvent)
		 */
		@Override
		public boolean onSingleTapConfirmed(MotionEvent e) {
			showZoomer(true);
			remoteMouseStayPut(e);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			return vncCanvas.processPointerEvent(e, false);
		}

		/*
		 * (non-Javadoc)
		 * double tap: do two  left mouse right mouse clicks on remote without moving the mouse.
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onDoubleTap(android.view.MotionEvent)
		 */
		@Override
		public boolean onDoubleTap(MotionEvent e) {
			remoteMouseStayPut(e);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			vncCanvas.processPointerEvent(e, false);
			SystemClock.sleep(50);
			vncCanvas.processPointerEvent(e, true);
			SystemClock.sleep(50);
			return vncCanvas.processPointerEvent(e, false);
		}
		
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see android.view.GestureDetector.SimpleOnGestureListener#onDown(android.view.MotionEvent)
		 */
		@Override
		public boolean onDown(MotionEvent e) {
			panner.stop();
			return true;
		}
	}
	
	private final static String TAG = "VncCanvasActivity";

	AbstractInputHandler inputHandler;

	VncCanvas vncCanvas;

	VncDatabase database;

	private MenuItem[] inputModeMenuItems;
	private MenuItem[] scalingModeMenuItems;
	private AbstractInputHandler inputModeHandlers[];
	private ConnectionBean connection;
	private boolean trackballButtonDown;
/*	private static final int inputModeIds[] = { R.id.itemInputFitToScreen,
		R.id.itemInputTouchpad,
		R.id.itemInputMouse, R.id.itemInputPan,
		R.id.itemInputTouchPanTrackballMouse,
		R.id.itemInputDPadPanTouchMouse, R.id.itemInputTouchPanZoomMouse };
 */
	private static final int inputModeIds[] = { R.id.itemInputTouchpad,
		                                        R.id.itemInputTouchPanZoomMouse };
	private static final int scalingModeIds[] = { R.id.itemZoomable, R.id.itemFitToScreen,
												  R.id.itemOneToOne};

	ZoomControls zoomer;
	Panner panner;

	/**
	 * Function used to initialize an empty SSH HostKey for a new VNC over SSH connection.
	 */
	private void initializeSshHostKey() {
		// If the SSH HostKey is empty, then we need to grab the HostKey from the server and save it.
		if (connection.getSshHostKey().equals("")) {
			Log.d("initializeSshHostKey", "Initializing SSH HostKey from server.");
			SSHConnection sshConnection = new SSHConnection(connection.getSshServer(), connection.getSshPort());
			if (!sshConnection.connect()) {
				Utils.showFatalErrorMessage(this, "Could not connect to SSH server to obtain server HostKey. " +
											"Check the address and port and try again.");
			}
			connection.setSshHostKey(sshConnection.getServerHostKey());
			connection.save(database.getWritableDatabase());
			sshConnection.disconnect();
		}
	}
		
	@Override
	public void onCreate(Bundle icicle) {

		super.onCreate(icicle);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);

		database = new VncDatabase(this);

		Intent i = getIntent();
		connection = new ConnectionBean(this);
		
		Uri data = i.getData();
		if ((data != null) && (data.getScheme().equals("vnc"))) {
			
			// TODO: Can we also handle VNC over SSH connections with a new URI format?
			
			String host = data.getHost();
			// This should not happen according to Uri contract, but bug introduced in Froyo (2.2)
			// has made this parsing of host necessary
			int index = host.indexOf(':');
			int port;
			if (index != -1)
			{
				try
				{
					port = Integer.parseInt(host.substring(index + 1));
				}
				catch (NumberFormatException nfe)
				{
					port = 0;
				}
				host = host.substring(0,index);
			}
			else
			{
				port = data.getPort();
			}
			if (host.equals(VncConstants.CONNECTION))
			{
				if (connection.Gen_read(database.getReadableDatabase(), port))
				{
					MostRecentBean bean = androidVNC.getMostRecent(database.getReadableDatabase());
					if (bean != null)
					{
						bean.setConnectionId(connection.get_Id());
						bean.Gen_update(database.getWritableDatabase());
					}
				}
			} else {
			    connection.setAddress(host);
			    connection.setNickname(connection.getAddress());
			    connection.setPort(port);
			    List<String> path = data.getPathSegments();
			    if (path.size() >= 1) {
			        connection.setColorModel(path.get(0));
			    }
			    if (path.size() >= 2) {
			        connection.setPassword(path.get(1));
			    }
			    connection.save(database.getWritableDatabase());
			}
		} else {
		
		    Bundle extras = i.getExtras();

		    if (extras != null) {
		  	    connection.Gen_populate((ContentValues) extras
				  	.getParcelable(VncConstants.CONNECTION));
		    }
		    if (connection.getPort() == 0)
			    connection.setPort(5900);
		    
		    if (connection.getSshPort() == 0)
			    connection.setSshPort(22);

            // Parse a HOST:PORT entry
		    String host = connection.getAddress();
		    if (host.indexOf(':') > -1) {
			    String p = host.substring(host.indexOf(':') + 1);
			    try {
				    connection.setPort(Integer.parseInt(p));
			    } catch (Exception e) {
			    }
			    connection.setAddress(host.substring(0, host.indexOf(':')));
	  	    }
		}
		
		// TODO: Switch away from numeric representation of VNC connection type.
		if (connection.getConnectionType() == 1)
			initializeSshHostKey();
		
		setContentView(R.layout.canvas);

		vncCanvas = (VncCanvas) findViewById(R.id.vnc_canvas);
		zoomer = (ZoomControls) findViewById(R.id.zoomer);

		vncCanvas.initializeVncCanvas(connection, new Runnable() {
			public void run() {
				setModes();
			}
		});
		zoomer.hide();
		zoomer.setOnZoomInClickListener(new View.OnClickListener() {

			/*
			 * (non-Javadoc)
			 * 
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				showZoomer(true);
				vncCanvas.scaling.zoomIn(VncCanvasActivity.this);

			}

		});
		zoomer.setOnZoomOutClickListener(new View.OnClickListener() {

			/*
			 * (non-Javadoc)
			 * 
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
				showZoomer(true);
				vncCanvas.scaling.zoomOut(VncCanvasActivity.this);

			}

		});
		zoomer.setOnZoomKeyboardClickListener(new View.OnClickListener() {

			/*
			 * (non-Javadoc)
			 * 
			 * @see android.view.View.OnClickListener#onClick(android.view.View)
			 */
			@Override
			public void onClick(View v) {
              InputMethodManager inputMgr = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
              inputMgr.toggleSoftInput(0, 0);
			}

		});
		panner = new Panner(this, vncCanvas.handler);

		inputHandler = getInputHandlerById(R.id.itemInputTouchPanZoomMouse);
	}

	/**
	 * Set modes on start to match what is specified in the ConnectionBean;
	 * color mode (already done) scaling, input mode
	 */
	void setModes() {
		AbstractInputHandler handler = getInputHandlerByName(connection
				.getInputMode());
		AbstractScaling.getByScaleType(connection.getScaleMode())
				.setScaleTypeForActivity(this);
		this.inputHandler = handler;
		showPanningState();
	}

	ConnectionBean getConnection() {
		return connection;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.app.Activity#onCreateDialog(int)
	 */
	@Override
	protected Dialog onCreateDialog(int id) {
		switch (id) {
		case R.layout.entertext:
			return new EnterTextDialog(this);
		case R.id.itemHelpInputMode:
			return new AlertDialog.Builder(this)
			.setMessage(R.string.input_mode_help_text)
			.setPositiveButton(R.string.close,
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog,
								int whichButton) {
							// We don't have to do anything.
						}
					}).create();
		}
		
		// Default to meta key dialog
		return new MetaKeyDialog(this);
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.app.Activity#onPrepareDialog(int, android.app.Dialog)
	 */
	@Override
	protected void onPrepareDialog(int id, Dialog dialog) {
		super.onPrepareDialog(id, dialog);
		if (dialog instanceof ConnectionSettable)
			((ConnectionSettable) dialog).setConnection(connection);
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		// ignore orientation/keyboard change
		super.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onStop() {
		vncCanvas.disableRepaints();
		super.onStop();
	}

	@Override
	protected void onRestart() {
		vncCanvas.enableRepaints();
		super.onRestart();
	}

	/** {@inheritDoc} */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.vnccanvasactivitymenu, menu);

		if (vncCanvas.scaling != null)
			menu.findItem(vncCanvas.scaling.getId()).setChecked(true);

		Menu inputMenu = menu.findItem(R.id.itemInputMode).getSubMenu();
		inputModeMenuItems = new MenuItem[inputModeIds.length];
		for (int i = 0; i < inputModeIds.length; i++) {
			inputModeMenuItems[i] = inputMenu.findItem(inputModeIds[i]);
		}
		updateInputMenu();
		
		Menu scalingMenu = menu.findItem(R.id.itemScaling).getSubMenu();
		scalingModeMenuItems = new MenuItem[scalingModeIds.length];
		for (int i = 0; i < scalingModeIds.length; i++) {
			scalingModeMenuItems[i] = scalingMenu.findItem(scalingModeIds[i]);
		}
		updateScalingMenu();
		
/*		menu.findItem(R.id.itemFollowMouse).setChecked(
				connection.getFollowMouse());
		menu.findItem(R.id.itemFollowPan).setChecked(connection.getFollowPan());
 */
		return true;
	}

	/**
	 * Change the scaling mode sub-menu to reflect available scaling modes.
	 */
	void updateScalingMenu() {
		if (scalingModeMenuItems == null) {
			return;
		}
		for (MenuItem item : scalingModeMenuItems) {
			// If the entire framebuffer is NOT contained in the bitmap, fit-to-screen is meaningless.
			if (item.getItemId() == R.id.itemFitToScreen) {
				if (vncCanvas.bitmapData.bitmapheight != vncCanvas.bitmapData.framebufferheight ||
					vncCanvas.bitmapData.bitmapwidth  != vncCanvas.bitmapData.framebufferwidth)
					item.setEnabled(false);
				else
					item.setEnabled(true);
			} else
				item.setEnabled(true);
		}
	}	
	
	/**
	 * Change the input mode sub-menu to reflect change in scaling
	 */
	void updateInputMenu() {
		if (inputModeMenuItems == null || vncCanvas.scaling == null) {
			return;
		}
		for (MenuItem item : inputModeMenuItems) {
			item.setEnabled(vncCanvas.scaling
					.isValidInputMode(item.getItemId()));
			if (getInputHandlerById(item.getItemId()) == inputHandler)
				item.setChecked(true);
		}
	}

	/**
	 * If id represents an input handler, return that; otherwise return null
	 * 
	 * @param id
	 * @return
	 */
	AbstractInputHandler getInputHandlerById(int id) {
		if (inputModeHandlers == null) {
			inputModeHandlers = new AbstractInputHandler[inputModeIds.length];
		}
		for (int i = 0; i < inputModeIds.length; ++i) {
			if (inputModeIds[i] == id) {
				if (inputModeHandlers[i] == null) {
					switch (id) {
/*					case R.id.itemInputFitToScreen:
						inputModeHandlers[i] = new FitToScreenMode();
						break;
					case R.id.itemInputPan:
						inputModeHandlers[i] = new PanMode();
						break;
					case R.id.itemInputTouchPanTrackballMouse:
						inputModeHandlers[i] = new TouchPanTrackballMouse();
						break;
					case R.id.itemInputMouse:
						inputModeHandlers[i] = new MouseMode();
						break; 

					case R.id.itemInputDPadPanTouchMouse:
						inputModeHandlers[i] = new DPadPanTouchMouseMode();
						break;
 */					
					case R.id.itemInputTouchPanZoomMouse:
						inputModeHandlers[i] = new ZoomInputHandler();
						break;
					case R.id.itemInputTouchpad:
						inputModeHandlers[i] = new TouchpadInputHandler();
						break;
					}
				}
				return inputModeHandlers[i];
			}
		}
		return null;
	}

	AbstractInputHandler getInputHandlerByName(String name) {
		AbstractInputHandler result = null;
		for (int id : inputModeIds) {
			AbstractInputHandler handler = getInputHandlerById(id);
			if (handler.getName().equals(name)) {
				result = handler;
				break;
			}
		}
		if (result == null) {
			result = getInputHandlerById(R.id.itemInputTouchPanZoomMouse);
		}
		return result;
	}
	
	int getModeIdFromHandler(AbstractInputHandler handler) {
		for (int id : inputModeIds) {
			if (handler == getInputHandlerById(id))
				return id;
		}
		return R.id.itemInputTouchPanZoomMouse;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		vncCanvas.afterMenu = true;
		switch (item.getItemId()) {
		case R.id.itemInfo:
			vncCanvas.showConnectionInfo();
			return true;
		case R.id.itemSpecialKeys:
			showDialog(R.layout.metakey);
			return true;
		case R.id.itemColorMode:
			selectColorModel();
			return true;
			// Following sets one of the scaling options
		case R.id.itemZoomable:
		case R.id.itemOneToOne:
		case R.id.itemFitToScreen:
			AbstractScaling.getById(item.getItemId()).setScaleTypeForActivity(this);
			item.setChecked(true);
			showPanningState();
			return true;
		case R.id.itemCenterMouse:
			vncCanvas.warpMouse(vncCanvas.absoluteXPosition
					+ vncCanvas.getVisibleWidth() / 2,
					vncCanvas.absoluteYPosition + vncCanvas.getVisibleHeight()
							/ 2);
			return true;
		case R.id.itemDisconnect:
			vncCanvas.closeConnection();
			finish();
			return true;
		case R.id.itemEnterText:
			showDialog(R.layout.entertext);
			return true;
		case R.id.itemCtrlAltDel:
			vncCanvas.sendMetaKey(MetaKeyBean.keyCtrlAltDel);
			return true;
/*		case R.id.itemFollowMouse:
			boolean newFollow = !connection.getFollowMouse();
			item.setChecked(newFollow);
			connection.setFollowMouse(newFollow);
			if (newFollow) {
				vncCanvas.panToMouse();
			}
			connection.save(database.getWritableDatabase());
			return true;
		case R.id.itemFollowPan:
			boolean newFollowPan = !connection.getFollowPan();
			item.setChecked(newFollowPan);
			connection.setFollowPan(newFollowPan);
			connection.save(database.getWritableDatabase());
			return true;
 */
		case R.id.itemArrowLeft:
			vncCanvas.sendMetaKey(MetaKeyBean.keyArrowLeft);
			return true;
		case R.id.itemArrowUp:
			vncCanvas.sendMetaKey(MetaKeyBean.keyArrowUp);
			return true;
		case R.id.itemArrowRight:
			vncCanvas.sendMetaKey(MetaKeyBean.keyArrowRight);
			return true;
		case R.id.itemArrowDown:
			vncCanvas.sendMetaKey(MetaKeyBean.keyArrowDown);
			return true;
		case R.id.itemSendKeyAgain:
			sendSpecialKeyAgain();
			return true;
		// Disabling Manual/Wiki Menu item as the original does not correspond to this project anymore.
		//case R.id.itemOpenDoc:
		//	Utils.showDocumentation(this);
		//	return true;
		case R.id.itemCtrl:
			if (vncCanvas.onScreenCtrlToggle()) {
				item.setIcon(R.drawable.ctrlon);
				item.setTitle(R.string.tap_disable);
			}
			else {
				item.setIcon(R.drawable.ctrloff);
				item.setTitle(R.string.tap_enable);
			}
			return true;
		case R.id.itemAlt:
			if (vncCanvas.onScreenAltToggle()) {
				item.setIcon(R.drawable.alton);
				item.setTitle(R.string.tap_disable);
			}
			else {
				item.setIcon(R.drawable.altoff);
				item.setTitle(R.string.tap_enable);
			}
			return true;
		case R.id.itemHelpInputMode:
			showDialog(R.id.itemHelpInputMode);
			return true;
		default:
			AbstractInputHandler input = getInputHandlerById(item.getItemId());
			if (input != null) {
				inputHandler = input;
				connection.setInputMode(input.getName());
				if (input.getName().equals(TOUCHPAD_MODE))
					connection.setFollowMouse(true);
				item.setChecked(true);
				showPanningState();
				connection.save(database.getWritableDatabase());
				return true;
			}
		}
		return super.onOptionsItemSelected(item);
	}

	private MetaKeyBean lastSentKey;

	private void sendSpecialKeyAgain() {
		if (lastSentKey == null
				|| lastSentKey.get_Id() != connection.getLastMetaKeyId()) {
			ArrayList<MetaKeyBean> keys = new ArrayList<MetaKeyBean>();
			Cursor c = database.getReadableDatabase().rawQuery(
					MessageFormat.format("SELECT * FROM {0} WHERE {1} = {2}",
							MetaKeyBean.GEN_TABLE_NAME,
							MetaKeyBean.GEN_FIELD__ID, connection
									.getLastMetaKeyId()),
					MetaKeyDialog.EMPTY_ARGS);
			MetaKeyBean.Gen_populateFromCursor(c, keys, MetaKeyBean.NEW);
			c.close();
			if (keys.size() > 0) {
				lastSentKey = keys.get(0);
			} else {
				lastSentKey = null;
			}
		}
		if (lastSentKey != null)
			vncCanvas.sendMetaKey(lastSentKey);
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		if (isFinishing()) {
			vncCanvas.closeConnection();
			vncCanvas.onDestroy();
			database.close();
		}
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent evt) {
		if (keyCode == KeyEvent.KEYCODE_MENU)
			return super.onKeyDown(keyCode, evt);

		return inputHandler.onKeyDown(keyCode, evt);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent evt) {
		if (keyCode == KeyEvent.KEYCODE_MENU)
			return super.onKeyUp(keyCode, evt);

		return inputHandler.onKeyUp(keyCode, evt);
	}

	public void showPanningState() {
		Toast.makeText(this, inputHandler.getHandlerDescription(),
				Toast.LENGTH_SHORT).show();
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see android.app.Activity#onTrackballEvent(android.view.MotionEvent)
	 */
	@Override
	public boolean onTrackballEvent(MotionEvent event) {
		switch (event.getAction()) {
		case MotionEvent.ACTION_DOWN:
			trackballButtonDown = true;
			break;
		case MotionEvent.ACTION_UP:
			trackballButtonDown = false;
			break;
		}
		return inputHandler.onTrackballEvent(event);
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		return inputHandler.onTouchEvent(event);
	}

	private void selectColorModel() {
		// Stop repainting the desktop
		// because the display is composited!
		vncCanvas.disableRepaints();

		String[] choices = new String[COLORMODEL.values().length];
		int currentSelection = -1;
		for (int i = 0; i < choices.length; i++) {
			COLORMODEL cm = COLORMODEL.values()[i];
			choices[i] = cm.toString();
			if (vncCanvas.isColorModel(cm))
				currentSelection = i;
		}

		final Dialog dialog = new Dialog(this);
		dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
		ListView list = new ListView(this);
		list.setAdapter(new ArrayAdapter<String>(this,
				android.R.layout.simple_list_item_checked, choices));
		list.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
		list.setItemChecked(currentSelection, true);
		list.setOnItemClickListener(new OnItemClickListener() {
			public void onItemClick(AdapterView<?> arg0, View arg1, int arg2,
					long arg3) {
				dialog.dismiss();
				COLORMODEL cm = COLORMODEL.values()[arg2];
				vncCanvas.setColorModel(cm);
				connection.setColorModel(cm.nameString());
				connection.save(database.getWritableDatabase());
				Toast.makeText(VncCanvasActivity.this,
						"Updating Color Model to " + cm.toString(),
						Toast.LENGTH_SHORT).show();
			}
		});
		dialog.setOnDismissListener(new OnDismissListener() {
			@Override
			public void onDismiss(DialogInterface arg0) {
				//Log.i(TAG, "Color Model Selector dismissed");
				// Restore desktop repaints
				vncCanvas.enableRepaints();
			}
		});
		dialog.setContentView(list);
		dialog.show();
	}

	float panTouchX, panTouchY;

	/**
	 * Pan based on touch motions
	 * 
	 * @param event
	 */
	private boolean pan(MotionEvent event) {
		float curX = event.getX();
		float curY = event.getY();
		int dX = (int) (panTouchX - curX);
		int dY = (int) (panTouchY - curY);

		return vncCanvas.pan(dX, dY);
	}

	boolean defaultKeyDownHandler(int keyCode, KeyEvent evt) {
		if (vncCanvas.processLocalKeyEvent(keyCode, evt))
			return true;
		return super.onKeyDown(keyCode, evt);
	}

	boolean defaultKeyUpHandler(int keyCode, KeyEvent evt) {
		if (vncCanvas.processLocalKeyEvent(keyCode, evt))
			return true;
		return super.onKeyUp(keyCode, evt);
	}

	boolean touchPan(MotionEvent event) {
		switch (event.getAction()) {
		case MotionEvent.ACTION_DOWN:
			panTouchX = event.getX();
			panTouchY = event.getY();
			break;
		case MotionEvent.ACTION_MOVE:
			pan(event);
			panTouchX = event.getX();
			panTouchY = event.getY();
			break;
		case MotionEvent.ACTION_UP:
			pan(event);
			break;
		}
		return true;
	}

	private static int convertTrackballDelta(double delta) {
		return (int) Math.pow(Math.abs(delta) * 6.01, 2.5)
				* (delta < 0.0 ? -1 : 1);
	}

	boolean trackballMouse(MotionEvent evt) {
		
		int dx = convertTrackballDelta(evt.getX());
		int dy = convertTrackballDelta(evt.getY());

		evt.offsetLocation(vncCanvas.mouseX + dx - evt.getX(), vncCanvas.mouseY
				+ dy - evt.getY());

		if (vncCanvas.processPointerEvent(evt, trackballButtonDown)) {
			return true;
		}
		return VncCanvasActivity.super.onTouchEvent(evt);
	}

	long hideZoomAfterMs;
	static final long ZOOM_HIDE_DELAY_MS = 2500;
	HideZoomRunnable hideZoomInstance = new HideZoomRunnable();

	private void showZoomer(boolean force) {
		if (force || zoomer.getVisibility() != View.VISIBLE) {
			zoomer.show();
			hideZoomAfterMs = SystemClock.uptimeMillis() + ZOOM_HIDE_DELAY_MS;
			vncCanvas.handler
					.postAtTime(hideZoomInstance, hideZoomAfterMs + 10);
		}
	}

	private class HideZoomRunnable implements Runnable {
		public void run() {
			if (SystemClock.uptimeMillis() >= hideZoomAfterMs) {
				zoomer.hide();
			}
		}

	}

	/**
	 * Touches and dpad (trackball) pan the screen
	 * 
	 * @author Michael A. MacDonald
	 * 
	 */
	class PanMode implements AbstractInputHandler {

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyDown(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent evt) {
			// DPAD KeyDown events are move MotionEvents in Panning Mode
			final int dPos = 100;
			boolean result = false;
			switch (keyCode) {
			case KeyEvent.KEYCODE_DPAD_CENTER:
				result = true;
				break;
			case KeyEvent.KEYCODE_DPAD_LEFT:
				onTouchEvent(MotionEvent
						.obtain(1, System.currentTimeMillis(),
								MotionEvent.ACTION_MOVE, panTouchX + dPos,
								panTouchY, 0));
				result = true;
				break;
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				onTouchEvent(MotionEvent
						.obtain(1, System.currentTimeMillis(),
								MotionEvent.ACTION_MOVE, panTouchX - dPos,
								panTouchY, 0));
				result = true;
				break;
			case KeyEvent.KEYCODE_DPAD_UP:
				onTouchEvent(MotionEvent
						.obtain(1, System.currentTimeMillis(),
								MotionEvent.ACTION_MOVE, panTouchX, panTouchY
										+ dPos, 0));
				result = true;
				break;
			case KeyEvent.KEYCODE_DPAD_DOWN:
				onTouchEvent(MotionEvent
						.obtain(1, System.currentTimeMillis(),
								MotionEvent.ACTION_MOVE, panTouchX, panTouchY
										- dPos, 0));
				result = true;
				break;
			default:
				result = defaultKeyDownHandler(keyCode, evt);
				break;
			}
			return result;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyUp(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyUp(int keyCode, KeyEvent evt) {
			// TODO: Review this.
			// Ignore KeyUp events for DPAD keys in Panning Mode; trackball
			// button switches to mouse mode
			switch (keyCode) {
			case KeyEvent.KEYCODE_DPAD_CENTER:
				inputHandler = getInputHandlerById(R.id.itemInputTouchPanZoomMouse);
				connection.setInputMode(inputHandler.getName());
				connection.save(database.getWritableDatabase());
				updateInputMenu();
				showPanningState();
				return true;
			case KeyEvent.KEYCODE_DPAD_LEFT:
				return true;
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				return true;
			case KeyEvent.KEYCODE_DPAD_UP:
				return true;
			case KeyEvent.KEYCODE_DPAD_DOWN:
				return true;
			}
			return defaultKeyUpHandler(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent event) {
			return touchPan(event);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTrackballEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTrackballEvent(MotionEvent evt) {
			return false;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#handlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getText(R.string.input_mode_panning);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return "PAN_MODE";
		}

	}

	/**
	 * The touchscreen pans the screen; the trackball moves and clicks the
	 * mouse.
	 * 
	 * @author Michael A. MacDonald
	 * 
	 */
	public class TouchPanTrackballMouse implements AbstractInputHandler {
		private DPadMouseKeyHandler keyHandler = new DPadMouseKeyHandler(VncCanvasActivity.this, vncCanvas.handler);
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyDown(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent evt) {
			return keyHandler.onKeyDown(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyUp(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyUp(int keyCode, KeyEvent evt) {
			return keyHandler.onKeyUp(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent evt) {
			return touchPan(evt);
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
		 * @see com.iiordanov.bVNC.AbstractInputHandler#handlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getText(
					R.string.input_mode_touchpad_pan_trackball_mouse);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return "TOUCH_PAN_TRACKBALL_MOUSE";
		}

	}

	static final String FIT_SCREEN_NAME = "FIT_SCREEN";
	/** Internal name for default input mode with Zoom scaling */
	static final String TOUCH_ZOOM_MODE = "TOUCH_ZOOM_MODE";
	
	static final String TOUCHPAD_MODE = "TOUCHPAD_MODE";

	/**
	 * In fit-to-screen mode, no panning. Trackball and touchscreen work as
	 * mouse.
	 * 
	 * @author Michael A. MacDonald
	 * 
	 */
	public class FitToScreenMode implements AbstractInputHandler {
		private DPadMouseKeyHandler keyHandler = new DPadMouseKeyHandler(VncCanvasActivity.this, vncCanvas.handler);
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyDown(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent evt) {
			return keyHandler.onKeyDown(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyUp(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyUp(int keyCode, KeyEvent evt) {
			return keyHandler.onKeyUp(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent evt) {
			return false;
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
		 * @see com.iiordanov.bVNC.AbstractInputHandler#handlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getText(R.string.input_mode_fit_to_screen);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return FIT_SCREEN_NAME;
		}

	}

	/**
	 * Touch screen controls, clicks the mouse.
	 * 
	 * @author Michael A. MacDonald
	 * 
	 */
	class MouseMode implements AbstractInputHandler {

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyDown(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent evt) {
			if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER)
				return true;
			return defaultKeyDownHandler(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyUp(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyUp(int keyCode, KeyEvent evt) {
			// TODO: Review this.
			if (keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
				inputHandler = getInputHandlerById(R.id.itemInputTouchPanZoomMouse);
				showPanningState();
				connection.setInputMode(inputHandler.getName());
				connection.save(database.getWritableDatabase());
				updateInputMenu();
				return true;
			}
			return defaultKeyUpHandler(keyCode, evt);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent event) {
			// Mouse Pointer Control Mode
			// Pointer event is absolute coordinates.

			vncCanvas.changeTouchCoordinatesToFullFrame(event);
			if (vncCanvas.processPointerEvent(event, true))
				return true;
			return VncCanvasActivity.super.onTouchEvent(event);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTrackballEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTrackballEvent(MotionEvent evt) {
			return false;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#handlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getText(R.string.input_mode_mouse);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return "MOUSE";
		}

	}

	/**
	 * Touch screen controls, clicks the mouse. DPad pans the screen
	 * 
	 * @author Michael A. MacDonald
	 * 
	 */
	class DPadPanTouchMouseMode implements AbstractInputHandler {

		private boolean isPanning;

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyDown(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyDown(int keyCode, KeyEvent evt) {
			int xv = 0;
			int yv = 0;
			boolean result = true;
			switch (keyCode) {
			case KeyEvent.KEYCODE_DPAD_LEFT:
				xv = -1;
				break;
			case KeyEvent.KEYCODE_DPAD_RIGHT:
				xv = 1;
				break;
			case KeyEvent.KEYCODE_DPAD_UP:
				yv = -1;
				break;
			case KeyEvent.KEYCODE_DPAD_DOWN:
				yv = 1;
				break;
			default:
				result = defaultKeyDownHandler(keyCode, evt);
				break;
			}
			if ((xv != 0 || yv != 0) && !isPanning) {
				final int x = xv;
				final int y = yv;
				isPanning = true;
				panner.start(x, y, new Panner.VelocityUpdater() {

					/*
					 * (non-Javadoc)
					 * 
					 * @see com.iiordanov.bVNC.Panner.VelocityUpdater#updateVelocity(android.graphics.Point,
					 *      long)
					 */
					@Override
					public boolean updateVelocity(PointF p, long interval) {
						double scale = (2.0 * (double) interval / 50.0);
						if (Math.abs(p.x) < 500)
							p.x += (int) (scale * x);
						if (Math.abs(p.y) < 500)
							p.y += (int) (scale * y);
						return true;
					}

				});
				vncCanvas.pan(x, y);
			}
			return result;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onKeyUp(int,
		 *      android.view.KeyEvent)
		 */
		@Override
		public boolean onKeyUp(int keyCode, KeyEvent evt) {
			boolean result = false;

			switch (keyCode) {
			case KeyEvent.KEYCODE_DPAD_LEFT:
			case KeyEvent.KEYCODE_DPAD_RIGHT:
			case KeyEvent.KEYCODE_DPAD_UP:
			case KeyEvent.KEYCODE_DPAD_DOWN:
				panner.stop();
				isPanning = false;
				result = true;
				break;
			default:
				result = defaultKeyUpHandler(keyCode, evt);
				break;
			}
			return result;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTouchEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTouchEvent(MotionEvent event) {
			// Mouse Pointer Control Mode
			// Pointer event is absolute coordinates.

			vncCanvas.changeTouchCoordinatesToFullFrame(event);
			if (vncCanvas.processPointerEvent(event, true))
				return true;
			return VncCanvasActivity.super.onTouchEvent(event);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#onTrackballEvent(android.view.MotionEvent)
		 */
		@Override
		public boolean onTrackballEvent(MotionEvent evt) {
			return false;
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#handlerDescription()
		 */
		@Override
		public CharSequence getHandlerDescription() {
			return getResources().getText(
					R.string.input_mode_dpad_pan_touchpad_mouse);
		}

		/*
		 * (non-Javadoc)
		 * 
		 * @see com.iiordanov.bVNC.AbstractInputHandler#getName()
		 */
		@Override
		public String getName() {
			return "DPAD_PAN_TOUCH_MOUSE";
		}

	}
}
