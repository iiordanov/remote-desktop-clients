/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 20?? Michael A. MacDonald
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


package com.iiordanov.bVNC;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.util.Base64;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import com.google.android.material.button.MaterialButtonToggleGroup;
import com.google.android.material.snackbar.Snackbar;
import com.iiordanov.bVNC.dialogs.AutoXCustomizeDialog;
import com.iiordanov.bVNC.dialogs.RepeaterDialog;
import com.iiordanov.bVNC.dialogs.SvncClientAuthDialog;
import com.morpheusly.common.Utilities;
import com.undatech.remoteClientUi.R;

import java.io.OutputStream;

/**
 * bVNC is the Activity for setting up VNC connections.
 */
public class bVNC extends MainConfiguration {
    private final static String TAG = "bVNC";
    private LinearLayout layoutUseX11Vnc;
    private LinearLayout repeaterEntry;
    private LinearLayout layoutUltraVncOptions;
    private LinearLayout layoutSecureVNCPlugin;
    private CompoundButton checkboxSvncEnabled;
    private CompoundButton checkboxKeepSvncPassphrase;
    private CompoundButton checkboxSvncClientAuth;
    private EditText textSvncPassphrase;
    private TextView repeaterText;
    private MaterialButtonToggleGroup groupForceFullScreen;
    private Spinner colorSpinner;
    private TextView autoXStatus;
    private CompoundButton checkboxPreferHextile;
    private CompoundButton checkboxViewOnly;
    private boolean repeaterTextSet;
    private Spinner spinnerVncGeometry;
    private SvncClientAuthDialog svncClientAuthDialog;

    @Override
    public void onCreate(Bundle icicle) {
        Log.d(TAG, "onCreate called");
        layoutID = R.layout.main;
        super.onCreate(icicle);

        layoutUseX11Vnc = findViewById(R.id.layoutUseX11Vnc);
        textUsername = findViewById(R.id.textUsername);
        autoXStatus = findViewById(R.id.autoXStatus);
        layoutUltraVncOptions = findViewById(R.id.layoutUltraVncOptions);
        layoutSecureVNCPlugin = findViewById(R.id.layoutSecureVNCPlugin);
        checkboxSvncEnabled = findViewById(R.id.checkboxSvncEnabled);
        checkboxKeepSvncPassphrase = findViewById(R.id.checkboxKeepSvncPassphrase);
        checkboxSvncClientAuth = findViewById(R.id.checkboxSvncClientAuth);
        textSvncPassphrase = findViewById(R.id.textSvncPassphrase);
        Button buttonSvncManageKey = findViewById(R.id.buttonSvncManageKey);
        buttonSvncManageKey.setOnClickListener(v -> showSvncClientAuthDialog());

        checkboxSvncEnabled.setOnCheckedChangeListener((buttonView, isChecked) -> {
            setVisibilityOfSecureVNCPluginWidgets(isChecked ? View.VISIBLE : View.GONE);
            if (selected != null) {
                selected.setSvncEnabled(isChecked);
                selected.save(bVNC.this);
            }
        });

        // Define what happens when the Repeater button is pressed.
        Button repeaterButton = findViewById(R.id.buttonRepeater);
        repeaterEntry = findViewById(R.id.repeaterEntry);
        repeaterButton.setOnClickListener(v -> showDialog(R.layout.repeater_dialog));

        // Define what happens when somebody clicks on the customize auto X session dialog.
        Button buttonCustomizeX11Vnc = findViewById(R.id.buttonCustomizeX11Vnc);
        buttonCustomizeX11Vnc.setOnClickListener(v -> {
            bVNC.this.updateSelectedFromView();
            showDialog(R.layout.auto_x_customize);
        });

        // Define what happens when somebody selects different VNC connection types.
        connectionType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @SuppressLint("SetTextI18n")
            @Override
            public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {
                Log.d(TAG, "connectionType onItemSelected called");
                selectedConnType = itemIndex;
                selected.setConnectionType(selectedConnType);
                selected.save(bVNC.this);
                if (selectedConnType == Constants.CONN_TYPE_PLAIN ||
                        selectedConnType == Constants.CONN_TYPE_ANONTLS ||
                        selectedConnType == Constants.CONN_TYPE_STUNNEL) {
                    setVisibilityOfSshWidgets(View.GONE);
                    setVisibilityOfUltraVncWidgets(View.GONE);
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint_optional);
                } else if (selectedConnType == Constants.CONN_TYPE_SSH) {
                    setVisibilityOfSshWidgets(View.VISIBLE);
                    setVisibilityOfUltraVncWidgets(View.GONE);
                    setIpTextToLocalhostIfEmpty();
                    ipText.setHint(R.string.address_caption_hint_tunneled);
                    textUsername.setHint(R.string.username_hint_optional);
                } else if (selectedConnType == Constants.CONN_TYPE_ULTRAVNC) {
                    setVisibilityOfSshWidgets(View.GONE);
                    setVisibilityOfUltraVncWidgets(View.VISIBLE);
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint);
                } else if (selectedConnType == Constants.CONN_TYPE_VENCRYPT) {
                    setVisibilityOfSshWidgets(View.GONE);
                    setVisibilityOfUltraVncWidgets(View.GONE);
                    textUsername.setVisibility(View.VISIBLE);
                    repeaterEntry.setVisibility(View.GONE);
                    Editable passwordTextEditable = passwordText.getText();
                    if (passwordTextEditable != null && passwordTextEditable.toString().isEmpty()) {
                        checkboxKeepPassword.setChecked(false);
                    }
                    ipText.setHint(R.string.address_caption_hint);
                    textUsername.setHint(R.string.username_hint_vencrypt);
                }
                updateViewFromSelected();
            }

            @Override
            public void onNothingSelected(AdapterView<?> ad) {
            }
        });

        colorSpinner = findViewById(R.id.colorformat);
        COLORMODEL[] models = COLORMODEL.values();
        ArrayAdapter<COLORMODEL> colorSpinnerAdapter = new ArrayAdapter<>(this, R.layout.connection_list_entry, models);
        groupForceFullScreen = findViewById(R.id.groupForceFullScreen);

        checkboxPreferHextile = findViewById(R.id.checkboxPreferHextile);
        checkboxViewOnly = findViewById(R.id.checkboxViewOnly);
        colorSpinner.setAdapter(colorSpinnerAdapter);
        colorSpinner.setSelection(0);

        spinnerVncGeometry = findViewById(R.id.spinnerVncGeometry);

        spinnerVncGeometry.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                if (selected != null) {
                    selected.setRdpResType(itemIndex);
                    setRemoteWidthAndHeight(Constants.VNC_GEOM_SELECT_CUSTOM);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });
        repeaterText = findViewById(R.id.textRepeaterId);

        setConnectionTypeSpinnerAdapter(R.array.connection_type);
    }

    /**
     * Makes the ssh-related widgets visible/invisible.
     */
    protected void setVisibilityOfSshWidgets(int visibility) {
        Log.d(TAG, "setVisibilityOfSshWidgets called");
        super.setVisibilityOfSshWidgets(visibility);
        layoutUseX11Vnc.setVisibility(visibility);
    }

    /**
     * Makes the UltraVNC-related widgets visible/invisible.
     * When hiding, also hides the SecureVNCPlugin sub-panel.
     */
    private void setVisibilityOfUltraVncWidgets(int visibility) {
        Log.d(TAG, "setVisibilityOfUltraVncWidgets called");
        repeaterEntry.setVisibility(visibility);
        layoutUltraVncOptions.setVisibility(visibility);
        if (visibility == View.GONE) {
            setVisibilityOfSecureVNCPluginWidgets(View.GONE);
        }
    }

    /**
     * Makes the SecureVNCPlugin detail widgets visible/invisible (passphrase, client auth).
     */
    private void setVisibilityOfSecureVNCPluginWidgets(int visibility) {
        Log.d(TAG, "setVisibilityOfSecureVNCPluginWidgets called");
        layoutSecureVNCPlugin.setVisibility(visibility);
    }

    /**
     * Opens the SecureVNCPlugin client authentication key management dialog.
     */
    private void showSvncClientAuthDialog() {
        Log.d(TAG, "showSvncClientAuthDialog called");
        updateSelectedFromView();
        selected.saveAndWriteRecent(true, this);
        svncClientAuthDialog = new SvncClientAuthDialog(this, selected);
        svncClientAuthDialog.show();
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onCreateDialog(int)
     */
    @Override
    protected Dialog onCreateDialog(int id) {
        Log.d(TAG, "onCreateDialog called");
        if (id == R.layout.repeater_dialog) {
            return new RepeaterDialog(this);
        } else if (id == R.layout.auto_x_customize) {
            Dialog d = new AutoXCustomizeDialog(this);
            d.setCancelable(false);
            return d;
        }
        return null;
    }

    @SuppressLint("SetTextI18n")
    public void updateViewFromSelected() {
        Log.d(TAG, "updateViewFromSelected called");
        if (selected == null)
            return;
        super.updateViewFromSelected();

        // If we are doing automatic X session discovery, then disable
        // vnc address, vnc port, and vnc password, and vice-versa
        if (selectedConnType == Constants.CONN_TYPE_SSH && selected.getAutoXEnabled()) {
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

        groupForceFullScreen.check(selected.getForceFull() == BitmapImplHint.AUTO ?
                R.id.radioForceFullScreenAuto : R.id.radioForceFullScreenOn);
        checkboxPreferHextile.setChecked(selected.getPrefEncoding() == RfbProto.EncodingHextile);
        checkboxViewOnly.setChecked(selected.getViewOnly());
        textUsername.setText(selected.getUserName());
        COLORMODEL cm = COLORMODEL.C24bit;
        try {
            cm = COLORMODEL.valueOf(selected.getColorModel());
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Error setting color model");
            Log.e(TAG, Log.getStackTraceString(e));
        }
        COLORMODEL[] colors = COLORMODEL.values();

        spinnerVncGeometry.setSelection(selected.getRdpResType());

        for (int i = 0; i < colors.length; ++i) {
            if (colors[i] == cm) {
                colorSpinner.setSelection(i);
                break;
            }
        }
        updateRepeaterInfo(selected.getUseRepeater(), selected.getRepeaterId());

        if (selectedConnType == Constants.CONN_TYPE_ULTRAVNC) {
            boolean svncEnabled = selected.getSvncEnabled();
            checkboxSvncEnabled.setChecked(svncEnabled);
            setVisibilityOfSecureVNCPluginWidgets(svncEnabled ? View.VISIBLE : View.GONE);
            if (svncEnabled) {
                checkboxKeepSvncPassphrase.setChecked(selected.getKeepSvncPassphrase());
                textSvncPassphrase.setText(selected.getSvncPassphrase());
                checkboxSvncClientAuth.setChecked(selected.getClientAuthEnabled());
            }
        }
    }

    /**
     * Called when changing view to match selected connection or from
     * Repeater dialog to update the repeater information shown.
     *
     * @param repeaterId If null or empty, show text for not using repeater
     */
    public void updateRepeaterInfo(boolean useRepeater, String repeaterId) {
        Log.d(TAG, "updateRepeaterInfo called");
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

    protected void updateSelectedFromView() {
        Log.d(TAG, "updateSelectedFromView called");
        if (selected == null) {
            return;
        }
        super.updateSelectedFromView();

        selected.setUserName(textUsername.getText().toString());
        selected.setForceFull(groupForceFullScreen.getCheckedButtonId() == R.id.radioForceFullScreenAuto ? BitmapImplHint.AUTO : (groupForceFullScreen.getCheckedButtonId() == R.id.radioForceFullScreenOn ? BitmapImplHint.FULL : BitmapImplHint.TILE));

        if (checkboxPreferHextile.isChecked())
            selected.setPrefEncoding(RfbProto.EncodingHextile);
        else
            selected.setPrefEncoding(RfbProto.EncodingTight);

        selected.setViewOnly(checkboxViewOnly.isChecked());
        selected.setRdpResType(spinnerVncGeometry.getSelectedItemPosition());

        selected.setColorModel(((COLORMODEL) colorSpinner.getSelectedItem()).nameString());
        if (repeaterTextSet) {
            selected.setRepeaterId(repeaterText.getText().toString());
            selected.setUseRepeater(true);
        } else {
            selected.setUseRepeater(false);
        }

        if (selectedConnType == Constants.CONN_TYPE_ULTRAVNC) {
            selected.setSvncEnabled(checkboxSvncEnabled.isChecked());
            if (checkboxSvncEnabled.isChecked()) {
                selected.setSvncPassphrase(textSvncPassphrase.getText().toString());
                selected.setKeepSvncPassphrase(checkboxKeepSvncPassphrase.isChecked());
                selected.setClientAuthEnabled(checkboxSvncClientAuth.isChecked());
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "onActivityResult called, requestCode=" + requestCode);
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == Constants.ACTIVITY_SVNC_IMPORT_KEY) {
            handleSvncImportKey(resultCode, data);
        } else if (requestCode == Constants.ACTIVITY_SVNC_EXPORT_PUBKEY) {
            handleSvncExportPubKey(resultCode, data);
        } else if (requestCode == Constants.ACTIVITY_SVNC_EXPORT_PKEY) {
            handleSvncExportPrivKey(resultCode, data);
        }
    }

    private void handleSvncExportPrivKey(int resultCode, Intent data) {
        if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
            String privKeyBase64 = selected.getClientAuthPrivKey();
            if (privKeyBase64 != null && !privKeyBase64.isEmpty()) {
                try {
                    byte[] privKeyDer = Base64.decode(privKeyBase64, Base64.DEFAULT);
                    OutputStream out = getContentResolver().openOutputStream(data.getData());
                    if (out != null) {
                        out.write(privKeyDer);
                        out.close();
                    }
                    Utils.showMessage(textNickname,
                            getString(R.string.svnc_client_auth_pkey_exported),
                            Snackbar.LENGTH_LONG);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to export pkey file", e);
                    Utils.showMessage(textNickname,
                            getString(R.string.svnc_client_auth_pkey_export_failed),
                            Snackbar.LENGTH_LONG);
                }
            }
        } else {
            Log.i(TAG, "The user cancelled SecureVNCPlugin pkey export.");
        }
    }

    private void handleSvncExportPubKey(int resultCode, Intent data) {
        byte[] pubKeyDer = svncClientAuthDialog != null
                ? svncClientAuthDialog.consumePendingPubKeyDer() : null;
        if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null
                && pubKeyDer != null) {
            try {
                OutputStream out = getContentResolver().openOutputStream(data.getData());
                if (out != null) {
                    out.write(pubKeyDer);
                    out.close();
                }
                Utils.showMessage(textNickname,
                        getString(R.string.securevncplugin_key_generated), Snackbar.LENGTH_LONG);
            } catch (Exception e) {
                Log.e(TAG, "Failed to save pubkey file", e);
                Utils.showMessage(textNickname,
                        getString(R.string.securevncplugin_pubkey_export_failed), Snackbar.LENGTH_LONG);
            }
        } else {
            // User cancelled the save dialog, but the private key is already stored.
            Utils.showMessage(textNickname,
                    getString(R.string.securevncplugin_key_generated), Snackbar.LENGTH_LONG);
        }
    }

    private void handleSvncImportKey(int resultCode, Intent data) {
        if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
            byte[] keyBytes = Utilities.Companion.getBytesDataFromIntent(
                    data, this, com.morpheusly.common.Constants.MAX_KEY_FILE_SIZE_BYTES);
            if (keyBytes != null && keyBytes.length > 0) {
                String base64Key = Base64.encodeToString(keyBytes, Base64.DEFAULT);
                selected.setClientAuthPrivKey(base64Key);
                selected.saveAndWriteRecent(true, this);
                if (svncClientAuthDialog != null) {
                    svncClientAuthDialog.onKeyImported();
                }
                Utils.showMessage(textNickname,
                        getString(R.string.securevncplugin_key_imported), Snackbar.LENGTH_LONG);
            } else {
                Utils.showMessage(textNickname,
                        getString(R.string.securevncplugin_key_import_failed), Snackbar.LENGTH_LONG);
            }
        } else {
            Log.i(TAG, "The user cancelled SecureVNCPlugin key import.");
        }
    }

    public void save(View item) {
        save(R.string.vnc_server_empty);
    }

    public void save(MenuItem item) {
        save(R.string.vnc_server_empty);
    }
}
