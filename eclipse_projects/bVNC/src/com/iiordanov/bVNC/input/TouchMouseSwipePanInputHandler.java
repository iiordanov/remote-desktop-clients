package com.iiordanov.bVNC.input;

import android.graphics.PointF;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

public class TouchMouseSwipePanInputHandler extends AbstractGestureInputHandler {
    static final String TAG = "TouchMouseSwipePanInputHandler";
    public static final String TOUCH_ZOOM_MODE = "TOUCH_ZOOM_MODE";

    /**
     * Divide stated fling velocity by this amount to get initial velocity
     * per pan interval
     */
    static final float FLING_FACTOR = 8;
    
    /**
     * @param c
     */
    public TouchMouseSwipePanInputHandler(RemoteCanvasActivity va, RemoteCanvas v, boolean slowScrolling) {
        super(va, v, slowScrolling);
    }

    /*
     * (non-Javadoc)
     * 
     * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
     */
    @Override
    public CharSequence getHandlerDescription() {
        return canvas.getResources().getString(R.string.input_mode_touch_pan_description);
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
     * @see com.iiordanov.bVNC.RemoteCanvasActivity.ZoomInputHandler#onKeyDown(int,
     *      android.view.KeyEvent)
     */
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent evt) {
        return keyHandler.onKeyDown(keyCode, evt);
    }

    /*
     * (non-Javadoc)
     * 
     * @see com.iiordanov.bVNC.RemoteCanvasActivity.ZoomInputHandler#onKeyUp(int,
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

    /*
     * (non-Javadoc)
     * 
     * @see android.view.GestureDetector.SimpleOnGestureListener#onFling(android.view.MotionEvent,
     *      android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {

        // TODO: Workaround for Android 4.2.
        boolean twoFingers = false;
        if (e1 != null)
            twoFingers = (e1.getPointerCount() > 1);
        if (e2 != null)
            twoFingers = twoFingers || (e2.getPointerCount() > 1);

        // onFling called while scaling/swiping gesture is in effect. We ignore the event and pretend it was
        // consumed. This prevents the mouse pointer from flailing around while we are scaling.
        // Also, if one releases one finger slightly earlier than the other when scaling, it causes Android 
        // to stick a spiteful onFling with a MASSIVE delta here. 
        // This would cause the mouse pointer to jump to another place suddenly.
        // Hence, we ignore onFling after scaling until we lift all pointers up.
        if (twoFingers||disregardNextOnFling||inSwiping||inScaling||scalingJustFinished) {
            return true;
        }

        activity.showToolbar();
        activity.getPanner().start(-(velocityX / FLING_FACTOR),
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
     * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent,
     *      android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {

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

        if (!inScrolling) {
            inScrolling = true;
            distanceX = sign(distanceX);
            distanceY = sign(distanceY);
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
        
        float scale = canvas.getScale();
        activity.showToolbar();
        return canvas.pan((int)(distanceX*scale), (int)(distanceY*scale));
    }
}

