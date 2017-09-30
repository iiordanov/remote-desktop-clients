package com.iiordanov.bVNC.input;

import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

public class SimulatedTouchpadInputHandler extends AbstractGestureInputHandler {
    static final String TAG = "SimulatedTouchpadInputHandler";
    public static final String TOUCHPAD_MODE = "TOUCHPAD_MODE";
    float sensitivity = 0;
    boolean acceleration = false;

    /**
     * @param c
     */
    public SimulatedTouchpadInputHandler(RemoteCanvasActivity va, RemoteCanvas v, boolean slowScrolling) {
        super(va, v, slowScrolling);
        acceleration = activity.getAccelerationEnabled();
        sensitivity = activity.getSensitivity();
    }

    /*
     * (non-Javadoc)
     * 
     * @see com.iiordanov.bVNC.AbstractInputHandler#getHandlerDescription()
     */
    @Override
    public CharSequence getHandlerDescription() {
        return canvas.getResources().getString(R.string.input_mode_touchpad_description);
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
     * @see android.view.GestureDetector.SimpleOnGestureListener#onScroll(android.view.MotionEvent,
     *      android.view.MotionEvent, float, float)
     */
    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        RemotePointer p = canvas.getPointer();
        final int action = e2.getActionMasked();
        final int meta   = e2.getMetaState();
        
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
        if (twoFingers||inSwiping||inScaling||scalingJustFinished)
            return true;

        activity.showToolbar();

        // If the gesture has just began, then don't allow a big delta to prevent
        // pointer jumps at the start of scrolling.
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

        // Make distanceX/Y display density independent.
        distanceX = sensitivity * distanceX / displayDensity;
        distanceY = sensitivity * distanceY / displayDensity;
        
        // Compute the absolute new mouse position on the remote site.
        int newRemoteX = (int) (p.getX() + getDelta(-distanceX));
        int newRemoteY = (int) (p.getY() + getDelta(-distanceY));
        p.processPointerEvent(newRemoteX, newRemoteY, action, meta, false, false, false, false, 0);
        canvas.panToMouse();
        return true;
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
    
    protected int getX (MotionEvent e) {
        RemotePointer p = canvas.getPointer();
        if (dragMode || rightDragMode || middleDragMode) {
            float distanceX = e.getX() - dragX;
            dragX = e.getX();
            // Compute the absolute new X coordinate on the remote site.
            return (int) (p.getX() + getDelta(distanceX));
        }
        dragX = e.getX();
        return p.getX();
    }

    protected int getY (MotionEvent e) {
        RemotePointer p = canvas.getPointer();
        if (dragMode || rightDragMode || middleDragMode) {
            float distanceY = e.getY() - dragY;
            dragY = e.getY();
            // Compute the absolute new Y coordinate on the remote site.
            return (int) (p.getY() + getDelta(distanceY));
        }
        dragY = e.getY();
        return p.getY();
    }

    private float getDelta(float distance) {
        // Compute the relative movement offset on the remote screen.
        float delta = (float) (distance * Math.cbrt(canvas.getScale()));
        return fineCtrlScale(delta);
    }

    /**
     * Scale down delta when it is small. This will allow finer control
     * when user is making a small movement on touch screen.
     * Scale up delta when delta is big. This allows fast mouse movement when
     * user is flinging.
     * @param deltaX
     * @return
     */
    private float fineCtrlScale(float delta) {
        float sign = sign(delta);
        delta = Math.abs(delta);
        if (delta <= 15) {
            delta *= 0.75;
        } else if (acceleration && delta <= 70 ) {
            delta *= delta/20;
        } else if (acceleration) {
            delta *= 4.5;
        }
        return sign * delta;
    }
}
