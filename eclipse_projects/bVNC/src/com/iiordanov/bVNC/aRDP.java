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

//
// androidVNC is the Activity for setting VNC server IP and port.
//

package com.iiordanov.bVNC;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ActivityManager.MemoryInfo;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.preference.PreferenceManager;
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
import android.widget.LinearLayout.LayoutParams;
import android.util.Log;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.pubkeygenerator.GeneratePubkeyActivity;

import java.util.ArrayList;
import java.util.Collections;
import com.google.ads.*;

public class aRDP extends Activity implements MainConfiguration {
	private final static String TAG = "aRDP";
	private Spinner connectionType;
	private int selectedConnType;
	private TextView sshCaption;
	private LinearLayout sshCredentials;
	private LinearLayout layoutUseSshPubkey;
	private LinearLayout sshServerEntry;
	private LinearLayout layoutAdvancedSettings;
	private EditText sshServer;
	private EditText sshPort;
	private EditText sshUser;
	private EditText sshPassword;
	private EditText ipText;
	private EditText portText;
	private EditText passwordText;
	private Button goButton;
	private Button buttonGeneratePubkey;
	private ToggleButton toggleAdvancedSettings;
	//private Spinner colorSpinner;
	private Spinner spinnerConnection;
	private Spinner spinnerRdpGeometry;
	private VncDatabase database;
	private ConnectionBean selected;
	private EditText textNickname;
	private EditText textUsername;
	private EditText rdpDomain;
	private EditText rdpWidth;
	private EditText rdpHeight;
	private CheckBox checkboxKeepPassword;
	private CheckBox checkboxUseDpadAsArrows;
	private CheckBox checkboxRemoteFx;
	private CheckBox checkboxDesktopBackground;
	private CheckBox checkboxFontSmoothing;
	private CheckBox checkboxDesktopComposition;
	private CheckBox checkboxWindowContents;
	private CheckBox checkboxMenuAnimation;
	private CheckBox checkboxVisualStyles;
	private CheckBox checkboxRotateDpad;
	private CheckBox checkboxLocalCursor;
	private CheckBox checkboxUseSshPubkey;
	private boolean isFree;
	private AdView adView;	

	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		System.gc();
		setContentView(R.layout.main_rdp);
		
		isFree = this.getPackageName().contains("free");
		
		ipText = (EditText) findViewById(R.id.textIP);
		sshServer = (EditText) findViewById(R.id.sshServer);
		sshPort = (EditText) findViewById(R.id.sshPort);
		sshUser = (EditText) findViewById(R.id.sshUser);
		sshPassword = (EditText) findViewById(R.id.sshPassword);
		sshCredentials = (LinearLayout) findViewById(R.id.sshCredentials);
		sshCaption = (TextView) findViewById(R.id.sshCaption);
		layoutUseSshPubkey = (LinearLayout) findViewById(R.id.layoutUseSshPubkey);
		sshServerEntry = (LinearLayout) findViewById(R.id.sshServerEntry);
		portText = (EditText) findViewById(R.id.textPORT);
		passwordText = (EditText) findViewById(R.id.textPASSWORD);
		textNickname = (EditText) findViewById(R.id.textNickname);
		textUsername = (EditText) findViewById(R.id.textUsername);
		rdpDomain = (EditText) findViewById(R.id.rdpDomain);

		// Here we say what happens when the Pubkey Checkbox is checked/unchecked.
		checkboxUseSshPubkey = (CheckBox) findViewById(R.id.checkboxUseSshPubkey);
		checkboxUseSshPubkey.setOnCheckedChangeListener(new OnCheckedChangeListener() {
			@Override
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
				selected.setUseSshPubKey(isChecked);
				setSshPasswordHint (isChecked);
				sshPassword.setText("");
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
		
		// Define what happens when somebody selects different VNC connection types.
		connectionType = (Spinner) findViewById(R.id.connectionType);
		connectionType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {

				selectedConnType = itemIndex;
				if (selectedConnType == VncConstants.CONN_TYPE_PLAIN) {
					setVisibilityOfSshWidgets (View.GONE);
				} else if (selectedConnType == VncConstants.CONN_TYPE_SSH) {
					setVisibilityOfSshWidgets (View.VISIBLE);
					if (ipText.getText().toString().equals(""))
						ipText.setText("localhost");
					setSshPasswordHint (checkboxUseSshPubkey.isChecked());
				}
			}

			@Override
			public void onNothingSelected(AdapterView<?> ad) {
			}
		});

		goButton = (Button) findViewById(R.id.buttonGO);
		checkboxKeepPassword = (CheckBox)findViewById(R.id.checkboxKeepPassword);
		checkboxUseDpadAsArrows = (CheckBox)findViewById(R.id.checkboxUseDpadAsArrows);
		checkboxRotateDpad = (CheckBox)findViewById(R.id.checkboxRotateDpad);
		checkboxLocalCursor = (CheckBox)findViewById(R.id.checkboxUseLocalCursor);
		spinnerConnection = (Spinner)findViewById(R.id.spinnerConnection);
		spinnerConnection.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {
				selected = (ConnectionBean)ad.getSelectedItem();
				updateViewFromSelected();
			}
			@Override
			public void onNothingSelected(AdapterView<?> ad) {
				selected = null;
			}
		});
		spinnerConnection.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {

			/* (non-Javadoc)
			 * @see android.widget.AdapterView.OnItemLongClickListener#onItemLongClick(android.widget.AdapterView, android.view.View, int, long)
			 */
			@Override
			public boolean onItemLongClick(AdapterView<?> arg0, View arg1,
					int arg2, long arg3) {
				spinnerConnection.setSelection(arg2);
				selected = (ConnectionBean)spinnerConnection.getItemAtPosition(arg2);
				canvasStart();
				return true;
			}
			
		});
		goButton.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View view) {
				if (ipText.getText().length() != 0 && portText.getText().length() != 0)
					canvasStart();
				else
					Toast.makeText(view.getContext(), R.string.rdp_server_empty, Toast.LENGTH_LONG).show();
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
		
		// The geometry type and dimensions boxes.
		spinnerRdpGeometry = (Spinner) findViewById(R.id.spinnerRdpGeometry);
		rdpWidth = (EditText) findViewById(R.id.rdpWidth);
		rdpHeight = (EditText) findViewById(R.id.rdpHeight);		
		spinnerRdpGeometry.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener () {
			@Override
			public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
				selected.setRdpResType(itemIndex);
				setRemoteWidthAndHeight ();
			}
			@Override
			public void onNothingSelected(AdapterView<?> arg0) {
			}
		});

		checkboxRemoteFx = (CheckBox)findViewById(R.id.checkboxRemoteFx);
		checkboxDesktopBackground = (CheckBox)findViewById(R.id.checkboxDesktopBackground);
		checkboxFontSmoothing = (CheckBox)findViewById(R.id.checkboxFontSmoothing);
		checkboxDesktopComposition = (CheckBox)findViewById(R.id.checkboxDesktopComposition);
		checkboxWindowContents = (CheckBox)findViewById(R.id.checkboxWindowContents);
		checkboxMenuAnimation = (CheckBox)findViewById(R.id.checkboxMenuAnimation);
		checkboxVisualStyles = (CheckBox)findViewById(R.id.checkboxVisualStyles);

		database = new VncDatabase(this);
		
		// Define what happens when the Import/Export button is pressed.
		((Button)findViewById(R.id.buttonImportExport)).setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				android.util.Log.e(TAG, "import/export!!");
				showDialog(R.layout.importexport);
			}
		});
	}
	
	/**
	 * Makes the ssh-related widgets visible/invisible.
	 */
	private void setVisibilityOfSshWidgets (int visibility) {
		sshCredentials.setVisibility(visibility);
		sshCaption.setVisibility(visibility);
		layoutUseSshPubkey.setVisibility(visibility);
		sshServerEntry.setVisibility(visibility);
	}

	/**
	 * Enables and disables the EditText boxes for width and height of remote desktop.
	 */
	private void setRemoteWidthAndHeight () {
		if (selected.getRdpResType() != VncConstants.RDP_GEOM_SELECT_CUSTOM) {
			rdpWidth.setEnabled(false);
			rdpHeight.setEnabled(false);
		} else {
			rdpWidth.setEnabled(true);
			rdpHeight.setEnabled(true);
		}
	}
	
	/**
	 * Sets the ssh password/passphrase hint appropriately.
	 */
	private void setSshPasswordHint (boolean isPassphrase) {
		if (isPassphrase) {
			sshPassword.setHint(R.string.ssh_passphrase_hint);
		} else {
			sshPassword.setHint(R.string.password_hint_ssh);
		}
	}

	protected void onDestroy() {
		database.close();
		System.gc();
		super.onDestroy();
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
		}
		return null;
	}
	
	/**
	 * Creates the help dialog for this activity.
	 */
	private Dialog createHelpDialog() {
	    AlertDialog.Builder adb = new AlertDialog.Builder(this)
	    		.setMessage(R.string.rdp_main_screen_help_text)
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
	    lp.width = WindowManager.LayoutParams.FILL_PARENT;
	    lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
	    d.show();
	    d.getWindow().setAttributes(lp);
	    return d;
	}

	/* (non-Javadoc)
	 * @see android.app.Activity#onCreateOptionsMenu(android.view.Menu)
	 */
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.androidvncmenu,menu);
		return true;
	}

	/* (non-Javadoc)
	 * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
	 */
	@Override
	public boolean onMenuOpened(int featureId, Menu menu) {
		menu.findItem(R.id.itemDeleteConnection).setEnabled(selected!=null && ! selected.isNew());
		menu.findItem(R.id.itemSaveAsCopy).setEnabled(selected!=null && ! selected.isNew());
		return true;
	}

	/* (non-Javadoc)
	 * @see android.app.Activity#onOptionsItemSelected(android.view.MenuItem)
	 */
	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId())
		{
		case R.id.itemSaveAsCopy :
			if (selected.getNickname().equals(textNickname.getText().toString()))
				textNickname.setText("Copy of "+selected.getNickname());
			updateSelectedFromView();
			selected.set_Id(0);
			saveAndWriteRecent();
			arriveOnPage();
			break;
		case R.id.itemDeleteConnection :
			Utils.showYesNoPrompt(this, "Delete?", "Delete " + selected.getNickname() + "?",
					new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int i)
				{
					selected.Gen_delete(database.getWritableDatabase());
	    			database.close();
					arriveOnPage();
				}
			}, null);
			break;
		case R.id.itemMainScreenHelp:
			showDialog(R.id.itemMainScreenHelp);
			break;
		// Disabling Manual/Wiki Menu item as the original does not correspond to this project anymore.
		//case R.id.itemOpenDoc :
		//	Utils.showDocumentation(this);
		//	break;
		}
		return true;
	}

	protected void updateViewFromSelected() {
		if (selected == null)
			return;
		selectedConnType = selected.getConnectionType();
		connectionType.setSelection(selectedConnType);
		sshServer.setText(selected.getSshServer());
		sshPort.setText(Integer.toString(selected.getSshPort()));
		sshUser.setText(selected.getSshUser());
		
		checkboxUseSshPubkey.setChecked(selected.getUseSshPubKey());
		setSshPasswordHint (checkboxUseSshPubkey.isChecked());

		if (selectedConnType == VncConstants.CONN_TYPE_SSH && selected.getAddress().equals(""))
			ipText.setText("localhost");
		else
			ipText.setText(selected.getAddress());

		// If we are doing automatic X session discovery, then disable
		// vnc address, vnc port, and vnc password, and vice-versa
		if (selectedConnType == 1 && selected.getAutoXEnabled()) {
			ipText.setVisibility(View.GONE);
			portText.setVisibility(View.GONE);
			passwordText.setVisibility(View.GONE);
			checkboxKeepPassword.setVisibility(View.GONE);
		} else {
			ipText.setVisibility(View.VISIBLE);
			portText.setVisibility(View.VISIBLE);
			passwordText.setVisibility(View.VISIBLE);
			checkboxKeepPassword.setVisibility(View.VISIBLE);
		}

		portText.setText(Integer.toString(selected.getPort()));
		
		if (selected.getKeepPassword() || selected.getPassword().length()>0) {
			passwordText.setText(selected.getPassword());
		}

		checkboxKeepPassword.setChecked(selected.getKeepPassword());
		checkboxUseDpadAsArrows.setChecked(selected.getUseDpadAsArrows());
		checkboxRotateDpad.setChecked(selected.getRotateDpad());
		checkboxLocalCursor.setChecked(selected.getUseLocalCursor());
		textNickname.setText(selected.getNickname());
		textUsername.setText(selected.getUserName());
		rdpDomain.setText(selected.getRdpDomain());
		spinnerRdpGeometry.setSelection(selected.getRdpResType());
		rdpWidth.setText(Integer.toString(selected.getRdpWidth()));
		rdpHeight.setText(Integer.toString(selected.getRdpHeight()));
		setRemoteWidthAndHeight ();
		checkboxRemoteFx.setChecked(selected.getRemoteFx());
		checkboxDesktopBackground.setChecked(selected.getDesktopBackground());
		checkboxFontSmoothing.setChecked(selected.getFontSmoothing());
		checkboxDesktopComposition.setChecked(selected.getDesktopComposition());
		checkboxWindowContents.setChecked(selected.getWindowContents());
		checkboxMenuAnimation.setChecked(selected.getMenuAnimation());
		checkboxVisualStyles.setChecked(selected.getVisualStyles());

		/* TODO: Reinstate color spinner but for RDP settings.
		colorSpinner = (Spinner)findViewById(R.id.colorformat);
		COLORMODEL[] models=COLORMODEL.values();
		ArrayAdapter<COLORMODEL> colorSpinnerAdapter = new ArrayAdapter<COLORMODEL>(this, R.layout.connection_list_entry, models);
		colorSpinner.setAdapter(colorSpinnerAdapter);
		colorSpinner.setSelection(0);
		COLORMODEL cm = COLORMODEL.valueOf(selected.getColorModel());
		COLORMODEL[] colors=COLORMODEL.values();
		for (int i=0; i<colors.length; ++i)
		{
			if (colors[i] == cm) {
				colorSpinner.setSelection(i);
				break;
			}
		}*/
	}

	/**
	 * Returns the current ConnectionBean.
	 */
	public ConnectionBean getCurrentConnection () {
		return selected;
	}

	/**
	 * Returns the display height, or if the device has software
	 * buttons, the 'bottom' of the view (in order to take into account the
	 * software buttons.
	 * @return the height in pixels.
	 */
	public int getHeight () {
		View v    = getWindow().getDecorView().findViewById(android.R.id.content);
		Display d = getWindowManager().getDefaultDisplay();
		int bottom = v.getBottom();
		int height = d.getHeight();
		
        if (android.os.Build.VERSION.SDK_INT >= 14) {
        	android.view.ViewConfiguration vc = ViewConfiguration.get(this);
        	if (vc.hasPermanentMenuKey())
        		return bottom;
        }
		return height;
	}
	
	/**
	 * Returns the display width, or if the device has software
	 * buttons, the 'right' of the view (in order to take into account the
	 * software buttons.
	 * @return the width in pixels.
	 */
	public int getWidth () {
		View v    = getWindow().getDecorView().findViewById(android.R.id.content);
		Display d = getWindowManager().getDefaultDisplay();
		int right = v.getRight();
		int width = d.getWidth();
        if (android.os.Build.VERSION.SDK_INT >= 14) {
        	android.view.ViewConfiguration vc = ViewConfiguration.get(this);
        	if (vc.hasPermanentMenuKey())
        		return right;
        }
		return width;
	}

	private void updateSelectedFromView() {
		if (selected == null) {
			return;
		}
		selected.setConnectionType(selectedConnType);
		selected.setAddress(ipText.getText().toString());
		try	{
			selected.setPort(Integer.parseInt(portText.getText().toString()));
			selected.setSshPort(Integer.parseInt(sshPort.getText().toString()));
		} catch (NumberFormatException nfe) {}
		
		selected.setNickname(textNickname.getText().toString());
		selected.setSshServer(sshServer.getText().toString());
		selected.setSshUser(sshUser.getText().toString());

		selected.setKeepSshPassword(false);
		
		// If we are using an SSH key, then the ssh password box is used
		// for the key pass-phrase instead.
		selected.setUseSshPubKey(checkboxUseSshPubkey.isChecked());
		selected.setSshPassPhrase(sshPassword.getText().toString());
		selected.setSshPassword(sshPassword.getText().toString());
		selected.setUserName(textUsername.getText().toString());
		selected.setRdpDomain(rdpDomain.getText().toString());
		selected.setRdpResType(spinnerRdpGeometry.getSelectedItemPosition());
		try	{
			selected.setRdpWidth(Integer.parseInt(rdpWidth.getText().toString()));
			selected.setRdpHeight(Integer.parseInt(rdpHeight.getText().toString()));
		} catch (NumberFormatException nfe) {}
		selected.setRemoteFx(checkboxRemoteFx.isChecked());
		selected.setDesktopBackground(checkboxDesktopBackground.isChecked());
		selected.setFontSmoothing(checkboxFontSmoothing.isChecked());
		selected.setDesktopComposition(checkboxDesktopComposition.isChecked());
		selected.setWindowContents(checkboxWindowContents.isChecked());
		selected.setMenuAnimation(checkboxMenuAnimation.isChecked());
		selected.setVisualStyles(checkboxVisualStyles.isChecked());
		selected.setPassword(passwordText.getText().toString());
		selected.setKeepPassword(checkboxKeepPassword.isChecked());
		selected.setUseDpadAsArrows(checkboxUseDpadAsArrows.isChecked());
		selected.setRotateDpad(checkboxRotateDpad.isChecked());
		selected.setUseLocalCursor(checkboxLocalCursor.isChecked());
		// TODO: Reinstate Color model spinner but for RDP settings.
		//selected.setColorModel(((COLORMODEL)colorSpinner.getSelectedItem()).nameString());
	}
	
	protected void onStart() {
		super.onStart();
		System.gc();
		arriveOnPage();
	}

	protected void onResume() {
		super.onStart();
		System.gc();
		arriveOnPage();
	}
	
	@Override
	public void onWindowFocusChanged (boolean visible) {
		if (visible && isFree)
			displayAd ();
	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		Log.e(TAG, "onConfigurationChanged called");
		super.onConfigurationChanged(newConfig);
		if (isFree)
			displayAd ();
	}

	/**
	 * Return the object representing the app global state in the database, or null
	 * if the object hasn't been set up yet
	 * @param db App's database -- only needs to be readable
	 * @return Object representing the single persistent instance of MostRecentBean, which
	 * is the app's global state
	 */
	static MostRecentBean getMostRecent(SQLiteDatabase db)
	{
		ArrayList<MostRecentBean> recents = new ArrayList<MostRecentBean>(1);
		MostRecentBean.getAll(db, MostRecentBean.GEN_TABLE_NAME, recents, MostRecentBean.GEN_NEW);
		if (recents.size() == 0)
			return null;
		return recents.get(0);
	}
	
	public void arriveOnPage() {
		ArrayList<ConnectionBean> connections=new ArrayList<ConnectionBean>();
		ConnectionBean.getAll(database.getReadableDatabase(), ConnectionBean.GEN_TABLE_NAME, connections, ConnectionBean.newInstance);
		Collections.sort(connections);
		connections.add(0, new ConnectionBean(this));
		int connectionIndex=0;
		if ( connections.size()>1)
		{
			MostRecentBean mostRecent = getMostRecent(database.getReadableDatabase());
			if (mostRecent != null)
			{
				for ( int i=1; i<connections.size(); ++i)
				{
					if (connections.get(i).get_Id() == mostRecent.getConnectionId())
					{
						connectionIndex=i;
						break;
					}
				}
			}
		}
		spinnerConnection.setAdapter(new ArrayAdapter<ConnectionBean>(this, R.layout.connection_list_entry,
				connections.toArray(new ConnectionBean[connections.size()])));
		spinnerConnection.setSelection(connectionIndex,false);
		selected=connections.get(connectionIndex);
		updateViewFromSelected();
		IntroTextDialog.showIntroTextIfNecessary(this, database, false);
	}
	
	private void displayAd () {
	    LinearLayout layout = (LinearLayout)findViewById(R.id.mainLayout);
		if (adView != null)
			layout.removeViewInLayout(adView);
			
	    // Create the adView
	    adView = new AdView(this, AdSize.SMART_BANNER, "a151744a8b1f640");
		WindowManager.LayoutParams lp = getWindow().getAttributes();
		lp.width     = LayoutParams.FILL_PARENT;
		lp.height    = LayoutParams.FILL_PARENT;
	    adView.setLayoutParams(lp);
		    
	    // Add the adView to the layout
	    layout.addView(adView, 2);

	    // Initiate a generic request to load it with an ad
		AdRequest adRequest = new AdRequest();
		adRequest.addTestDevice("255443C58D5D1959D075281106206D06");
		adRequest.addTestDevice("09FDE4DD4719E7977E76123975E26EC4");
	    adView.loadAd(adRequest);
	}
	
	protected void onStop() {
		super.onStop();
		if ( selected == null ) {
			return;
		}
		updateSelectedFromView();

		// We need VNC server or SSH server to be filled out to save. Otherwise, we keep adding empty
		// connections when onStop gets called.
		if (selected.getConnectionType() == VncConstants.CONN_TYPE_SSH && selected.getSshServer().equals("") ||
			selected.getAddress().equals(""))
			return;
		
		saveAndWriteRecent();
	}
	
	public VncDatabase getDatabaseHelper()
	{
		return database;
	}
	
	private void canvasStart() {
		if (selected == null) return;
		MemoryInfo info = Utils.getMemoryInfo(this);
		if (info.lowMemory)
			System.gc();
		vnc();
	}
	
	protected void saveAndWriteRecent()
	{
		SQLiteDatabase db = database.getWritableDatabase();
		db.beginTransaction();
		try
		{
			selected.save(db);
			MostRecentBean mostRecent = getMostRecent(db);
			if (mostRecent == null)
			{
				mostRecent = new MostRecentBean();
				mostRecent.setConnectionId(selected.get_Id());
				mostRecent.Gen_insert(db);
			}
			else
			{
				mostRecent.setConnectionId(selected.get_Id());
				mostRecent.Gen_update(db);
			}
			db.setTransactionSuccessful();
		}
		finally
		{
			db.endTransaction();
			db.close();
		}
	}

	/**
	 * Starts the activity which makes a VNC connection and displays the remote desktop.
	 */
	private void vnc () {
		updateSelectedFromView();
		saveAndWriteRecent();
		Intent intent = new Intent(this, VncCanvasActivity.class);
		intent.putExtra(VncConstants.CONNECTION,selected.Gen_getValues());
		startActivity(intent);
	}
	
	/**
	 * Starts the activity which manages keys.
	 */
	private void generatePubkey () {
		updateSelectedFromView();
		saveAndWriteRecent();
		Intent intent = new Intent(this, GeneratePubkeyActivity.class);
		intent.putExtra("PrivateKey",selected.getSshPrivKey());
		startActivityForResult(intent, VncConstants.ACTIVITY_GEN_KEY);
	}
	
	/**
	 * This function is used to retrieve data returned by activities started with startActivityForResult.
	 */
	@Override
	public void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		switch(requestCode) {
		case (VncConstants.ACTIVITY_GEN_KEY):
			if (resultCode == Activity.RESULT_OK) {
				Bundle b = data.getExtras();
				String privateKey = (String)b.get("PrivateKey");
				if (!privateKey.equals(selected.getSshPrivKey()) && privateKey.length() != 0)
					Toast.makeText(getBaseContext(), "New key generated/imported successfully. Tap 'Generate/Export Key' " +
							" button to share, copy to clipboard, or export the public key now.", Toast.LENGTH_LONG).show();
				selected.setSshPrivKey(privateKey);
				selected.setSshPubKey((String)b.get("PublicKey"));
				saveAndWriteRecent();
			} else
				Log.i (TAG, "The user cancelled SSH key generation.");
			break;
		}
	}
}
