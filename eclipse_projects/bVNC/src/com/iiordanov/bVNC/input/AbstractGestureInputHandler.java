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

package com.iiordanov.bVNC.input;

import java.util.LinkedList;
import java.util.Queue;

import android.os.SystemClock;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.android.bc.IBCScaleGestureDetector;
import com.iiordanov.android.bc.OnScaleGestureListener;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;

/**
 * An AbstractInputHandler that uses GestureDetector to detect standard gestures in touch events
 * 
 * @author Michael A. MacDonald
 */
abstract class AbstractGestureInputHandler extends GestureDetector.SimpleOnGestureListener 
                                           implements AbstractInputHandler, OnScaleGestureListener {
    private static final String TAG = "AbstractGestureInputHandler";

    protected GestureDetector gestures;
    protected IBCScaleGestureDetector scaleGestures;

    /**
     * Handles to the VncCanvas view and RemoteCanvasActivity activity.
     */
    protected RemoteCanvas canvas;
    protected RemoteCanvasActivity activity;
    
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
    
    // The variables which indicates how many scroll events to send per swipe 
    // event and the maximum number to send at one time.
    long swipeSpeed    = 1;
    int  maxSwipeSpeed = 20;
    // If swipe events are registered once every baseSwipeTime miliseconds, then
    // swipeSpeed will be one. If more often, swipe-speed goes up, if less, down.
    final long    baseSwipeTime = 600;
    // This is how far the swipe has to travel before a swipe event is generated.
    final float   baseSwipeDist = 40.f;
    // This is how far from the top and bottom edge to detect immersive swipe.
    final float   immersiveSwipeDistance = 50.f;
    boolean immersiveSwipe = false;
    
    boolean inScrolling         = false;
    boolean inScaling           = false;
    boolean scalingJustFinished = false;
    // The minimum distance a scale event has to traverse the FIRST time before scaling starts.
    final double  minScaleFactor = 0.1;
    
    // What action was previously performed by a mouse or stylus.
    int prevMouseOrStylusAction = 0;
    
    // What the display density is.
    float displayDensity = 0;
    
    // Indicates that the next onFling will be disregarded.
    boolean disregardNextOnFling = false;
    
    // Queue which holds the last two MotionEvents which triggered onScroll
    Queue<Float> distXQueue;
    Queue<Float> distYQueue;
    
    /**
     * In the drag modes, we process mouse events without sending them through
     * the gesture detector.
     */
    protected boolean panMode        = false;
    protected boolean dragMode       = false;
    protected boolean rightDragMode  = false;
    protected boolean middleDragMode = false;
    protected float   dragX, dragY;
    protected boolean singleHandedGesture = false;
    protected boolean singleHandedJustEnded = false;


    /**
     * These variables keep track of which pointers have seen ACTION_DOWN events.
     */
    protected boolean secondPointerWasDown = false;
    protected boolean thirdPointerWasDown  = false;
    
    AbstractGestureInputHandler(RemoteCanvasActivity c, RemoteCanvas v, boolean slowScrolling)
    {
        activity = c;
        canvas = v;
        gestures=BCFactory.getInstance().getBCGestureDetector().createGestureDetector(c, this);
        gestures.setOnDoubleTapListener(this);
        scaleGestures=BCFactory.getInstance().getScaleGestureDetector(c, this);
        useDpadAsArrows = activity.getUseDpadAsArrows();
        rotateDpad      = activity.getRotateDpad();
        keyHandler = new DPadMouseKeyHandler(activity, canvas.handler, useDpadAsArrows, rotateDpad);
        displayDensity = canvas.getDisplayDensity();
        
        distXQueue = new LinkedList<Float>();
        distYQueue = new LinkedList<Float>();
        
        if (slowScrolling) {
            maxSwipeSpeed = 2;
        }
    }

    /**
     * Function to get appropriate X coordinate from motion event for this input handler.
     * @return the appropriate X coordinate.
     */
    protected int getX (MotionEvent e) {
        float scale = canvas.getScale();
        return (int)(canvas.getAbsoluteX() + e.getX() / scale);
    }

    /**
     * Function to get appropriate Y coordinate from motion event for this input handler.
     * @return the appropriate Y coordinate.
     */
    protected int getY (MotionEvent e) {
        float scale = canvas.getScale();
        return (int)(canvas.getAbsoluteY() + (e.getY() - 1.f * canvas.getTop()) / scale);
    }

    /**
     * Handles actions performed by a mouse.
     * @param e touch or generic motion event
     * @return
     */
    protected boolean handleMouseActions (MotionEvent e) {
        final int action = e.getActionMasked();
        final int meta   = e.getMetaState();
        final int bstate = e.getButtonState();
        RemotePointer p  = canvas.getPointer();
        float scale = canvas.getScale();
        int x = (int)(canvas.getAbsoluteX() +  e.getX()                          / scale);
        int y = (int)(canvas.getAbsoluteY() + (e.getY() - 1.f * canvas.getTop()) / scale);

        switch (action) {
        // If a mouse button was pressed or mouse was moved.
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_MOVE:
            switch (bstate) {
            case MotionEvent.BUTTON_PRIMARY:
                canvas.panToMouse();
                return p.processPointerEvent(x, y, action, meta, true, false, false, false, 0);
            case MotionEvent.BUTTON_SECONDARY:
                canvas.panToMouse();
                return p.processPointerEvent(x, y, action, meta, true, true, false, false, 0);
            case MotionEvent.BUTTON_TERTIARY:
                canvas.panToMouse();
                return p.processPointerEvent(x, y, action, meta, true, false, true, false, 0);
            }
            break;
        // If a mouse button was released.
        case MotionEvent.ACTION_UP:
            switch (bstate) {
            case 0:
                if (e.getToolType(0) != MotionEvent.TOOL_TYPE_MOUSE) {
                    break;
                }
            case MotionEvent.BUTTON_PRIMARY:
            case MotionEvent.BUTTON_SECONDARY:
            case MotionEvent.BUTTON_TERTIARY:
                canvas.panToMouse();
                return p.processPointerEvent(x, y, action, meta, false, false, false, false, 0);
            }
            break;
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
                
            int numEvents = 0;
            while (numEvents < swipeSpeed) {
                p.processPointerEvent(x, y, action, meta, true, false, false, true, direction);
                p.processPointerEvent(x, y, action, meta, false, false, false, false, 0);
                numEvents++;
            }
            break;
        // If the mouse was moved OR as reported, some external mice trigger this when a
        // mouse button is pressed as well, so we check bstate here too.
        case MotionEvent.ACTION_HOVER_MOVE:
            canvas.panToMouse();
            switch (bstate) {
            case MotionEvent.BUTTON_PRIMARY:
                return p.processPointerEvent(x, y, action, meta, true, false, false, false, 0);
            case MotionEvent.BUTTON_SECONDARY:
                return p.processPointerEvent(x, y, action, meta, true, true, false, false, 0);
            case MotionEvent.BUTTON_TERTIARY:
                return p.processPointerEvent(x, y, action, meta, true, false, true, false, 0);
            default:
                return p.processPointerEvent(x, y, action, meta, false, false, false, false, 0);                
            }
        }
        
        prevMouseOrStylusAction = action;
        return false;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onSingleTapConfirmed(android.view.MotionEvent)
     */
    @Override
    public boolean onSingleTapConfirmed(MotionEvent e) {
        RemotePointer p  = canvas.getPointer();
        final int action = e.getActionMasked();
        final int meta   = e.getMetaState();
        activity.showToolbar();
        p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, false, 0);
        SystemClock.sleep(50);
        p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
        canvas.panToMouse();
        return true;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onDoubleTap(android.view.MotionEvent)
     */
    @Override
    public boolean onDoubleTap (MotionEvent e) {
        RemotePointer p  = canvas.getPointer();
        final int action = e.getActionMasked();
        final int meta   = e.getMetaState();
        
        p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, false, 0);
        SystemClock.sleep(50);
        p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
        SystemClock.sleep(50);
        p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, false, 0);
        SystemClock.sleep(50);
        p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
        canvas.panToMouse();
        return true;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onLongPress(android.view.MotionEvent)
     */
    @Override
    public void onLongPress(MotionEvent e) {
        RemotePointer p = canvas.getPointer();

        // If we've performed a right/middle-click and the gesture is not over yet, do not start drag mode.
        if (secondPointerWasDown || thirdPointerWasDown)
            return;
        
        BCFactory.getInstance().getBCHaptic().performLongPressHaptic(canvas);
        dragMode = true;
        p.processPointerEvent(getX(e), getY(e), e.getActionMasked(), e.getMetaState(), true, false, false, false, 0);
    }
    
    protected boolean endDragModesAndScrolling () {
        canvas.inScrolling = false;
        panMode               = false;
        inScaling             = false;
        inSwiping             = false;
        inScrolling           = false;
        immersiveSwipe        = false;
        if (dragMode || rightDragMode || middleDragMode) {
            dragMode          = false;
            rightDragMode     = false;
            middleDragMode    = false;
            return true;
        } else {
            return false;
        }
    }
    
    private void detectImmersiveSwipe (float y) {
        if (Constants.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT &&
            (y <= immersiveSwipeDistance || canvas.getHeight() - y <= immersiveSwipeDistance)) {
            inSwiping = true;
            immersiveSwipe = true;
        } else {
            inSwiping = false;
            immersiveSwipe = false;
        }
    }
    
    /**
     * Modify the event so that the mouse goes where we specify.
     * @param e event to be modified.
     * @param x new x coordinate.
     * @param y new y coordinate.
     */
    private void setEventCoordinates(MotionEvent e, float x, float y) {
        e.setLocation(x, y);
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
            canvas.bitmapData.drawable._defaultPaint.setFilterBitmap(true);
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
                
                canvas.inScrolling = true;
                // If we are manipulating the desktop, turn off bitmap filtering for faster response.
                canvas.bitmapData.drawable._defaultPaint.setFilterBitmap(false);
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
                if (endDragModesAndScrolling())
                    return p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
                
                break;
            case MotionEvent.ACTION_MOVE:
                // Send scroll up/down events if swiping is happening.
                if (panMode) {
                    float scale = canvas.getScale();
                    canvas.pan(-(int)((e.getX() - dragX)*scale), -(int)((e.getY() - dragY)*scale));
                    dragX = e.getX();
                    dragY = e.getY();
                    return true;
                } else if (dragMode) {
                    canvas.panToMouse();
                    return p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, false, 0);
                } else if (rightDragMode) {
                    canvas.panToMouse();
                    return p.processPointerEvent(getX(e), getY(e), action, meta, true, true, false, false, 0);
                } else if (middleDragMode) {
                    canvas.panToMouse();
                    return p.processPointerEvent(getX(e), getY(e), action, meta, true, false, true, false, 0);
                } else if (inSwiping) {
                    // Save the coordinates and restore them afterward.
                    float x = e.getX();
                    float y = e.getY();
                    // Set the coordinates to where the swipe began (i.e. where scaling started).
                    setEventCoordinates(e, xInitialFocus, yInitialFocus);
                    int numEvents = 0;
                    while (numEvents < swipeSpeed && numEvents < maxSwipeSpeed) {
                        if        (twoFingerSwipeUp)   {
                            p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, true, 0);
                            p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
                        } else if (twoFingerSwipeDown) {
                            p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, true, 1);
                            p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
                        } else if (twoFingerSwipeLeft)   {
                            p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, true, 2);
                            p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
                        } else if (twoFingerSwipeRight) {
                            p.processPointerEvent(getX(e), getY(e), action, meta, true, false, false, true, 3);
                            p.processPointerEvent(getX(e), getY(e), action, meta, false, false, false, false, 0);
                        }
                        numEvents++;
                    }
                    // Restore the coordinates so that onScale doesn't get all muddled up.
                    setEventCoordinates(e, x, y);
                } else if (immersiveSwipe) {
                    // If this is part of swipe that shows the nav bar, consume.
                    return true;
                }
            }
            break;

        case 1:
            switch (action) {
            case MotionEvent.ACTION_POINTER_DOWN:
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
                    p.processPointerEvent(getX(e), getY(e), action, meta, true, true, false, false, 0);
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
                    p.processPointerEvent(getX(e), getY(e), action, meta, true, false, true, false, 0);
                    // Enter middle-drag mode.
                    middleDragMode      = true;
                }
            }
            break;
        }
        
        scaleGestures.onTouchEvent(e);
        return gestures.onTouchEvent(e);
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

            if (consumed) {
                inScaling = true;
                //Log.i(TAG,"Adjust scaling " + detector.getScaleFactor());
                if (canvas != null && canvas.scaling != null)
                    canvas.scaling.adjust(activity, detector.getScaleFactor(), xCurrentFocus, yCurrentFocus);
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
        return (int) Math.pow(Math.abs(delta) * 6.01, 2.5) * (delta < 0.0 ? -1 : 1);
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
        
        RemotePointer pointer = canvas.getPointer();
        evt.offsetLocation(pointer.getX() + dx - evt.getX(),
                            pointer.getY() + dy - evt.getY());

        if (pointer.processPointerEvent(evt, trackballButtonDown, false))
            return true;
        
        return activity.onTouchEvent(evt);
    }
    
    /**
     * Returns the sign of the given number.
     * @param number the given number
     * @return -1 for negative and 1 for positive.
     */
    protected float sign (float number) {
        return (number > 0) ? 1 : -1;
    }
}