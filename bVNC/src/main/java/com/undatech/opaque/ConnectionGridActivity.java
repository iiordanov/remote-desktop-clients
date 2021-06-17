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
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.appcompat.widget.AppCompatImageButton;

import android.text.ClipboardManager;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AdapterView.OnItemLongClickListener;
import android.widget.EditText;
import android.widget.GridView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.gson.internal.LinkedHashTreeMap;
import com.iiordanov.bVNC.App;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.iiordanov.bVNC.dialogs.ImportExportDialog;
import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.input.InputHandlerDirectSwipePan;

import com.trinity.android.apiclient.models.Node;
import com.trinity.android.apiclient.models.TunnelValues;
import com.trinity.android.apiclient.models.Action;
import com.trinity.android.apiclient.utils.ClientAPISettings;
import com.trinity.android.apiclient.utils.WireguardBaseRepository;

import com.undatech.opaque.util.ConnectionLoader;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.opaque.util.LogcatReader;
import com.iiordanov.util.PermissionsManager;

import java.io.File;
import java.util.ArrayList;
import java.util.UUID;

import com.undatech.remoteClientUi.R;

public class ConnectionGridActivity extends FragmentActivity implements GetTextFragment.OnFragmentDismissedListener {
    private static String TAG = "ConnectionGridActivity";
    FragmentManager fragmentManager = getSupportFragmentManager();
    private Context appContext;
    private GridView gridView;
    protected PermissionsManager permissionsManager;
    private ConnectionLoader connectionLoader;
    private EditText search;
    private boolean isConnecting = false;
    protected Database database;
    private boolean togglingMasterPassword = false;
    private boolean refreshMorpheuslyData = false;
    GetTextFragment getPassword = null;
    GetTextFragment getNewPassword = null;
    GetTextFragment morpheuslyCreds = null;
    protected boolean isStarting = true;
    private AppCompatImageButton addNewConnection = null;

    // Morpheusly Login Event Bus Function
    public void setLoginSuccessReceiver(Intent intent) {
        if (intent != null) {
            android.util.Log.i(TAG, "Login Success Receiver Triggered");
            Toast.makeText(App.getContext(), "Login success", Toast.LENGTH_SHORT).show();

            ClientAPISettings.getInstance(null).setCookie(intent.getStringExtra("cookie"));

            android.util.Log.i(TAG, "Login Success Relaunching Main Activity");
            ConnectionGridActivity.this.recreate();
        }
    }

    // Morpheusly Update Event Receiver
    public void setUpdateSuccessReceiver(Intent intent) {
        if (intent != null) {
            android.util.Log.i(TAG, "Update Success");

            ClientAPISettings.getInstance(null).setCookie(intent.getStringExtra("cookie"));
            loadSavedConnections();
        }
    }

    // Morpheusly Unauthorized Event Bus Function
    public void setUnauthorizedReceiver(Intent intent) {
        if (intent != null) {
            android.util.Log.i(TAG, "Unauthorized, clearing cookie.");
            Toast.makeText(App.getContext(), "Unauthorized, please re-login", Toast.LENGTH_LONG).show();
            removeTextFragments(null);

            ClientAPISettings.getInstance(null).setCookie(null);

            android.util.Log.i(TAG, "Unauthorized, Relaunching Main Activity");
            ConnectionGridActivity.this.recreate();

        }
    }

    // Morpheusly RefreshData Event Bus Function
    public void setRefreshData(Intent intent) {
        if (intent != null) {
            android.util.Log.i(TAG, "Reloading Actions Now.");
            Toast.makeText(App.getContext(), "Reloading Actions Now", Toast.LENGTH_LONG).show();
            loadSavedConnections();
        }
    }

    // Morpheusly StartURI Intent Receiver Event Bus Function
    public void setStartUriIntentReceiver(Intent intent) {
        if (intent != null) {
            android.util.Log.i(TAG, "Starting URI intent");
            String uriString = intent.getStringExtra("uriString");
            android.util.Log.d(TAG, "Starting URI intent, uriString: " + uriString);
            String uriType = intent.getStringExtra("type");
            android.util.Log.d(TAG, "Starting URI intent, uriType:" + uriType);
            String protocol = intent.getStringExtra("protocol");
            android.util.Log.d(TAG, "Starting URI intent, protocol: " + protocol);
            Intent newIntent = new Intent(Intent.ACTION_VIEW).setType(uriType)
                    .setData(Uri.parse(uriString));
            try {
                startActivity(newIntent);
            } catch (ActivityNotFoundException e) {
                android.util.Log.e(TAG, "User does not have app to handle " + protocol);
            }
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appContext = getApplicationContext();
        setContentView(R.layout.grid_view_activity);

        //Create appropriate Event Listeners
        App.clientAPISettings.getBus().subscribe(ClientAPISettings.LOGIN_SUCCESS, this, (data) -> {
            setLoginSuccessReceiver((Intent) data);
        });

        App.clientAPISettings.getBus().subscribe(ClientAPISettings.UPDATE_SUCCESS, this, (data) -> {
            setUpdateSuccessReceiver((Intent) data);
        });

        App.clientAPISettings.getBus().subscribe(ClientAPISettings.UNAUTHORIZED, this, (data) -> {
            setUnauthorizedReceiver((Intent) data);
        });

        App.clientAPISettings.getBus().subscribe(ClientAPISettings.REFRESH_DATA_RETRIEVED, this, (data) -> {
            setRefreshData((Intent) data);
        });

        App.clientAPISettings.getBus().subscribe(ClientAPISettings.START_URI_INTENT, this, (data) -> {
            setStartUriIntentReceiver((Intent) data);
        });

        if (ClientAPISettings.getInstance(null).getCookie() == null) {
            android.util.Log.d(TAG, "Cookie Not Set");
        }
        else {
            android.util.Log.d(TAG, "Cookie Set");
            com.trinity.android.apiclient.utils.Utils.update();
        }

        gridView = (GridView) findViewById(R.id.gridView);
        gridView.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                launchConnection(v);
            }
        });

        //Turn Off Edit Connections if they are auto generated from Morpheusly API
        if (ClientAPISettings.getInstance(null).getCookie()  == null) {
            gridView.setOnItemLongClickListener(new OnItemLongClickListener() {
                @Override
                public boolean onItemLongClick(AdapterView<?> parent, View v, int position, long id) {
                    AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(ConnectionGridActivity.this);
                    String gridItemText = (String) ((TextView) v.findViewById(R.id.grid_item_text)).getText();
                    alertDialogBuilder.setTitle(getString(R.string.connection_edit_delete_prompt) + " " + gridItemText + " ?");
                    CharSequence[] cs = {getString(R.string.connection_edit), getString(R.string.connection_delete)};
                    alertDialogBuilder.setItems(cs, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int item) {
                            if (cs[item].toString() == getString(R.string.connection_edit)) {
                                editConnection(v);
                            } else if (cs[item].toString() == getString(R.string.connection_delete)) {
                                deleteConnection(v);
                            }
                        }
                    });
                    AlertDialog alertDialog = alertDialogBuilder.create();
                    alertDialog.show();
                    return true;
                }
            });
        }
        permissionsManager = new PermissionsManager();
        permissionsManager.requestPermissions(ConnectionGridActivity.this, false);

        search = findViewById(R.id.search);
        search.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) { }
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override
            public void afterTextChanged(Editable s) {
                if (connectionLoader == null) {
                    return;
                }
                gridView.setNumColumns(2);
                gridView.setAdapter(new LabeledImageApapter(ConnectionGridActivity.this,
                        connectionLoader.getConnectionsById(),
                        search.getText().toString().toLowerCase().split(" "), 2));
            }
        });
        database = ((App)getApplication()).getDatabase();
        if (getPassword == null) {
            getPassword = GetTextFragment.newInstance(GetTextFragment.DIALOG_ID_GET_MASTER_PASSWORD,
                    getString(R.string.master_password_verify), this,
                    GetTextFragment.Password, R.string.master_password_verify_message,
                    R.string.master_password_set_error, null, null, null, false);
        }
        if (getNewPassword == null) {
            getNewPassword = GetTextFragment.newInstance(GetTextFragment.DIALOG_ID_GET_MATCHING_MASTER_PASSWORDS,
                    getString(R.string.master_password_set), this,
                    GetTextFragment.MatchingPasswordTwice, R.string.master_password_set_message,
                    R.string.master_password_set_error, null, null, null, false);
        }
        // Morpheusly Login Text Fragment
        if (morpheuslyCreds == null){
            morpheuslyCreds = GetTextFragment.newInstance(GetTextFragment.DIALOG_ID_GET_MORPH_CREDENTIALS, 
                    getString(R.string.morpheusly_credentials), this,
                    GetTextFragment.Credentials, R.string.morpheusly_credentials_set_message,
                    R.string.morpheusly_credentials_set_error, null, null, null, false);
        }

        FileUtils.logFilesInPrivateStorage(this);
        FileUtils.deletePrivateFileIfExisting(this, ".config/freerdp/licenses");

        addNewConnection = findViewById(R.id.addNewConnection);
        //Do not show addNewConnection option when viewing Morpheusly Actions
        if (ClientAPISettings.getInstance(null).getCookie() == null) {
            addNewConnection.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    addNewConnection();
                }
            });
        }
        else {
            addNewConnection.setVisibility(View.INVISIBLE);
        }
    }

    private void launchConnection(View v) {
        android.util.Log.i(TAG, "Launch Connection");

        ActivityManager.MemoryInfo info = Utils.getMemoryInfo(this);
        if (info.lowMemory)
            System.gc();

        isConnecting = true;
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        // If not associated with a Morpheusly Action assume normal operations
        if (connectionLoader.getConnectionsById().get(runtimeId).getAction() == null) {
            Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.iiordanov.bVNC.RemoteCanvasActivity"));
            if (Utils.isOpaque(getPackageName())) {
                ConnectionSettings cs = (ConnectionSettings) connectionLoader.getConnectionsById().get(runtimeId);
                cs.loadFromSharedPreferences(appContext);
                intent.putExtra("com.undatech.opaque.ConnectionSettings", cs);
            } else {
                ConnectionBean conn = (ConnectionBean) connectionLoader.getConnectionsById().get(runtimeId);
                intent.putExtra(Utils.getConnectionString(appContext), conn.Gen_getValues());
            }
            startActivity(intent);
        }
        else {
            // Connection associated with Morpheusly Action
            android.util.Log.d(TAG, "Stopping dead tunnels before starting action.");
            WireguardBaseRepository.stopAllWireguardTunnels();
            Action action = connectionLoader.getConnectionsById().get(runtimeId).getAction();
            int port = com.trinity.android.apiclient.utils.Utils.retrievePortForAction(action, ClientAPISettings.getInstance(null).getActions());
            if (port > 0) {
                TunnelValues tunnelValues = new TunnelValues(
                        UUID.randomUUID(),
                        port,
                        ClientAPISettings.getInstance(null).getTunnels().size(),
                        action.getToNodeName(),
                        action.getActionId(),
                        action.isPersistent()
                );
                ClientAPISettings.getInstance(null).getTunnels().add(tunnelValues);
                WireguardBaseRepository wireguardBaseRepository = new WireguardBaseRepository(tunnelValues, action, this);
                wireguardBaseRepository.start();
            }
        }

    }

    private void editConnection(View v) {
        android.util.Log.d(TAG, "Modify Connection");
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        Connection conn = connectionLoader.getConnectionsById().get(runtimeId);
        Intent intent = new Intent(ConnectionGridActivity.this, Utils.getConnectionSetupClass(getPackageName()));
        if (Utils.isOpaque(getPackageName())) {
            ConnectionSettings cs = (ConnectionSettings) connectionLoader.getConnectionsById().get(runtimeId);
            intent.putExtra("com.undatech.opaque.connectionToEdit", cs.getFilename());

        }
        else {
            intent.putExtra("isNewConnection", false);
            intent.putExtra("connID", conn.getId());
        }
        startActivity(intent);
    }

    private void deleteConnection(View v) {
        android.util.Log.d(TAG, "Delete Connection");
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        String gridItemText = (String) ((TextView) v.findViewById(R.id.grid_item_text)).getText();
        Utils.showYesNoPrompt(this, getString(R.string.delete_connection) + "?", getString(R.string.delete_connection) + " " + gridItemText+ " ?",
                new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int i) {
                if (Utils.isOpaque(getPackageName())) {

                    String newListOfConnections = new String();

                    SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
                    String currentConnectionsStr = sp.getString("connections", null);

                    ConnectionSettings cs = (ConnectionSettings) connectionLoader.getConnectionsById().get(runtimeId);
                    if (sp != null) {
                        String [] currentConnections = currentConnectionsStr.split(" ");
                        for (String connection : currentConnections) {
                            if (!connection.equals(cs.getFilename())) {
                                newListOfConnections += " " + connection;
                            }
                        }
                        android.util.Log.d(TAG, "Deleted connection, current list: " + newListOfConnections);
                        Editor editor = sp.edit();
                        editor.putString("connections", newListOfConnections.trim());
                        editor.apply();
                        File toDelete = new File (getFilesDir() + "/" + cs.getFilename() + ".png");
                        toDelete.delete();
                    }
                }
                else {
                    ConnectionBean conn = (ConnectionBean) connectionLoader.getConnectionsById().get(runtimeId);
                    conn.Gen_delete(database.getWritableDatabase());
                    database.close();
                }
                onResume();
            }
        }, null);
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.i(TAG, "onResume of version " + Utils.getVersionAndCode(this));
        refreshMorpheuslyOnResume();
        if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
            showGetTextFragment(getPassword);
        } else {
            // TODO: ASK WHY ARE WE LOADING CONNECTIONS TWICE onResume() and onResumeFragments()
            if (ClientAPISettings.getInstance(null).getCookie() == null || ClientAPISettings.getInstance(null).getCookie() != null && ClientAPISettings.getInstance(null).getApiClient().proxyClientService != null) {
                loadSavedConnections();
            }
            IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && isStarting);
        }
        destroyUnreferencedResourcesOnResume();
        isStarting = false;
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");
        if (database != null)
            database.close();
    }

    @Override
    protected void onResumeFragments() {
        Log.i(TAG, "onResumeFragments called");
        super.onResumeFragments();
        System.gc();
        if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
            showGetTextFragment(getPassword);
        }
        else if (ClientAPISettings.getInstance(null).getCookie() == null) {
                loadSavedConnections();
        }
    }
    
    private void loadSavedConnections() {
        boolean connectionsInSharedPrefs = Utils.isOpaque(getPackageName());
        connectionLoader = new ConnectionLoader(appContext, this, connectionsInSharedPrefs,
                ClientAPISettings.getInstance(null).getCookie() );
        if (connectionLoader.getNumConnections() > 0) {
            gridView.setNumColumns(2);
            gridView.setAdapter(new LabeledImageApapter(this,
                    connectionLoader.getConnectionsById(),
                    search.getText().toString().toLowerCase().split(" "), 2));
        } else {
            gridView.setAdapter(new LabeledImageApapter(this,
                    null,
                    search.getText().toString().toLowerCase().split(" "), 2));
        }
    }

    public void destroyUnreferencedResourcesOnResume(){
        System.gc();
    }

    /**
     * Starts a new connection.
     */
    public void addNewConnection () {
        Intent intent = new Intent(ConnectionGridActivity.this,
                Utils.getConnectionSetupClass(getPackageName()));
        intent.putExtra("isNewConnection", true);
        startActivity(intent);
    }

    /**
     * Linked with android:onClick to the add new connection action bar item.
     */
    public void addNewConnection (MenuItem menuItem) {
        addNewConnection();
    }

    /**
     * Linked with android:onClick to the add new connection item in the activity.
     */
    public void addNewConnection(View view) {
        addNewConnection();
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
        android.util.Log.d(TAG, "editDefaultSettings selected.");
        if (Utils.isOpaque(getPackageName())) {
            Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.undatech.opaque.AdvancedSettingsActivity"));
            ConnectionSettings defaultConnection = new ConnectionSettings(RemoteClientLibConstants.DEFAULT_SETTINGS_FILE);
            defaultConnection.loadFromSharedPreferences(getApplicationContext());
            intent.putExtra("com.undatech.opaque.ConnectionSettings", defaultConnection);
            startActivityForResult(intent, RemoteClientLibConstants.DEFAULT_SETTINGS);
        } else {
            Intent intent = new Intent();
            intent.setClassName(this, "com.iiordanov.bVNC.GlobalPreferencesActivity");
            startActivity(intent);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.grid_view_activity_actions, menu);
        inflater.inflate(R.menu.input_mode_menu_item, menu);

        MenuItem itemSignIn = menu.findItem(R.id.signInMorpheusly);
        MenuItem itemSignOut = menu.findItem(R.id.signOutMorpheusly);
        MenuItem itemNewConnection = menu.findItem(R.id.actionNewConnection);
        MenuItem itemWgTunnels = menu.findItem(R.id.shutdownWireguard);

        if( ClientAPISettings.getInstance(null).getCookie() != null && (Utils.isRdp(getPackageName()) || Utils.isVnc(getPackageName()))){
            itemSignIn.setVisible(false);
            itemSignOut.setVisible(true);
            itemNewConnection.setVisible(false);
        }
        else if ( ClientAPISettings.getInstance(null).getCookie() == null && (Utils.isRdp(getPackageName()) || Utils.isVnc(getPackageName())) ) {
            itemSignIn.setVisible(true);
            itemSignOut.setVisible(false);
            itemNewConnection.setVisible(true);
        }
        else {
            itemSignIn.setVisible(true);
            itemSignOut.setVisible(true);
            itemNewConnection.setVisible(true);
        }

        if (!WireguardBaseRepository.tunnelsRunning()) {
            itemWgTunnels.setVisible(false);
        }
        else {
            itemWgTunnels.setVisible(true);
        }
        return super.onCreateOptionsMenu(menu);
    }


    /* (non-Javadoc)
     * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
     */
    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        android.util.Log.d(TAG, "onMenuOpened");
        try {
            updateInputMenu(menu.findItem(R.id.itemInputMode).getSubMenu());
            MenuItem itemMasterPassword = menu.findItem(R.id.itemMasterPassword);
            itemMasterPassword.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag));
        } catch (NullPointerException e) {}
        return true;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onOptionsItemSelected(android.view.MenuItem)
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.itemExportImport) {
            permissionsManager.requestPermissions(ConnectionGridActivity.this, true);
            showDialog(R.layout.importexport);
        } else if (itemId == R.id.itemMasterPassword) {
            if (Utils.isFree(this)) {
                IntroTextDialog.showIntroTextIfNecessary(this, database, true);
            } else {
                togglingMasterPassword = true;
                if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
                    showGetTextFragment(getPassword);
                } else {
                    showGetTextFragment(getNewPassword);
                }
            }
        } else if (item.getGroupId() == R.id.itemInputModeGroup) {
            Log.d(TAG, RemoteCanvasActivity.inputModeMap.get(item.getItemId()));
            Utils.setSharedPreferenceString(this, Constants.defaultInputMethodTag,
                    RemoteCanvasActivity.inputModeMap.get(item.getItemId()));
        }
        return true;
    }

    /**
     * Check the right item in the input mode sub-menu
     */
    void updateInputMenu(Menu inputMenu) {
        MenuItem[] inputModeMenuItems = new MenuItem[RemoteCanvasActivity.inputModeIds.length];
        for (int i = 0; i < RemoteCanvasActivity.inputModeIds.length; i++) {
            inputModeMenuItems[i] = inputMenu.findItem(RemoteCanvasActivity.inputModeIds[i]);
        }
        String defaultInputHandlerId = Utils.querySharedPreferenceString(
                this, Constants.defaultInputMethodTag, InputHandlerDirectSwipePan.ID);
        android.util.Log.d(TAG, "Default Input Mode Item: " + defaultInputHandlerId);

        try {
            for (MenuItem item : inputModeMenuItems) {
                android.util.Log.d(TAG, "Input Mode Item: " +
                        RemoteCanvasActivity.inputModeMap.get(item.getItemId()));

                if (defaultInputHandlerId.equals(RemoteCanvasActivity.inputModeMap.get(item.getItemId()))) {
                    item.setChecked(true);
                }
            }
        } catch (NullPointerException e) { }
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



    /* (non-Javadoc)
     * @see android.app.Activity#onCreateDialog(int)
     */
    @Override
    protected Dialog onCreateDialog(int id) {
        if (id == R.layout.importexport) {
            boolean connectionsInSharedPrefs = Utils.isOpaque(getPackageName());
            return new ImportExportDialog(this, database, connectionsInSharedPrefs);
        }
        return null;
    }

    private boolean checkMasterPassword (String password) {
        Log.i(TAG, "Checking master password.");
        boolean result = false;

        Database testPassword = new Database(this);
        testPassword.close();
        try {
            testPassword.getReadableDatabase(password);
            result = true;
        } catch (Exception e) {
            result = false;
        }
        testPassword.close();
        return result;
    }

    public void onTextObtained(String dialogId, String[] obtainedStrings, boolean wasCancelled, boolean keep) {
        Log.d(TAG, "onTextObtained was called, status of wasCancelled: " + wasCancelled +
                ", status of dialogId: " + dialogId);
        if (dialogId == GetTextFragment.DIALOG_ID_GET_MORPH_CREDENTIALS) {
            handleMorpheuslyLogin(obtainedStrings[0], obtainedStrings[1], wasCancelled);
        }
        else {
            handlePassword(obtainedStrings[0], wasCancelled);
        }
    }

    public void handleMorpheuslyLogin(String username, String password, Boolean wasCancelled){
        Log.d(TAG, "handleMorpheuslyLogin was called");
        if (!wasCancelled) {
            android.util.Log.d(TAG, "Attempting command node registration");
            registerMorpheuslyCommandNode(username, password);
        }
        removeTextFragments(morpheuslyCreds);
    }

    public void signOutMorpheusly(MenuItem item){
        Log.d(TAG, "signOutMorpheusly was called");

        AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(ConnectionGridActivity.this);
        alertDialogBuilder.setTitle(getString(R.string.morpheusly_verify_signout) + " ?");
        CharSequence[] cs = {getString(R.string.morpheusly_verify_signout_confirm), getString(R.string.morpheusly_verify_signout_cancel)};
        alertDialogBuilder.setNegativeButton(cs[1], new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int item) {
                Log.d(TAG, "signOutMorpheusly: was cancelled");
            }
        });
        alertDialogBuilder.setPositiveButton(cs[0], new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int item) {
                Log.d(TAG, "signOutMorpheusly: was confirmed");
                Log.d(TAG, "signOutMorpheusly: Signing out of Morpheusly Labs");

                WireguardBaseRepository.stopAllWireguardTunnels();
                com.trinity.android.apiclient.utils.Utils.deregister(ClientAPISettings.getInstance(null).getNodeId(), true);

                ClientAPISettings.getInstance(null).setActions(new LinkedHashTreeMap<String, Action>());
                ClientAPISettings.getInstance(null).setActionsLastRetrieved(0);

                ClientAPISettings.getInstance(null).setNodes(new LinkedHashTreeMap<String, Node>());
                ClientAPISettings.getInstance(null).setNodesLastRetrieved(0);

                ClientAPISettings.getInstance(null).setCookie(null);
                ConnectionGridActivity.this.recreate();
            }
        });
        AlertDialog alertDialog = alertDialogBuilder.create();
        alertDialog.show();
    }

    public void shutdownWireguard(MenuItem item){
        Log.d(TAG, "shutdownWireguard was called");
        Log.d(TAG, "shutdownWireguard: Shutting down Wireguard Tunnel");
        WireguardBaseRepository.stopAllWireguardTunnels();
        ConnectionGridActivity.this.recreate();
    }

    public void handlePassword(String providedPassword, boolean wasCancelled) {
        if (togglingMasterPassword) {
            Log.i(TAG, "Asked to toggle master pasword.");
            // The user has requested the password to be enabled or disabled.
            togglingMasterPassword = false;
            if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
                Log.i(TAG, "Master password is enabled.");
                // Master password is enabled
                if (wasCancelled) {
                    Log.i(TAG, "Dialog cancelled, so quitting.");
                    Utils.showFatalErrorMessage(this, getResources().getString(R.string.master_password_error_password_necessary));
                } else if (checkMasterPassword(providedPassword)) {
                    Log.i(TAG, "Entered password correct, disabling password.");
                    // Disable the password since the user input the correct password.
                    Database.setPassword(providedPassword);
                    if (database.changeDatabasePassword("")) {
                        Utils.toggleSharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag);
                    } else {
                        Utils.showErrorMessage(this, getResources().getString(R.string.master_password_error_failed_to_disable));
                    }
                    removeTextFragments(null);
                    loadSavedConnections();
                } else {
                    Log.i(TAG, "Entered password is wrong or dialog cancelled, so quitting.");
                    Utils.showFatalErrorMessage(this, getResources().getString(R.string.master_password_error_wrong_password));
                }
            } else {
                Log.i(TAG, "Master password is disabled.");
                if (!wasCancelled) {
                    // The password is disabled, so set it in the preferences.
                    Log.i(TAG, "Setting master password.");
                    Database.setPassword("");
                    if (database.changeDatabasePassword(providedPassword)) {
                        Utils.toggleSharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag);
                    } else {
                        Utils.showErrorMessage(this, getResources().getString(R.string.master_password_error_failed_to_enable));
                    }
                } else {
                    // No need to show error message because user cancelled consciously.
                    Log.i(TAG, "Dialog cancelled, not setting master password.");
                    Utils.showErrorMessage(this, getResources().getString(R.string.master_password_error_password_not_set));
                }
                removeTextFragments(null);
                loadSavedConnections();
            }
        } else {
            // We are just trying to check the password.
            Log.i(TAG, "Just checking the password.");
            if (wasCancelled) {
                Log.i(TAG, "Dialog cancelled, so quitting.");
                Utils.showFatalErrorMessage(this, getResources().getString(R.string.master_password_error_password_necessary));
            } else if (checkMasterPassword(providedPassword)) {
                Log.i(TAG, "Entered password is correct, so proceeding.");
                Database.setPassword(providedPassword);
                removeTextFragments(null);
                loadSavedConnections();
            } else {
                // Finish the activity if the password was wrong.
                Log.i(TAG, "Entered password is wrong, so quitting.");
                Utils.showFatalErrorMessage(this, getResources().getString(R.string.master_password_error_wrong_password));
            }
        }
    }

    private void showGetTextFragment(GetTextFragment f) {
        if (!f.isVisible()) {
            removeTextFragments(null);
            f.setCancelable(false);
            f.show(fragmentManager, "");
        }
    }

    private void removeTextFragments(GetTextFragment excludeTextFragment) {
        if (getPassword.isAdded() && excludeTextFragment != getPassword) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getPassword);
            tx.commit();
            fragmentManager.executePendingTransactions();
        }
        if (getNewPassword.isAdded() && excludeTextFragment != getNewPassword) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getNewPassword);
            tx.commit();
            fragmentManager.executePendingTransactions();
        }
        if (morpheuslyCreds.isAdded() && excludeTextFragment != morpheuslyCreds) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(morpheuslyCreds);
            tx.commit();
            fragmentManager.executePendingTransactions();
        }
    }

    public void showMainScreenHelp(MenuItem item) {
        Log.d(TAG, "Showing main screen help.");
        Utils.createMainScreenDialog(this);
    }

    public void showSupportForum(MenuItem item) {
        Log.d(TAG, "Showing support forum.");
        String url = "https://groups.google.com/forum/#!forum/bvnc-ardp-aspice-opaque-remote-desktop-clients";
        Intent i = new Intent(Intent.ACTION_VIEW);
        i.setData(Uri.parse(url));
        startActivity(i);
    }

    public void reportBug(MenuItem item) {
        Log.d(TAG, "Showing report bug page.");
        String url = "https://github.com/iiordanov/remote-desktop-clients/issues";
        Intent i = new Intent(Intent.ACTION_VIEW);
        i.setData(Uri.parse(url));
        startActivity(i);
    }

    public void signIntoMorpheuslyLabs(MenuItem item) {
        Log.d(TAG, "signIntoMorpheuslyLabs: Showing login prompt for Morpheusly Labs.");
        showGetTextFragment(morpheuslyCreds);
    }

    private void registerMorpheuslyCommandNode(String username, String password) {
        android.util.Log.d(TAG, "registerMorpheuslyCommandNode: Performing registration");
        com.trinity.android.apiclient.utils.Utils.register(username, password);
    }

    public void refreshMorpheuslyOnResume(){
        android.util.Log.d(TAG, "refreshMorpheuslyOnResume: Checking if apiClient should be refreshed");
        boolean refreshApiClient = com.trinity.android.apiclient.utils.Utils.checkAndRefreshApiClient();
        android.util.Log.d(TAG, "refreshMorpheuslyOnResume: apiClient refresh result is " + refreshApiClient);
    }
}
