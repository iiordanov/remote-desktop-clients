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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.ClipboardManager;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.GridView;
import android.widget.Toast;

import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.bVNC;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.opaque.util.ConnectionLoader;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.opaque.util.LogcatReader;
import com.undatech.opaque.util.PermissionsManager;
import org.json.JSONException;
import java.io.IOException;
import com.undatech.remoteClientUi.R;

public class ConnectionGridActivity extends FragmentActivity {
    private static String TAG = "ConnectionGridActivity";
    private Context appContext;
    private GridView gridView;
    private String[] connectionPreferenceFiles;
    protected PermissionsManager permissionsManager;
    private ConnectionLoader connectionLoader;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appContext = getApplicationContext();
        setContentView(R.layout.grid_view_activity);
 
        gridView = (GridView) findViewById(R.id.gridView);
        gridView.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                launchConnection(position);
            }
        });
        
        gridView.setOnItemLongClickListener(new OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View v, int position, long id) {
                editConnection(position);
                return true;
            }
        });
        permissionsManager = new PermissionsManager();
        permissionsManager.requestPermissions(ConnectionGridActivity.this);
    }

    private void launchConnection(int position) {
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        if (connections != null) {
            connectionPreferenceFiles = connections.split(" ");
        }

        Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.iiordanov.bVNC.RemoteCanvasActivity"));
        if (connectionPreferenceFiles != null && position < connectionPreferenceFiles.length) {
            ConnectionSettings cs = new ConnectionSettings(connectionPreferenceFiles[position]);
            cs.loadFromSharedPreferences(appContext);
            intent.putExtra("com.undatech.opaque.ConnectionSettings", cs);
        }
        startActivity(intent);

    }

    private void editConnection(int position) {
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        if (connections != null) {
            connectionPreferenceFiles = connections.split(" ");
        }
        Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.undatech.opaque.ConnectionSetupActivity"));
        if (connectionPreferenceFiles != null && position < connectionPreferenceFiles.length) {
            intent.putExtra("com.undatech.opaque.connectionToEdit", connectionPreferenceFiles[position]);
        }
        startActivity(intent);
    }

    @Override
    public void onResume() {
        super.onResume();
        android.util.Log.e(TAG, "onResume");
        loadSavedConnections();
    }
    
    private void loadSavedConnections() {
        connectionLoader = new ConnectionLoader(appContext, this, getSupportFragmentManager(), Utils.isOpaque(getPackageName()));
        if (connectionLoader.getNumConnections() > 0) {
            int numCols = 1;
            if (connectionLoader.getNumConnections() > 2) {
                numCols = 2;
            }
            gridView.setNumColumns(numCols);
            gridView.setAdapter(new LabeledImageApapter(this, connectionLoader.getScreenshotFiles(), connectionLoader.getConnectionLabels(), numCols));
        } else {
            gridView.setAdapter(new LabeledImageApapter(this, null, null, 1));
        }
    }

    /**
     * Linked with android:onClick to the add new connection action bar item.
     * @param menuItem
     */
    public void addNewConnection (MenuItem menuItem) {
        Intent intent = new Intent(ConnectionGridActivity.this,
                Utils.getConnectionSetupClass(getPackageName()));
        intent.putExtra("AddNewConnection", true);
        startActivity(intent);
    }

    /**
     * Linked with android:onClick to the copyLogcat action bar item.
     * @param menuItem
     */
    public void copyLogcat (MenuItem menuItem) {
        LogcatReader logcatReader = new LogcatReader();
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setText(logcatReader.getMyLogcat(RemoteClientLibConstants.LOGCAT_MAX_LINES));
        Toast.makeText(getBaseContext(), getResources().getString(R.string.log_copied),
                Toast.LENGTH_LONG).show();
    }

    /**
     * Linked with android:onClick to the edit default settings action bar item.
     * @param menuItem
     */
    public void editDefaultSettings (MenuItem menuItem) {
        Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.undatech.opaque.AdvancedSettingsActivity"));
        ConnectionSettings defaultConnection = new ConnectionSettings(RemoteClientLibConstants.DEFAULT_SETTINGS_FILE);
        defaultConnection.loadFromSharedPreferences(getApplicationContext());
        intent.putExtra("com.undatech.opaque.ConnectionSettings", defaultConnection);
        startActivityForResult(intent, RemoteClientLibConstants.DEFAULT_SETTINGS);
    }

    /**
     * Linked with android:onClick to the export settings action bar item.
     * @param menuItem
     */
    public void exportSettings (MenuItem menuItem) {
        connectionLoader.exportSettings();
    }

    /**
     * Linked with android:onClick to the export settings action bar item.
     * @param menuItem
     */
    public void importSettings (MenuItem menuItem) {
        connectionLoader.importSettings();
        loadSavedConnections();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.grid_view_activity_actions, menu);
        return super.onCreateOptionsMenu(menu);
    }
    
    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        android.util.Log.i(TAG, "onActivityResult");

        super.onActivityResult(requestCode, resultCode, data);
        switch(requestCode) {
        case (RemoteClientLibConstants.DEFAULT_SETTINGS):
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
