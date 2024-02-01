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

package com.iiordanov.bVNC;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.Toast;

import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.util.PermissionGroups;
import com.iiordanov.util.PermissionsManager;
import com.morpheusly.common.Utilities;
import com.undatech.remoteClientUi.R;

import java.util.List;

/**
 * aRDP is the Activity for setting up RDP connections.
 */
public class aRDP extends MainConfiguration {
    private final static String TAG = "aRDP";

    private Spinner spinnerRdpGeometry;
    private EditText rdpDomain;
    private EditText resWidth;
    private EditText resHeight;
    private RadioGroup groupRemoteSoundType;
    private CheckBox checkboxEnableRecording;
    private CheckBox checkboxConsoleMode;
    private CheckBox checkboxRedirectSdCard;
    private CheckBox checkboxRemoteFx;
    private CheckBox checkboxDesktopBackground;
    private CheckBox checkboxFontSmoothing;
    private CheckBox checkboxDesktopComposition;
    private CheckBox checkboxWindowContents;
    private CheckBox checkboxMenuAnimation;
    private CheckBox checkboxVisualStyles;
    private CheckBox checkboxEnableGfx;
    private CheckBox checkboxEnableGfxH264;
    private CheckBox checkboxPreferSendingUnicode;
    private Spinner spinnerRdpColor;
    private List<String> rdpColorArray;

    @Override
    public void onCreate(Bundle icicle) {
        layoutID = R.layout.main_rdp;
        super.onCreate(icicle);
        textUsername = findViewById(R.id.textUsername);
        rdpDomain = findViewById(R.id.rdpDomain);
        rdpColorArray = Utilities.Companion.toList(getResources().getStringArray(R.array.rdp_colors));
        spinnerRdpColor = findViewById(R.id.spinnerRdpColor);
        spinnerRdpColor.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                selected.setRdpColor(Integer.parseInt(rdpColorArray.get(itemIndex)));
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });

        // The geometry type and dimensions boxes.
        spinnerRdpGeometry = findViewById(R.id.spinnerRdpGeometry);
        spinnerRdpGeometry.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                selected.setRdpResType(itemIndex);
                setRemoteWidthAndHeight(Constants.RDP_GEOM_SELECT_CUSTOM);
            }

            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });

        groupRemoteSoundType = findViewById(R.id.groupRemoteSoundType);
        checkboxEnableRecording = findViewById(R.id.checkboxEnableRecording);
        checkboxConsoleMode = findViewById(R.id.checkboxConsoleMode);
        checkboxRedirectSdCard = findViewById(R.id.checkboxRedirectSdCard);
        checkboxRemoteFx = findViewById(R.id.checkboxRemoteFx);
        checkboxDesktopBackground = findViewById(R.id.checkboxDesktopBackground);
        checkboxFontSmoothing = findViewById(R.id.checkboxFontSmoothing);
        checkboxDesktopComposition = findViewById(R.id.checkboxDesktopComposition);
        checkboxWindowContents = findViewById(R.id.checkboxWindowContents);
        checkboxMenuAnimation = findViewById(R.id.checkboxMenuAnimation);
        checkboxVisualStyles = findViewById(R.id.checkboxVisualStyles);
        checkboxEnableGfx = findViewById(R.id.checkboxEnableGfx);
        checkboxEnableGfxH264 = findViewById(R.id.checkboxEnableGfxH264);
        checkboxPreferSendingUnicode = findViewById(R.id.checkboxPreferSendingUnicode);
        setConnectionTypeSpinnerAdapter(R.array.rdp_connection_type);
    }

    @SuppressLint("SetTextI18n")
    public void updateViewFromSelected() {
        if (selected == null)
            return;
        super.updateViewFromSelected();
        textUsername.setText(selected.getUserName());
        rdpDomain.setText(selected.getRdpDomain());
        spinnerRdpColor.setSelection(rdpColorArray.indexOf(String.valueOf(selected.getRdpColor())));
        spinnerRdpGeometry.setSelection(selected.getRdpResType());
        setRemoteWidthAndHeight(Constants.RDP_GEOM_SELECT_CUSTOM);
        setRemoteSoundTypeFromSettings(selected.getRemoteSoundType());
        checkboxEnableRecording.setChecked(selected.getEnableRecording());
        checkboxConsoleMode.setChecked(selected.getConsoleMode());
        checkboxRedirectSdCard.setChecked(selected.getRedirectSdCard());
        checkboxRemoteFx.setChecked(selected.getRemoteFx());
        checkboxDesktopBackground.setChecked(selected.getDesktopBackground());
        checkboxFontSmoothing.setChecked(selected.getFontSmoothing());
        checkboxDesktopComposition.setChecked(selected.getDesktopComposition());
        checkboxWindowContents.setChecked(selected.getWindowContents());
        checkboxMenuAnimation.setChecked(selected.getMenuAnimation());
        checkboxVisualStyles.setChecked(selected.getVisualStyles());
        checkboxEnableGfx.setChecked(selected.getEnableGfx());
        checkboxEnableGfxH264.setChecked(selected.getEnableGfxH264());
        checkboxPreferSendingUnicode.setChecked(selected.getPreferSendingUnicode());
    }

    protected void updateSelectedFromView() {
        if (selected == null) {
            return;
        }
        super.updateSelectedFromView();
        selected.setUserName(textUsername.getText().toString());
        selected.setRdpDomain(rdpDomain.getText().toString());
        selected.setRdpResType(spinnerRdpGeometry.getSelectedItemPosition());
        setRemoteSoundTypeFromView(groupRemoteSoundType);
        selected.setEnableRecording(checkboxEnableRecording.isChecked());
        selected.setConsoleMode(checkboxConsoleMode.isChecked());
        selected.setRedirectSdCard(checkboxRedirectSdCard.isChecked());
        selected.setRemoteFx(checkboxRemoteFx.isChecked());
        selected.setDesktopBackground(checkboxDesktopBackground.isChecked());
        selected.setFontSmoothing(checkboxFontSmoothing.isChecked());
        selected.setDesktopComposition(checkboxDesktopComposition.isChecked());
        selected.setWindowContents(checkboxWindowContents.isChecked());
        selected.setMenuAnimation(checkboxMenuAnimation.isChecked());
        selected.setVisualStyles(checkboxVisualStyles.isChecked());
        selected.setEnableGfx(checkboxEnableGfx.isChecked());
        selected.setEnableGfxH264(checkboxEnableGfxH264.isChecked());
        selected.setPreferSendingUnicode(checkboxPreferSendingUnicode.isChecked());
    }

    /**
     * Automatically linked with android:onClick in the layout.
     */
    public void toggleEnableRecording(View view) {
        CheckBox b = (CheckBox) view;
        if (Utils.isFree(this)) {
            IntroTextDialog.showIntroTextIfNecessary(this, database, true);
            b.setChecked(false);
        } else {
            PermissionsManager.requestPermissions(this, PermissionGroups.RECORD_AND_MODIFY_AUDIO, true);
        }
        selected.setEnableRecording(b.isChecked());
    }

    /**
     * Automatically linked with android:onClick in the layout.
     */
    public void remoteSoundTypeToggled(View view) {
        if (Utils.isFree(this)) {
            IntroTextDialog.showIntroTextIfNecessary(this, database, true);
        }
    }

    /**
     * Sets the remote sound type in the settings from the specified parameter.
     */
    public void setRemoteSoundTypeFromView(View view) {
        RadioGroup g = (RadioGroup) view;
        if (Utils.isFree(this)) {
            IntroTextDialog.showIntroTextIfNecessary(this, database, true);
            g.check(R.id.radioRemoteSoundDisabled);
        }

        int id = g.getCheckedRadioButtonId();
        int soundType = Constants.REMOTE_SOUND_DISABLED;
        if (id == R.id.radioRemoteSoundOnServer) {
            soundType = Constants.REMOTE_SOUND_ON_SERVER;
        } else if (id == R.id.radioRemoteSoundOnDevice) {
            soundType = Constants.REMOTE_SOUND_ON_DEVICE;
        }
        selected.setRemoteSoundType(soundType);
    }

    public void setRemoteSoundTypeFromSettings(int type) {
        if (Utils.isFree(this)) {
            type = Constants.REMOTE_SOUND_DISABLED;
        }

        int id = 0;
        switch (type) {
            case Constants.REMOTE_SOUND_DISABLED:
                id = R.id.radioRemoteSoundDisabled;
                break;
            case Constants.REMOTE_SOUND_ON_DEVICE:
                id = R.id.radioRemoteSoundOnDevice;
                break;
            case Constants.REMOTE_SOUND_ON_SERVER:
                id = R.id.radioRemoteSoundOnServer;
                break;
        }
        groupRemoteSoundType.check(id);
    }

    public void save(MenuItem item) {
        Log.d(TAG, "save called");
        if (ipText.getText().length() != 0 && portText.getText().length() != 0) {
            saveConnectionAndCloseLayout();
        } else {
            Toast.makeText(this, R.string.rdp_server_empty, Toast.LENGTH_LONG).show();
        }
    }
}
