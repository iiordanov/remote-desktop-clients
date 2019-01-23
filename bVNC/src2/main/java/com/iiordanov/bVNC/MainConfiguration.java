package com.iiordanov.bVNC;

import java.util.ArrayList;
import java.util.Collections;

import net.sqlcipher.database.SQLiteDatabase;

import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.iiordanov.bVNC.input.InputHandlerDirectSwipePan;
import com.iiordanov.pubkeygenerator.GeneratePubkeyActivity;

import android.app.Activity;
import android.app.ActivityManager.MemoryInfo;
import android.content.Context;
import android.support.v4.app.FragmentTransaction;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Point;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.ClipboardManager;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;

import com.undatech.opaque.util.LogcatReader;
import com.undatech.opaque.util.PermissionsManager;

import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

public abstract class MainConfiguration extends FragmentActivity implements GetTextFragment.OnFragmentDismissedListener {
    private final static String TAG = "MainConfiguration";

    private boolean togglingMasterPassword = false;

    protected ConnectionBean selected;
    protected Database database;
    protected Spinner spinnerConnection;
    protected EditText textNickname;
    protected boolean startingOrHasPaused = true;
    protected int layoutID;
    GetTextFragment getPassword = null;
    GetTextFragment getNewPassword = null;
    private boolean isConnecting = false;
    private Button buttonGeneratePubkey;
    private TextView versionAndCode;
    protected PermissionsManager permissionsManager;

    protected abstract void updateViewFromSelected();
    protected abstract void updateSelectedFromView();

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Utils.showMenu(this);
        setContentView(layoutID);
        System.gc();

        permissionsManager = new PermissionsManager();

        if (getPassword == null) {
            getPassword = GetTextFragment.newInstance(getString(R.string.master_password_verify),
              this, GetTextFragment.Password, R.string.master_password_verify_message, R.string.master_password_set_error);
        }
        if (getNewPassword == null) {
            getNewPassword = GetTextFragment.newInstance(getString(R.string.master_password_set),
              this, GetTextFragment.MatchingPasswordTwice, R.string.master_password_set_message, R.string.master_password_set_error);
        }

        textNickname = (EditText) findViewById(R.id.textNickname);

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

        // Here we say what happens when the Pubkey Generate button is pressed.
        buttonGeneratePubkey = (Button) findViewById(R.id.buttonGeneratePubkey);
        buttonGeneratePubkey.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                generatePubkey ();
            }
        });

        versionAndCode = (TextView) findViewById(R.id.versionAndCode);
        try {
            PackageInfo pInfo = this.getPackageManager().getPackageInfo(getPackageName(), 0);
            versionAndCode.setText(pInfo.versionName + "_" + pInfo.versionCode);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        database = ((App)getApplication()).getDatabase();

        // Define what happens when the Import/Export button is pressed.
        ((Button) findViewById(R.id.buttonImportExport)).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                permissionsManager.requestPermissions(MainConfiguration.this);
                showDialog(R.layout.importexport);
            }
        });

        ((Button) findViewById(R.id.copyLogcat)).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                LogcatReader logcatReader = new LogcatReader();
                ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                cm.setText(logcatReader.getMyLogcat(Constants.LOGCAT_MAX_LINES));
                Toast.makeText(getBaseContext(), getResources().getString(R.string.log_copied),
                        Toast.LENGTH_LONG).show();
            }
        });

        permissionsManager.requestPermissions(MainConfiguration.this);
    }

    @Override
    protected void onStart() {
        Log.i(TAG, "onStart called");
        super.onStart();
        System.gc();
    }

    @Override
    protected void onResume() {
        Log.i(TAG, "onResume called");
        super.onResume();
        System.gc();
    }

    @Override
    protected void onResumeFragments() {
        Log.i(TAG, "onResumeFragments called");
        super.onResumeFragments();
        System.gc();
        if (Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag)) {
            showGetTextFragment(getPassword);
        } else {
            arriveOnPage();
        }
    }

    @Override
    public void onWindowFocusChanged (boolean visible) { }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        Log.i(TAG, "onConfigurationChanged called");
        super.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.i(TAG, "onStop called");
        if (database != null)
            database.close();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause called");
        if (database != null)
            database.close();
        if (!isConnecting) {
            startingOrHasPaused = true;
        } else {
            isConnecting = false;
        }
        if (selected != null) {
            updateSelectedFromView();
            selected.saveAndWriteRecent(false, database);
        }
    }

    @Override
    protected void onDestroy() {
        if (database != null)
            database.close();
        System.gc();
        super.onDestroy();
    }

    protected void canvasStart() {
        if (selected == null) return;
        MemoryInfo info = Utils.getMemoryInfo(this);
        if (info.lowMemory)
            System.gc();
        start();
    }

    /**
     * Starts the activity which makes a VNC connection and displays the remote desktop.
     */
    private void start() {
        isConnecting = true;
        updateSelectedFromView();
        Intent intent = new Intent(this, RemoteCanvasActivity.class);
        intent.putExtra(Utils.getConnectionString(this), selected.Gen_getValues());
        startActivity(intent);
    }

    public void arriveOnPage() {
        Log.i(TAG, "arriveOnPage called");
        SQLiteDatabase db = database.getReadableDatabase();
        ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
        ConnectionBean.getAll(db,
                              ConnectionBean.GEN_TABLE_NAME, connections,
                              ConnectionBean.newInstance);
        Collections.sort(connections);
        connections.add(0, new ConnectionBean(this));
        int connectionIndex = 0;
        if (connections.size() > 1) {
            MostRecentBean mostRecent = ConnectionBean.getMostRecent(db);
            if (mostRecent != null) {
                for (int i = 1; i < connections.size(); ++i) {
                    if (connections.get(i).get_Id() == mostRecent.getConnectionId()) {
                        connectionIndex = i;
                        break;
                    }
                }
            }
        }
        database.close();
        spinnerConnection.setAdapter(new ArrayAdapter<ConnectionBean>(this, R.layout.connection_list_entry,
                                     connections.toArray(new ConnectionBean[connections.size()])));
        spinnerConnection.setSelection(connectionIndex, false);
        selected = connections.get(connectionIndex);
        updateViewFromSelected();
        IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && startingOrHasPaused);
        startingOrHasPaused = false;
    }

    /**
     * Starts the activity which manages keys.
     */
    protected void generatePubkey () {
        updateSelectedFromView();
        selected.saveAndWriteRecent(true, database);
        Intent intent = new Intent(this, GeneratePubkeyActivity.class);
        intent.putExtra("PrivateKey",selected.getSshPrivKey());
        startActivityForResult(intent, Constants.ACTIVITY_GEN_KEY);
    }

    public Database getDatabaseHelper() {
        return database;
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
        Point outSize = new Point();
        d.getSize(outSize);
        int height = outSize.y;
        int value = height;
        if (android.os.Build.VERSION.SDK_INT >= 14) {
            android.view.ViewConfiguration vc = ViewConfiguration.get(this);
            if (vc.hasPermanentMenuKey())
                value = bottom;
        }
        if (Utils.isBlackBerry ()) {
            value = bottom;
        }
        return value;
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
        Point outSize = new Point();
        d.getSize(outSize);
        int width = outSize.x;
        if (android.os.Build.VERSION.SDK_INT >= 14) {
            android.view.ViewConfiguration vc = ViewConfiguration.get(this);
            if (vc.hasPermanentMenuKey())
                return right;
        }
        return width;
    }


    /* (non-Javadoc)
     * @see android.app.Activity#onCreateOptionsMenu(android.view.Menu)
     */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.androidvncmenu, menu);
        getMenuInflater().inflate(R.menu.input_mode_menu_item, menu);
        return true;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
     */
    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        android.util.Log.d(TAG, "onMenuOpened");
        try {
            updateInputMenu(menu.findItem(R.id.itemInputMode).getSubMenu());
            menu.findItem(R.id.itemDeleteConnection).setEnabled(selected != null && !selected.isNew());
            menu.findItem(R.id.itemSaveAsCopy).setEnabled(selected != null && !selected.isNew());
            MenuItem itemMasterPassword = menu.findItem(R.id.itemMasterPassword);
            itemMasterPassword.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.masterPasswordEnabledTag));
            MenuItem keepScreenOn = menu.findItem(R.id.itemKeepScreenOn);
            keepScreenOn.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.keepScreenOnTag));
            MenuItem disableImmersive = menu.findItem(R.id.itemDisableImmersive);
            disableImmersive.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.disableImmersiveTag));
            MenuItem forceLandscape = menu.findItem(R.id.itemForceLandscape);
            forceLandscape.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.forceLandscapeTag));
            MenuItem rAltAsIsoL3Shift = menu.findItem(R.id.itemRAltAsIsoL3Shift);
            rAltAsIsoL3Shift.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.rAltAsIsoL3ShiftTag));
            MenuItem itemLeftHandedMode = menu.findItem(R.id.itemLeftHandedMode);
            itemLeftHandedMode.setChecked(Utils.querySharedPreferenceBoolean(this, Constants.leftHandedModeTag));
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
        android.util.Log.e(TAG, "Default Input Mode Item: " + defaultInputHandlerId);

        try {
            for (MenuItem item : inputModeMenuItems) {
                android.util.Log.e(TAG, "Input Mode Item: " +
                        RemoteCanvasActivity.inputModeMap.get(item.getItemId()));

                if (defaultInputHandlerId.equals(RemoteCanvasActivity.inputModeMap.get(item.getItemId()))) {
                    item.setChecked(true);
                }
            }
        } catch (NullPointerException e) { }
    }


    /* (non-Javadoc)
     * @see android.app.Activity#onOptionsItemSelected(android.view.MenuItem)
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId())
        {
        case R.id.itemSaveAsCopy:
            if (selected.getNickname().equals(textNickname.getText().toString()))
                textNickname.setText("Copy of "+selected.getNickname());
            updateSelectedFromView();
            selected.set_Id(0);
            selected.saveAndWriteRecent(false, database);
            arriveOnPage();
            break;
        case R.id.itemDeleteConnection:
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
        case R.id.itemExportImport:
            permissionsManager.requestPermissions(MainConfiguration.this);
            showDialog(R.layout.importexport);
            break;
        case R.id.itemMasterPassword:
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
            break;
        case R.id.itemKeepScreenOn:
            Utils.toggleSharedPreferenceBoolean(this, Constants.keepScreenOnTag);
            break;
        case R.id.itemDisableImmersive:
            Utils.toggleSharedPreferenceBoolean(this, Constants.disableImmersiveTag);
            break;
        case R.id.itemForceLandscape:
            Utils.toggleSharedPreferenceBoolean(this, Constants.forceLandscapeTag);
            break;
        case R.id.itemRAltAsIsoL3Shift:
            Utils.toggleSharedPreferenceBoolean(this, Constants.rAltAsIsoL3ShiftTag);
            break;
        case R.id.itemLeftHandedMode:
            Utils.toggleSharedPreferenceBoolean(this, Constants.leftHandedModeTag);
            break;
        default:
            if (item.getGroupId() == R.id.itemInputModeGroup) {
                android.util.Log.e(TAG, RemoteCanvasActivity.inputModeMap.get(item.getItemId()));
                Utils.setSharedPreferenceString(this, Constants.defaultInputMethodTag,
                        RemoteCanvasActivity.inputModeMap.get(item.getItemId()));
            }
            break;
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
    
    public void onTextObtained(String obtainedString, boolean wasCancelled) {
        handlePassword(obtainedString, wasCancelled);
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
                    arriveOnPage();
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
                arriveOnPage();
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
                arriveOnPage();
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
            FragmentManager fm = ((FragmentActivity)this).getSupportFragmentManager();
            f.setCancelable(false);
            f.show(fm, "");
        }
    }
    
    private void removeGetPasswordFragments() {
        if (getPassword.isAdded()) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getPassword);
            tx.commit();
            getSupportFragmentManager().executePendingTransactions();
        }
        if (getNewPassword.isAdded()) {
            FragmentTransaction tx = this.getSupportFragmentManager().beginTransaction();
            tx.remove(getNewPassword);
            tx.commit();
            getSupportFragmentManager().executePendingTransactions();
        }
    }
    
    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch(requestCode) {
        case (Constants.ACTIVITY_GEN_KEY):
            if (resultCode == Activity.RESULT_OK) {
                Bundle b = data.getExtras();
                String privateKey = (String)b.get("PrivateKey");
                if (!privateKey.equals(selected.getSshPrivKey()) && privateKey.length() != 0)
                    Toast.makeText(getBaseContext(), "New key generated/imported successfully. Tap 'Generate/Export Key' " +
                            " button to share, copy to clipboard, or export the public key now.", Toast.LENGTH_LONG).show();
                selected.setSshPrivKey(privateKey);
                selected.setSshPubKey((String)b.get("PublicKey"));
                selected.saveAndWriteRecent(true, database);
            } else
                Log.i (TAG, "The user cancelled SSH key generation.");
            break;
        }
    }
}
