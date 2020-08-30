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


package com.undatech.opaque.dialogs;

import android.app.Activity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.text.method.PasswordTransformationMethod;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.remoteClientUi.R;

public class GetTextFragment extends DialogFragment {
    public static String TAG = "GetTextFragment";
    
    public interface OnFragmentDismissedListener {
        public void onTextObtained(String id, String [] obtainedStrings, boolean wasCancelled);
    }

    private TextView message;

    private EditText textBox;
    private EditText textBox2;

    private Button okButton;
    private Button cancelButton;

    private OnFragmentDismissedListener dismissalListener;
    private String title;
    private String id;
    private String username;
    private String passwordContent;

    private boolean password = false;
    private boolean wasCancelled = false;

    private String obtained = "";
    
    public GetTextFragment () {}
    
    /**
     * We make sure that if this dialog is produced automatically as part of an activity rebuild
     * and its dismissalListener is null, it closes itself upon being attached to the window.
     */
    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        if (dismissalListener == null) {
            this.dismiss();
        }
    }
    
    public void setOnFragmentDismissedListener (OnFragmentDismissedListener dismissalListener) {
        this.dismissalListener = dismissalListener;
    }
    
    public static GetTextFragment newInstance(String id, String title,
                                              OnFragmentDismissedListener dismissalListener, boolean passwordBool) {
        android.util.Log.i(TAG, "newInstance called");
        GetTextFragment f = new GetTextFragment();
        f.setOnFragmentDismissedListener(dismissalListener);
        
        Bundle args = new Bundle();
        args.putString("id", id);
        args.putString("title", title);
        args.putBoolean("password", passwordBool);
        f.setArguments(args);
        f.setRetainInstance(true);

        return f;
    }

    public static GetTextFragment newInstance(String id, String title,
                                              OnFragmentDismissedListener dismissalListener,
                                              String username,
                                              String password,
                                              boolean passwordBool) {
        android.util.Log.i(TAG, "newInstance called");
        GetTextFragment f = new GetTextFragment();
        f.setOnFragmentDismissedListener(dismissalListener);

        Bundle args = new Bundle();
        args.putString("id", id);
        args.putString("title", title);
        args.putString("username", username);
        args.putString("passwordContent", password);
        args.putBoolean("password", passwordBool);
        f.setArguments(args);
        f.setRetainInstance(true);

        return f;
    }
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        android.util.Log.i(TAG, "onCreate called");
        this.id = getArguments().getString("id");
        this.title = getArguments().getString("title");
        if (this.id == RemoteClientLibConstants.GET_CREDENTIALS_ID) {
            this.username = getArguments().getString("username");
            this.passwordContent = getArguments().getString("passwordContent");
        }
        this.password = getArguments().getBoolean("password");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        android.util.Log.i(TAG, "onCreateView called");

        // Set title for this dialog
        getDialog().setTitle(title);
        View v = null;

        switch (this.id) {
            case RemoteClientLibConstants.GET_CREDENTIALS_ID:
                v = inflater.inflate(R.layout.get_credentials_rm_keep, container, false);
                message = (TextView) v.findViewById(R.id.message);
                message.setText(title);
                textBox = (EditText) v.findViewById(R.id.textBox);
                textBox2 = (EditText) v.findViewById(R.id.textBox2);
                hideText(textBox2);
                textBox2.requestFocus();
                okButton = (Button) v.findViewById(R.id.buttonConfirm);
                cancelButton = (Button) v.findViewById(R.id.buttonCancel);
                dismissOnCancel(cancelButton);

                if (textBox != null)
                    textBox.setText(username);
                if (textBox2 != null)
                    textBox2.setText(passwordContent);
                break;
            default:
                v = inflater.inflate(R.layout.get_text_with_ok, container, false);
                textBox = (EditText) v.findViewById(R.id.textBox);
                if (password) {
                    hideText(textBox);
                }
                textBox.setOnEditorActionListener(new OnEditorActionListener () {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        boolean consumed = false;
                        if (actionId == EditorInfo.IME_NULL || actionId == EditorInfo.IME_ACTION_SEND) {
                            getDialog().dismiss();
                            consumed = true;
                        }
                        return consumed;
                    }
                });
                textBox.setHint(title);

                okButton = (Button)v.findViewById(R.id.okButton);
        }
        dismissOnConfirm(okButton);

        return v;
    }

    private void hideText (EditText textBox) {
        textBox.setTransformationMethod(new PasswordTransformationMethod());
    }

    private void dismissOnCancel (Button cancelButton) {
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                wasCancelled = true;
                getDialog().dismiss();
            }
        });
    }

    private void dismissOnConfirm (Button buttonConfirm) {
        buttonConfirm.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                getDialog().dismiss();
            }
        });
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
        android.util.Log.i(TAG, "onDismiss called: Sending data back to Activity");
        String[] results = new String[2];
        if (textBox != null) {
            results[0] = textBox.getText().toString();
            textBox.setText("");
        }
        if (textBox2 != null) {
            results[1] = textBox2.getText().toString();
            textBox2.setText("");
        }
        dismissalListener.onTextObtained(this.id, results, wasCancelled);
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }
}
