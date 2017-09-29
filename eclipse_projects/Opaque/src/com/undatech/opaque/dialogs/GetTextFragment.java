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
        public void onTextObtained(String id, String obtainedString);
    }
	
    private EditText textBox;
    private Button okButton;
    private OnFragmentDismissedListener dismissalListener;
    private String title;
    private String id;
	
	private boolean password = false;
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
	                                          OnFragmentDismissedListener dismissalListener, boolean password) {
    	android.util.Log.i(TAG, "newInstance called");
    	GetTextFragment f = new GetTextFragment();
    	f.setOnFragmentDismissedListener(dismissalListener);
    	
        Bundle args = new Bundle();
        args.putString("id", id);
        args.putString("title", title);
        args.putBoolean("password", password);
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
        this.password = getArguments().getBoolean("password");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    	android.util.Log.i(TAG, "onCreateView called");

    	// Set title for this dialog
    	getDialog().setTitle(title);

    	View v = inflater.inflate(R.layout.get_text, container, false);
    	textBox = (EditText) v.findViewById(R.id.textBox);
    	if (password) {
    		textBox.setTransformationMethod(new PasswordTransformationMethod());
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
    	okButton.setOnClickListener(new View.OnClickListener() {
            
            @Override
            public void onClick(View v) {
                getDialog().dismiss();
            }
        });
    	
        return v;
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
    	android.util.Log.i(TAG, "dismiss: sending back data to listener");
    	dismissalListener.onTextObtained(this.id, textBox.getText().toString());
    }

    @Override
    public void onDestroyView() {
      if (getDialog() != null && getRetainInstance())
        getDialog().setOnDismissListener(null);
      super.onDestroyView();
    }
}
