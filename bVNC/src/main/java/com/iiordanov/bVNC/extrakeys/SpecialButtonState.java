/**
 * Copyright (C) 2026 Iordan Iordanov
 * Copyright (C) 2021-2026 Termux App authors
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU GPL version 3 General Public License
 * as published by the Free Software Foundation.
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

package com.iiordanov.bVNC.extrakeys;

import com.google.android.material.button.MaterialButton;

import java.util.ArrayList;
import java.util.List;

/** The {@link Class} that maintains a state of a {@link SpecialButton} */
public class SpecialButtonState {

    /** If special button has been created for the {@link ExtraKeysView}. */
    boolean isCreated = false;
    /** If special button is active. */
    boolean isActive = false;
    /** If special button is locked due to long hold on it and should not be deactivated if its
     * state is read. */
    boolean isLocked = false;

    List<MaterialButton> buttons = new ArrayList<>();

    ExtraKeysView mExtraKeysView;

    /**
     * Initialize a {@link SpecialButtonState} to maintain state of a {@link SpecialButton}.
     *
     * @param extraKeysView The {@link ExtraKeysView} instance in which the {@link SpecialButton}
     *                      is to be registered.
     */
    public SpecialButtonState(ExtraKeysView extraKeysView) {
        mExtraKeysView = extraKeysView;
    }

    /** Set {@link #isCreated}. */
    public void setIsCreated(boolean value) {
        isCreated = value;
    }

    /** Set {@link #isActive}. */
    public void setIsActive(boolean value) {
        isActive = value;
        for (MaterialButton button : buttons) {
            button.setTextColor(value ? mExtraKeysView.getButtonActiveTextColor() : mExtraKeysView.getButtonTextColor());
        }
    }

    /** Set {@link #isLocked}. */
    public void setIsLocked(boolean value) {
        isLocked = value;
    }

}
