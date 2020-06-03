/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
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

import android.os.Handler;
import android.os.SystemClock;

/**
 * Specialization of panner that moves the mouse instead of panning the screen
 * 
 * @author Michael A. MacDonald
 *
 */
class MouseMover extends Panner {

    public MouseMover(RemoteCanvasActivity act, Handler hand) {
        super(act, hand);
    }

    /* (non-Javadoc)
     * @see java.lang.Runnable#run()
     */
    @Override
    public void run() {
        long interval = SystemClock.uptimeMillis() - lastSent;
        lastSent += interval;
        double scale = (double)interval / 50.0;
        RemoteCanvas canvas = activity.getCanvas();
        RemotePointer p = canvas.getPointer();
        
        //Log.v(TAG, String.format("panning %f %d %d", scale, (int)((double)velocity.x * scale), (int)((double)velocity.y * scale)));
        p.moveMouseButtonUp((int)(p.getX() + ((double)velocity.x * scale)),
                                              (int)(p.getY() + ((double)velocity.y * scale)), 0);
        if (updater.updateVelocity(velocity, interval)) {
            handler.postDelayed(this, 50);
        } else {
            //Log.v(TAG, "Updater requests stop");
            stop();
        }
    }

}
