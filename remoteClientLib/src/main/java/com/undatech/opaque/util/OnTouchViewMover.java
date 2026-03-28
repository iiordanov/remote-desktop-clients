package com.undatech.opaque.util;

import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;


public class OnTouchViewMover implements OnTouchListener {
    public static String TAG = OnTouchViewMover.class.getSimpleName();

    public interface BoundsProvider {
        android.graphics.Rect getBounds();
    }

    View viewToMove;
    Handler handler;
    Runnable runOnDrop;
    Runnable runOnMoveWithDelay;
    long delay;
    float dX, dY;
    int lastX, lastY;
    BoundsProvider boundsProvider;

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

    public void setBoundsProvider(BoundsProvider boundsProvider) {
        this.boundsProvider = boundsProvider;
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
                float newX = event.getRawX() + dX;
                float newY = event.getRawY() + dY;
                if (boundsProvider != null) {
                    android.graphics.Rect bounds = boundsProvider.getBounds();
                    int maxX = Math.max(0, bounds.right - viewToMove.getWidth());
                    int maxY = Math.max(0, bounds.bottom - viewToMove.getHeight());
                    newX = Math.max(bounds.left, Math.min(newX, maxX));
                    newY = Math.max(bounds.top, Math.min(newY, maxY));
                }
                viewToMove.animate().x(newX).y(newY).setDuration(0).start();
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
