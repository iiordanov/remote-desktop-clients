package com.undatech.opaque.util;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import androidx.appcompat.widget.Toolbar;

import com.undatech.opaque.RemoteClientLibConstants;


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
                android.util.Log.d(TAG, "Moving toolbar, ACTION_DOWN");
                dX = viewToMove.getX() - event.getRawX();
                dY = viewToMove.getY() - event.getRawY();
                break;
            case MotionEvent.ACTION_MOVE:
                android.util.Log.d(TAG, "Moving toolbar, ACTION_MOVE");
                viewToMove.animate().x(event.getRawX()+dX).y(event.getRawY()+dY).setDuration(0).start();
                if (handler != null) { 
                    handler.postAtTime(runnable, SystemClock.uptimeMillis() + delay);
                }
                break;
            default:
                android.util.Log.d(TAG, "Moving toolbar, default");
                if (handler != null) {
                    android.util.Log.d(TAG, "Moving toolbar, default, handler not null");
                    handler.postAtTime(runnable, SystemClock.uptimeMillis() + delay);

                    Message m = new Message();
                    m.what = RemoteClientLibConstants.REPORT_TOOLBAR_POSITION;
                    Bundle d = new Bundle();
                    d.putInt("useLastPositionToolbarX", (int) viewToMove.getX());
                    android.util.Log.d(TAG, "Moving toolbar, default, handler not null" + viewToMove.getLeft());
                    android.util.Log.d(TAG, "Moving toolbar, default, handler not null" + viewToMove.getX());
                    d.putInt("useLastPositionToolbarY", (int) viewToMove.getY());
                    android.util.Log.d(TAG, "Moving toolbar, default, handler not null" + viewToMove.getTop());
                    android.util.Log.d(TAG, "Moving toolbar, default, handler not null" + viewToMove.getY());
                    m.setData(d);

                    handler.handleMessage(m);
                }
                return false;
        }
        return true;
    }
}
