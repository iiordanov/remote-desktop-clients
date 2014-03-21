/**
 * Copyright (C) 2012 Iordan Iordanov
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

package com.iiordanov.bVNC;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ActivityManager.MemoryInfo;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.database.sqlite.SQLiteDatabase;
import android.os.Bundle;
import android.util.Log;
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
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import com.iiordanov.bVNC.dialogs.ImportExportDialog;
import com.iiordanov.bVNC.dialogs.ImportTlsCaDialog;
import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.pubkeygenerator.GeneratePubkeyActivity;

/**
 * aSPICE is the Activity for setting up SPICE connections.
 */
public class aSPICE extends Activity implements MainConfiguration {
    private final static String TAG = "aSPICE";
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
    private Button buttonImportCa;
    private EditText tlsPort;
    private EditText passwordText;
    private Button goButton;
    private Button buttonGeneratePubkey;
    private ToggleButton toggleAdvancedSettings;
    private Spinner spinnerConnection;
    private Spinner spinnerGeometry;
    private Database database;
    private ConnectionBean selected;
    private EditText textNickname;
    private EditText resWidth;
    private EditText resHeight;
    private CheckBox checkboxKeepPassword;
    private CheckBox checkboxUseDpadAsArrows;
    private CheckBox checkboxRotateDpad;
    private CheckBox checkboxLocalCursor;
    private CheckBox checkboxUseSshPubkey;
    private boolean isFree;
    private boolean startingOrHasPaused = true;
    private boolean isConnecting = false;
    private CheckBox checkboxEnableSound;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        System.gc();
        setContentView(R.layout.main_spice);

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
        tlsPort = (EditText) findViewById(R.id.tlsPort);
        passwordText = (EditText) findViewById(R.id.textPASSWORD);
        textNickname = (EditText) findViewById(R.id.textNickname);

        buttonImportCa = (Button) findViewById(R.id.buttonImportCa);
        buttonImportCa.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                aSPICE.this.updateSelectedFromView();
                showDialog(R.layout.import_tls_ca_dialog);
            }
        });
        
        // Here we say what happens when the Pubkey Checkbox is
        // checked/unchecked.
        checkboxUseSshPubkey = (CheckBox) findViewById(R.id.checkboxUseSshPubkey);
        checkboxUseSshPubkey
                .setOnCheckedChangeListener(new OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView,
                            boolean isChecked) {
                        selected.setUseSshPubKey(isChecked);
                        setSshPasswordHint(isChecked);
                        sshPassword.setText("");
                    }
                });

        // Here we say what happens when the Pubkey Generate button is pressed.
        buttonGeneratePubkey = (Button) findViewById(R.id.buttonGeneratePubkey);
        buttonGeneratePubkey.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                generatePubkey();
            }
        });

        // Define what happens when somebody selects different VNC connection
        // types.
        connectionType = (Spinner) findViewById(R.id.connectionType);
        connectionType
                .setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(AdapterView<?> ad, View view,
                            int itemIndex, long id) {

                        selectedConnType = itemIndex;
                        if (selectedConnType == Constants.CONN_TYPE_PLAIN) {
                            setVisibilityOfSshWidgets(View.GONE);
                        } else if (selectedConnType == Constants.CONN_TYPE_SSH) {
                            setVisibilityOfSshWidgets(View.VISIBLE);
                            if (ipText.getText().toString().equals(""))
                                ipText.setText("localhost");
                            setSshPasswordHint(checkboxUseSshPubkey.isChecked());
                        }
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> ad) {
                    }
                });

        checkboxKeepPassword = (CheckBox) findViewById(R.id.checkboxKeepPassword);
        checkboxUseDpadAsArrows = (CheckBox) findViewById(R.id.checkboxUseDpadAsArrows);
        checkboxRotateDpad = (CheckBox) findViewById(R.id.checkboxRotateDpad);
        checkboxLocalCursor = (CheckBox) findViewById(R.id.checkboxUseLocalCursor);
        checkboxEnableSound = (CheckBox) findViewById(R.id.checkboxEnableSound);
        
        spinnerConnection = (Spinner) findViewById(R.id.spinnerConnection);
        spinnerConnection.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(AdapterView<?> ad, View view,
                            int itemIndex, long id) {
                        selected = (ConnectionBean) ad.getSelectedItem();
                        updateViewFromSelected();
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> ad) {
                        selected = null;
                    }
                });
        
        goButton = (Button) findViewById(R.id.buttonGO);
        goButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (ipText.getText().length() != 0
                        && (portText.getText().length() != 0 || tlsPort.getText().length() != 0))
                    canvasStart();
                else
                    Toast.makeText(view.getContext(),
                            R.string.spice_server_empty, Toast.LENGTH_LONG)
                            .show();
            }
        });

        // The advanced settings button.
        toggleAdvancedSettings = (ToggleButton) findViewById(R.id.toggleAdvancedSettings);
        layoutAdvancedSettings = (LinearLayout) findViewById(R.id.layoutAdvancedSettings);
        toggleAdvancedSettings.setOnCheckedChangeListener(new OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton arg0,
                            boolean checked) {
                        if (checked)
                            layoutAdvancedSettings.setVisibility(View.VISIBLE);
                        else
                            layoutAdvancedSettings.setVisibility(View.GONE);
                    }
                });

        // The geometry type and dimensions boxes.
        spinnerGeometry = (Spinner) findViewById(R.id.spinnerRdpGeometry);
        resWidth = (EditText) findViewById(R.id.rdpWidth);
        resHeight = (EditText) findViewById(R.id.rdpHeight);        
        spinnerGeometry.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener () {
            @Override
            public void onItemSelected(AdapterView<?> arg0, View view, int itemIndex, long id) {
                selected.setRdpResType(itemIndex);
                setRemoteWidthAndHeight ();
            }
            @Override
            public void onNothingSelected(AdapterView<?> arg0) {
            }
        });
        
        database = new Database(this);

        // Define what happens when the Import/Export button is pressed.
        ((Button) findViewById(R.id.buttonImportExport))
                .setOnClickListener(new View.OnClickListener() {
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
    private void setVisibilityOfSshWidgets(int visibility) {
        sshCredentials.setVisibility(visibility);
        sshCaption.setVisibility(visibility);
        layoutUseSshPubkey.setVisibility(visibility);
        sshServerEntry.setVisibility(visibility);
    }

    /**
     * Enables and disables the EditText boxes for width and height of remote desktop.
     */
    private void setRemoteWidthAndHeight () {
        if (selected.getRdpResType() != Constants.RDP_GEOM_SELECT_CUSTOM) {
            resWidth.setEnabled(false);
            resHeight.setEnabled(false);
        } else {
            resWidth.setEnabled(true);
            resHeight.setEnabled(true);
        }
    }
    
    /**
     * Sets the ssh password/passphrase hint appropriately.
     */
    private void setSshPasswordHint(boolean isPassphrase) {
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

    /*
     * (non-Javadoc)
     * 
     * @see android.app.Activity#onCreateDialog(int)
     */
    @Override
    protected Dialog onCreateDialog(int id) {
        switch (id) {
        case R.layout.importexport:
            return new ImportExportDialog(this);
        case R.id.itemMainScreenHelp:
            return createHelpDialog();
        case R.layout.import_tls_ca_dialog:
            return new ImportTlsCaDialog(this);
        }
        return null;
    }

    /**
     * Creates the help dialog for this activity.
     */
    private Dialog createHelpDialog() {
        AlertDialog.Builder adb = new AlertDialog.Builder(this).setMessage(
                R.string.spice_main_screen_help_text).setPositiveButton(
                R.string.close, new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        // We don't have to do anything.
                    }
                });
        Dialog d = adb.setView(new ListView(this)).create();
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
        lp.copyFrom(d.getWindow().getAttributes());
        lp.width = WindowManager.LayoutParams.FILL_PARENT;
        lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
        d.show();
        d.getWindow().setAttributes(lp);
        return d;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.app.Activity#onCreateOptionsMenu(android.view.Menu)
     */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.androidvncmenu, menu);
        return true;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
     */
    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        if (menu != null) {
            menu.findItem(R.id.itemDeleteConnection).setEnabled(selected!=null && ! selected.isNew());
            menu.findItem(R.id.itemSaveAsCopy).setEnabled(selected!=null && ! selected.isNew());
        }
        return true;
    }

    /*
     * (non-Javadoc)
     * 
     * @see android.app.Activity#onOptionsItemSelected(android.view.MenuItem)
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case R.id.itemSaveAsCopy:
            if (selected.getNickname()
                    .equals(textNickname.getText().toString()))
                textNickname.setText("Copy of " + selected.getNickname());
            updateSelectedFromView();
            selected.set_Id(0);
            saveAndWriteRecent();
            arriveOnPage();
            break;
        case R.id.itemDeleteConnection:
            Utils.showYesNoPrompt(this, "Delete?",
                    "Delete " + selected.getNickname() + "?",
                    new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int i) {
                            selected.Gen_delete(database.getWritableDatabase());
                            database.close();
                            arriveOnPage();
                        }
                    }, null);
            break;
        case R.id.itemMainScreenHelp:
            showDialog(R.id.itemMainScreenHelp);
            break;
        // Disabling Manual/Wiki Menu item as the original does not correspond
        // to this project anymore.
        // case R.id.itemOpenDoc :
        // Utils.showDocumentation(this);
        // break;
        }
        return true;
    }

    public void updateViewFromSelected() {
        if (selected == null)
            return;
        selectedConnType = selected.getConnectionType();
        connectionType.setSelection(selectedConnType);
        sshServer.setText(selected.getSshServer());
        sshPort.setText(Integer.toString(selected.getSshPort()));
        sshUser.setText(selected.getSshUser());

        checkboxUseSshPubkey.setChecked(selected.getUseSshPubKey());
        setSshPasswordHint(checkboxUseSshPubkey.isChecked());

        if (selectedConnType == Constants.CONN_TYPE_SSH
                && selected.getAddress().equals(""))
            ipText.setText("localhost");
        else
            ipText.setText(selected.getAddress());

        if (selected.getPort() < 0) {
            portText.setText("");
        } else {
            portText.setText(Integer.toString(selected.getPort()));
        }
        if (selected.getTlsPort() < 0) {
            tlsPort.setText("");
        } else {
            tlsPort.setText(Integer.toString(selected.getTlsPort()));
        }

        if (selected.getKeepPassword() || selected.getPassword().length() > 0) {
            passwordText.setText(selected.getPassword());
        }

        checkboxKeepPassword.setChecked(selected.getKeepPassword());
        checkboxUseDpadAsArrows.setChecked(selected.getUseDpadAsArrows());
        checkboxRotateDpad.setChecked(selected.getRotateDpad());
        checkboxLocalCursor.setChecked(selected.getUseLocalCursor());
        checkboxEnableSound.setChecked(selected.getEnableSound());
        textNickname.setText(selected.getNickname());
        spinnerGeometry.setSelection(selected.getRdpResType());
        resWidth.setText(Integer.toString(selected.getRdpWidth()));
        resHeight.setText(Integer.toString(selected.getRdpHeight()));
        setRemoteWidthAndHeight ();
        
        // Write out CA to file if it doesn't exist.
        String caCertData = selected.getCaCert();
        try {
            // If a cert has been set, write out a unique file containing the cert and save the path to that file to give to libspice.
            String filename = getFilesDir() + "/ca" + Integer.toString(selected.getCaCert().hashCode()) + ".pem";
            selected.setCaCertPath(filename);
            File file = new File(filename);
            if (!file.exists() && !caCertData.equals("")) {
                android.util.Log.e(TAG, filename);
                PrintWriter fout = new PrintWriter(filename);
                fout.println(selected.getCaCert().toString());
                fout.close();
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }
    }

    /**
     * Returns the current ConnectionBean.
     */
    public ConnectionBean getCurrentConnection() {
        return selected;
    }

    /**
     * Returns the display height, or if the device has software buttons, the
     * 'bottom' of the view (in order to take into account the software buttons.
     * 
     * @return the height in pixels.
     */
    public int getHeight() {
        View v = getWindow().getDecorView().findViewById(android.R.id.content);
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
     * Returns the display width, or if the device has software buttons, the
     * 'right' of the view (in order to take into account the software buttons.
     * 
     * @return the width in pixels.
     */
    public int getWidth() {
        View v = getWindow().getDecorView().findViewById(android.R.id.content);
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
        
        String port = portText.getText().toString();
        if (!port.equals("")) {
            try {
                selected.setPort(Integer.parseInt(portText.getText().toString()));
            } catch (NumberFormatException nfe) { }
        } else {
            selected.setPort(-1);
        }
        
        String tlsport = tlsPort.getText().toString();
        if (!tlsport.equals("")) {
            try {
                selected.setTlsPort(Integer.parseInt(tlsPort.getText().toString()));
            } catch (NumberFormatException nfe) { }
        } else {
            selected.setTlsPort(-1);
        }
        
        try {
            selected.setSshPort(Integer.parseInt(sshPort.getText().toString()));
        } catch (NumberFormatException nfe) {
        }
        
        selected.setNickname(textNickname.getText().toString());
        selected.setSshServer(sshServer.getText().toString());
        selected.setSshUser(sshUser.getText().toString());

        selected.setKeepSshPassword(false);

        // If we are using an SSH key, then the ssh password box is used
        // for the key pass-phrase instead.
        selected.setUseSshPubKey(checkboxUseSshPubkey.isChecked());
        selected.setSshPassPhrase(sshPassword.getText().toString());
        selected.setSshPassword(sshPassword.getText().toString());
        selected.setRdpResType(spinnerGeometry.getSelectedItemPosition());
        try    {
            selected.setRdpWidth(Integer.parseInt(resWidth.getText().toString()));
            selected.setRdpHeight(Integer.parseInt(resHeight.getText().toString()));
        } catch (NumberFormatException nfe) {}
        selected.setPassword(passwordText.getText().toString());
        selected.setKeepPassword(checkboxKeepPassword.isChecked());
        selected.setUseDpadAsArrows(checkboxUseDpadAsArrows.isChecked());
        selected.setRotateDpad(checkboxRotateDpad.isChecked());
        selected.setUseLocalCursor(checkboxLocalCursor.isChecked());
        selected.setEnableSound(checkboxEnableSound.isChecked());
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
    public void onWindowFocusChanged(boolean visible) {}

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        Log.e(TAG, "onConfigurationChanged called");
        super.onConfigurationChanged(newConfig);
    }

    /**
     * Return the object representing the app global state in the database, or
     * null if the object hasn't been set up yet
     * 
     * @param db
     *            App's database -- only needs to be readable
     * @return Object representing the single persistent instance of
     *         MostRecentBean, which is the app's global state
     */
    public static MostRecentBean getMostRecent(SQLiteDatabase db) {
        ArrayList<MostRecentBean> recents = new ArrayList<MostRecentBean>(1);
        MostRecentBean.getAll(db, MostRecentBean.GEN_TABLE_NAME, recents,
                MostRecentBean.GEN_NEW);
        if (recents.size() == 0)
            return null;
        return recents.get(0);
    }

    public void arriveOnPage() {
        ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
        ConnectionBean.getAll(database.getReadableDatabase(),
                ConnectionBean.GEN_TABLE_NAME, connections,
                ConnectionBean.newInstance);
        Collections.sort(connections);
        connections.add(0, new ConnectionBean(this));
        int connectionIndex = 0;
        if (connections.size() > 1) {
            MostRecentBean mostRecent = getMostRecent(database
                    .getReadableDatabase());
            if (mostRecent != null) {
                for (int i = 1; i < connections.size(); ++i) {
                    if (connections.get(i).get_Id() == mostRecent
                            .getConnectionId()) {
                        connectionIndex = i;
                        break;
                    }
                }
            }
        }
        spinnerConnection.setAdapter(new ArrayAdapter<ConnectionBean>(this,
                R.layout.connection_list_entry, connections
                        .toArray(new ConnectionBean[connections.size()])));
        spinnerConnection.setSelection(connectionIndex, false);
        selected = connections.get(connectionIndex);
        updateViewFromSelected();
        IntroTextDialog.showIntroTextIfNecessary(this, database, isFree&&startingOrHasPaused);
        startingOrHasPaused = false;
    }
    
    protected void onStop() {
        super.onStop();
        if (selected == null) {
            return;
        }
        updateSelectedFromView();

        saveAndWriteRecent();
    }
    
    protected void onPause() {
        Log.e(TAG, "onPause called");
        super.onPause();
        if (!isConnecting) {
            startingOrHasPaused = true;
        } else {
            isConnecting = false;
        }
    }
    
    public Database getDatabaseHelper() {
        return database;
    }
    
    private void canvasStart() {
        if (selected == null)
            return;
        MemoryInfo info = Utils.getMemoryInfo(this);
        if (info.lowMemory)
            System.gc();
        start();
    }

    public void saveAndWriteRecent() {
        // We need server address or SSH server to be filled out to save. Otherwise,
        // we keep adding empty connections.
        if (selected.getConnectionType() == Constants.CONN_TYPE_SSH
            && selected.getSshServer().equals("")
            || selected.getAddress().equals(""))
            return;
        
        SQLiteDatabase db = database.getWritableDatabase();
        db.beginTransaction();
        try {
            selected.save(db);
            MostRecentBean mostRecent = getMostRecent(db);
            if (mostRecent == null) {
                mostRecent = new MostRecentBean();
                mostRecent.setConnectionId(selected.get_Id());
                mostRecent.Gen_insert(db);
            } else {
                mostRecent.setConnectionId(selected.get_Id());
                mostRecent.Gen_update(db);
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
            db.close();
        }
    }

    /**
     * Starts the activity which makes a VNC connection and displays the remote
     * desktop.
     */
    private void start () {
        isConnecting = true;
        updateSelectedFromView();
        saveAndWriteRecent();
        Intent intent = new Intent(this, RemoteCanvasActivity.class);
        intent.putExtra(Constants.CONNECTION, selected.Gen_getValues());
        startActivity(intent);
    }

    /**
     * Starts the activity which manages keys.
     */
    private void generatePubkey() {
        updateSelectedFromView();
        saveAndWriteRecent();
        Intent intent = new Intent(this, GeneratePubkeyActivity.class);
        intent.putExtra("PrivateKey", selected.getSshPrivKey());
        startActivityForResult(intent, Constants.ACTIVITY_GEN_KEY);
    }

    /**
     * This function is used to retrieve data returned by activities started
     * with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode) {
        case (Constants.ACTIVITY_GEN_KEY):
            if (resultCode == Activity.RESULT_OK) {
                Bundle b = data.getExtras();
                String privateKey = (String) b.get("PrivateKey");
                if (!privateKey.equals(selected.getSshPrivKey())
                        && privateKey.length() != 0)
                    Toast.makeText(
                            getBaseContext(),
                            "New key generated/imported successfully. Tap 'Generate/Export Key' "
                                    + " button to share, copy to clipboard, or export the public key now.",
                            Toast.LENGTH_LONG).show();
                selected.setSshPrivKey(privateKey);
                selected.setSshPubKey((String) b.get("PublicKey"));
                saveAndWriteRecent();
            } else
                Log.i(TAG, "The user cancelled SSH key generation.");
            break;
        }
    }
}
