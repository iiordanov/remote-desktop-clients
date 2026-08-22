package com.undatech.opaque.util;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;

/**
 * Floating toolbar used in remote canvas activities.
 * Positioning logic (clamping, bounds, overflow menu padding) lives in RemoteCanvasActivity.
 */
public class RemoteToolbar extends Toolbar {
    private static final String TAG = "RemoteToolbar";

    public RemoteToolbar(Context context) {
        super(context);
    }

    public RemoteToolbar(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public RemoteToolbar(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }
}
