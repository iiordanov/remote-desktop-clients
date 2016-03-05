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

package com.undatech.opaque;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.PrintWriter;

import com.undatech.opaque.R;

import android.app.Activity;
import android.app.DialogFragment;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.widget.Button;
import android.widget.ToggleButton;

public class AdvancedSettingsActivity extends FragmentActivity implements ManageCustomCaFragment.OnFragmentDismissedListener {
	private static String TAG = "AdvancedSettingsActivity";
	
	private ConnectionSettings currentConnection;
	private ToggleButton toggleAudioPlayback;
	private ToggleButton toggleAutoRotation;
	private ToggleButton toggleAutoRequestDisplayResolution;
    private ToggleButton toggleSslStrict;
    private ToggleButton toggleUsbEnabled;
	private ToggleButton toggleUsingCustomOvirtCa;
	private Button buttonManageOvirtCa;
	
	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		setContentView(R.layout.advanced_settings_activity);

		Intent i = getIntent();
		currentConnection = (ConnectionSettings)i.getSerializableExtra("com.undatech.opaque.ConnectionSettings");
		
		toggleAudioPlayback = (ToggleButton)findViewById(R.id.toggleAudioPlayback);
		toggleAudioPlayback.setChecked(currentConnection.isAudioPlaybackEnabled());
		
		toggleUsbEnabled = (ToggleButton)findViewById(R.id.toggleUsbEnabled);
		toggleUsbEnabled.setChecked(currentConnection.isUsbEnabled());
		
		toggleAutoRotation = (ToggleButton)findViewById(R.id.toggleAutoRotation);
		toggleAutoRotation.setChecked(currentConnection.isRotationEnabled());
		
		toggleAutoRequestDisplayResolution = (ToggleButton)findViewById(R.id.toggleAutoRequestDisplayResolution);
		toggleAutoRequestDisplayResolution.setChecked(currentConnection.isRequestingNewDisplayResolution());

		toggleSslStrict = (ToggleButton)findViewById(R.id.toggleSslStrict);
		toggleSslStrict.setChecked(currentConnection.isSslStrict());
		
		toggleUsingCustomOvirtCa = (ToggleButton)findViewById(R.id.toggleUsingCustomOvirtCa);
		toggleUsingCustomOvirtCa.setChecked(currentConnection.isUsingCustomOvirtCa());
		
		buttonManageOvirtCa = (Button)findViewById(R.id.buttonManageOvirtCa);
		buttonManageOvirtCa.setEnabled(currentConnection.isUsingCustomOvirtCa());

		// Send the generated data back to the calling activity.
		Intent databackIntent = new Intent();
		databackIntent.putExtra("com.undatech.opaque.ConnectionSettings", currentConnection);
		setResult(Activity.RESULT_OK, databackIntent);
	}
	
	/**
	 * Automatically linked with android:onClick to the toggleAudio button.
	 * @param view
	 */
	public void toggleAudioPlaybackSetting (View view) {
		ToggleButton s = (ToggleButton) view;
		currentConnection.setAudioPlaybackEnabled(s.isChecked());	
	}
	
	/**
	 * Automatically linked with android:onClick to the toggleUsbEnabled button.
	 * @param view
	 */
	public void toggleUsbEnabledSetting (View view) {
		ToggleButton s = (ToggleButton) view;
		currentConnection.setUsbEnabled(s.isChecked());	
	}
	
	/**
     * Automatically linked with android:onClick to the toggleAutoRotation button.
     * @param view
     */
    public void toggleAutoRotation (View view) {
        ToggleButton s = (ToggleButton) view;
        currentConnection.setRotationEnabled(s.isChecked());    
    }
	
	/**
	 * Automatically linked with android:onClick to the toggleRotation button.
	 * @param view
	 */
	public void toggleAutoRequestDisplayResolution (View view) {
		ToggleButton s = (ToggleButton) view;
		currentConnection.setRequestingNewDisplayResolution(s.isChecked());	
	}	
	
	/**
	 * Automatically linked with android:onClick to the toggleSslStrict button.
	 * @param view
	 */
	public void toggleSslStrict (View view) {
		ToggleButton s = (ToggleButton) view;
		currentConnection.setSslStrict(s.isChecked());	
	}
	
	/**
	 * Automatically linked with android:onClick to the toggleUsingCustomOvirtCa button.
	 * @param view
	 */
	public void toggleUsingCustomOvirtCa (View view) {
		ToggleButton s = (ToggleButton) view;
		boolean usingCustomOvirtCa = s.isChecked();
		currentConnection.setUsingCustomOvirtCa(usingCustomOvirtCa);
		buttonManageOvirtCa.setEnabled(usingCustomOvirtCa);
	}
	
	/**
	 * Automatically linked with android:onClick to the buttonManageOvirtCa button.
	 * @param view
	 */
	public void showManageOvirtCaDialog (View view) {
		showCaDialog (ManageCustomCaFragment.TYPE_OVIRT);
	}
	
	private void showCaDialog (int caPurpose) {
		FragmentManager fm = this.getSupportFragmentManager();
		ManageCustomCaFragment newFragment = ManageCustomCaFragment.newInstance(caPurpose, currentConnection);
	    newFragment.show(fm, "customCa");
	}

	@Override
	public void onFragmentDismissed(ConnectionSettings currentConnection) {
		this.currentConnection = currentConnection;
	}
}
