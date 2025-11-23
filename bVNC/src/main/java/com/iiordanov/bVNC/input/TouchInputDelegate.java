/**
 * Copyright (C) 2025 Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC.input;

import android.view.View;

/**
 * Interface that abstracts the functionality that touch input handlers need
 * from the activity, removing direct dependency on RemoteCanvasActivity.
 */
public interface TouchInputDelegate {
    
    /**
     * Show the action bar
     */
    void showActionBar();
    
    /**
     * Send a short vibration feedback to the user
     */
    void sendShortVibration();
    
    /**
     * Find a view by its ID
     * @param id the resource ID of the view
     * @return the view with the specified ID, or null if not found
     */
    <T extends View> T findViewById(int id);

    /**
     * Check if DPad should be used as arrow keys
     * @return true if DPad should be used as arrow keys
     */
    boolean getUseDpadAsArrows();

    /**
     * Check if DPad should be rotated
     * @return true if DPad should be rotated
     */
    boolean getRotateDpad();


    /**
     * Gets pointer offset in x axis due to e.g. insets
     * @return the width of the left inset
     */
    int getxPointerOffset();

    /**
     * Gets pointer offset in y axis due to e.g. window decorations
     * @return the height of the window decoration
     */
    int getyPointerOffset();
}