/**
 * Copyright (C) 2012 Iordan Iordanov
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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Point;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.Spinner;
import android.widget.ToggleButton;

import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.bVNC;
import com.iiordanov.util.RandomString;
import com.undatech.remoteClientUi.R;

import java.util.Objects;

/**
 * @author Iordan K Iordanov
 *
 */
public class AutoXCustomizeDialog extends AlertDialog {
    private final static String TAG = "AutoXCustomizeDialog";
    private static final Intent docIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://iiordanov.blogspot.ca/2012/10/looking-for-nx-client-for-android-or.html"));
    private final bVNC mainConfigDialog;
    private int commandIndex;
    private int origCommandIndex;
    private String command;
    private Spinner spinnerAutoXType;
    private String geometry = "";
    private String sessionProg = "";
    private String pw = "";
    private ToggleButton toggleAutoXAdvanced;
    private LinearLayout layoutAdvancedSettings;
    private Spinner spinnerAutoXGeometry;
    private EditText autoXWidth;
    private EditText autoXHeight;
    private Spinner spinnerAutoXSession;
    private EditText autoXSessionProg;
    private CheckBox checkboxAutoXUnixpw;
    private CheckBox checkboxAutoXUnixAuth;
    private final RandomString rnd;

    /**
     */
    public AutoXCustomizeDialog(Context context) {
        super(context);
        setOwnerActivity((Activity) context);
        mainConfigDialog = (bVNC) context;
        rnd = new RandomString();
    }

    private ConnectionBean getCurrentConnectionOrDismissIfNull() {
        ConnectionBean selected = mainConfigDialog.getCurrentConnection();
        if (selected == null) {
            dismiss();
            return new ConnectionBean(getContext());
        }
        return selected;
    }

    public static void showDocumentation(Context c) {
        c.startActivity(docIntent);
    }

    /* Don't do anything if back is pressed.
     *
     * (non-Javadoc)
     * @see android.app.Dialog#onBackPressed()
     */
    @Override
    public void onBackPressed() {
        retainAutoXInfo();
        dismiss();
    }

    /* This function needs to be overloaded because the dialog does not get
     * destroyed and recreated every time the button on the main screen is
     * pressed, and so the widgets' state wasn't set correctly. This makes
     * sure the widgets' state is set when the dialog gets displayed.
     * (non-Javadoc)
     * @see android.app.Dialog#onAttachedToWindow()
     */
    @Override
    public void onAttachedToWindow() {
        setWidgetStateAppropriately();
    }

    private void setWidgetStateAppropriately() {
        commandIndex = getCurrentConnectionOrDismissIfNull().getAutoXType();
        origCommandIndex = getCurrentConnectionOrDismissIfNull().getAutoXType();
        // Set current selection to the one corresponding to saved setting.
        spinnerAutoXType.setSelection(commandIndex);
        // Set the widgets controlling the remote resolution.
        setRemoteWidthAndHeight();
        // Set the widgets controlling the remote session program.
        setSessionProg();
        // Sets the password option.
        setPwOption();
        // Sets the state of the auto unixpw authentication checkbox.
        checkboxAutoXUnixAuth.setChecked(getCurrentConnectionOrDismissIfNull().getAutoXUnixAuth());
        // Set the toggle state of the advanced button.
        setAdvancedToggleState();
    }

    /**
     * Sets the state of the advanced button.
     */
    private void setAdvancedToggleState() {
        boolean adv = ((commandIndex != Constants.COMMAND_AUTO_X_DISABLED) &&
                ((getCurrentConnectionOrDismissIfNull().getAutoXResType() != Constants.AUTOX_GEOM_SELECT_NATIVE) ||
                        (getCurrentConnectionOrDismissIfNull().getAutoXSessionType() != Constants.AUTOX_SESS_PROG_SELECT_AUTO) ||
                        getCurrentConnectionOrDismissIfNull().getAutoXUnixpw() || getCurrentConnectionOrDismissIfNull().getAutoXUnixAuth()));
        toggleAutoXAdvanced.setChecked(adv);
    }

    /**
     * Make sure commandIndex and command match the state of the checkbox which
     * specifies whether .dmrc should be moved away.
     */
    private void setCommandIndexAndCommand(int itemIndex) {
        commandIndex = itemIndex;
        if (commandIndex != Constants.COMMAND_AUTO_X_DISABLED)
            command = Constants.getCommandString(commandIndex, geometry + sessionProg + pw);
        else
            command = "";
    }

    /**
     * Enables and disables the EditText boxes for width and height of remote desktop.
     */
    @SuppressLint("SetTextI18n")
    private void setRemoteWidthAndHeight() {

        // Android devices with SDK newer than KITKAT use immersive mode and therefore
        // we get the resolution of the whole display.
        int nativeWidth;
        int nativeHeight;
        if (Constants.SDK_INT < android.os.Build.VERSION_CODES.KITKAT) {
            nativeWidth = Math.max(mainConfigDialog.getWidth(), mainConfigDialog.getHeight());
            nativeHeight = Math.min(mainConfigDialog.getWidth(), mainConfigDialog.getHeight());
        } else {
            Point dS = new Point();
            mainConfigDialog.getWindowManager().getDefaultDisplay().getRealSize(dS);
            nativeWidth = Math.max(dS.x, dS.y);
            nativeHeight = Math.min(dS.x, dS.y);
        }

        spinnerAutoXGeometry.setSelection(getCurrentConnectionOrDismissIfNull().getAutoXResType());
        if (getCurrentConnectionOrDismissIfNull().getAutoXResType() == Constants.AUTOX_GEOM_SELECT_NATIVE) {
            autoXWidth.setEnabled(false);
            autoXHeight.setEnabled(false);
            autoXWidth.setText(Integer.toString(nativeWidth));
            autoXHeight.setText(Integer.toString(nativeHeight));
            getCurrentConnectionOrDismissIfNull().setAutoXWidth(nativeWidth);
            getCurrentConnectionOrDismissIfNull().setAutoXHeight(nativeHeight);
        } else {
            autoXWidth.setEnabled(true);
            autoXHeight.setEnabled(true);
            autoXWidth.setText(Integer.toString(getCurrentConnectionOrDismissIfNull().getAutoXWidth()));
            autoXHeight.setText(Integer.toString(getCurrentConnectionOrDismissIfNull().getAutoXHeight()));
        }

        geometry = " -env FD_GEOM=" + getCurrentConnectionOrDismissIfNull().getAutoXWidth() + "x" + getCurrentConnectionOrDismissIfNull().getAutoXHeight();
    }

    /**
     * Sets the sessionProg variable according to selection.
     */
    private void setSessionProg() {

        spinnerAutoXSession.setSelection(getCurrentConnectionOrDismissIfNull().getAutoXSessionType());

        if (getCurrentConnectionOrDismissIfNull().getAutoXSessionType() != Constants.AUTOX_SESS_PROG_SELECT_CUSTOM) {
            autoXSessionProg.setEnabled(false);
            autoXSessionProg.setText(Constants.getSessionProgString(getCurrentConnectionOrDismissIfNull().getAutoXSessionType()));
            sessionProg = " -env FD_PROG=\""
                    + Constants.getSessionProgString(getCurrentConnectionOrDismissIfNull().getAutoXSessionType()) + "\" ";
        } else {
            autoXSessionProg.setEnabled(true);
            autoXSessionProg.setText(getCurrentConnectionOrDismissIfNull().getAutoXSessionProg());
            sessionProg = " -env FD_PROG=\"" + autoXSessionProg.getText().toString() + "\" ";
        }
    }

    /**
     * Sets the UNIXsessionProg variable according to selection.
     */
    private void setPwOption() {
        checkboxAutoXUnixpw.setChecked(getCurrentConnectionOrDismissIfNull().getAutoXUnixpw());
        if (getCurrentConnectionOrDismissIfNull().getAutoXUnixpw()) {
            pw = Constants.AUTO_X_USERPW;
        } else {
            // Generate, save, and use random file extension.
            getCurrentConnectionOrDismissIfNull().setAutoXRandFileNm(rnd.randomLowerCaseString(20));
            pw = Constants.AUTO_X_PASSWDFILE + Constants.AUTO_X_PWFILEBASENAME + getCurrentConnectionOrDismissIfNull().getAutoXRandFileNm() + " \"";
        }
    }

    /* (non-Javadoc)
     * @see android.app.Dialog#onCreate(android.os.Bundle)
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.auto_x_customize);
        Objects.requireNonNull(getWindow()).clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
        WindowManager.LayoutParams lp = getWindow().getAttributes();
        lp.dimAmount = 1.0f;
        lp.width = LayoutParams.FILL_PARENT;
        lp.height = LayoutParams.WRAP_CONTENT;
        getWindow().setAttributes(lp);

        // Define the spinner which allows one to choose the X-server type
        spinnerAutoXType = findViewById(R.id.spinnerAutoXType);
        spinnerAutoXType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {
                // Set our preferred commandIndex, and command accordingly.
                setCommandIndexAndCommand(itemIndex);
                // Set the toggle state of the advanced button.
                setAdvancedToggleState();
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });

        // Set up the help button.
        Button buttonAutoXHelp = findViewById(R.id.buttonAutoXHelp);
        buttonAutoXHelp.setOnClickListener(v -> showDocumentation(AutoXCustomizeDialog.this.mainConfigDialog));

        // The advanced settings button.
        toggleAutoXAdvanced = findViewById(R.id.toggleAutoXAdvanced);
        layoutAdvancedSettings = findViewById(R.id.layoutAdvancedSettings);
        toggleAutoXAdvanced.setOnCheckedChangeListener((arg0, checked) -> {
            if (checked)
                layoutAdvancedSettings.setVisibility(View.VISIBLE);
            else
                layoutAdvancedSettings.setVisibility(View.GONE);
        });

        // The geometry type and dimensions boxes.
        spinnerAutoXGeometry = findViewById(R.id.spinnerAutoXGeometry);
        autoXWidth = findViewById(R.id.autoXWidth);
        autoXHeight = findViewById(R.id.autoXHeight);
        spinnerAutoXGeometry.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {

            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                getCurrentConnectionOrDismissIfNull().setAutoXResType(itemIndex);
                setRemoteWidthAndHeight();
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }

        });

        // Define the type of session to start.
        spinnerAutoXSession = findViewById(R.id.spinnerAutoXSession);
        autoXSessionProg = findViewById(R.id.autoXSessionProg);
        spinnerAutoXSession.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {

            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                getCurrentConnectionOrDismissIfNull().setAutoXSessionType(itemIndex);
                setSessionProg();
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });

        // Define the unixpw checkbox.
        checkboxAutoXUnixpw = findViewById(R.id.checkboxAutoXUnixpw);
        checkboxAutoXUnixpw.setOnCheckedChangeListener((arg0, checked) -> {
            getCurrentConnectionOrDismissIfNull().setAutoXUnixpw(checked);
            setPwOption();
        });

        // Define the auto unix authentication checkbox.
        checkboxAutoXUnixAuth = findViewById(R.id.checkboxAutoXUnixAuth);
        checkboxAutoXUnixAuth.setOnCheckedChangeListener((arg0, checked) -> getCurrentConnectionOrDismissIfNull().setAutoXUnixAuth(checked));


        // Define Confirm button
        Button autoXConfirm = findViewById(R.id.autoXConfirm);
        autoXConfirm.setOnClickListener(v -> {
            updateAutoXInfo();
            dismiss();
        });

        // Define Cancel button
        Button autoXCancel = findViewById(R.id.autoXCancel);
        autoXCancel.setOnClickListener(v -> {
            retainAutoXInfo();
            // If the user cancels, exit without changes.
            dismiss();
        });

        // Set the widgets' state appropriately.
        setWidgetStateAppropriately();
    }

    /**
     * Called from Auto X Session dialog to update the Auto X config options.
     */
    public void updateAutoXInfo() {
        // Save any user modified resolution.
        getCurrentConnectionOrDismissIfNull().setAutoXResType(spinnerAutoXGeometry.getSelectedItemPosition());
        try {
            getCurrentConnectionOrDismissIfNull().setAutoXWidth(Integer.parseInt(autoXWidth.getText().toString()));
            getCurrentConnectionOrDismissIfNull().setAutoXHeight(Integer.parseInt(autoXHeight.getText().toString()));
        } catch (NumberFormatException e) {
            Log.e(TAG, Log.getStackTraceString(e));
        }

        // Save any remote session command the user set.
        getCurrentConnectionOrDismissIfNull().setAutoXSessionType(spinnerAutoXSession.getSelectedItemPosition());
        getCurrentConnectionOrDismissIfNull().setAutoXSessionProg(autoXSessionProg.getText().toString());

        // Ensure the values of command and commandIndex match the widgets.
        setRemoteWidthAndHeight();
        setSessionProg();
        setCommandIndexAndCommand(commandIndex);

        boolean autoXenabled = commandIndex != Constants.COMMAND_AUTO_X_DISABLED;
        getCurrentConnectionOrDismissIfNull().setAutoXEnabled(autoXenabled);
        if (autoXenabled) {
            getCurrentConnectionOrDismissIfNull().setAddress("localhost");
        }
        getCurrentConnectionOrDismissIfNull().setAutoXType(commandIndex);
        getCurrentConnectionOrDismissIfNull().setAutoXCommand(command);
        getCurrentConnectionOrDismissIfNull().setAutoXUnixpw(checkboxAutoXUnixpw.isChecked());
        getCurrentConnectionOrDismissIfNull().setAutoXUnixAuth(checkboxAutoXUnixAuth.isChecked());
        // Set a random VNC password (for the built-in security mechanism).
        getCurrentConnectionOrDismissIfNull().setPassword(rnd.randomString(8));

        // Update and save.
        mainConfigDialog.updateViewFromSelected();
        getCurrentConnectionOrDismissIfNull().saveAndWriteRecent(false, getContext());
    }

    public void retainAutoXInfo() {
        setCommandIndexAndCommand(origCommandIndex);
        getCurrentConnectionOrDismissIfNull().setAutoXType(origCommandIndex);
        getCurrentConnectionOrDismissIfNull().setAutoXCommand(command);

        // Update and save.
        mainConfigDialog.updateViewFromSelected();
        getCurrentConnectionOrDismissIfNull().saveAndWriteRecent(false, getContext());
    }
}
