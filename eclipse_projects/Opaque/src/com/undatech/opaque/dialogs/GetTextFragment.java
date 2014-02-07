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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.ListIterator;

import org.xmlpull.v1.XmlPullParser;

import com.undatech.opaque.R;

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
	
    public interface OnFragmentDismissedListener {
        public void onTextObtained(String obtainedString);
    }
	
    private EditText textBox;
    private OnFragmentDismissedListener dismissalListener;
	private String title;
	
	private boolean password = false;
	private String obtained = "";
    
	public GetTextFragment (OnFragmentDismissedListener dismissalListener) {
		this.dismissalListener = dismissalListener;
	}
    
	public static GetTextFragment newInstance(String title, OnFragmentDismissedListener dismissalListener, boolean password) {
    	android.util.Log.e(TAG, "newInstance called");
    	GetTextFragment f = new GetTextFragment(dismissalListener);

        Bundle args = new Bundle();
        args.putString("title", title);
        args.putBoolean("password", password);
        f.setArguments(args);
        f.setRetainInstance(true);

        return f;
    }
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    	android.util.Log.e(TAG, "onCreate called");
        title = getArguments().getString("title");
        password = getArguments().getBoolean("password");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    	android.util.Log.e(TAG, "onCreateView called");

    	// Set title for this dialog
    	getDialog().setTitle(title);

    	View v = inflater.inflate(R.layout.get_text, container, false);
    	textBox = (EditText) v.findViewById(R.id.textBox);
    	if (password) {
    		textBox.setTransformationMethod(new PasswordTransformationMethod());
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

    	return v;
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
    	android.util.Log.e(TAG, "dismiss: sending back data to Activity");
    	dismissalListener.onTextObtained(textBox.getText().toString());
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }
}
