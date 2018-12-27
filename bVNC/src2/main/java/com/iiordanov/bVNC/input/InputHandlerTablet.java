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

import android.support.v4.view.MotionEventCompat;
import android.util.Log;
import android.view.MotionEvent;
import android.os.Vibrator;
import android.view.ViewConfiguration;

import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.input.RemotePointer;

public class InputHandlerTablet extends InputHandlerGeneric {
	static final String TAG = "InputHandlerTablet";
	public static final String ID = "TABLET_MODE";

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
		return canvas.getResources().getString(R.string.input_method_tablet_description);
	}

	/*
	 * (non-Javadoc)
	 * @see com.iiordanov.bVNC.input.InputHandler#getId()
	 */
	@Override
	public String getId() {
		return ID;
	}


	@Override
	public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
		RemotePointer p = canvas.getPointer();
		//scrollMode = true;

		// If we are scaling, allow panning around by moving two fingers around the screen
		if (scrollMode){
			//Log.d("tablet", "scroll! distanceX: " + distanceX + " distanceY: " + distanceY);
			if (distanceX > 10 || distanceY > 10 || distanceX < -10 || distanceY < -10) return true;
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

		}

		return true;

	}

	private void resetAllModes(){
		scrollMode = false;
		dragMode = false;
	}

	protected boolean scrollMode = false;
	private int alternateScroll = 0; // to reduce scroll speed, send every 3rd scroll event
	private long touchDownTime = 0;
	final long longPressTimeout = ViewConfiguration.getLongPressTimeout();
	@Override
	public boolean onTouchEvent(MotionEvent e){
		//Log.d("tablet", "touch event captured!");
		final int action = MotionEventCompat.getActionMasked(e);
		final long currentTime = System.currentTimeMillis();
		RemotePointer p = canvas.getPointer();
		long touchDuration = currentTime - touchDownTime;
		activity.showToolbar();

		switch(action){
			case MotionEvent.ACTION_DOWN:
				touchDownTime = System.currentTimeMillis();
				resetAllModes();
				return true;
			case MotionEvent.ACTION_UP:
				//Log.d("tablet", "Duration: " + touchDuration + " longDuration: " + longPressTimeout);
				if (0.5*longPressTimeout < touchDuration && touchDuration < longPressTimeout) {
					//right click
					p.rightButtonDown(getX(e), getY(e), e.getMetaState());

				} else if (touchDuration < 0.5*longPressTimeout){ // left click
					p.leftButtonDown(getX(e),getY(e),e.getMetaState());
					p.releaseButton(getX(e), getY(e), e.getMetaState());
				}
				resetAllModes();
				return true;
			case MotionEvent.ACTION_MOVE:
				if (touchDuration > 4*longPressTimeout){
					//Log.d("tablet", "dragMode");
					if (dragMode) {
						p.moveMouseButtonDown(getX(e), getY(e), e.getMetaState());
					} else {
						p.leftButtonDown(getX(e),getY(e),e.getMetaState());
						canvas.displayShortToastMessage("Drag Mode!");
						dragMode = true;
					}
					return true;
				}
				else if (longPressTimeout<touchDuration && touchDuration<4*longPressTimeout){
					if (scrollMode) {
						if (alternateScroll == 2) {
							gestureDetector.onTouchEvent(e);
							sendScrollEvents(getX(e), getY(e), e.getMetaState());
							alternateScroll = 0;
						}else alternateScroll++;
						return true;
					}else{
						//Log.d("tablet", "scrollMode");
						dragMode = false;
						scrollMode = true;
						return gestureDetector.onTouchEvent(e);
					}
				}
			default:
				return true;
		}
	}
}


