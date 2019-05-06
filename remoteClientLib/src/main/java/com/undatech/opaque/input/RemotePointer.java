/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */


package com.undatech.opaque.input;

import android.view.KeyEvent;
import android.view.MotionEvent;

public abstract class RemotePointer {
    public static final int POINTER_DOWN_MASK         = 0x8000;

    /**
     * Indicates where the mouse pointer is located.
     */
    protected int pointerX, pointerY;

    protected boolean relativeEvents = false;

    public static float DEFAULT_SENSITIVITY = 2.0f;
    public static boolean DEFAULT_ACCELERATED = true;

    protected float sensitivity = DEFAULT_SENSITIVITY;
    protected boolean accelerated = DEFAULT_ACCELERATED;

    public abstract int getX();
    public abstract int getY();
    public abstract void setX(int newX);
    public abstract void setY(int newY);
    public abstract void movePointer(int x, int y);
    public abstract void movePointerToMakeVisible();
    public abstract boolean hardwareButtonsAsMouseEvents(int keyCode, KeyEvent evt, int combinedMetastate);
    public abstract void leftButtonDown  (int x, int y, int metaState);
    public abstract void middleButtonDown(int x, int y, int metaState);
    public abstract void rightButtonDown (int x, int y, int metaState);
    public abstract void scrollUp        (int x, int y, int metaState);
    public abstract void scrollDown      (int x, int y, int metaState);
    public abstract void scrollLeft      (int x, int y, int metaState);
    public abstract void scrollRight     (int x, int y, int metaState);
    public abstract void releaseButton   (int x, int y, int metaState);
    public abstract void moveMouse       (int x, int y, int metaState);
    public abstract void moveMouseButtonDown (int x, int y, int metaState);
    public abstract void moveMouseButtonUp   (int x, int y, int metaState);

    public boolean isRelativeEvents() {
        return relativeEvents;
    }

    public void setRelativeEvents(boolean relativeEvents) {
        this.relativeEvents = relativeEvents;
        if (relativeEvents) {
            setSensitivity(1.0f);
            setAccelerated(false);
        } else {
            setSensitivity(DEFAULT_SENSITIVITY);
            setAccelerated(DEFAULT_ACCELERATED);
        }
    }

    public float getSensitivity() {
        return sensitivity;
    }

    public void setSensitivity(float sensitivity) {
        this.sensitivity = sensitivity;
    }

    public boolean isAccelerated() {
        return accelerated;
    }

    public void setAccelerated(boolean accelerated) {
        this.accelerated = accelerated;
    }

}
