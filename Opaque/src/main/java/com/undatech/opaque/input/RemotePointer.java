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

public interface RemotePointer {
    public int getX();
    public int getY();
    public void setX(int newX);
    public void setY(int newY);
    public void movePointer(int x, int y);
    public void movePointerToMakeVisible();
    public boolean hardwareButtonsAsMouseEvents(int keyCode, KeyEvent evt, int combinedMetastate);
    public void leftButtonDown  (int x, int y, int metaState);
    public void middleButtonDown(int x, int y, int metaState);
    public void rightButtonDown (int x, int y, int metaState);
    public void scrollUp        (int x, int y, int metaState);
    public void scrollDown      (int x, int y, int metaState);
    public void scrollLeft      (int x, int y, int metaState);
    public void scrollRight     (int x, int y, int metaState);
    public void releaseButton   (int x, int y, int metaState);
    public void moveMouse       (int x, int y, int metaState);
    public void moveMouseButtonDown (int x, int y, int metaState);
    public void moveMouseButtonUp   (int x, int y, int metaState);
}
