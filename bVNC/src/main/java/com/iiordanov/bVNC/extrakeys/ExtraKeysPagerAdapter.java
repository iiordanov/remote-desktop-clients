/**
 * Copyright (C) 2026 Iordan Iordanov
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

import android.content.Context;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.viewpager.widget.PagerAdapter;

import com.iiordanov.bVNC.dialogs.SendTextPanel;
import com.iiordanov.bVNC.input.RemoteKeyboard;

import org.json.JSONException;

/**
 * PagerAdapter for the extra-keys toolbar. Manages three pages:
 * page 0 - Send Text panel, page 1 - modifier/navigation keys, page 2 - function keys.
 */
public class ExtraKeysPagerAdapter extends PagerAdapter {

    private static final String TAG = "ExtraKeysPagerAdapter";

    /**
     * Callbacks supplied by the host to provide runtime dependencies.
     */
    public interface Callbacks {
        /** Called when the user requests text to be sent to the remote session. */
        void onSendText(String text);
        /**
         * Returns the keyboard to send key events to, or {@code null} if not yet available.
         * Called lazily when pages are instantiated.
         */
        @Nullable RemoteKeyboard getKeyboard();
    }

    private final Context context;
    private final Callbacks callbacks;

    private SendTextPanel sendTextPanel;
    private ExtraKeysView extraKeysView;

    public ExtraKeysPagerAdapter(@NonNull Context context, @NonNull Callbacks callbacks) {
        this.context = context;
        this.callbacks = callbacks;
    }

    /** Returns the Send Text panel, or {@code null} if page 0 has not been instantiated yet. */
    @Nullable
    public SendTextPanel getSendTextPanel() {
        return sendTextPanel;
    }

    /** Returns the modifier/navigation keys view, or {@code null} if page 1 has not been instantiated yet. */
    @Nullable
    public ExtraKeysView getExtraKeysView() {
        return extraKeysView;
    }

    @Override
    public int getCount() {
        return 3;
    }

    @Override
    public boolean isViewFromObject(@NonNull View view, @NonNull Object object) {
        return view == object;
    }

    @NonNull
    @Override
    public Object instantiateItem(@NonNull ViewGroup container, int position) {
        View page;
        if (position == 0) {
            sendTextPanel = new SendTextPanel(context, null);
            sendTextPanel.initialize(new SendTextPanel.Callback() {
                @Override
                public void onSendText(String text) {
                    callbacks.onSendText(text);
                }
                @Override
                public void onAfterSend() {
                    // Stay on the panel; text is already cleared internally.
                }
            });
            page = sendTextPanel;
        } else if (position == 1) {
            extraKeysView = new ExtraKeysView(context, null);
            try {
                ExtraKeysInfo info = new ExtraKeysInfo(
                    "[[\"ESC\",\"TAB\",\"SHIFT\",\"PGUP\",\"PGDN\",\"HOME\",\"UP\",\"END\"]," +
                    "[\"CTRL\",\"SUPER\",\"ALT\",\"DEL\",\"|\",\"LEFT\",\"DOWN\",\"RIGHT\"]]",
                    "default",
                    ExtraKeysConstants.CONTROL_CHARS_ALIASES);
                extraKeysView.reload(info, 0);
                extraKeysView.setExtraKeysViewClient(
                    new RemoteExtraKeysHandler(extraKeysView, callbacks.getKeyboard()));
                extraKeysView.setSpecialButtonsChangeListener(this::syncKeyboardModifierState);
            } catch (JSONException e) {
                Log.e(TAG, "Failed to initialize extra keys", e);
            }
            page = extraKeysView;
        } else {
            ExtraKeysView functionKeysView = new ExtraKeysView(context, null);
            try {
                ExtraKeysInfo info = new ExtraKeysInfo(
                    "[[\"F1\",\"F2\",\"F3\",\"F4\",\"F5\",\"F6\",\"F7\",\"F8\"]," +
                    "[\"F9\",\"F10\",\"F11\",\"F12\",\"`\",\"~\",\"/\",\"\\\\\"]]",
                    "default",
                    ExtraKeysConstants.CONTROL_CHARS_ALIASES);
                functionKeysView.reload(info, 0);
                functionKeysView.setExtraKeysViewClient(
                    new RemoteExtraKeysHandler(functionKeysView, extraKeysView, callbacks.getKeyboard()));
                functionKeysView.setSpecialButtonsChangeListener(this::syncKeyboardModifierState);
            } catch (JSONException e) {
                Log.e(TAG, "Failed to initialize function keys", e);
            }
            page = functionKeysView;
        }
        container.addView(page);
        return page;
    }

    /**
     * Mirrors the ExtraKeysView special button state into the keyboard's onScreenMetaState so
     * that soft keyboard key events are sent with the correct modifier flags.
     */
    private void syncKeyboardModifierState(ExtraKeysView view) {
        RemoteKeyboard keyboard = callbacks.getKeyboard();
        if (keyboard == null) return;
        keyboard.clearMetaState();
        if (Boolean.TRUE.equals(view.readSpecialButton(SpecialButton.CTRL,  false))) keyboard.onScreenCtrlToggle();
        if (Boolean.TRUE.equals(view.readSpecialButton(SpecialButton.ALT,   false))) keyboard.onScreenAltToggle();
        if (Boolean.TRUE.equals(view.readSpecialButton(SpecialButton.SHIFT, false))) keyboard.onScreenShiftToggle();
        if (Boolean.TRUE.equals(view.readSpecialButton(SpecialButton.SUPER, false))) keyboard.onScreenSuperToggle();
    }

    @Override
    public void destroyItem(@NonNull ViewGroup container, int position, @NonNull Object object) {
        container.removeView((View) object);
    }
}
