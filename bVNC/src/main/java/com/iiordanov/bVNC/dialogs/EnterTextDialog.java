/**
 * Copyright (C) 2012 Iordan Iordanov
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

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;

import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.undatech.remoteClientUi.R;

/**
 * @author Michael A. MacDonald
 */
public class EnterTextDialog extends Dialog {

    private final RemoteCanvasActivity _canvasActivity;
    private SendTextPanel panel;

    public EnterTextDialog(Context context) {
        super(context);
        setOwnerActivity((Activity) context);
        _canvasActivity = (RemoteCanvasActivity) context;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        panel = new SendTextPanel(getContext(), null);
        setContentView(panel);
        setTitle(R.string.enter_text_title);
        panel.initialize(new SendTextPanel.Callback() {
            @Override
            public void onSendText(String text) {
                _canvasActivity.inputListener.sendText(text);
            }

            @Override
            public void onAfterSend() {
                dismiss();
            }
        });
    }

    @Override
    protected void onStart() {
        super.onStart();
        if (panel != null) panel.requestFocusOnText();
    }
}
