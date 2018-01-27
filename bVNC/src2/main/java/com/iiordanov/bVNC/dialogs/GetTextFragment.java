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
import android.support.v4.app.DialogFragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.method.PasswordTransformationMethod;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

public class GetTextFragment extends DialogFragment {
	public static String TAG = "GetTextFragment";
    public static final int Plaintext = 1;
    public static final int Password = 2;
    public static final int MatchingPasswordTwice = 3;

    public interface OnFragmentDismissedListener {
        public void onTextObtained(String obtainedString, boolean dialogCancelled);
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
    private TextView message;
    private TextView error;
    private EditText textBox;
    private EditText textBox2;
    private Button buttonConfirm;
    private Button buttonCancel;
    private OnFragmentDismissedListener dismissalListener;
	private String title;
	
    private int dialogType = 0;
    private int messageNum = 0;
    private int errorNum = 0;

    public GetTextFragment () {
    }

	public static GetTextFragment newInstance(String title, OnFragmentDismissedListener dismissalListener,
	                                          int dialogType, int messageNum, int errorNum) {
    	android.util.Log.i(TAG, "newInstance called");
    	GetTextFragment f = new GetTextFragment();
    	f.setDismissalListener(dismissalListener);

        Bundle args = new Bundle();
        args.putString("title", title);
        args.putInt("dialogType", dialogType);
        args.putInt("messageNum", messageNum);
        args.putInt("errorNum", errorNum);
        f.setArguments(args);
        f.setRetainInstance(false);

        return f;
    }
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    	android.util.Log.i(TAG, "onCreate called");
        title = getArguments().getString("title");
        dialogType = getArguments().getInt("dialogType");
        messageNum = getArguments().getInt("messageNum");
        errorNum = getArguments().getInt("errorNum");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    	android.util.Log.i(TAG, "onCreateView called");
    	wasCancelled = false;
    	
        getDialog().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN|WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);

    	// Set title for this dialog
    	getDialog().setTitle(title);
    	View v = null;
        
        switch (dialogType) {
        case Plaintext:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        case Password:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            hideText(textBox);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            dismissOnConfirm(buttonConfirm);
            break;
        case MatchingPasswordTwice:
            v = inflater.inflate(R.layout.get_text_twice, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textBox = (EditText) v.findViewById(R.id.textBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            hideText(textBox);
            hideText(textBox2);
            buttonConfirm = (Button) v.findViewById(R.id.buttonConfirm);
            buttonCancel = (Button) v.findViewById(R.id.buttonCancel);
            dismissOnCancel(buttonCancel);
            ensureMatchingDismissOnConfirm (buttonConfirm, textBox, textBox2, error);
            break;
        default:
            getDialog().dismiss();
            break;
        }
        
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
                getDialog().dismiss();
            }
        });
    }
    
    private void ensureMatchingDismissOnConfirm (Button buttonConfirm, final EditText textBox1, final EditText textBox2, final TextView error) {
        buttonConfirm.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (textBox1.getText().toString().equals(textBox2.getText().toString())) {
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
    	String result = textBox.getText().toString();
    	if (wasCancelled || result.equals("")) {
    	    wasCancelled = true;
    	    result = "";
    	}
    	
    	if (textBox != null) {
    	    textBox.setText("");
    	}
    	if (textBox2 != null) {
    	    textBox2.setText("");
    	}
    	if (dismissalListener != null) {
            dismissalListener.onTextObtained(result, wasCancelled);
        }
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }
}