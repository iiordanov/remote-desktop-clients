/*
 * Copyright (C) 2010 The Android Open Source Project
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

package com.undatech.opaque.input;

import android.content.Context;
import android.util.DisplayMetrics;
import android.util.FloatMath;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ScaleGestureDetector.OnScaleGestureListener;
import android.view.ViewConfiguration;

/**
 * Detects transformation gestures involving more than one pointer ("multitouch")
 * using the supplied {@link MotionEvent}s. The {@link OnScaleGestureListener}
 * callback will notify users when a particular gesture event has occurred.
 * This class should only be used with {@link MotionEvent}s reported via touch.
 * 
 * To use this class:
 * <ul>
 *  <li>Create an instance of the {@code ScaleGestureDetector} for your
 *      {@link View}
 *  <li>In the {@link View#onTouchEvent(MotionEvent)} method ensure you call
 *          {@link #onTouchEvent(MotionEvent)}. The methods defined in your
 *          callback will be executed when the events occur.
 * </ul>
 */
class MyScaleGestureDetector extends ScaleGestureDetector {
	private static final String TAG = "MyScaleGestureDetector";

	/**
     * This value is the threshold ratio between our previous combined pressure
     * and the current combined pressure. We will only fire an onScale event if
     * the computed ratio between the current and previous event pressures is
     * greater than this value. When pressure decreases rapidly between events
     * the position values can often be imprecise, as it usually indicates
     * that the user is in the process of lifting a pointer off of the device.
     * Its value was tuned experimentally.
     */
    private static final float PRESSURE_THRESHOLD = 0.67f;

    private final Context mContext;
    private final OnScaleGestureListener mListener;
    private boolean mGestureInProgress;

    private MotionEvent mPrevEvent;
    private MotionEvent mCurrEvent;

    private float mFocusX;
    private float mFocusY;
    private float mPrevFingerDiffX;
    private float mPrevFingerDiffY;
    private float mCurrFingerDiffX;
    private float mCurrFingerDiffY;
    private float mCurrLen;
    private float mPrevLen;
    private float mScaleFactor;
    private float mCurrPressure;
    private float mPrevPressure;
    private long mTimeDelta;
    
    private final float mEdgeSlop;
    private float mRightSlopEdge;
    private float mBottomSlopEdge;
    private boolean mSloppyGesture;

    public MyScaleGestureDetector(Context context, OnScaleGestureListener listener) {
    	super (context, listener);
        ViewConfiguration config = ViewConfiguration.get(context);
        mContext = context;
        mListener = listener;
        mEdgeSlop = config.getScaledEdgeSlop();
    }

    /* (non-Javadoc)
     * @see com.iiordanov.android.bc.IBCScaleGestureDetector#onTouchEvent(android.view.MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
    	final int action = event.getAction();
    	boolean handled = true;
    	boolean secondpointer = event.getPointerCount() > 1;

    	try {
    		if (!mGestureInProgress) {
    			switch (action & MotionEvent.ACTION_MASK) {
    			case MotionEvent.ACTION_POINTER_DOWN: {
    				// We have a new multi-finger gesture

    				// as orientation can change, query the metrics in touch down
    				DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();
    				mRightSlopEdge = metrics.widthPixels - mEdgeSlop;
    				mBottomSlopEdge = metrics.heightPixels - mEdgeSlop;

    				// Be paranoid in case we missed an event
    				reset();

    				mPrevEvent = MotionEvent.obtain(event);
    				mTimeDelta = 0;

    				setContext(event);

    				// Check if we have a sloppy gesture. If so, delay
    				// the beginning of the gesture until we're sure that's
    				// what the user wanted. Sloppy gestures can happen if the
    				// edge of the user's hand is touching the screen, for example.
    				final float edgeSlop = mEdgeSlop;
    				final float rightSlop = mRightSlopEdge;
    				final float bottomSlop = mBottomSlopEdge;
    				final float x0 = event.getRawX();
    				final float y0 = event.getRawY();
    				boolean p1sloppy = false;
    				if (secondpointer) {
    					final float x1 = getRawX(event, 1);
    					final float y1 = getRawY(event, 1);
    					p1sloppy = x1 < edgeSlop || y1 < edgeSlop
    							|| x1 > rightSlop || y1 > bottomSlop;
    				}

    				boolean p0sloppy = x0 < edgeSlop || y0 < edgeSlop
    						|| x0 > rightSlop || y0 > bottomSlop;

    						if (p0sloppy && p1sloppy) {
    							mFocusX = -1;
    							mFocusY = -1;
    							//mSloppyGesture = true;
    						} else if (p0sloppy && secondpointer) {
    							mFocusX = event.getX(1);
    							mFocusY = event.getY(1);
    							//mSloppyGesture = true;
    						} else if (p1sloppy) {
    							mFocusX = event.getX(0);
    							mFocusY = event.getY(0);
    							//mSloppyGesture = true;
    						} else {
    							mGestureInProgress = mListener.onScaleBegin(this);
    						}
    			}
    			break;

    			case MotionEvent.ACTION_MOVE:
    				if (mSloppyGesture) {
    					// Initiate sloppy gestures if we've moved outside of the slop area.
    					final float edgeSlop = mEdgeSlop;
    					final float rightSlop = mRightSlopEdge;
    					final float bottomSlop = mBottomSlopEdge;
    					final float x0 = event.getRawX();
    					final float y0 = event.getRawY();

    					boolean p0sloppy = x0 < edgeSlop || y0 < edgeSlop
    							|| x0 > rightSlop || y0 > bottomSlop;

    							boolean p1sloppy = false;
    							if (secondpointer) {
    								final float x1 = getRawX(event, 1);
    								final float y1 = getRawY(event, 1);
    								p1sloppy = x1 < edgeSlop || y1 < edgeSlop
    										|| x1 > rightSlop || y1 > bottomSlop;
    							}

    							if(p0sloppy && p1sloppy) {
    								mFocusX = -1;
    								mFocusY = -1;
    							} else if (p0sloppy && secondpointer) {
    								mFocusX = event.getX(1);
    								mFocusY = event.getY(1);
    							} else if (p1sloppy) {
    								mFocusX = event.getX(0);
    								mFocusY = event.getY(0);
    							} else {
    								mSloppyGesture = false;
    								mGestureInProgress = mListener.onScaleBegin(this);
    							}
    				}
    				break;

    			case MotionEvent.ACTION_POINTER_UP:
    				if (mSloppyGesture) {
    					// Set focus point to the remaining finger
    					int id = (((action & MotionEvent.ACTION_POINTER_ID_MASK)
    							>> MotionEvent.ACTION_POINTER_ID_SHIFT) == 0) ? 1 : 0;
    					if (id == 0 || secondpointer) {
    						mFocusX = event.getX(id);
    						mFocusY = event.getY(id);
    					}
    				}
    				break;
    			}
    		} else {
    			// Transform gesture in progress - attempt to handle it
    			switch (action & MotionEvent.ACTION_MASK) {
    			case MotionEvent.ACTION_POINTER_UP:
    				// Gesture ended
    				setContext(event);

    				// Set focus point to the remaining finger
    				int id = (((action & MotionEvent.ACTION_POINTER_ID_MASK)
    						>> MotionEvent.ACTION_POINTER_ID_SHIFT) == 0) ? 1 : 0;
    				if (id == 0 || secondpointer) {
    					mFocusX = event.getX(id);
    					mFocusY = event.getY(id);
    				}

    				if (!mSloppyGesture) {
    					mListener.onScaleEnd(this);
    				}

    				reset();
    				break;

    			case MotionEvent.ACTION_CANCEL:
    				if (!mSloppyGesture) {
    					mListener.onScaleEnd(this);
    				}

    				reset();
    				break;

    			case MotionEvent.ACTION_MOVE:
    				setContext(event);

    				// Only accept the event if our relative pressure is within
    				// a certain limit - this can help filter shaky data as a
    				// finger is lifted.
    				if (mCurrPressure / mPrevPressure > PRESSURE_THRESHOLD) {
    					final boolean updatePrevious = mListener.onScale(this);

    					if (updatePrevious) {
    						mPrevEvent.recycle();
    						mPrevEvent = MotionEvent.obtain(event);
    					}
    				}
    				break;
    			}
    		}
    	} catch (Exception e) {
    		Log.e(TAG, "There was an error in onTouchEvent.");
    		e.printStackTrace();
    	}
    	return handled;
    }
    
    /**
     * MotionEvent has no getRawX(int) method; simulate it pending future API approval. 
     */
    private static float getRawX(MotionEvent event, int pointerIndex) {
        float offset = event.getX() - event.getRawX();
        return event.getX(pointerIndex) + offset;
    }
    
    /**
     * MotionEvent has no getRawY(int) method; simulate it pending future API approval. 
     */
    private static float getRawY(MotionEvent event, int pointerIndex) {
        float offset = event.getY() - event.getRawY();
        return event.getY(pointerIndex) + offset;
    }

    private void setContext(MotionEvent curr) {
        boolean secondpointer = curr.getPointerCount() > 1;

        if (mCurrEvent != null) {
            mCurrEvent.recycle();
        }
        mCurrEvent = MotionEvent.obtain(curr);

        mCurrLen = -1;
        mPrevLen = -1;
        mScaleFactor = -1;

        final MotionEvent prev = mPrevEvent;

        final float px0 = prev.getX(0);
        final float py0 = prev.getY(0);
        final float cx0 = curr.getX(0);
        final float cy0 = curr.getY(0);

        float prevpress = 0;
        if (secondpointer) {
            final float px1 = prev.getX(1);
            final float py1 = prev.getY(1);        	
            final float pvx = px1 - px0;
            final float pvy = py1 - py0;
            mPrevFingerDiffX = pvx;
            mPrevFingerDiffY = pvy;
            prevpress = prev.getPressure(1);
        } else {
            mPrevFingerDiffX = 0;
            mPrevFingerDiffY = 0;
        }
        
        float cvx = 0;
        float cvy = 0;
        float currpress = 0;
        if (secondpointer) {
            final float cx1 = curr.getX(1);
            final float cy1 = curr.getY(1);        	
            cvx = cx1 - cx0;
            cvy = cy1 - cy0;
            mCurrFingerDiffX = cvx;
            mCurrFingerDiffY = cvy;
            currpress = curr.getPressure(1);
        } else {
            mCurrFingerDiffX = 0;
            mCurrFingerDiffY = 0;
        }

        mFocusX = cx0 + cvx * 0.5f;
        mFocusY = cy0 + cvy * 0.5f;
        mTimeDelta = curr.getEventTime() - prev.getEventTime();
        mCurrPressure = curr.getPressure(0) + currpress;
        mPrevPressure = prev.getPressure(0) + prevpress;
    }

    private void reset() {
        if (mPrevEvent != null) {
            mPrevEvent.recycle();
            mPrevEvent = null;
        }
        if (mCurrEvent != null) {
            mCurrEvent.recycle();
            mCurrEvent = null;
        }
        mSloppyGesture = false;
        mGestureInProgress = false;
    }

    /**
     * Returns {@code true} if a two-finger scale gesture is in progress.
     * @return {@code true} if a scale gesture is in progress, {@code false} otherwise.
     */
    public boolean isInProgress() {
        return mGestureInProgress;
    }

    /**
     * Get the X coordinate of the current gesture's focal point.
     * If a gesture is in progress, the focal point is directly between
     * the two pointers forming the gesture.
     * If a gesture is ending, the focal point is the location of the
     * remaining pointer on the screen.
     * If {@link #isInProgress()} would return false, the result of this
     * function is undefined.
     * 
     * @return X coordinate of the focal point in pixels.
     */
    public float getFocusX() {
        return mFocusX;
    }

    /**
     * Get the Y coordinate of the current gesture's focal point.
     * If a gesture is in progress, the focal point is directly between
     * the two pointers forming the gesture.
     * If a gesture is ending, the focal point is the location of the
     * remaining pointer on the screen.
     * If {@link #isInProgress()} would return false, the result of this
     * function is undefined.
     * 
     * @return Y coordinate of the focal point in pixels.
     */
    public float getFocusY() {
        return mFocusY;
    }

    /**
     * Return the current distance between the two pointers forming the
     * gesture in progress.
     * 
     * @return Distance between pointers in pixels.
     */
    public float getCurrentSpan() {
        if (mCurrLen == -1) {
            final double cvx = mCurrFingerDiffX;
            final double cvy = mCurrFingerDiffY;
            mCurrLen = (float) Math.sqrt(cvx*cvx + cvy*cvy);
        }
        return mCurrLen;
    }

    /**
     * Return the previous distance between the two pointers forming the
     * gesture in progress.
     * 
     * @return Previous distance between pointers in pixels.
     */
    public float getPreviousSpan() {
        if (mPrevLen == -1) {
            final double pvx = mPrevFingerDiffX;
            final double pvy = mPrevFingerDiffY;
            mPrevLen = (float) Math.sqrt(pvx*pvx + pvy*pvy);
        }
        return mPrevLen;
    }

    /**
     * Return the scaling factor from the previous scale event to the current
     * event. This value is defined as
     * ({@link #getCurrentSpan()} / {@link #getPreviousSpan()}).
     * 
     * @return The current scaling factor.
     */
    public float getScaleFactor() {
        if (mScaleFactor == -1) {
            mScaleFactor = getCurrentSpan() / getPreviousSpan();
        }
        return mScaleFactor;
    }
    
    /**
     * Return the time difference in milliseconds between the previous
     * accepted scaling event and the current scaling event.
     * 
     * @return Time difference since the last scaling event in milliseconds.
     */
    public long getTimeDelta() {
        return mTimeDelta;
    }
    
    /**
     * Return the event time of the current event being processed.
     * 
     * @return Current event time in milliseconds.
     */
    public long getEventTime() {
        return mCurrEvent.getEventTime();
    }
}
