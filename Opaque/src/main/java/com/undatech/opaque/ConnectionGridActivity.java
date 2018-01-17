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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.json.JSONException;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.Environment;
import android.widget.AdapterView;
import android.widget.GridView;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AdapterView.OnItemLongClickListener;

import com.undatech.opaque.R;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.opaque.util.FileUtils;

public class ConnectionGridActivity extends FragmentActivity {
	private static String TAG = "ConnectionGridActivity";
	private Context appContext;
	private GridView gridView;
	private String[] connectionPreferenceFiles;
	private String[] screenshotFiles;
	private String[] connectionLabels;
	
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		appContext = getApplicationContext();
		setContentView(R.layout.grid_view_activity);
 
		gridView = (GridView) findViewById(R.id.gridView);
		gridView.setOnItemClickListener(new OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
				SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
				String connections = sp.getString("connections", null);
				if (connections != null) {
					connectionPreferenceFiles = connections.split(" ");
				}
				
				Intent intent = new Intent(ConnectionGridActivity.this, RemoteCanvasActivity.class);
				if (connectionPreferenceFiles != null && position < connectionPreferenceFiles.length) {
					ConnectionSettings cs = new ConnectionSettings(connectionPreferenceFiles[position]);
					cs.loadFromSharedPreferences(appContext);
					intent.putExtra("com.undatech.opaque.ConnectionSettings", cs);
				}
				startActivity(intent);
			}
		});
		
		gridView.setOnItemLongClickListener(new OnItemLongClickListener() {
			@Override
			public boolean onItemLongClick(AdapterView<?> parent, View v, int position, long id) {
				SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
				String connections = sp.getString("connections", null);
				if (connections != null) {
					connectionPreferenceFiles = connections.split(" ");
				}
				Intent intent = new Intent(ConnectionGridActivity.this, ConnectionSetupActivity.class);
				if (connectionPreferenceFiles != null && position < connectionPreferenceFiles.length) {
					intent.putExtra("com.undatech.opaque.connectionToEdit", connectionPreferenceFiles[position]);
				}
				startActivity(intent);
				return true;
			}
		});
	}
	
	@Override
	public void onResume() {
		super.onResume();
		android.util.Log.e(TAG, "onResume");
		loadSavedConnections();
	}
	
	private void loadSavedConnections() {
		SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
		String connections = sp.getString("connections", null);
		android.util.Log.d(TAG, "Loading connections from this list: " + connections);
		if (connections != null && !connections.equals("")) {
			connectionPreferenceFiles = connections.split(" ");
			int numConnections = connectionPreferenceFiles.length;
			screenshotFiles = new String[numConnections];
			connectionLabels = new String[numConnections];
			for (int i = 0; i < numConnections; i++) {
				ConnectionSettings cs = new ConnectionSettings(connectionPreferenceFiles[i]);
				cs.loadFromSharedPreferences(appContext);
				connectionLabels[i] = cs.getVmname();
				android.util.Log.d(TAG, "Adding label: " + connectionLabels[i]);
				String location = cs.getFilename();
				screenshotFiles[i] = getFilesDir() + "/" + location + ".png";
			}
			
			int numCols = 1;
			if (numConnections > 2) {
				numCols = 2;
			}
			gridView.setNumColumns(numCols);
			gridView.setAdapter(new LabeledImageApapter(this, screenshotFiles, connectionLabels, numCols));
		} else {
			gridView.setAdapter(new LabeledImageApapter(this, null, null, 1));
		}
	}

	/**
	 * Linked with android:onClick to the add new connection action bar item.
	 * @param view
	 */
	public void addNewConnection (MenuItem menuItem) {
		Intent intent = new Intent(ConnectionGridActivity.this, ConnectionSetupActivity.class);
		startActivity(intent);
	}
	
    /**
     * Linked with android:onClick to the edit default settings action bar item.
     * @param view
     */
    public void editDefaultSettings (MenuItem menuItem) {
        Intent intent = new Intent(ConnectionGridActivity.this, AdvancedSettingsActivity.class);
        ConnectionSettings defaultConnection = new ConnectionSettings(Constants.DEFAULT_SETTINGS_FILE);
        defaultConnection.loadFromSharedPreferences(getApplicationContext());
        intent.putExtra("com.undatech.opaque.ConnectionSettings", defaultConnection);
        startActivityForResult(intent, Constants.DEFAULT_SETTINGS);
    }

    /**
     * Linked with android:onClick to the export settings action bar item.
     * @param view
     */
    public void exportSettings (MenuItem menuItem) {
        String pathToFile = FileUtils.join(Environment.getExternalStorageDirectory().toString(),
                                           Constants.EXPORT_SETTINGS_FILE);
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        FragmentManager fm = getSupportFragmentManager();
        try {
            ConnectionSettings.exportPrefsToFile(this, connections, pathToFile);
            MessageFragment message = MessageFragment.newInstance(getString(R.string.info_dialog_title),
                    "Exported settings to " + pathToFile, "OK", null);
            message.show(fm, "successExportingSettings");
        } catch (JSONException e) {
            MessageFragment message = MessageFragment.newInstance(getString(R.string.error_dialog_title),
                    "Could not convert settings to JSON", "OK", null);
            message.show(fm, "errorExportingSettings");
            e.printStackTrace();
        } catch (IOException e) {
            MessageFragment message = MessageFragment.newInstance(getString(R.string.error_dialog_title),
                    "Could write to settings file " + pathToFile, "OK", null);
            message.show(fm, "errorExportingSettings");
            e.printStackTrace();
        }
    }

    /**
     * Linked with android:onClick to the export settings action bar item.
     * @param view
     */
    public void importSettings (MenuItem menuItem) {
        String pathToFile = FileUtils.join(Environment.getExternalStorageDirectory().toString(),
                                           Constants.EXPORT_SETTINGS_FILE);
        FragmentManager fm = getSupportFragmentManager();

        try {
            String connections = ConnectionSettings.importPrefsFromFile(this, pathToFile);
            SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
            Editor editor = sp.edit();
            editor.putString("connections", connections);
            editor.apply();
            loadSavedConnections();
            MessageFragment message = MessageFragment.newInstance(getString(R.string.info_dialog_title),
                    "Imported settings from " + pathToFile, "OK", null);
            message.show(fm, "successImportingSettings");
        } catch (IOException e) {
            MessageFragment message = MessageFragment.newInstance(getString(R.string.info_dialog_title),
                    "Could not read settings settings file " + pathToFile, "OK", null);
            message.show(fm, "errorImportingSettings");
            e.printStackTrace();
        } catch (JSONException e) {
            MessageFragment message = MessageFragment.newInstance(getString(R.string.info_dialog_title),
                    "Could not parse JSON from settings file " + pathToFile, "OK", null);
            message.show(fm, "errorImportingSettings");
            e.printStackTrace();
        }

    }

    
    
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.grid_view_activity_actions, menu);
	    return super.onCreateOptionsMenu(menu);
	}
	
	@Override
	public boolean onOptionsItemSelected(MenuItem menuItem) {
		int itemID = menuItem.getItemId();
		switch (itemID) {
		case R.id.actionNewConnection:
			addNewConnection(menuItem);
			break;
        case R.id.actionEditDefaultSettings:
            editDefaultSettings(menuItem);
            break;
        case R.id.actionExportSettings:
            exportSettings(menuItem);
            break;
        case R.id.actionImportSettings:
            importSettings(menuItem);
            break;
		}
		return true;
	}
	
    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        android.util.Log.i(TAG, "onActivityResult");

        super.onActivityResult(requestCode, resultCode, data);
        switch(requestCode) {
        case (Constants.DEFAULT_SETTINGS):
            if (resultCode == Activity.RESULT_OK) {
                Bundle b = data.getExtras();
                ConnectionSettings defaultSettings = (ConnectionSettings)b.get("com.undatech.opaque.ConnectionSettings");
                defaultSettings.saveToSharedPreferences(this);
            } else {
                android.util.Log.i (TAG, "Error during AdvancedSettingsActivity.");
            }
            break;
        }
    }
}