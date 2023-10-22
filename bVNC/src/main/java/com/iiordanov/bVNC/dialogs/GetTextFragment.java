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


package com.iiordanov.bVNC.dialogs;

import android.content.DialogInterface;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.method.PasswordTransformationMethod;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Switch;
import android.widget.TextView;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import com.undatech.remoteClientUi.R;

public class GetTextFragment extends DialogFragment {
	public static String TAG = "GetTextFragment";
    public static final int Plaintext = 1;
    public static final int Password = 2;
    public static final int MatchingPasswordTwice = 3;
    public static final int Credentials = 4;
    public static final int CredentialsWithDomain = 5;
    public static final int CredentialsWithReadOnlyUser = 6;
    public static final int PasswordNoKeep = 7;

    public static final String DIALOG_ID_GET_PASSWORD                  = "DIALOG_ID_GET_PASSWORD";
    public static final String DIALOG_ID_GET_MASTER_PASSWORD           = "DIALOG_ID_GET_MASTER_PASSWORD";
    public static final String DIALOG_ID_GET_MATCHING_MASTER_PASSWORDS = "DIALOG_ID_GET_MATCHING_MASTER_PASSWORDS";
    public static final String DIALOG_ID_GET_VERIFICATIONCODE          = "DIALOG_ID_GET_VERIFICATIONCODE";
    public static final String DIALOG_ID_GET_SSH_CREDENTIALS           = "DIALOG_ID_GET_SSH_CREDENTIALS";
    public static final String DIALOG_ID_GET_SSH_PASSPHRASE            = "DIALOG_ID_GET_SSH_PASSPHRASE";
    public static final String DIALOG_ID_GET_VNC_CREDENTIALS           = "DIALOG_ID_GET_VNC_CREDENTIALS";
    public static final String DIALOG_ID_GET_VNC_PASSWORD              = "DIALOG_ID_GET_VNC_PASSWORD";
    public static final String DIALOG_ID_GET_RDP_CREDENTIALS           = "DIALOG_ID_GET_RDP_CREDENTIALS";
    public static final String DIALOG_ID_GET_SPICE_PASSWORD            = "DIALOG_ID_GET_SPICE_PASSWORD";
    public static final String DIALOG_ID_GET_OPAQUE_CREDENTIALS        = "DIALOG_ID_GET_OPAQUE_CREDENTIALS";
    public static final String DIALOG_ID_GET_OPAQUE_PASSWORD           = "DIALOG_ID_GET_OPAQUE_PASSWORD";
    public static final String DIALOG_ID_GET_OPAQUE_OTP_CODE           = "DIALOG_ID_GET_OPAQUE_OTP_CODE";

    public interface OnFragmentDismissedListener {
        void onTextObtained(String dialogId, String[] obtainedStrings, boolean dialogCancelled, boolean save, boolean[] obtainedBools);
    }

    private class TextMatcher implements TextWatcher {
        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            error.setVisibility(View.GONE);
        }
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
        @Override
        public void afterTextChanged(Editable arg0) {}
    }

    private boolean wasCancelled = false;
    private boolean wasConfirmed = false;
    private TextView message;
    private TextView error;
    private TextView textViewBox;
    private EditText textBox;
    private EditText textBox2;
    private EditText textBox3;
    private Switch switch1;
    private Button buttonConfirm;
    private Button buttonCancel;
    private CheckBox checkboxKeepPassword;
    private OnFragmentDismissedListener dismissalListener;
	private String title;
    private String dialogId;
    private String t1;
    private String t2;
    private String t3;
    private boolean keepPassword;
    private boolean sw1;
    private int dialogType = 0;
    private int messageNum = 0;
    private int errorNum = 0;

    public GetTextFragment () {
    }

    public static GetTextFragment newInstance(String dialogId, String title,
                                              OnFragmentDismissedListener dismissalListener,
	                                          int dialogType, int messageNum, int errorNum,
                                              String t1, String t2, String t3, boolean keepPassword,
                                              boolean sw1) {
    	android.util.Log.i(TAG, "newInstance called");
    	GetTextFragment f = new GetTextFragment();
    	f.setDismissalListener(dismissalListener);

        Bundle args = new Bundle();
        args.putString("dialogId", dialogId);
        args.putString("title", title);
        args.putInt("dialogType", dialogType);
        args.putInt("messageNum", messageNum);
        args.putInt("errorNum", errorNum);
        args.putString("t1", t1);
        args.putString("t2", t2);
        args.putString("t3", t3);
        args.putBoolean("keepPassword", keepPassword);
        args.putBoolean("sw1", sw1);
        f.setArguments(args);
        f.setRetainInstance(false);

        return f;
    }
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    	android.util.Log.i(TAG, "onCreate called");
        dialogId = getArguments().getString("dialogId");
        title = getArguments().getString("title");
        dialogType = getArguments().getInt("dialogType");
        messageNum = getArguments().getInt("messageNum");
        errorNum = getArguments().getInt("errorNum");
        t1 = getArguments().getString("t1");
        t2 = getArguments().getString("t2");
        t3 = getArguments().getString("t3");
        keepPassword = getArguments().getBoolean("keepPassword");
        sw1 = getArguments().getBoolean("sw1");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    	android.util.Log.i(TAG, "onCreateView called");
    	wasCancelled = false;
        wasConfirmed = false;
    	
        getDialog().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN|WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);

    	// Set title for this dialog
    	getDialog().setTitle(title);
    	View v = null;
        
        switch (dialogType) {
        case Plaintext:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            checkboxKeepPassword = v.findViewById(R.id.checkboxKeepPassword);
            if (dialogId == DIALOG_ID_GET_OPAQUE_OTP_CODE) {
                textBox.setHint("");
                checkboxKeepPassword.setVisibility(View.INVISIBLE);
            }
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        case PasswordNoKeep:
        case Password:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            hideText(textBox);
            checkboxKeepPassword = v.findViewById(R.id.checkboxKeepPassword);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            if (dialogType == PasswordNoKeep) {
                checkboxKeepPassword.setVisibility(View.GONE);
            }
            break;
        case MatchingPasswordTwice:
            v = inflater.inflate(R.layout.get_text_twice, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textBox = (EditText) v.findViewById(R.id.textBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            switch1 = (Switch) v.findViewById(R.id.switch1);
            switch1.setVisibility(View.VISIBLE);
            switch1.setText(R.string.master_password_only_lock_connection_editing);
            hideText(textBox);
            hideText(textBox2);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            ensureMatchingDismissOnConfirm (buttonConfirm, textBox, textBox2, switch1, error);
            break;
        case CredentialsWithDomain:
            v = inflater.inflate(R.layout.get_credentials_with_domain, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textBox = (EditText) v.findViewById(R.id.textBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            textBox3 = (EditText) v.findViewById(R.id.textBox3);
            hideText(textBox3);
            textBox3.requestFocus();
            checkboxKeepPassword = v.findViewById(R.id.checkboxKeepPassword);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        case Credentials:
            v = inflater.inflate(R.layout.get_credentials, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textBox = (EditText) v.findViewById(R.id.textBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            hideText(textBox2);
            textBox2.requestFocus();
            checkboxKeepPassword = v.findViewById(R.id.checkboxKeepPassword);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        case CredentialsWithReadOnlyUser:
            v = inflater.inflate(R.layout.get_credentials_with_read_only_user, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textViewBox = v.findViewById(R.id.textViewBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            hideText(textBox2);
            textBox2.requestFocus();
            checkboxKeepPassword = v.findViewById(R.id.checkboxKeepPassword);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        default:
            getDialog().dismiss();
            break;
        }

        if (textViewBox != null)
            textViewBox.setText(t1);
        if (textBox != null && t1 != null)
            textBox.setText(t1);
        if (textBox2 != null && t2 != null)
            textBox2.setText(t2);
        if (textBox3 != null && t3 != null)
            textBox3.setText(t3);
        if (checkboxKeepPassword != null)
            checkboxKeepPassword.setChecked(keepPassword);
        if (switch1 != null)
            switch1.setChecked(sw1);

        message = (TextView) v.findViewById(R.id.message);
        message.setText(messageNum);
        
        this.setRetainInstance(true);
        
        return v;
    }

    public void setDismissalListener (OnFragmentDismissedListener dismissalListener) {
        this.dismissalListener = dismissalListener;
    }

    private void hideText (EditText textBox) {
        textBox.setTransformationMethod(new PasswordTransformationMethod());
    }
    
    private void dismissOnCancel (Button cancelButton) { 
        cancelButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                wasCancelled = true;
                getDialog().dismiss();
            }
        });
    }
    
    private void dismissOnConfirm (Button buttonConfirm) { 
        buttonConfirm.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                wasConfirmed = true;
                getDialog().dismiss();
            }
        });
    }
    
    private void ensureMatchingDismissOnConfirm (Button buttonConfirm, final EditText textBox1, final EditText textBox2, final Switch switch1, final TextView error) {
        buttonConfirm.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (textBox1.getText().toString().equals(textBox2.getText().toString())) {
                    wasConfirmed = true;
                    getDialog().dismiss();
                } else {
                    error.setText(errorNum);
                    error.setVisibility(View.VISIBLE);
                    error.invalidate();
                }
            }
        });
        
        textBox1.addTextChangedListener(new TextMatcher());
        textBox2.addTextChangedListener(new TextMatcher());
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
    	android.util.Log.i(TAG, "onDismiss called: Sending data back to Activity");
        String[] results = new String[3];
        boolean[] b_results = new boolean[3];
        if (textViewBox != null) {
            results[0] = textViewBox.getText().toString();
            textViewBox.setText("");
        }
        if (textBox != null) {
            results[0] = textBox.getText().toString();
            textBox.setText("");
        }
        if (textBox2 != null) {
            results[1] = textBox2.getText().toString();
            textBox2.setText("");
        }
        if (textBox3 != null) {
            results[2] = textBox3.getText().toString();
            textBox3.setText("");
        }
        if (checkboxKeepPassword != null) {
            keepPassword = checkboxKeepPassword.isChecked();
        }
        if (switch1 != null) {
            b_results[0] = switch1.isChecked();
            switch1.setChecked(false);
        }
        if (dismissalListener != null && (wasConfirmed||wasCancelled)) {
            boolean cancelled = wasCancelled;
            wasCancelled = false;
            wasConfirmed = false;
            dismissalListener.onTextObtained(dialogId, results, cancelled, keepPassword, b_results);
        }
    	super.onDismiss(dialog);
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }

    @Override
    public void show(FragmentManager fm, String tag) {
        try {
            FragmentTransaction ft = fm.beginTransaction();
            for (Fragment fragment : fm.getFragments()) {
                fm.beginTransaction().remove(fragment).commit();
            }
            ft.add(this, tag); //.addToBackStack(null);
            ft.commitAllowingStateLoss();
        } catch (IllegalStateException e) {
            Log.e("IllegalStateException", "Exception", e);
            e.printStackTrace();
        }
    }

}
