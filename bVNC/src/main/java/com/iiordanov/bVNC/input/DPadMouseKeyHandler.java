/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.bVNC.input;

import com.iiordanov.bVNC.RemoteCanvas;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.undatech.opaque.input.RemoteKeyboard;

import android.graphics.PointF;
import android.os.Handler;
import android.view.KeyEvent;

/**
 * Input handlers delegate to this class to handle keystrokes; this detects keystrokes
 * from the DPad and uses them to perform mouse actions; other keystrokes are passed to
 * RemoteCanvasActivity.defaultKeyXXXHandler
 * 
 * @author Iordan Iordanov
 * @author Michael A. MacDonald
 *
 */
class DPadMouseKeyHandler {
    private MouseMover mouseMover;
    private boolean mouseDown;
    private RemoteCanvas canvas;
    private boolean isMoving;
    private boolean useDpadAsArrows = false;
    private boolean rotateDpad      = false;
    RemoteKeyboard keyboard;
    RemotePointer pointer;

    DPadMouseKeyHandler(RemoteCanvasActivity activity, Handler handler, boolean arrows, boolean rotate)
    {
        canvas = activity.getCanvas();
        mouseMover = new MouseMover(activity, handler);
        useDpadAsArrows = arrows;
        rotateDpad      = rotate;
    }

    public boolean onKeyDown(int keyCode, KeyEvent evt) {
        int xv = 0;
        int yv = 0;
        boolean result = true;
        keyboard = canvas.getKeyboard();
        pointer  = canvas.getPointer();
        boolean cameraButtonDown = keyboard.getCameraButtonDown();

        // If we are instructed to rotate the Dpad at 90 degrees, reassign KeyCodes.
        if (rotateDpad) {
            switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:
                keyCode = KeyEvent.KEYCODE_DPAD_UP;
                break;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                keyCode = KeyEvent.KEYCODE_DPAD_DOWN;
                break;
            case KeyEvent.KEYCODE_DPAD_UP:
                keyCode = KeyEvent.KEYCODE_DPAD_RIGHT;
                break;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                keyCode = KeyEvent.KEYCODE_DPAD_LEFT;
                break;
            }
        }

        // If we are supposed to use the Dpad as arrows, pass the event to the default handler.
        if (useDpadAsArrows) {
            return keyboard.keyEvent(keyCode, evt);
            // Otherwise, control the mouse.
        } else {
            switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:
                xv = -1;
                break;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                xv = 1;
                break;
            case KeyEvent.KEYCODE_DPAD_UP:
                yv = -1;
                break;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                yv = 1;
                break;
            case KeyEvent.KEYCODE_DPAD_CENTER:
                if (!mouseDown) {
                    mouseDown = true;
                    result = true;
                    pointer.leftButtonDown(pointer.getX(), pointer.getY(), evt.getMetaState());
                }
                break;
            default:
                result = keyboard.keyEvent(keyCode, evt);
                break;
            }
        }
        if ((xv != 0 || yv != 0) && !isMoving) {
            final int x = xv;
            final int y = yv;
            isMoving = true;
            mouseMover.start(x, y, new Panner.VelocityUpdater() {

                /*
                 * (non-Javadoc)
                 * 
                 * @see com.iiordanov.bVNC.Panner.VelocityUpdater#updateVelocity(android.graphics.Point,
                 *      long)
                 */
                @Override
                public boolean updateVelocity(PointF p, long interval) {
                    double scale = (1.2 * (double) interval / 50.0);
                    if (Math.abs(p.x) < 500)
                        p.x += (int) (scale * x);
                    if (Math.abs(p.y) < 500)
                        p.y += (int) (scale * y);
                    return true;
                }

            });
            if (mouseDown) {
                pointer.moveMouseButtonDown(pointer.getX(), pointer.getY(), evt.getMetaState());
            } else {
                pointer.moveMouseButtonUp(pointer.getX(), pointer.getY(), evt.getMetaState());
            }
        }
        return result;
    }

    public boolean onKeyUp(int keyCode, KeyEvent evt) {

        boolean cameraButtonDown = keyboard.getCameraButtonDown();
        pointer  = canvas.getPointer();

        // Pass the event on if we are not controlling the mouse.
        if (useDpadAsArrows)
            return keyboard.keyEvent(keyCode, evt);

        boolean result = false;

        switch (keyCode) {
        case KeyEvent.KEYCODE_DPAD_LEFT:
        case KeyEvent.KEYCODE_DPAD_RIGHT:
        case KeyEvent.KEYCODE_DPAD_UP:
        case KeyEvent.KEYCODE_DPAD_DOWN:
            mouseMover.stop();
            isMoving = false;
            result = true;
            break;
        case KeyEvent.KEYCODE_DPAD_CENTER:
            if (mouseDown) {
                mouseDown = false;
                pointer.releaseButton(pointer.getX(), pointer.getY(), evt.getMetaState());
            } else {
                result = true;
            }
            break;
        default:
            result = keyboard.keyEvent(keyCode, evt);
            break;
        }
        return result;
    }
}
