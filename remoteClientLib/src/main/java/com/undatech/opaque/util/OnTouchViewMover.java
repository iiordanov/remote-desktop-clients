package com.undatech.opaque.util;

import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;


public class OnTouchViewMover implements OnTouchListener {
    public static String TAG = OnTouchViewMover.class.getSimpleName();

    View viewToMove;
    Handler handler;
    Runnable runOnDrop;
    Runnable runOnMoveWithDelay;
    long delay;
    float dX, dY;
    int lastX, lastY;

    public int getLastX() {
        return lastX;
    }

    public int getLastY() {
        return lastY;
    }

    public OnTouchViewMover(
            View viewToMove, Handler handler, Runnable runOnDrop, Runnable runOnMoveWithDelay, long delay
    ) {
        this.viewToMove = viewToMove;
        this.handler = handler;
        this.runOnDrop = runOnDrop;
        this.runOnMoveWithDelay = runOnMoveWithDelay;
        this.delay = delay;
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        Log.d(TAG, "onTouch called, moving toolbar");
        if (handler != null) {
            handler.removeCallbacks(runOnMoveWithDelay);
        }
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                Log.d(TAG, "onTouch called ACTION_DOWN");
                dX = viewToMove.getX() - event.getRawX();
                dY = viewToMove.getY() - event.getRawY();
                break;
            case MotionEvent.ACTION_MOVE:
                Log.d(TAG, "onTouch called ACTION_MOVE");
                viewToMove.animate().x(event.getRawX() + dX).y(event.getRawY() + dY).setDuration(0).start();
                if (handler != null) {
                    handler.postAtTime(runOnMoveWithDelay, SystemClock.uptimeMillis() + delay);
                }
                break;
            default:
                Log.d(TAG, "onTouch called default");
                lastX = (int)viewToMove.getX();
                lastY = (int)viewToMove.getY();
                if (handler != null) {
                    handler.post(runOnDrop);
                    handler.postAtTime(runOnMoveWithDelay, SystemClock.uptimeMillis() + delay);
                }
                return false;
        }
        return true;
    }
}
