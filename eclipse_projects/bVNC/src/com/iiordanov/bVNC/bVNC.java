/** 
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 20?? Michael A. MacDonald
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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


package com.iiordanov.bVNC;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ActivityManager.MemoryInfo;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import net.sqlcipher.database.SQLiteDatabase;
import android.graphics.Point;
import android.os.Bundle;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.util.Log;

import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.dialogs.AutoXCustomizeDialog;
import com.iiordanov.bVNC.dialogs.ImportExportDialog;
import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.dialogs.RepeaterDialog;
import com.iiordanov.pubkeygenerator.GeneratePubkeyActivity;

import java.util.ArrayList;
import java.util.Collections;

/**
 * bVNC is the Activity for setting up VNC connections.
 */
public class bVNC extends MainConfiguration {
    private final static String TAG = "androidVNC";
    private Spinner connectionType;
    private int selectedConnType;
    private TextView sshCaption;
    private LinearLayout sshCredentials;
    private LinearLayout layoutUseSshPubkey;
    private LinearLayout layoutUseX11Vnc;
    private LinearLayout sshServerEntry;
    private LinearLayout layoutAdvancedSettings;
    private EditText sshServer;
    private EditText sshPort;
    private EditText sshUser;
    private EditText sshPassword;
    private EditText sshPassphrase;
    private EditText ipText;
    private EditText portText;
    private EditText passwordText;
    private Button goButton;
    private Button repeaterButton;
    private Button buttonGeneratePubkey;
    private Button buttonCustomizeX11Vnc;
    private ToggleButton toggleAdvancedSettings;
    private LinearLayout repeaterEntry;
    private TextView repeaterText;
    private RadioGroup groupForceFullScreen;
    private Spinner colorSpinner;
    private EditText textNickname;
    private EditText textUsername;
    private TextView autoXStatus;
    private CheckBox checkboxKeepPassword;
    private CheckBox checkboxUseDpadAsArrows;
    private CheckBox checkboxRotateDpad;
    private CheckBox checkboxLocalCursor;
    private CheckBox checkboxUseSshPubkey;
    private CheckBox checkboxPreferHextile;
    private CheckBox checkboxViewOnly;
    private boolean repeaterTextSet;
    
    @Override
    public void onCreate(Bundle icicle) {
        layoutID = R.layout.main;
        super.onCreate(icicle);
        
        ipText = (EditText) findViewById(R.id.textIP);
        sshServer = (EditText) findViewById(R.id.sshServer);
        sshPort = (EditText) findViewById(R.id.sshPort);
        sshUser = (EditText) findViewById(R.id.sshUser);
        sshPassword = (EditText) findViewById(R.id.sshPassword);
        sshPassphrase = (EditText) findViewById(R.id.sshPassphrase);
        sshCredentials = (LinearLayout) findViewById(R.id.sshCredentials);
        sshCaption = (TextView) findViewById(R.id.sshCaption);
        layoutUseSshPubkey = (LinearLayout) findViewById(R.id.layoutUseSshPubkey);
        layoutUseX11Vnc = (LinearLayout) findViewById(R.id.layoutUseX11Vnc);
        sshServerEntry = (LinearLayout) findViewById(R.id.sshServerEntry);
        portText = (EditText) findViewById(R.id.textPORT);
        passwordText = (EditText) findViewById(R.id.textPASSWORD);
        textNickname = (EditText) findViewById(R.id.textNickname);
        textUsername = (EditText) findViewById(R.id.textUsername);
        autoXStatus = (TextView) findViewById(R.id.autoXStatus);
        
        // Define what happens when the Repeater button is pressed.
        repeaterButton = (Button) findViewById(R.id.buttonRepeater);
        repeaterEntry = (LinearLayout) findViewById(R.id.repeaterEntry);
        repeaterButton.setOnClickListener(new View.OnClickListener() {    
            @Override
            public void onClick(View v) {
                showDialog(R.layout.repeater_dialog);
            }
        });

        // Here we say what happens when the Pubkey Checkbox is checked/unchecked.
        checkboxUseSshPubkey = (CheckBox) findViewById(R.id.checkboxUseSshPubkey);
        checkboxUseSshPubkey.setOnCheckedChangeListener(new OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                selected.setUseSshPubKey(isChecked);
            }
        });
        
        // Here we say what happens when the Pubkey Generate button is pressed.
        buttonGeneratePubkey = (Button) findViewById(R.id.buttonGeneratePubkey);
        buttonGeneratePubkey.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                generatePubkey ();
            }
        });

        // Define what happens when somebody clicks on the customize auto X session dialog.
        buttonCustomizeX11Vnc = (Button) findViewById(R.id.buttonCustomizeX11Vnc);
        buttonCustomizeX11Vnc.setOnClickListener(new View.OnClickListener() {    
            @Override
            public void onClick(View v) {
                bVNC.this.updateSelectedFromView();
                showDialog(R.layout.auto_x_customize);
            }
        });
        
        // Define what happens when somebody selects different VNC connection types.
        connectionType = (Spinner) findViewById(R.id.connectionType);
        connectionType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {

                selectedConnType = itemIndex;
                if (selectedConnType == Constants.CONN_TYPE_PLAIN ||
                    selectedConnType == Constants.CONN_TYPE_ANONTLS ||
                    selectedConnType == Constants.CONN_TYPE_STUNNEL) {
                    setVisibilityOfSshWidgets (View.GONE);
                    setVisibilityOfUltraVncWidgets (View.GONE);
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint_optional);
                } else if (selectedConnType == Constants.CONN_TYPE_SSH) {
                    setVisibilityOfSshWidgets (View.VISIBLE);
                    setVisibilityOfUltraVncWidgets (View.GONE);
                    if (ipText.getText().toString().equals(""))
                        ipText.setText("localhost");
                    ipText.setHint(R.string.address_caption_hint_tunneled);
                    textUsername.setHint(R.string.username_hint_optional);
                } else if (selectedConnType == Constants.CONN_TYPE_ULTRAVNC) {
                    setVisibilityOfSshWidgets (View.GONE);
                    setVisibilityOfUltraVncWidgets (View.VISIBLE);
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint);
                } else if (selectedConnType == Constants.CONN_TYPE_VENCRYPT) {
                    setVisibilityOfSshWidgets (View.GONE);
                    textUsername.setVisibility(View.VISIBLE);
                    repeaterEntry.setVisibility(View.GONE);
                    if (passwordText.getText().toString().equals(""))
                        checkboxKeepPassword.setChecked(false);
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint_vencrypt);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> ad) {
            }
        });

        // The advanced settings button.
        toggleAdvancedSettings = (ToggleButton) findViewById(R.id.toggleAdvancedSettings);
        layoutAdvancedSettings = (LinearLayout) findViewById(R.id.layoutAdvancedSettings);
        toggleAdvancedSettings.setOnCheckedChangeListener(new OnCheckedChangeListener () {
            @Override
            public void onCheckedChanged(CompoundButton arg0, boolean checked) {
                if (checked)
                    layoutAdvancedSettings.setVisibility(View.VISIBLE);
                else
                    layoutAdvancedSettings.setVisibility(View.GONE);
            }
        });
        
        // Define what happens when the Import/Export button is pressed.
        ((Button)findViewById(R.id.buttonImportExport)).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showDialog(R.layout.importexport);
            }
        });
        
        colorSpinner = (Spinner)findViewById(R.id.colorformat);
        COLORMODEL[] models=COLORMODEL.values();
        ArrayAdapter<COLORMODEL> colorSpinnerAdapter = new ArrayAdapter<COLORMODEL>(this, R.layout.connection_list_entry, models);
        groupForceFullScreen = (RadioGroup)findViewById(R.id.groupForceFullScreen);
        checkboxKeepPassword = (CheckBox)findViewById(R.id.checkboxKeepPassword);
        checkboxUseDpadAsArrows = (CheckBox)findViewById(R.id.checkboxUseDpadAsArrows);
        checkboxRotateDpad = (CheckBox)findViewById(R.id.checkboxRotateDpad);
        checkboxLocalCursor = (CheckBox)findViewById(R.id.checkboxUseLocalCursor);
        checkboxPreferHextile = (CheckBox)findViewById(R.id.checkboxPreferHextile);
        checkboxViewOnly = (CheckBox)findViewById(R.id.checkboxViewOnly);
        colorSpinner.setAdapter(colorSpinnerAdapter);
        colorSpinner.setSelection(0);

        goButton = (Button) findViewById(R.id.buttonGO);
        goButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (ipText.getText().length() != 0 && portText.getText().length() != 0)
                    canvasStart();
                else
                    Toast.makeText(view.getContext(), R.string.vnc_server_empty, Toast.LENGTH_LONG).show();
            }
        });

        repeaterText = (TextView)findViewById(R.id.textRepeaterId);
    }
    
    /**
     * Makes the ssh-related widgets visible/invisible.
     */
    private void setVisibilityOfSshWidgets (int visibility) {
        sshCredentials.setVisibility(visibility);
        sshCaption.setVisibility(visibility);
        layoutUseSshPubkey.setVisibility(visibility);
        layoutUseX11Vnc.setVisibility(visibility);
        sshServerEntry.setVisibility(visibility);
    }

    /**
     * Makes the uvnc-related widgets visible/invisible.
     */
    private void setVisibilityOfUltraVncWidgets (int visibility) {
        repeaterEntry.setVisibility(visibility);
    }
    
    /* (non-Javadoc)
     * @see android.app.Activity#onCreateDialog(int)
     */
    @Override
    protected Dialog onCreateDialog(int id) {
        switch (id) {
        case R.layout.importexport:
            return new ImportExportDialog(this);
        case R.id.itemMainScreenHelp:
            return createHelpDialog();
        case R.layout.repeater_dialog:
            return new RepeaterDialog(this);
        case R.layout.auto_x_customize:
            Dialog d = new AutoXCustomizeDialog(this);
            d.setCancelable(false);
            return d;
        }
        return null;
    }
    
    /**
     * Creates the help dialog for this activity.
     */
    private Dialog createHelpDialog() {
        AlertDialog.Builder adb = new AlertDialog.Builder(this)
                .setMessage(R.string.main_screen_help_text)
                .setPositiveButton(R.string.close,
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog,
                                    int whichButton) {
                                // We don't have to do anything.
                            }
                        });
        Dialog d = adb.setView(new ListView (this)).create();
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
        lp.copyFrom(d.getWindow().getAttributes());
        lp.width = WindowManager.LayoutParams.MATCH_PARENT;
        lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
        d.show();
        d.getWindow().setAttributes(lp);
        return d;
    }
    
    public void updateViewFromSelected() {
        if (selected == null)
            return;
        selectedConnType = selected.getConnectionType();
        connectionType.setSelection(selectedConnType);
        sshServer.setText(selected.getSshServer());
        sshPort.setText(Integer.toString(selected.getSshPort()));
        sshUser.setText(selected.getSshUser());
        
        checkboxUseSshPubkey.setChecked(selected.getUseSshPubKey());

        if (selectedConnType == Constants.CONN_TYPE_SSH && selected.getAddress().equals(""))
            ipText.setText("localhost");
        else
            ipText.setText(selected.getAddress());

        // If we are doing automatic X session discovery, then disable
        // vnc address, vnc port, and vnc password, and vice-versa
        if (selectedConnType == 1 && selected.getAutoXEnabled()) {
            ipText.setVisibility(View.GONE);
            portText.setVisibility(View.GONE);
            textUsername.setVisibility(View.GONE);
            passwordText.setVisibility(View.GONE);
            checkboxKeepPassword.setVisibility(View.GONE);
            autoXStatus.setText(R.string.auto_x_enabled);
        } else {
            ipText.setVisibility(View.VISIBLE);
            portText.setVisibility(View.VISIBLE);
            textUsername.setVisibility(View.VISIBLE);
            passwordText.setVisibility(View.VISIBLE);
            checkboxKeepPassword.setVisibility(View.VISIBLE);
            autoXStatus.setText(R.string.auto_x_disabled);
        }

        portText.setText(Integer.toString(selected.getPort()));
        
        if (selected.getKeepPassword() || selected.getPassword().length()>0) {
            passwordText.setText(selected.getPassword());
        }
        groupForceFullScreen.check(selected.getForceFull()==BitmapImplHint.AUTO ?
                            R.id.radioForceFullScreenAuto : R.id.radioForceFullScreenOn);
        checkboxKeepPassword.setChecked(selected.getKeepPassword());
        checkboxUseDpadAsArrows.setChecked(selected.getUseDpadAsArrows());
        checkboxRotateDpad.setChecked(selected.getRotateDpad());
        checkboxLocalCursor.setChecked(selected.getUseLocalCursor());
        checkboxPreferHextile.setChecked(selected.getPrefEncoding() == RfbProto.EncodingHextile);
        checkboxViewOnly.setChecked(selected.getViewOnly());
        textNickname.setText(selected.getNickname());
        textUsername.setText(selected.getUserName());
        COLORMODEL cm = COLORMODEL.valueOf(selected.getColorModel());
        COLORMODEL[] colors=COLORMODEL.values();
        for (int i=0; i<colors.length; ++i)
        {
            if (colors[i] == cm) {
                colorSpinner.setSelection(i);
                break;
            }
        }
        updateRepeaterInfo(selected.getUseRepeater(), selected.getRepeaterId());
    }

    /**
     * Called when changing view to match selected connection or from
     * Repeater dialog to update the repeater information shown.
     * @param repeaterId If null or empty, show text for not using repeater
     */
    public void updateRepeaterInfo(boolean useRepeater, String repeaterId)
    {
        if (useRepeater) {
            repeaterText.setText(repeaterId);
            repeaterTextSet = true;
            ipText.setHint(R.string.repeater_caption_hint);
        } else {
            repeaterText.setText(getText(R.string.repeater_empty_text));
            repeaterTextSet = false;
            ipText.setHint(R.string.address_caption_hint);
        }
    }

    /**
     * Returns the current ConnectionBean.
     */
    public ConnectionBean getCurrentConnection () {
        return selected;
    }

    protected void updateSelectedFromView() {
        if (selected == null) {
            return;
        }
        selected.setConnectionType(selectedConnType);
        selected.setAddress(ipText.getText().toString());
        try    {
            selected.setPort(Integer.parseInt(portText.getText().toString()));
            selected.setSshPort(Integer.parseInt(sshPort.getText().toString()));
        }
        catch (NumberFormatException nfe) {}
        
        selected.setNickname(textNickname.getText().toString());
        selected.setSshServer(sshServer.getText().toString());
        selected.setSshUser(sshUser.getText().toString());

        selected.setKeepSshPassword(false);
        
        // If we are using an SSH key, then the ssh password box is used
        // for the key pass-phrase instead.
        selected.setUseSshPubKey(checkboxUseSshPubkey.isChecked());
        selected.setSshPassPhrase(sshPassphrase.getText().toString());
        selected.setSshPassword(sshPassword.getText().toString());
        selected.setUserName(textUsername.getText().toString());
        selected.setForceFull(groupForceFullScreen.getCheckedRadioButtonId()==R.id.radioForceFullScreenAuto ? BitmapImplHint.AUTO : (groupForceFullScreen.getCheckedRadioButtonId()==R.id.radioForceFullScreenOn ? BitmapImplHint.FULL : BitmapImplHint.TILE));
        selected.setPassword(passwordText.getText().toString());
        selected.setKeepPassword(checkboxKeepPassword.isChecked());
        selected.setUseDpadAsArrows(checkboxUseDpadAsArrows.isChecked());
        selected.setRotateDpad(checkboxRotateDpad.isChecked());
        selected.setUseLocalCursor(checkboxLocalCursor.isChecked());
        if (checkboxPreferHextile.isChecked())
            selected.setPrefEncoding(RfbProto.EncodingHextile);
        else
            selected.setPrefEncoding(RfbProto.EncodingTight);
        selected.setViewOnly(checkboxViewOnly.isChecked());

        selected.setColorModel(((COLORMODEL)colorSpinner.getSelectedItem()).nameString());
        if (repeaterTextSet) {
            selected.setRepeaterId(repeaterText.getText().toString());
            selected.setUseRepeater(true);
        } else {
            selected.setUseRepeater(false);
        }
    }
    
    /**
     * Starts the activity which manages keys.
     */
    private void generatePubkey () {
        updateSelectedFromView();
        selected.saveAndWriteRecent(false);
        Intent intent = new Intent(this, GeneratePubkeyActivity.class);
        intent.putExtra("PrivateKey",selected.getSshPrivKey());
        startActivityForResult(intent, Constants.ACTIVITY_GEN_KEY);
    }
    
    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch(requestCode) {
        case (Constants.ACTIVITY_GEN_KEY):
            if (resultCode == Activity.RESULT_OK) {
                Bundle b = data.getExtras();
                String privateKey = (String)b.get("PrivateKey");
                if (!privateKey.equals(selected.getSshPrivKey()) && privateKey.length() != 0)
                    Toast.makeText(getBaseContext(), "New key generated/imported successfully. Tap 'Generate/Export Key' " +
                            " button to share, copy to clipboard, or export the public key now.", Toast.LENGTH_LONG).show();
                selected.setSshPrivKey(privateKey);
                selected.setSshPubKey((String)b.get("PublicKey"));
                selected.saveAndWriteRecent(false);
            } else
                Log.i (TAG, "The user cancelled SSH key generation.");
            break;
        }
    }
}
