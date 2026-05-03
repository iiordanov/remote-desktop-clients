/**
 * Copyright (C) 2026 Iordan Iordanov
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
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout.LayoutParams;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.SecureVNCPluginClientAuth;
import com.undatech.opaque.Connection;
import com.undatech.remoteClientUi.R;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Dialog for managing SecureVNCPlugin client authentication keys.
 * Allows generating, importing (.pkey), and exporting (.pkey) RSA key pairs.
 */
public class SvncClientAuthDialog extends AlertDialog {
    private static final String TAG = "SvncClientAuthDialog";

    private static final int[] KEY_SIZES = {1024, 2048, 3072};
    private static final int DEFAULT_KEY_SIZE_INDEX = 2; // 3072

    private final Connection selected;
    private final Activity ownerActivity;

    private TextView textKeyStatus;
    private TextView textKeySizeValue;
    private SeekBar seekBarKeySize;
    private Button buttonExportPkey;

    /**
     * Temporarily holds the generated public key DER for export.
     */
    private byte[] pendingPubKeyDer;

    public SvncClientAuthDialog(Context context, Connection selected) {
        super(context);
        this.ownerActivity = (Activity) context;
        setOwnerActivity(ownerActivity);
        this.selected = selected;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.svnc_client_auth_dialog);
        setTitle(R.string.svnc_client_auth_dialog_title);

        Window window = getWindow();
        if (window != null) {
            WindowManager.LayoutParams lp = window.getAttributes();
            lp.width = LayoutParams.MATCH_PARENT;
            lp.height = LayoutParams.WRAP_CONTENT;
            window.setAttributes(lp);
        }

        textKeyStatus = findViewById(R.id.textKeyStatus);
        textKeySizeValue = findViewById(R.id.textKeySizeValue);
        seekBarKeySize = findViewById(R.id.seekBarKeySize);
        buttonExportPkey = findViewById(R.id.buttonExportPkey);

        seekBarKeySize.setMax(KEY_SIZES.length - 1);
        seekBarKeySize.setProgress(DEFAULT_KEY_SIZE_INDEX);
        updateKeySizeLabel(DEFAULT_KEY_SIZE_INDEX);

        seekBarKeySize.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                updateKeySizeLabel(progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        findViewById(R.id.buttonGenerate).setOnClickListener(v -> onGenerateClicked());
        findViewById(R.id.buttonImportPkey).setOnClickListener(v -> onImportClicked());
        buttonExportPkey.setOnClickListener(v -> onExportPkeyClicked());

        updateKeyStatus();
    }

    @Override
    public void onAttachedToWindow() {
        updateKeyStatus();
    }

    private void updateKeySizeLabel(int index) {
        textKeySizeValue.setText(
                getContext().getString(R.string.svnc_client_auth_key_size_value, KEY_SIZES[index]));
    }

    private boolean hasExistingKey() {
        String privKey = selected.getClientAuthPrivKey();
        return privKey != null && !privKey.isEmpty();
    }

    private void updateKeyStatus() {
        if (hasExistingKey()) {
            textKeyStatus.setText(R.string.svnc_client_auth_key_present);
            buttonExportPkey.setEnabled(true);
        } else {
            textKeyStatus.setText(R.string.svnc_client_auth_no_key);
            buttonExportPkey.setEnabled(false);
        }
    }

    private void onGenerateClicked() {
        if (hasExistingKey()) {
            new AlertDialog.Builder(getContext())
                    .setTitle(R.string.svnc_client_auth_overwrite_title)
                    .setMessage(R.string.svnc_client_auth_overwrite_message)
                    .setPositiveButton(R.string.svnc_client_auth_overwrite_confirm, (d, w) -> doGenerate())
                    .setNegativeButton(android.R.string.cancel, null)
                    .show();
        } else {
            doGenerate();
        }
    }

    private void doGenerate() {
        int keyBits = KEY_SIZES[seekBarKeySize.getProgress()];
        Log.i(TAG, "Generating RSA-" + keyBits + " key pair");
        try {
            SecureVNCPluginClientAuth.GeneratedKeyPair keyPair =
                    SecureVNCPluginClientAuth.generateKeyPair(keyBits);

            selected.setClientAuthPrivKey(keyPair.privateKeyBase64());
            selected.saveAndWriteRecent(true, getContext());
            updateKeyStatus();

            pendingPubKeyDer = keyPair.publicKeyDer();

            new AlertDialog.Builder(getContext())
                    .setTitle(R.string.securevncplugin_key_generated_title)
                    .setMessage(R.string.securevncplugin_key_generated_instructions)
                    .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                        String date = new SimpleDateFormat("yyyyMMdd",
                                Locale.US).format(new Date());
                        String filename = date + "_bVNC_ClientAuth.pubkey";
                        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                        intent.addCategory(Intent.CATEGORY_OPENABLE);
                        intent.setType("application/octet-stream");
                        intent.putExtra(Intent.EXTRA_TITLE, filename);
                        ownerActivity.startActivityForResult(
                                intent, Constants.ACTIVITY_SVNC_EXPORT_PUBKEY);
                    })
                    .setCancelable(false)
                    .show();
        } catch (Exception e) {
            Log.e(TAG, "Key generation failed", e);
            Toast.makeText(getContext(), R.string.securevncplugin_key_generation_failed,
                    Toast.LENGTH_LONG).show();
        }
    }

    private void onImportClicked() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        ownerActivity.startActivityForResult(intent, Constants.ACTIVITY_SVNC_IMPORT_KEY);
    }

    private void onExportPkeyClicked() {
        String privKeyBase64 = selected.getClientAuthPrivKey();
        if (privKeyBase64 == null || privKeyBase64.isEmpty()) {
            return;
        }

        String date = new SimpleDateFormat("yyyyMMdd", Locale.US).format(new Date());
        String filename = date + "_bVNC_ClientAuth.pkey";
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        intent.putExtra(Intent.EXTRA_TITLE, filename);
        ownerActivity.startActivityForResult(intent, Constants.ACTIVITY_SVNC_EXPORT_PKEY);
    }

    /**
     * Returns the pending public key DER for export, then clears it.
     */
    public byte[] consumePendingPubKeyDer() {
        byte[] result = pendingPubKeyDer;
        pendingPubKeyDer = null;
        return result;
    }

    /**
     * Called by the activity after a successful import to refresh the key status.
     */
    public void onKeyImported() {
        updateKeyStatus();
    }
}
