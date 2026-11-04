package com.iiordanov.bVNC.input;

import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;

/** Translates IME deletion requests into keys for the remote desktop. */
public class RemoteInputConnection extends BaseInputConnection {
    public RemoteInputConnection(View targetView) {
        super(targetView, false);
    }

    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        if (beforeLength < 0 || afterLength < 0) {
            return false;
        }

        // BaseInputConnection only edits its local buffer. The text and cursor
        // belong to the remote desktop, so send deletion keys even if it is empty.
        sendDeletionKeys(KeyEvent.KEYCODE_DEL, beforeLength);
        sendDeletionKeys(KeyEvent.KEYCODE_FORWARD_DEL, afterLength);
        return true;
    }

    @Override
    public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        // There is no local copy of the remote text to convert to UTF-16 offsets.
        return deleteSurroundingText(beforeLength, afterLength);
    }

    private void sendDeletionKeys(int keyCode, int count) {
        for (int i = 0; i < count; i++) {
            sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
            sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
        }
    }
}
