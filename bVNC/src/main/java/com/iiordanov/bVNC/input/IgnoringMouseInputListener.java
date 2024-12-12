package com.iiordanov.bVNC.input;

import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

public class IgnoringMouseInputListener implements View.OnGenericMotionListener {
    private static String TAG = "IgnoringMouseInputListener";

    @Override
    public boolean onGenericMotion(View v, MotionEvent event) {
        Log.d(TAG, "Intentionally ignoring motion event in this listener.");
        return true;
    }
}
