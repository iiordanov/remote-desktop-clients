package com.iiordanov.bVNC;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Field;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.util.ArrayList;
import java.util.Collections;

import net.sqlcipher.database.SQLiteDatabase;

import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.dialogs.GetTextFragment;

import android.app.Activity;
import android.app.ActivityManager.MemoryInfo;
import android.support.v4.app.FragmentTransaction;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Point;
import android.nfc.Tag;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Spinner;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;

public abstract class MainConfiguration extends FragmentActivity implements GetTextFragment.OnFragmentDismissedListener {
    private final static String TAG = "MainConfiguration";

    private String masterPassword = "";
    private boolean togglingMasterPassword = false;
    protected static PasswordManager passwordManager = null;

    protected ConnectionBean selected;
    protected Database database;
    protected Spinner spinnerConnection;
    protected EditText textNickname;
    protected boolean startingOrHasPaused = true;
    protected int layoutID;
    GetTextFragment getPassword = null;
    GetTextFragment getNewPassword = null;
    private boolean isConnecting = false;
    
    protected abstract void updateViewFromSelected();
    protected abstract void updateSelectedFromView();

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Utils.showMenu(this);
        setContentView(layoutID);
        System.gc();
        
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
    }
    
    @Override
    protected void onStart() {
        Log.i(TAG, "onStart called");
        super.onStart();
        System.gc();
        //arriveOnPage();
    }
    
    @Override
    protected void onResume() {
        Log.i(TAG, "onResume called");
        super.onResume();
        System.gc();
        //arriveOnPage();
    }
    
    @Override
    protected void onResumeFragments() {
        Log.i(TAG, "onResumeFragments called");
        super.onResumeFragments();
        System.gc();
        if (isMasterPasswordEnabled()) {
            showGetPasswordFragment();
        } else {
            database = new Database(this);
        }
        arriveOnPage();
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
        Log.i(TAG, "onStop called");
        super.onStop();
        if ( selected == null ) {
            return;
        }
        updateSelectedFromView();
        selected.saveAndWriteRecent(false);
    }
    
    @Override
    protected void onPause() {
        Log.i(TAG, "onPause called");
        super.onPause();
        if (!isConnecting) {
            startingOrHasPaused = true;
        } else {
            isConnecting = false;
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
    private void start () {
        isConnecting = true;
        updateSelectedFromView();
        selected.saveAndWriteRecent(false);
        Intent intent = new Intent(this, RemoteCanvasActivity.class);
        intent.putExtra(Constants.CONNECTION, selected.Gen_getValues());
        startActivity(intent);
    }
    
    public void arriveOnPage() {
        if (database == null) {
            return;
        }
        Log.i(TAG, "arriveOnPage called");
        ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
        ConnectionBean.getAll(database.getReadableDatabase(),
                              ConnectionBean.GEN_TABLE_NAME, connections,
                              ConnectionBean.newInstance);
        database.close();
        Collections.sort(connections);
        connections.add(0, new ConnectionBean(this));
        int connectionIndex = 0;
        if (connections.size() > 1) {
            MostRecentBean mostRecent = ConnectionBean.getMostRecent(database.getReadableDatabase());
            database.close();
            if (mostRecent != null) {
                for (int i = 1; i < connections.size(); ++i) {
                    if (connections.get(i).get_Id() == mostRecent.getConnectionId()) {
                        connectionIndex = i;
                        break;
                    }
                }
            }
        }
        spinnerConnection.setAdapter(new ArrayAdapter<ConnectionBean>(this, R.layout.connection_list_entry,
                                     connections.toArray(new ConnectionBean[connections.size()])));
        spinnerConnection.setSelection(connectionIndex, false);
        selected = connections.get(connectionIndex);
        updateViewFromSelected();
        startingOrHasPaused = false;
        IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && startingOrHasPaused);
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
        getMenuInflater().inflate(R.menu.androidvncmenu,menu);
        return true;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
     */
    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        if (menu != null) {
            menu.findItem(R.id.itemDeleteConnection).setEnabled(selected != null && !selected.isNew());
            menu.findItem(R.id.itemSaveAsCopy).setEnabled(selected != null && !selected.isNew());
            MenuItem itemMasterPassword = menu.findItem(R.id.itemMasterPassword);
            if (isMasterPasswordEnabled()) {
                itemMasterPassword.setTitle(R.string.master_password_disable);
            } else {
                itemMasterPassword.setTitle(R.string.master_password_enable);
            }
        }
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
            selected.saveAndWriteRecent(false);
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
        case R.id.itemMasterPassword:
            togglingMasterPassword = true;
            if (isMasterPasswordEnabled()) {
                showGetPasswordFragment();
            } else {
                showGetNewPasswordFragment();
            }
            break;
            
        // Disabling Manual/Wiki Menu item as the original does not correspond to this project anymore.
        //case R.id.itemOpenDoc :
        //    Utils.showDocumentation(this);
        //    break;
        }
        return true;
    }
    
    private boolean isMasterPasswordEnabled () {
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        return sp.getBoolean("masterPasswordEnabled", false);
    }
    
    private void toggleMasterPasswordState () {
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        boolean state = sp.getBoolean("masterPasswordEnabled", false);
        Editor editor = sp.edit();
        editor.putBoolean("masterPasswordEnabled", !state);
        editor.apply();
        Log.i(TAG, "Toggled master password state");

    }
    
    private void setMasterPasswordHash (String password) throws UnsupportedEncodingException,
                                                                NoSuchAlgorithmException, InvalidKeySpecException {
        // Now compute and store the hash of the provided password and saved salt.
        String salt = PasswordManager.randomBase64EncodedString(Constants.saltLength);
        String hash = PasswordManager.computeHash(password, PasswordManager.b64Decode(salt));
        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        Editor editor = sp.edit();
        editor.putString("masterPasswordSalt", salt);
        editor.putString("masterPasswordHash", hash);
        editor.apply();
        Log.i(TAG, "Setting master password hash.");
        //Log.i(TAG, String.format("hash: %s, salt: %s", hash, new String(PasswordManager.b64Decode(salt))));
    }
    
    private boolean checkMasterPassword (String password) {
        Log.i(TAG, "Checking master password.");
        boolean result = false;
        
        Database testPassword = new Database(this);
        try {
            testPassword.getReadableDatabase(password);
            result = true;
        } catch (Exception e) {
            result = false;
        }
        
/*        SharedPreferences sp = getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String savedHash = sp.getString("masterPasswordHash", null);
        byte[] savedSalt = PasswordManager.b64Decode(sp.getString("masterPasswordSalt", null));
        //String savedSalt = sp.getString("masterPasswordSalt", null);
        if (savedHash != null && savedSalt != null) {
            String newHash = null;
            try {
                newHash = PasswordManager.computeHash(password, savedSalt);
                //Log.i(TAG, String.format("savedHash: %s, savedSalt: %s, newHash: %s", savedHash, new String(savedSalt), newHash));
                if (newHash.equals(savedHash)) {
                    result = true;
                }
            } catch (Exception e) { }
            
        }*/
        return result;
    }
    
    public void onTextObtained(String obtainedString) {
        handlePasword(obtainedString);
    }
    
    public void handlePasword(String providedPassword) {
        if (togglingMasterPassword) {
            Log.i(TAG, "Asked to toggle master pasword.");
            // The user has requested the password to be enabled or disabled.
            togglingMasterPassword = false;
            if (isMasterPasswordEnabled()) {
                Log.i(TAG, "Master password is enabled.");
                // Master password is enabled
                if (checkMasterPassword(providedPassword)) {
                    Log.i(TAG, "Entered password correct, disabling password.");
                    // Disable the password since the user input the correct password.
                    Database.setPassword(masterPassword);
                    database = new Database(this);
                    database.changeDatabasePassword("");
                    database.close();
                    
                    Database.setPassword("");
                    database = new Database(this);
                    database.close();
                    toggleMasterPasswordState();
                } else {
                    //deleteTempDatabase();
                    Log.i(TAG, "Entered password is wrong, quitting.");
                    Utils.showFatalErrorMessage(this, "TODO: Show localized error about wrong password, and QUIT.");
                }
            } else {
                Log.i(TAG, "Master password is disabled.");
                // The password is disabled, so set it in the preferences.
                try {
                    masterPassword = providedPassword;
                    Database.setPassword("");
                    database = new Database(this);
                    Log.i(TAG, "Changing database password.");
                    database.changeDatabasePassword(masterPassword);
                    database.close();
                    
                    // Set password, and reopen database.
                    Database.setPassword(masterPassword);
                    database = new Database(this);
                    database.close();
                    passwordManager = new PasswordManager(masterPassword);
                    
                    //setMasterPasswordHash (masterPassword);
                    toggleMasterPasswordState();
                } catch (Exception e) {
                    //deleteTempDatabase();
                    // TODO: Throw up a localized non-fatal error dialog
                    Utils.showErrorMessage(this, "TODO: Show localized error about enabling master password.");
                }
            }
        } else {
            // We are just trying to check the password.
            Log.i(TAG, "Just checking the password.");
            if (checkMasterPassword(providedPassword)) {
                Log.i(TAG, "Entered password is correct, proceeding.");
                masterPassword = providedPassword;
                Database.setPassword(masterPassword);
                database = new Database(this);
                database.close();
                passwordManager = new PasswordManager(masterPassword);
            } else {
                Log.i(TAG, "Entered password is wrong, quitting.");
                // Finish the activity if the password was wrong.
                Utils.showFatalErrorMessage(this, "TODO: Show localized error about wrong password, and ASK AGAIN.");
            }
        }
        removeGetPasswordFragments();
        arriveOnPage();
    }
    
    private void showGetPasswordFragment() {
        if (!getPassword.isVisible()) {
            removeGetPasswordFragments();
            FragmentManager fm = ((FragmentActivity)this).getSupportFragmentManager();
            getPassword.setCancelable(false);
            getPassword.show(fm, "getPassword");
        }
    }
    
    private void showGetNewPasswordFragment() {
        if (!getNewPassword.isVisible()) {
            removeGetPasswordFragments();
            FragmentManager fm = ((FragmentActivity)this).getSupportFragmentManager();
            getNewPassword.setCancelable(false);
            getNewPassword.show(fm, "getNewPassword");
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
}
