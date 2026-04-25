/**
 * Copyright (C) 2013- Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static com.iiordanov.bVNC.Constants.CONNECTION_TO_EDIT_INTENT_KEY;
import static com.iiordanov.bVNC.Utils.createMainScreenDialog;
import static com.iiordanov.bVNC.Utils.setClipboard;
import static com.iiordanov.bVNC.Utils.startUriIntent;
import static com.undatech.opaque.RemoteClientLibConstants.DEFAULT_SETTINGS_REQUEST_CODE;
import static com.undatech.opaque.RemoteClientLibConstants.LAUNCH_CONNECTION_REQUEST_CODE;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.net.Uri;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.GridView;
import android.widget.PopupMenu;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.AppCompatImageButton;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import com.google.android.material.snackbar.Snackbar;
import com.iiordanov.bVNC.App;
import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.RemoteCanvasActivity;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.dialogs.DiscoveryBottomSheet;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.iiordanov.bVNC.dialogs.ImportExportDialog;
import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.dialogs.NetworkDiscovery;
import com.iiordanov.bVNC.dialogs.RateOrShareFragment;
import com.iiordanov.permissions.BatteryOptimizationDisabler;
import com.iiordanov.util.MasterPasswordDelegate;
import com.undatech.opaque.util.ConnectionLoader;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.opaque.util.LogcatReader;
import com.undatech.remoteClientUi.R;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class ConnectionGridActivity extends AppCompatActivity implements GetTextFragment.OnFragmentDismissedListener {
    private static final String TAG = "ConnectionGridActivity";
    protected Database database;
    protected boolean isStarting = true;
    FragmentManager fragmentManager = getSupportFragmentManager();
    GetTextFragment getPassword = null;
    GetTextFragment getNewPassword = null;
    private Context appContext;
    private GridView gridView;
    private EditText search;
    private boolean togglingMasterPassword = false;
    private View addNewConnection = null;
    private AppCompatImageButton popUpMenuButton = null;
    private AppCompatImageButton sortButton = null;

    private enum SortOrder {
        NONE, ASC, DESC;

        SortOrder next() {
            SortOrder[] v = values();
            return v[(ordinal() + 1) % v.length];
        }

        static SortOrder fromString(String s) {
            try {
                return valueOf(s);
            } catch (IllegalArgumentException e) {
                return NONE;
            }
        }
    }

    private SortOrder sortOrder = SortOrder.NONE;

    private final RateOrShareFragment rateOrShareFragment = new RateOrShareFragment();
    private boolean showMasterPasswordDialog = true;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Utils.showActionBarWithTitle(this);
        appContext = getApplicationContext();
        setContentView(R.layout.grid_view_activity);

        gridView = findViewById(R.id.gridView);
        gridView.setEmptyView(findViewById(R.id.emptyState));
        gridView.setOnItemClickListener((parent, v, position, id) -> launchConnection(v));
        gridView.setOnItemLongClickListener((parent, v, position, id) -> {
            AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(ConnectionGridActivity.this);
            String gridItemText = (String) ((TextView) v.findViewById(R.id.grid_item_text)).getText();
            alertDialogBuilder.setTitle(getString(R.string.connection_edit_delete_prompt) + " " + gridItemText + " ?");
            CharSequence[] cs = {getString(R.string.connection_edit), getString(R.string.connection_delete)};
            alertDialogBuilder.setItems(cs, (dialog, item) -> {
                if (getString(R.string.connection_edit).equals(cs[item].toString())) {
                    editConnection(v);
                } else if (getString(R.string.connection_delete).equals(cs[item].toString())) {
                    deleteConnection(v);
                }
            });
            AlertDialog alertDialog = alertDialogBuilder.create();
            alertDialog.show();
            return true;
        });
        new BatteryOptimizationDisabler(this, gridView).requestBatteryOptimizationExemptionAutomaticallyOnce();

        search = findViewById(R.id.search);
        search.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                createAndSetLabeledImageAdapterAndNumberOfColumns();
            }
        });
        database = ((App) getApplication()).getDatabase();
        if (getPassword == null) {
            getPassword = GetTextFragment.newInstance(GetTextFragment.DIALOG_ID_GET_MASTER_PASSWORD,
                    getString(R.string.master_password_verify), this,
                    GetTextFragment.PasswordNoKeep, R.string.master_password_verify_message,
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
        addNewConnection.setOnClickListener(v -> addNewConnection());
        popUpMenuButton = findViewById(R.id.popUpMenuButton);
        if (popUpMenuButton != null) {
            popUpMenuButton.setOnClickListener(v -> popUpMenu());
        }
        sortButton = findViewById(R.id.sortButton);
        if (sortButton != null) {
            sortOrder = loadSortOrder();
            sortButton.setOnClickListener(v -> cycleSortState());
            updateSortIcon();
        }
    }

    private ConnectionLoader getConnectionLoader(Context context) {
        boolean connectionsInSharedPrefs = Utils.isOpaque(context);
        return new ConnectionLoader(appContext, connectionsInSharedPrefs);
    }

    private void createAndSetLabeledImageAdapterAndNumberOfColumns() {
        LabeledImageApapter labeledImageApapter = new LabeledImageApapter(
                ConnectionGridActivity.this,
                sortConnections(getConnectionLoader(this).loadConnectionsById()),
                search.getText().toString().toLowerCase().split(" "),
                2);
        gridView.setAdapter(labeledImageApapter);
        gridView.setNumColumns(labeledImageApapter.getNumCols());
    }

    private Map<String, Connection> sortConnections(Map<String, Connection> connections) {
        if (sortOrder == SortOrder.NONE) return connections;
        List<Connection> list = new ArrayList<>(connections.values());
        Collections.sort(list, (a, b) -> {
            String la = a.getLabel();
            String lb = b.getLabel();
            return sortOrder == SortOrder.ASC ? la.compareToIgnoreCase(lb) : lb.compareToIgnoreCase(la);
        });
        Map<String, Connection> sorted = new LinkedHashMap<>();
        for (int i = 0; i < list.size(); i++) {
            sorted.put(String.valueOf(i), list.get(i));
        }
        return sorted;
    }

    private void cycleSortState() {
        sortOrder = sortOrder.next();
        saveSortOrder(sortOrder);
        updateSortIcon();
        createAndSetLabeledImageAdapterAndNumberOfColumns();
    }

    private void updateSortIcon() {
        int iconRes;
        if (sortOrder == SortOrder.ASC) {
            iconRes = R.drawable.ic_sort_asc_24;
        } else if (sortOrder == SortOrder.DESC) {
            iconRes = R.drawable.ic_sort_desc_24;
        } else {
            iconRes = R.drawable.ic_sort_24;
        }
        sortButton.setImageResource(iconRes);
    }

    private SortOrder loadSortOrder() {
        return SortOrder.fromString(
                Utils.querySharedPreferenceString(appContext, Constants.PREF_SORT_ORDER, SortOrder.NONE.name()));
    }

    private void saveSortOrder(SortOrder order) {
        Utils.setSharedPreferenceString(appContext, Constants.PREF_SORT_ORDER, order.name());
    }

    private void launchConnection(View v) {
        Utils.hideKeyboard(this, getCurrentFocus());
        Log.i(TAG, "Launch Connection");

        ActivityManager.MemoryInfo info = Utils.getMemoryInfo(this);
        if (info.lowMemory)
            System.gc();

        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        Intent intent = new IntentHelper().getIntent(
                getConnectionLoader(this),
                runtimeId,
                appContext,
                this
        );
        try {
            startActivityForResult(intent, LAUNCH_CONNECTION_REQUEST_CODE);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Error launching connection: " + e);
            Snackbar.make(gridView, R.string.no_application_to_handle_vpn, Snackbar.LENGTH_LONG).show();
            startUriIntent(this, "market://search?q=pub:\"Morpheusly\"");
        }
    }

    private void editConnection(View v) {
        Log.d(TAG, "editConnection - Modifying an existing connection");
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        Intent intent = new Intent(ConnectionGridActivity.this, Utils.getConnectionSetupClass(this));
        intent.putExtra("isNewConnection", false);
        if (Utils.isOpaque(this)) {
            editOpaqueConnection(intent, runtimeId);
        } else {
            editConnection(intent, runtimeId);
        }
        startActivity(intent);
    }

    private void editConnection(Intent intent, String runtimeId) {
        Connection connection = getConnectionLoader(this).getConnectionsById().get(runtimeId);
        if (connection != null) {
            Log.d(TAG, "editConnection - Editing non-Opaque connection with ID: " + connection.getId());
            intent.putExtra(CONNECTION_TO_EDIT_INTENT_KEY, connection.getId());
        }
    }

    private void editOpaqueConnection(Intent intent, String runtimeId) {
        Connection connection = getConnectionLoader(this).getConnectionsById().get(runtimeId);
        if (connection != null) {
            Log.d(TAG, "editConnection - Editing Opaque with file: " + connection.getFilename());
            intent.putExtra(Constants.OPAQUE_CONNECTION_TO_EDIT_INTENT_KEY, connection.getFilename());
        }
    }

    private void deleteConnection(View v) {
        Log.d(TAG, "Delete Connection");
        String runtimeId = (String) ((TextView) v.findViewById(R.id.grid_item_id)).getText();
        String gridItemText = (String) ((TextView) v.findViewById(R.id.grid_item_text)).getText();
        Utils.showYesNoPrompt(this, getString(R.string.delete_connection) + "?", getString(R.string.delete_connection) + " " + gridItemText + " ?",
                (dialog, i) -> {
                    ConnectionLoader connectionLoader = getConnectionLoader(ConnectionGridActivity.this);
                    if (Utils.isOpaque(ConnectionGridActivity.this)) {

                        StringBuilder newListOfConnections = new StringBuilder();

                        SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
                        String currentConnectionsStr = sp.getString("connections", null);

                        ConnectionSettings cs = (ConnectionSettings) connectionLoader.getConnectionsById().get(runtimeId);
                        String[] currentConnections = {};
                        if (currentConnectionsStr != null) {
                            currentConnections = currentConnectionsStr.split(" ");
                        }
                        for (String connection : currentConnections) {
                            if (cs != null && !connection.equals(cs.getFilename())) {
                                newListOfConnections.append(" ").append(connection);
                            }
                        }
                        Log.d(TAG, "Deleted connection, current list: " + newListOfConnections);
                        Editor editor = sp.edit();
                        editor.putString("connections", newListOfConnections.toString().trim());
                        editor.apply();
                        File toDelete;
                        if (cs != null) {
                            toDelete = new File(getFilesDir() + "/" + cs.getFilename() + ".png");
                            boolean deleted = toDelete.delete();
                            Log.d(TAG, "Successfully deleted connection: " + deleted);
                        }
                    } else {
                        ConnectionBean conn = (ConnectionBean) connectionLoader.getConnectionsById().get(runtimeId);
                        if (conn != null) {
                            conn.Gen_delete(database.getWritableDatabase());
                        }
                        database.close();
                    }
                    onResume();
                }, null);
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.i(TAG, "onResume of version " + Utils.getVersionAndCode(this));
        showMasterPasswordDialogOrConnections(true);
        isStarting = false;
    }

    private void showMasterPasswordDialogOrConnections(boolean showIntroText) {
        if (showMasterPasswordDialog && Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
            showGetTextFragment(getPassword);
        } else {
            loadSavedConnections();
            if (showIntroText) {
                IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && isStarting, false);
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        showMasterPasswordDialog = true;
        Log.i(TAG, "onPause");
        if (database != null)
            database.close();
    }

    @Override
    protected void onResumeFragments() {
        Log.i(TAG, "onResumeFragments called");
        super.onResumeFragments();
        System.gc();
        showMasterPasswordDialogOrConnections(false);
    }

    private void loadSavedConnections() {
        createAndSetLabeledImageAdapterAndNumberOfColumns();
    }

    /**
     * Starts a new connection, showing local network discovery first.
     */
    public void addNewConnection() {
        DiscoveryBottomSheet sheet = new DiscoveryBottomSheet(
                NetworkDiscovery.serviceTypeForApp(this),
                new DiscoveryBottomSheet.Callback() {
                    @Override
                    public void onServerSelected(NetworkDiscovery.DiscoveredServer server) {
                        launchConnectionSetup(server.host(), server.port());
                    }

                    @Override
                    public void onEnterManually() {
                        launchConnectionSetup(null, -1);
                    }
                });
        sheet.show(getSupportFragmentManager(), "discovery");
    }

    private void launchConnectionSetup(String address, int port) {
        Intent intent = new Intent(ConnectionGridActivity.this,
                Utils.getConnectionSetupClass(this));
        intent.putExtra("isNewConnection", true);
        if (address != null) intent.putExtra(Constants.PREFILL_ADDRESS, address);
        if (port >= 0) intent.putExtra(Constants.PREFILL_PORT, port);
        startActivity(intent);
    }

    /**
     * Linked with android:onClick to the add new connection action bar item.
     */
    public void addNewConnection(MenuItem menuItem) {
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
     *
     */
    public void copyLogcat(MenuItem menuItem) {
        LogcatReader logcatReader = new LogcatReader();
        setClipboard(this, logcatReader.getMyLogcat(RemoteClientLibConstants.LOGCAT_MAX_LINES));
        Snackbar.make(gridView, getResources().getString(R.string.log_copied),
                Snackbar.LENGTH_LONG).show();
    }

    /**
     * Linked with android:onClick to the edit default settings action bar item.
     *
     */
    public void editDefaultSettings(MenuItem menuItem) {
        editDefaultSettings();
    }

    public void editDefaultSettings() {
        Log.d(TAG, "editDefaultSettings selected.");
        if (Utils.isOpaque(this)) {
            Intent intent = new Intent(ConnectionGridActivity.this, GeneralUtils.getClassByName("com.undatech.opaque.AdvancedSettingsActivity"));
            ConnectionSettings defaultConnection = new ConnectionSettings(RemoteClientLibConstants.DEFAULT_SETTINGS_FILE);
            defaultConnection.loadFromSharedPreferences(getApplicationContext());
            intent.putExtra(Constants.opaqueConnectionSettingsClassPath, defaultConnection);
            startActivityForResult(intent, DEFAULT_SETTINGS_REQUEST_CODE);
        } else {
            Intent intent = new Intent();
            intent.setClassName(this, "com.iiordanov.bVNC.GlobalPreferencesActivity");
            startActivity(intent);
        }
    }

    /**
     * Used to programmatically show the menu from a button. Mainly needed for Android TV
     */
    public void popUpMenu() {
        View anchor = popUpMenuButton != null ? popUpMenuButton : addNewConnection;
        PopupMenu popupMenu = new PopupMenu(ConnectionGridActivity.this, anchor);
        popupMenu.getMenuInflater().inflate(R.menu.grid_view_activity_actions, popupMenu.getMenu());
        popupMenu.show();
    }

    /**
     * Linked with android:onClick to share or rate action bar item.
     *
     */
    public void rateOrShare(MenuItem menuItem) {
        Log.d(TAG, "rateOrShare selected.");
        if (!rateOrShareFragment.isVisible()) {
            rateOrShareFragment.show(fragmentManager, "");
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.grid_view_activity_actions, menu);
        MenuItem actionMasterPassword = menu.findItem(R.id.actionMasterPassword);
        actionMasterPassword.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag));
        return super.onCreateOptionsMenu(menu);
    }

    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(TAG, "onActivityResult");

        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
            case LAUNCH_CONNECTION_REQUEST_CODE:
                Log.i(TAG, "onActivityResult LAUNCH_CONNECTION_REQUEST_CODE");
                showMasterPasswordDialog = false;
                break;
            case DEFAULT_SETTINGS_REQUEST_CODE:
                if (resultCode == Activity.RESULT_OK) {
                    Bundle b = data.getExtras();
                    ConnectionSettings defaultSettings = null;
                    if (b != null) {
                        defaultSettings = (ConnectionSettings) b.get(Constants.opaqueConnectionSettingsClassPath);
                    }
                    if (defaultSettings != null) {
                        defaultSettings.saveToSharedPreferences(this);
                    }
                } else {
                    Log.i(TAG, "Error during AdvancedSettingsActivity.");
                }
                break;
            case RemoteClientLibConstants.IMPORT_SETTINGS_REQUEST_CODE:
                if (resultCode == Activity.RESULT_OK) {
                    if (data != null && data.getData() != null) {
                        Uri uri = data.getData();
                        ContentResolver resolver = getContentResolver();

                        boolean connectionsInSharedPrefs = Utils.isOpaque(this);
                        InputStream in = FileUtils.getInputStreamFromUri(resolver, uri);
                        if (in == null) {
                            CharSequence error = getString(R.string.error) + ": " + uri;
                            Snackbar.make(gridView, error, Snackbar.LENGTH_LONG).show();
                            break;
                        }
                        if (connectionsInSharedPrefs) {
                            ConnectionSettings.importSettingsFromJsonToSharedPrefs(in, this);
                        } else {
                            Utils.importSettingsFromXml(in, database.getWritableDatabase());
                        }
                        recreate();
                    } else {
                        Log.e(TAG, "File uri not found, not importing settings");
                    }
                } else {
                    Log.e(TAG, "Error while selecting file to import settings from");
                }
                break;
            case RemoteClientLibConstants.EXPORT_SETTINGS_REQUEST_CODE:
                if (resultCode == Activity.RESULT_OK) {
                    if (data != null && data.getData() != null) {
                        ContentResolver resolver = getContentResolver();

                        boolean connectionsInSharedPrefs = Utils.isOpaque(this);
                        OutputStream out = FileUtils.getOutputStreamFromUri(resolver, data.getData());
                        if (connectionsInSharedPrefs) {
                            ConnectionSettings.exportSettingsFromSharedPrefsToJson(out, this);
                        } else {
                            Utils.exportSettingsToXml(out, database.getReadableDatabase());
                        }
                    } else {
                        Log.e(TAG, "File uri not found, not exporting settings");
                    }
                } else {
                    Log.e(TAG, "Error while selecting file to export settings to");
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
            boolean connectionsInSharedPrefs = Utils.isOpaque(this);
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
        if (itemId == R.id.actionExportImport) {
            importExportSettings(null);
        } else if (itemId == R.id.actionMasterPassword) {
            toggleMasterPassword(null);
        } else if (item.getGroupId() == R.id.itemInputModeGroup) {
            String inputMode = RemoteCanvasActivity.inputModeMap.get(item.getItemId());
            Log.d(TAG, "Setting input mode: " + inputMode);
            Utils.setSharedPreferenceString(this, Constants.defaultInputMethodTag, inputMode);
        }
        return true;
    }

    public void toggleMasterPassword(MenuItem menuItem) {
        Log.i(TAG, "toggleMasterPassword");
        if (Utils.isFree(this)) {
            IntroTextDialog.showIntroTextIfNecessary(this, database, true, true);
        } else {
            togglingMasterPassword = true;
            if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
                showGetTextFragment(getPassword);
            } else {
                showGetTextFragment(getNewPassword);
            }
        }
    }

    public void disableBatteryOptimizations(MenuItem menuItem) {
        Log.i(TAG, "disableBatteryOptimizations");
        new BatteryOptimizationDisabler(this, gridView).requestBatteryOptimizationExemption(true);
    }

    public void importExportSettings(MenuItem menuItem) {
        Log.i(TAG, "importExportSettings");
        showDialog(R.layout.importexport);
    }

    public void onTextObtained(String dialogId, String[] obtainedStrings, boolean wasCancelled, boolean keep) {
        Log.i(TAG, "onTextObtained");
        handlePassword(obtainedStrings[0], wasCancelled);
    }

    public void handlePassword(String providedPassword, boolean dialogWasCancelled) {
        Log.i(TAG, "handlePassword");
        boolean loadConnections;
        boolean wasTogglingMasterPassword = togglingMasterPassword;
        MasterPasswordDelegate passwordDelegate = new MasterPasswordDelegate(this, database);
        if (togglingMasterPassword) {
            loadConnections = passwordDelegate.toggleMasterPassword(providedPassword, dialogWasCancelled);
            togglingMasterPassword = false;
        } else {
            loadConnections = passwordDelegate.checkMasterPasswordAndQuitIfWrong(providedPassword, dialogWasCancelled);
        }
        if (loadConnections) {
            removeGetPasswordFragments();
            loadSavedConnections();
            if (wasTogglingMasterPassword) {
                invalidateOptionsMenu();
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

    public void showMainScreenHelp(View item) {
        Log.d(TAG, "showMainScreenHelp: Showing main screen help.");
        createMainScreenDialog(this);
    }

    public void showSupportForum(View item) {
        startUriIntent(this, "https://groups.google.com/forum/#!forum/bvnc-ardp-aspice-opaque-remote-desktop-clients");
    }

    public void emailUs(View item) {
        final Intent selectorIntent = new Intent(Intent.ACTION_SENDTO);
        selectorIntent.setData(Uri.parse("mailto:"));
        final Intent emailIntent = new Intent(Intent.ACTION_SEND);
        String packageName = Utils.pName(App.getContext());
        String versionAndBuild = Utils.getVersionAndCode(App.getContext());
        emailIntent.putExtra(Intent.EXTRA_EMAIL, new String[]{"support@morpheusly.com"});
        emailIntent.putExtra(
                Intent.EXTRA_SUBJECT,
                String.format("Help with: %s, version: %s", packageName, versionAndBuild)
        );
        emailIntent.addFlags(FLAG_ACTIVITY_NEW_TASK);
        emailIntent.setSelector(selectorIntent);
        App.getContext().startActivity(emailIntent);
    }

    public void reportBug(View item) {
        startUriIntent(this, "https://github.com/iiordanov/remote-desktop-clients/issues");
    }

    public void rateApp(View item) {
        Log.d(TAG, "rateApp: Showing rate app functionality");
        Utils.showRateAppDialog(this);
    }

    public void shareApp(View item) {
        Log.d(TAG, "shareApp: Copying app link to clipboard");
        String url = Utils.getDonationPackageUrl(this);
        setClipboard(this, url);
        Snackbar.make(gridView, R.string.share_app_toast, Snackbar.LENGTH_LONG).show();
    }

    public void donateToProject(View item) {
        startUriIntent(this, Utils.getDonationPackageLink(this));
    }

    public void moreApps(View item) {
        startUriIntent(this, "market://search?q=pub:\"Iordan Iordanov (Undatech)\"");
    }

    public void previousVersions(View item) {
        startUriIntent(this, "https://github.com/iiordanov/remote-desktop-clients/releases");
    }
}
