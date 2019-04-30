package com.undatech.opaque.util;

import android.os.Handler;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;


public class OnTouchViewMover implements OnTouchListener {
    public static String TAG = OnTouchViewMover.class.getSimpleName();

    View viewToMove;
    Handler handler;
    Runnable runnable;
    long delay;
    float dX, dY;
    
    public OnTouchViewMover(View viewToMove, Handler handler, Runnable runnable, long delay) {
        this.viewToMove = viewToMove;
        this.handler = handler;
        this.runnable = runnable;
        this.delay = delay;
    }
    
    @Override
    public boolean onTouch(View view, MotionEvent event) {
        android.util.Log.i(TAG, "Moving toolbar");
        if (handler != null) { 
            handler.removeCallbacks(runnable);
        }
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                dX = viewToMove.getX() - event.getRawX();
                dY = viewToMove.getY() - event.getRawY();
                break;
            case MotionEvent.ACTION_MOVE:
                viewToMove.animate().x(event.getRawX()+dX).y(event.getRawY()+dY).setDuration(0).start();
                if (handler != null) { 
                    handler.postAtTime(runnable, SystemClock.uptimeMillis() + delay);
                }
                break;
            default:
                if (handler != null) {
                    handler.postAtTime(runnable, SystemClock.uptimeMillis() + delay);
                }
                return false;
        }
        return true;
    }
}
