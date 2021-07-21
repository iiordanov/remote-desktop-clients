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
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
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
import com.undatech.opaque.util.ConnectionLoader;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.opaque.util.LogcatReader;
import com.iiordanov.util.PermissionsManager;

import java.io.File;

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
    GetTextFragment getPassword = null;
    GetTextFragment getNewPassword = null;
    protected boolean isStarting = true;
    private AppCompatImageButton addNewConnection = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appContext = getApplicationContext();
        setContentView(R.layout.grid_view_activity);

        gridView = (GridView) findViewById(R.id.gridView);
        gridView.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                launchConnection(v);
            }
        });
        gridView.setOnItemLongClickListener(new OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> parent, View v, int position, long id) {
                AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(ConnectionGridActivity.this);
                String gridItemText = (String) ((TextView) v.findViewById(R.id.grid_item_text)).getText();
                alertDialogBuilder.setTitle(getString(R.string.connection_edit_delete_prompt) + " " + gridItemText + " ?");
                CharSequence [] cs = {getString(R.string.connection_edit), getString(R.string.connection_delete)};
                alertDialogBuilder.setItems(cs, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int item) {
                        if (cs[item].toString() == getString(R.string.connection_edit)) {
                            editConnection(v);
                        }
                        else if (cs[item].toString() == getString(R.string.connection_delete)) {
                            deleteConnection(v);
                        }
                    }
                });
                AlertDialog alertDialog = alertDialogBuilder.create();
                alertDialog.show();
                return true;
            }
        });
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
        FileUtils.logFilesInPrivateStorage(this);
        FileUtils.deletePrivateFileIfExisting(this, ".config/freerdp/licenses");
        addNewConnection = findViewById(R.id.addNewConnection);
        addNewConnection.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                addNewConnection();
            }
        });
    }

    private void launchConnection(View v) {
        android.util.Log.i(TAG, "Launch Connection");

        ActivityManager.MemoryInfo info = Utils.getMemoryInfo(this);
        if (info.lowMemory)
            System.gc();

        isConnecting = true;
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.iiordanov.bVNC.RemoteCanvasActivity"));
        if (Utils.isOpaque(getPackageName())) {
            ConnectionSettings cs = (ConnectionSettings) connectionLoader.getConnectionsById().get(runtimeId);
            cs.loadFromSharedPreferences(appContext);
            intent.putExtra("com.undatech.opaque.ConnectionSettings", cs);
        }
        else{
            ConnectionBean conn = (ConnectionBean) connectionLoader.getConnectionsById().get(runtimeId);
            intent.putExtra(Utils.getConnectionString(appContext), conn.Gen_getValues());
        }
        startActivity(intent);

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
        if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
            showGetTextFragment(getPassword);
        } else {
            loadSavedConnections();
            IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && isStarting);
        }
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
        } else {
            loadSavedConnections();
        }
    }
    
    private void loadSavedConnections() {
        boolean connectionsInSharedPrefs = Utils.isOpaque(getPackageName());
        connectionLoader = new ConnectionLoader(appContext, this, connectionsInSharedPrefs);
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
        handlePassword(obtainedStrings[0], wasCancelled);
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
                    removeGetPasswordFragments();
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
                removeGetPasswordFragments();
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
                removeGetPasswordFragments();
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
            removeGetPasswordFragments();
            f.setCancelable(false);
            f.show(fragmentManager, "");
        }
    }

    private void removeGetPasswordFragments() {
        if (getPassword.isAdded()) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getPassword);
            tx.commit();
            fragmentManager.executePendingTransactions();
        }
        if (getNewPassword.isAdded()) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getNewPassword);
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
}
