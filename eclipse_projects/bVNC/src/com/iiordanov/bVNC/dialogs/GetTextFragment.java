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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.ListIterator;

import org.xmlpull.v1.XmlPullParser;

import com.iiordanov.bVNC.R;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.text.method.PasswordTransformationMethod;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.util.Xml;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;
import android.widget.Toast;
import android.widget.LinearLayout.LayoutParams;

public class GetTextFragment extends DialogFragment {
	public static String TAG = "GetTextFragment";
    public static final int Plaintext = 1;
    public static final int Password = 2;
    public static final int MatchingPasswordTwice = 3;

    public interface OnFragmentDismissedListener {
        public void onTextObtained(String obtainedString, boolean dialogCancelled);
    }
    
	private boolean wasCancelled = false;
    private TextView message;
    private TextView error;
    private EditText textBox;
    private EditText textBox2;
    private OnFragmentDismissedListener dismissalListener;
	private String title;
	
    private int dialogType = 0;
    private int messageNum = 0;
    private int errorNum = 0;
	private String obtained = "";
    
	public GetTextFragment (OnFragmentDismissedListener dismissalListener) {
		this.dismissalListener = dismissalListener;
	}
    
	public static GetTextFragment newInstance(String title, OnFragmentDismissedListener dismissalListener,
	                                          int dialogType, int messageNum, int errorNum) {
    	android.util.Log.i(TAG, "newInstance called");
    	GetTextFragment f = new GetTextFragment(dismissalListener);

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

    	// Set title for this dialog
    	getDialog().setTitle(title);
    	View v = null;
    	
        switch (dialogType) {
        case Plaintext:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            break;
        case Password:
            v = inflater.inflate(R.layout.get_text, container, false);
            textBox = (EditText) v.findViewById(R.id.textBox);
            hideText(textBox);
            dismissOnEnter(textBox);
            break;
        case MatchingPasswordTwice:
            v = inflater.inflate(R.layout.get_text_twice, container, false);
            error = (TextView) v.findViewById(R.id.error);
            textBox = (EditText) v.findViewById(R.id.textBox);
            textBox2 = (EditText) v.findViewById(R.id.textBox2);
            hideText(textBox);
            hideText(textBox2);
            ensureMatchingDismissOnEnter (textBox, textBox2, error);
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
    
    private void hideText (EditText textBox) {
        textBox.setTransformationMethod(new PasswordTransformationMethod());
    }
    
    private void dismissOnEnter (EditText textBox){ 
        textBox.setOnEditorActionListener(new OnEditorActionListener () {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                boolean consumed = false;
                if (actionId == EditorInfo.IME_NULL) {
                    getDialog().dismiss();
                    consumed = true;
                }
                return consumed;
            }
        });
    }
    
    private void ensureMatchingDismissOnEnter (final EditText textBox1, final EditText textBox2, final TextView error){
        // Focus on the 2nd textbox if Enter key is detected.
        textBox1.setOnEditorActionListener(new OnEditorActionListener () {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (textBox1.getText().toString().equals(textBox2.getText().toString())) {
                    error.setVisibility(View.GONE);
                }
                
                boolean consumed = false;
                if (actionId == EditorInfo.IME_NULL) {
                    consumed = true;
                    textBox2.requestFocus();
                }
                return consumed;
            }
        });
        // Check if text is matching and show error if not when Enter key is detected.
        textBox2.setOnEditorActionListener(new OnEditorActionListener () {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (textBox1.getText().toString().equals(textBox2.getText().toString())) {
                    error.setVisibility(View.GONE);
                }
                
                boolean consumed = false;
                if (actionId == EditorInfo.IME_NULL) {
                    consumed = true;
                    if (textBox1.getText().toString().equals(textBox2.getText().toString())) {
                        getDialog().dismiss();
                    } else {
                        error.setText(errorNum);
                        error.setVisibility(View.VISIBLE);
                        error.invalidate();
                    }
                }
                return consumed;
            }
        });
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
    	android.util.Log.i(TAG, "onDismiss called: Sending data back to Activity");
    	String result = textBox.getText().toString();
    	if (result.isEmpty()) {
    	    android.util.Log.i(TAG, "Dialog was cancelled, so sending wasCancelled == true back.");
    	    wasCancelled = true;
    	}
    	if (textBox != null) {
    	    textBox.setText("");
    	}
    	if (textBox2 != null) {
    	    textBox2.setText("");
    	}
    	dismissalListener.onTextObtained(result, wasCancelled);
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }
}