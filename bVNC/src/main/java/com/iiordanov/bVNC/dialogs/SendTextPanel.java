/**
 * Copyright (C) 2026 Iordan Iordanov
 * Copyright (C) 2009 Michael A. MacDonald
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.bVNC.dialogs;

import android.content.Context;
import android.database.Cursor;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.SentTextBean;
import com.undatech.remoteClientUi.R;

import net.sqlcipher.database.SQLiteDatabase;

import java.util.ArrayList;

/**
 * Reusable view that provides the full Send Text panel: an EditText with history navigation
 * (previous/next/delete) and send buttons. Used both by {@link EnterTextDialog} and by the
 * extra-keys ViewPager toolbar.
 */
public class SendTextPanel extends LinearLayout {

    static final int NUMBER_SENT_SAVED = 100;
    static final int DELETED_ID = -10;

    /**
     * Callbacks fired by user actions on this panel.
     */
    public interface Callback {
        /**
         * Called when the user requests text to be sent to the remote session.
         */
        void onSendText(String text);

        /**
         * Called after a successful send action. A dialog host should dismiss itself here;
         * an embedded host (ViewPager page) can leave this as a no-op.
         */
        void onAfterSend();
    }

    private EditText textEnterText;
    private ImageButton buttonNextEntry;
    private ImageButton buttonPreviousEntry;

    private final ArrayList<SentTextBean> history = new ArrayList<>();
    private int historyIndex;

    public SendTextPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(VERTICAL);
        LayoutInflater.from(context).inflate(R.layout.extra_keys_send_text, this, true);
    }

    /**
     * Wires up button listeners and loads send history from the database.
     * Must be called once after construction before the panel is used.
     */
    public void initialize(Callback callback) {

        textEnterText = findViewById(R.id.textEnterText);
        buttonNextEntry = findViewById(R.id.buttonNextEntry);
        buttonPreviousEntry = findViewById(R.id.buttonPreviousEntry);

        loadHistory();

        buttonPreviousEntry.setOnClickListener(v -> {
            if (historyIndex > 0) {
                saveText(false);
                historyIndex--;
                textEnterText.setText(history.get(historyIndex).getSentText());
            }
            updateButtons();
        });

        buttonNextEntry.setOnClickListener(v -> {
            int oldSize = history.size();
            if (historyIndex < oldSize) {
                saveText(false);
                historyIndex++;
                if (history.size() > oldSize && historyIndex == oldSize)
                    historyIndex++;
                if (historyIndex < history.size()) {
                    textEnterText.setText(history.get(historyIndex).getSentText());
                } else {
                    textEnterText.setText("");
                }
            }
            updateButtons();
        });

        findViewById(R.id.buttonSendText).setOnClickListener(v -> {
            String s = saveText(true);
            callback.onSendText(s);
            textEnterText.setText("");
            historyIndex = history.size();
            updateButtons();
            callback.onAfterSend();
        });

        findViewById(R.id.buttonSendWithoutSaving).setOnClickListener(v -> {
            String s = textEnterText.getText().toString();
            callback.onSendText(s);
            textEnterText.setText("");
            historyIndex = history.size();
            updateButtons();
            callback.onAfterSend();
        });

        findViewById(R.id.buttonTextDelete).setOnClickListener(v -> {
            if (historyIndex < history.size()) {
                String s = textEnterText.getText().toString();
                SentTextBean bean = history.get(historyIndex);
                if (s.equals(bean.getSentText())) {
                    bean.Gen_delete(new Database(getContext()).getWritableDatabase());
                    history.remove(historyIndex);
                    if (historyIndex > 0)
                        historyIndex--;
                }
            }
            String s = historyIndex < history.size() ? history.get(historyIndex).getSentText() : "";
            textEnterText.setText(s);
            updateButtons();
        });

        // IME action (Enter key) sends text without saving to history.
        textEnterText.setOnEditorActionListener((v, actionId, event) -> {
            String text = textEnterText.getText().toString();
            callback.onSendText(text.isEmpty() ? "\n" : text);
            textEnterText.setText("");
            historyIndex = history.size();
            updateButtons();
            callback.onAfterSend();
            return true;
        });

        updateButtons();
    }

    /**
     * Moves focus to the text input field.
     */
    public void requestFocusOnText() {
        if (textEnterText != null)
            textEnterText.requestFocus();
    }

    private void loadHistory() {
        try (Cursor cursor = new Database(getContext()).getReadableDatabase().rawQuery(
                "select * from " + SentTextBean.GEN_TABLE_NAME + " ORDER BY _id", null)) {
            SentTextBean.Gen_populateFromCursor(cursor, history, SentTextBean.GEN_NEW);
        }
        historyIndex = history.size();
    }

    private String saveText(boolean wasSent) {
        CharSequence cs = textEnterText.getText();
        if (cs.length() == 0) return "";
        String s = cs.toString();
        if (wasSent || historyIndex >= history.size()
                || !s.equals(history.get(historyIndex).getSentText())) {
            SentTextBean added = new SentTextBean();
            added.setSentText(s);
            SQLiteDatabase db = new Database(getContext()).getWritableDatabase();
            added.Gen_insert(db);
            history.add(added);
            for (int i = 0; i < historyIndex - NUMBER_SENT_SAVED; i++) {
                SentTextBean candidate = history.get(i);
                if (candidate.get_Id() != DELETED_ID) {
                    candidate.Gen_delete(db);
                    candidate.set_Id(DELETED_ID);
                }
            }
        }
        return s;
    }

    private void updateButtons() {
        if (buttonPreviousEntry != null)
            buttonPreviousEntry.setEnabled(historyIndex > 0);
        if (buttonNextEntry != null)
            buttonNextEntry.setEnabled(historyIndex < history.size());
    }
}
