/* 
 * Copyright (C) 2012 Iordan Iordanov
 * 
 * No previous copyright attribution was present, but I am guessing:
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
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.util.Log;
import com.iiordanov.bVNC.Utils;

import java.util.ArrayList;
import java.util.Collections;

public class androidVNC extends Activity {
	private final static String TAG = "androidVNC";
	private Spinner connectionType;
	private int selectedConnType;
	private TextView sshCredentialsCaption;
	private LinearLayout sshCredentials;
	private TextView sshServerEntryCaption;
	private LinearLayout sshServerEntry;
	private EditText sshServer;
	private EditText sshPort;
	private EditText sshUser;
	private EditText sshPassword;
	private TextView sshPubKey;
	private boolean sshPubkeyTextSet = false;
	private EditText ipText;
	private EditText portText;
	private EditText passwordText;
	private Button goButton;
	private Button repeaterButton;
	private LinearLayout repeaterEntry;
	private TextView repeaterText;
	private RadioGroup groupForceFullScreen;
	private Spinner colorSpinner;
	private Spinner spinnerConnection;
	private VncDatabase database;
	private ConnectionBean selected;
	private EditText textNickname;
	private EditText textUsername;
	private TextView textUsernameCaption;
	private CheckBox checkboxKeepPassword;
	private CheckBox checkboxUseDpadAsArrows;
	private CheckBox checkboxRotateDpad;
	private CheckBox checkboxUsePortrait;
	private CheckBox checkboxLocalCursor;
	private boolean repeaterTextSet;

	@Override
	public void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		System.gc();
		setContentView(R.layout.main);

		ipText = (EditText) findViewById(R.id.textIP);
		sshServer = (EditText) findViewById(R.id.sshServer);
		sshPort = (EditText) findViewById(R.id.sshPort);
		sshUser = (EditText) findViewById(R.id.sshUser);
		sshPassword = (EditText) findViewById(R.id.sshPassword);
		sshCredentials = (LinearLayout) findViewById(R.id.sshCredentials);
		sshCredentialsCaption = (TextView) findViewById(R.id.sshCredentialsCaption);
		sshServerEntry = (LinearLayout) findViewById(R.id.sshServerEntry);
		sshServerEntryCaption = (TextView) findViewById(R.id.sshServerEntryCaption);
		portText = (EditText) findViewById(R.id.textPORT);
		passwordText = (EditText) findViewById(R.id.textPASSWORD);
		textNickname = (EditText) findViewById(R.id.textNickname);
		textUsername = (EditText) findViewById(R.id.textUsername);
		textUsernameCaption = (TextView) findViewById(R.id.textUsernameCaption);

		// Define what happens when the Repeater button is pressed.
		repeaterButton = (Button) findViewById(R.id.buttonRepeater);
		repeaterEntry = (LinearLayout) findViewById(R.id.repeaterEntry);
		repeaterButton.setOnClickListener(new View.OnClickListener() {	
			@Override
			public void onClick(View v) {
				showDialog(R.layout.repeater_dialog);
			}
		});
	
		// Define what happens when somebody selects different VNC connection types.
		connectionType = (Spinner) findViewById(R.id.connectionType);
		connectionType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
			@Override
			public void onItemSelected(AdapterView<?> ad, View view, int itemIndex, long id) {

				// TODO: Make position values not hard-coded numbers but derived positions based on ID's.
				selectedConnType = itemIndex;
				if (selectedConnType == 0 ||
					selectedConnType == 3) {
					sshCredentials.setVisibility(View.GONE);
					sshCredentialsCaption.setVisibility(View.GONE);
					sshServerEntry.setVisibility(View.GONE);
					ipText.setHint(R.string.address_caption_hint);
					sshServerEntryCaption.setVisibility(View.GONE);
					textUsername.setVisibility(View.GONE);
					textUsernameCaption.setVisibility(View.GONE);
					repeaterEntry.setVisibility(View.GONE);
				} else if (selectedConnType == 1) {
					sshCredentials.setVisibility(View.VISIBLE);
					sshCredentialsCaption.setVisibility(View.VISIBLE);
					sshServerEntry.setVisibility(View.VISIBLE);
					if (ipText.getText().toString().equals(""))
						ipText.setText("localhost");
					ipText.setHint(R.string.address_caption_hint_tunneled);
					sshServerEntryCaption.setVisibility(View.VISIBLE);
					textUsername.setVisibility(View.GONE);
					textUsernameCaption.setVisibility(View.GONE);
					repeaterEntry.setVisibility(View.GONE);
				} else if (selectedConnType == 2) {
					sshCredentials.setVisibility(View.GONE);
					sshCredentialsCaption.setVisibility(View.GONE);
					sshServerEntry.setVisibility(View.GONE);
					ipText.setHint(R.string.address_caption_hint);
					sshServerEntryCaption.setVisibility(View.GONE);
					textUsernameCaption.setVisibility(View.VISIBLE);
					textUsername.setVisibility(View.VISIBLE);
					textUsername.setHint(R.string.username_hint);
					repeaterEntry.setVisibility(View.VISIBLE);
				} else if (selectedConnType == 4) {
					sshCredentials.setVisibility(View.GONE);
					sshCredentialsCaption.setVisibility(View.GONE);
					sshServerEntry.setVisibility(View.GONE);
					ipText.setHint(R.string.address_caption_hint);
					sshServerEntryCaption.setVisibility(View.GONE);
					textUsernameCaption.setVisibility(View.VISIBLE);
					textUsername.setVisibility(View.VISIBLE);
					textUsername.setHint(R.string.username_hint_vencrypt);
					repeaterEntry.setVisibility(View.GONE);
					if (passwordText.getText().toString().equals(""))
						checkboxKeepPassword.setChecked(false);
				}
			}
			@Override
			public void onNothingSelected(AdapterView<?> ad) {
				sshCredentials.setVisibility(View.GONE);
				sshCredentialsCaption.setVisibility(View.GONE);
				sshServerEntry.setVisibility(View.GONE);
				sshServerEntryCaption.setVisibility(View.GONE);
				textUsername.setVisibility(View.GONE);
			}
		});

		goButton = (Button) findViewById(R.id.buttonGO);

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
		checkboxUsePortrait = (CheckBox)findViewById(R.id.checkboxUsePortrait);
		checkboxLocalCursor = (CheckBox)findViewById(R.id.checkboxUseLocalCursor);
		colorSpinner.setAdapter(colorSpinnerAdapter);
		colorSpinner.setSelection(0);
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
		repeaterText = (TextView)findViewById(R.id.textRepeaterId);
		goButton.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View view) {
				if (ipText.getText().length() != 0 && portText.getText().length() != 0)
					canvasStart();
				else
					Toast.makeText(view.getContext(), "VNC Server or port empty. Cannot connect!",
									Toast.LENGTH_LONG).show();
			}
		});
		
		database = new VncDatabase(this);
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
			return createMainScreenHelpDialog();
		default:
			return new RepeaterDialog(this);
		}
	}
	
	/*
	 * Creates the main screen help dialog.
	 * TODO: I need a high resolution device device to develop and test this on.
	 */
	private Dialog createMainScreenHelpDialog() {
		Dialog d = new AlertDialog.Builder(this)
		.setMessage(R.string.main_screen_help_text)
		.setPositiveButton(R.string.close,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog,
							int whichButton) {
						// We don't have to do anything.
					}
				}).create();

		WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
	    lp.copyFrom(d.getWindow().getAttributes());
	    lp.width = WindowManager.LayoutParams.FILL_PARENT;
	    lp.height = WindowManager.LayoutParams.FILL_PARENT;
	    
		//d.getWindow().setLayout(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
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

	private void updateViewFromSelected() {
		if (selected==null)
			return;
		selectedConnType = selected.getConnectionType();
		connectionType.setSelection(selectedConnType);
		sshServer.setText(selected.getSshServer());
		sshPort.setText(Integer.toString(selected.getSshPort()));
		sshUser.setText(selected.getSshUser());
		sshPassword.setText(selected.getSshPassword());
		updateSshPubKeyInfo(selected.getUseSshPubKey(), selected.getSshPubKey());

		// TODO: Switch away from using hard-coded numeric value for connection type.
		if (selectedConnType == 1 && selected.getAddress().equals(""))
			ipText.setText("localhost");
		else
			ipText.setText(selected.getAddress());

		portText.setText(Integer.toString(selected.getPort()));
		
		if (selected.getKeepPassword() || selected.getPassword().length()>0) {
			passwordText.setText(selected.getPassword());
		}
		groupForceFullScreen.check(selected.getForceFull()==BitmapImplHint.AUTO ? R.id.radioForceFullScreenAuto : (selected.getForceFull() == BitmapImplHint.FULL ? R.id.radioForceFullScreenOn : R.id.radioForceFullScreenOff));
		checkboxKeepPassword.setChecked(selected.getKeepPassword());
		checkboxUseDpadAsArrows.setChecked(selected.getUseDpadAsArrows());
		checkboxRotateDpad.setChecked(selected.getRotateDpad());
		checkboxUsePortrait.setChecked(selected.getUsePortrait());
		checkboxLocalCursor.setChecked(selected.getUseLocalCursor());
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
	void updateRepeaterInfo(boolean useRepeater, String repeaterId)
	{
		if (useRepeater)
		{
			repeaterText.setText(repeaterId);
			repeaterTextSet = true;
		}
		else
		{
			repeaterText.setText(getText(R.string.repeater_empty_text));
			repeaterTextSet = false;
		}
	}
	
	/**
	 * Called when changing view to match selected connection or from
	 * SSH PubKey dialog to update the PubKey information shown.
	 * @param sshPubKey If null or empty, show text for not using sshPubKey
	 */
	void updateSshPubKeyInfo(boolean useSshPubKey, String newSshPubKey)
	{
		if (useSshPubKey)
		{
			sshPubKey.setText(newSshPubKey);
			useSshPubKey = true;
		}
		else
		{
			//TODO: When I am done implementing the interface for SSH pubkeys, I can re-enable this.
			//sshPubKey.setText("");
			useSshPubKey = false;
		}
	}
	
	
	private void updateSelectedFromView() {
		if (selected==null) {
			return;
		}
		selected.setConnectionType(selectedConnType);
		selected.setAddress(ipText.getText().toString());
		try
		{
			selected.setPort(Integer.parseInt(portText.getText().toString()));
			selected.setSshPort(Integer.parseInt(sshPort.getText().toString()));
		}
		catch (NumberFormatException nfe)
		{
			
		}
		selected.setNickname(textNickname.getText().toString());
		selected.setSshServer(sshServer.getText().toString());
		selected.setSshUser(sshUser.getText().toString());
		selected.setSshPassword(sshPassword.getText().toString());
		selected.setKeepSshPassword(false);
		if (sshPubkeyTextSet)
		{
			selected.setSshPubKey(repeaterText.getText().toString());
			selected.setUseSshPubKey(true);
		}
		else
		{
			selected.setUseSshPubKey(false);
			selected.setSshPubKey("");
			selected.setSshPassPhrase("");
		}
		selected.setUserName(textUsername.getText().toString());
		selected.setForceFull(groupForceFullScreen.getCheckedRadioButtonId()==R.id.radioForceFullScreenAuto ? BitmapImplHint.AUTO : (groupForceFullScreen.getCheckedRadioButtonId()==R.id.radioForceFullScreenOn ? BitmapImplHint.FULL : BitmapImplHint.TILE));
		selected.setPassword(passwordText.getText().toString());
		selected.setKeepPassword(checkboxKeepPassword.isChecked());
		selected.setUseDpadAsArrows(checkboxUseDpadAsArrows.isChecked());
		selected.setRotateDpad(checkboxRotateDpad.isChecked());
		selected.setUsePortrait(checkboxUsePortrait.isChecked());
		selected.setUseLocalCursor(checkboxLocalCursor.isChecked());
		selected.setColorModel(((COLORMODEL)colorSpinner.getSelectedItem()).nameString());
		if (repeaterTextSet)
		{
			selected.setRepeaterId(repeaterText.getText().toString());
			selected.setUseRepeater(true);
		}
		else
		{
			selected.setUseRepeater(false);
		}
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
	
	void arriveOnPage() {
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
		IntroTextDialog.showIntroTextIfNecessary(this, database);
	}
	
	protected void onStop() {
		super.onStop();
		if ( selected == null ) {
			return;
		}
		updateSelectedFromView();

		// We need VNC server or SSH server to be filled out to save. Otherwise, we keep adding empty
		// connections when onStop gets called.
		// TODO: Switch away from numeric values for connection type.
		if (selected.getConnectionType() == 1 && selected.getSshServer().equals("") ||
			selected.getAddress().equals(""))
			return;
		
		saveAndWriteRecent();
	}
	
	VncDatabase getDatabaseHelper()
	{
		return database;
	}
	
	private void canvasStart() {
		if (selected == null) return;
		MemoryInfo info = Utils.getMemoryInfo(this);
		if (info.lowMemory) {
			// Low Memory situation.  Prompt.
			Utils.showYesNoPrompt(this, "Continue?", "Android reports low system memory.\nContinue with VNC connection?", new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface dialog, int which) {
					vnc();
				}
			}, null);
		} else
			vnc();
	}
	
	private void saveAndWriteRecent()
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
	
	private void vnc() {
		updateSelectedFromView();
		saveAndWriteRecent();
		Intent intent = new Intent(this, VncCanvasActivity.class);
		intent.putExtra(VncConstants.CONNECTION,selected.Gen_getValues());
		startActivity(intent);
	}
}
