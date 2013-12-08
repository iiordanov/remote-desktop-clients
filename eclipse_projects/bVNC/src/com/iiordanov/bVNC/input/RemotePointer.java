package com.iiordanov.bVNC.input;

import android.view.KeyEvent;
import android.view.MotionEvent;

public interface RemotePointer {
    public int getX();
    public int getY();
    public void setX(int newX);
    public void setY(int newY);
    public void warpMouse(int x, int y);
    public void mouseFollowPan();
    boolean handleHardwareButtons(int keyCode, KeyEvent evt, int combinedMetastate);
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent);
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction);
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton);
    public boolean processPointerEvent(MotionEvent evt, boolean downEvent, boolean useRightButton);
    public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton);
    
    public boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton,
                                        boolean useMiddleButton, boolean useScrollButton, int direction);
}
