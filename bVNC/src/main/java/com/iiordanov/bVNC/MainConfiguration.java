package com.iiordanov.bVNC;

import java.util.ArrayList;
import java.util.Collections;

import net.sqlcipher.database.SQLiteDatabase;

import com.iiordanov.bVNC.dialogs.IntroTextDialog;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.iiordanov.bVNC.input.InputHandlerDirectSwipePan;
import com.iiordanov.pubkeygenerator.GeneratePubkeyActivity;

import android.app.Activity;
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
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;

import com.undatech.opaque.util.LogcatReader;
import com.undatech.opaque.util.PermissionsManager;

import com.undatech.remoteClientUi.*;

public abstract class MainConfiguration extends FragmentActivity {
    private final static String TAG = "MainConfiguration";

    protected ConnectionBean selected;
    protected Database database;
    protected EditText textNickname;
    protected boolean startingOrHasPaused = true;
    protected int layoutID;
    private boolean isConnecting = false;
    private Button buttonGeneratePubkey;
    private TextView versionAndCode;
    protected PermissionsManager permissionsManager;
    private RadioGroup radioCursor;

    private boolean isNewConnection;
    private long connID = 0;

    protected abstract void updateViewFromSelected();
    protected abstract void updateSelectedFromView();

    public void commonUpdateViewFromSelected() {
        if (selected.getUseLocalCursor() == Constants.CURSOR_AUTO) {
            radioCursor.check(R.id.radioCursorAuto);
        } else if (selected.getUseLocalCursor() == Constants.CURSOR_FORCE_LOCAL) {
            radioCursor.check(R.id.radioCursorForceLocal);
        } else if (selected.getUseLocalCursor() == Constants.CURSOR_FORCE_DISABLE) {
            radioCursor.check(R.id.radioCursorForceDisable);
        }
    }

    public void commonUpdateSelectedFromView() {
        if (radioCursor.getCheckedRadioButtonId() == R.id.radioCursorAuto) {
            selected.setUseLocalCursor(Constants.CURSOR_AUTO);
        } else if (radioCursor.getCheckedRadioButtonId() == R.id.radioCursorForceLocal) {
            selected.setUseLocalCursor(Constants.CURSOR_FORCE_LOCAL);
        } else if (radioCursor.getCheckedRadioButtonId() == R.id.radioCursorForceDisable) {
            selected.setUseLocalCursor(Constants.CURSOR_FORCE_DISABLE);
        }
    }

    @Override
    public void onCreate(Bundle icicle) {
        Intent intent = getIntent();
        isNewConnection = intent.getBooleanExtra("isNewConnection", false);
        if (!isNewConnection) {
            connID = Long.parseLong(intent.getStringExtra("connID"));
        }

        super.onCreate(icicle);
        Utils.showMenu(this);
        setContentView(layoutID);
        System.gc();

        permissionsManager = new PermissionsManager();

        textNickname = (EditText) findViewById(R.id.textNickname);

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
            android.util.Log.d(TAG, "Version of " + getPackageName() +
                                         " is " + pInfo.versionName + "_" + pInfo.versionCode);
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

        radioCursor = findViewById(R.id.radioCursor);
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
            selected.saveAndWriteRecent(false, this);
        }
    }

    @Override
    protected void onDestroy() {
        if (database != null)
            database.close();
        System.gc();
        super.onDestroy();
    }

    protected void saveConnectionAndCloseLayout() {
        if (selected != null) {
            updateSelectedFromView();
            selected.saveAndWriteRecent(false, this);
        }
        finish();
    }

    public void arriveOnPage() {
        Log.i(TAG, "arriveOnPage called");
        if(!isNewConnection) {
            SQLiteDatabase db = database.getReadableDatabase();
            ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
            ConnectionBean.getAll(db,
                    ConnectionBean.GEN_TABLE_NAME, connections,
                    ConnectionBean.newInstance);
            Collections.sort(connections);
            connections.add(0, new ConnectionBean(this));
            for (int i = 1; i < connections.size(); ++i) {

                if (connections.get(i).get_Id() == connID) {
                    selected = connections.get(i);
                    break;
                }
            }
            database.close();
        }
        if(selected == null) {
            selected = new ConnectionBean(this);
        }
        updateViewFromSelected();
        IntroTextDialog.showIntroTextIfNecessary(this, database, Utils.isFree(this) && startingOrHasPaused);
        startingOrHasPaused = false;
    }

    /**
     * Starts the activity which manages keys.
     */
    protected void generatePubkey () {
        updateSelectedFromView();
        selected.saveAndWriteRecent(true, this);
        Intent intent = new Intent(this, GeneratePubkeyActivity.class);
        intent.putExtra("PrivateKey",selected.getSshPrivKey());
        startActivityForResult(intent, Constants.ACTIVITY_GEN_KEY);
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
        getMenuInflater().inflate(R.menu.connectionsetupmenu, menu);
        return true;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onMenuOpened(int, android.view.Menu)
     */
    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        android.util.Log.d(TAG, "onMenuOpened");
        try {
            menu.findItem(R.id.itemDeleteConnection).setEnabled(selected != null && !selected.isNew());
            menu.findItem(R.id.itemSaveAsCopy).setEnabled(selected != null && !selected.isNew());
        } catch (NullPointerException e) {}
        return true;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onOptionsItemSelected(android.view.MenuItem)
     */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.itemSaveAsCopy) {
            if (selected.getNickname().equals(textNickname.getText().toString()))
                textNickname.setText(new String(getString(R.string.copy_of) + " " + selected.getNickname()));
            updateSelectedFromView();
            selected.set_Id(0);
            selected.saveAndWriteRecent(false, this);
            arriveOnPage();
        } else if (itemId == R.id.itemDeleteConnection) {
            Utils.showYesNoPrompt(this, getString(R.string.delete_connection) + "?", getString(R.string.delete_connection) + " " + selected.getNickname() + "?",
                    new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int i) {
                            selected.Gen_delete(database.getWritableDatabase());
                            database.close();
                            arriveOnPage();
                        }
                    }, null);
        }
        return true;
    }

    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch(requestCode) {
        case (Constants.ACTIVITY_GEN_KEY):
            if (resultCode == Activity.RESULT_OK && data != null && data.getExtras() != null) {
                Bundle b = data.getExtras();
                String privateKey = (String)b.get("PrivateKey");
                if (!privateKey.equals(selected.getSshPrivKey()) && privateKey.length() != 0)
                    Toast.makeText(getBaseContext(), getString(R.string.ssh_key_generated), Toast.LENGTH_LONG).show();
                selected.setSshPrivKey(privateKey);
                selected.setSshPubKey((String)b.get("PublicKey"));
                selected.saveAndWriteRecent(true, this);
            } else
                Log.i (TAG, "The user cancelled SSH key generation.");
            break;
        }
    }

    /**
     * Returns the current ConnectionBean.
     */
    public ConnectionBean getCurrentConnection() {
        return selected;
    }
}
