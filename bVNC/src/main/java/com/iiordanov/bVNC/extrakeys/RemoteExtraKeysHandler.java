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

import android.os.SystemClock;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;

import com.google.android.material.button.MaterialButton;
import com.iiordanov.bVNC.input.RemoteKeyboard;

/**
 * Handles extra key button clicks by translating them into key events sent to the remote session.
 * Implements {@link ExtraKeysView.IExtraKeysView}.
 */
public class RemoteExtraKeysHandler implements ExtraKeysView.IExtraKeysView {

    private final ExtraKeysView modifierSource;
    private final RemoteKeyboard remoteKeyboard;

    /**
     * @param extraKeysView  The {@link ExtraKeysView} that owns the buttons being clicked.
     * @param remoteKeyboard The {@link RemoteKeyboard} to send key events to.
     */
    public RemoteExtraKeysHandler(ExtraKeysView extraKeysView, RemoteKeyboard remoteKeyboard) {
        this(extraKeysView, null, remoteKeyboard);
    }

    /**
     * @param extraKeysView  The {@link ExtraKeysView} that owns the buttons being clicked.
     * @param modifierSource A separate {@link ExtraKeysView} to read modifier state from (e.g. when
     *                       keys and modifiers live on different pages). Pass {@code null} to read
     *                       modifiers from {@code extraKeysView} itself.
     * @param remoteKeyboard The {@link RemoteKeyboard} to send key events to.
     */
    public RemoteExtraKeysHandler(ExtraKeysView extraKeysView, ExtraKeysView modifierSource, RemoteKeyboard remoteKeyboard) {
        this.modifierSource = modifierSource != null ? modifierSource : extraKeysView;
        this.remoteKeyboard = remoteKeyboard;
    }

    @Override
    public void onExtraKeyButtonClick(View view, ExtraKeyButton buttonInfo, MaterialButton button) {
        // Read and auto-reset modifier state once, then send each key in the sequence (a single
        // non-macro key is just a one-element sequence).
        boolean ctrlDown  = Boolean.TRUE.equals(modifierSource.readSpecialButton(SpecialButton.CTRL,  true));
        boolean altDown   = Boolean.TRUE.equals(modifierSource.readSpecialButton(SpecialButton.ALT,   true));
        boolean shiftDown = Boolean.TRUE.equals(modifierSource.readSpecialButton(SpecialButton.SHIFT, true));
        boolean fnDown    = Boolean.TRUE.equals(modifierSource.readSpecialButton(SpecialButton.FN,    true));
        boolean superDown = Boolean.TRUE.equals(modifierSource.readSpecialButton(SpecialButton.SUPER, true));

        int additionalMetaState = buildMetaState(ctrlDown, altDown, shiftDown, fnDown, superDown);

        for (String key : buttonInfo.getKey().split(" ")) {
            sendKey(key, additionalMetaState);
        }
        // Fire the listener so syncKeyboardModifierState clears keyboard.onScreenMetaState for any
        // non-locked modifiers that were consumed above. Locked modifiers remain active.
        modifierSource.resetSpecialButtons();
    }

    private int buildMetaState(boolean ctrlDown, boolean altDown, boolean shiftDown, boolean fnDown, boolean superDown) {
        int additionalMetaState = 0;
        if (ctrlDown)  additionalMetaState |= KeyEvent.META_CTRL_ON  | KeyEvent.META_CTRL_LEFT_ON;
        if (altDown)   additionalMetaState |= KeyEvent.META_ALT_ON   | KeyEvent.META_ALT_LEFT_ON;
        if (shiftDown) additionalMetaState |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_LEFT_ON;
        if (fnDown)    additionalMetaState |= KeyEvent.META_FUNCTION_ON;
        if (superDown) additionalMetaState |= KeyEvent.META_META_ON  | KeyEvent.META_META_LEFT_ON;
        return additionalMetaState;
    }

    private void sendKey(String key, int additionalMetaState) {
        Integer keyCode = ExtraKeysConstants.PRIMARY_KEY_CODES_FOR_STRINGS.get(key);
        if (keyCode != null) {
            long now = SystemClock.uptimeMillis();
            remoteKeyboard.processLocalKeyEvent(keyCode,
                new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, 0), additionalMetaState);
            remoteKeyboard.processLocalKeyEvent(keyCode,
                new KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0, 0), additionalMetaState);
        } else {
            // Literal character key (e.g. "|") — use ACTION_MULTIPLE + KEYCODE_UNKNOWN path.
            long now = SystemClock.uptimeMillis();
            KeyEvent charEvent = new KeyEvent(now, key, KeyCharacterMap.FULL, 0);
            remoteKeyboard.processLocalKeyEvent(KeyEvent.KEYCODE_UNKNOWN, charEvent, additionalMetaState);
        }
    }

    @Override
    public boolean performExtraKeyButtonHapticFeedback(View view, ExtraKeyButton buttonInfo, MaterialButton button) {
        // Let ExtraKeysView handle haptic feedback using system settings.
        return false;
    }

}
