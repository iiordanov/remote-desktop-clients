package com.iiordanov.bVNC.input;

import android.view.KeyEvent;

public interface PointerInputHandler {
    boolean onKeyAsPointerEvent(int keyCode, KeyEvent event);
}
