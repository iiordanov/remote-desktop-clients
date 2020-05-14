package com.undatech.opaque.util;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Environment;
import android.support.v4.app.FragmentManager;
import android.view.MenuItem;

import com.undatech.opaque.Connection;
import com.undatech.opaque.ConnectionGridActivity;
import com.undatech.opaque.ConnectionSettings;
import com.undatech.opaque.LabeledImageApapter;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.remoteClientUi.R;

import org.json.JSONException;

import java.io.IOException;

public class ConnectionLoader {
    private static String TAG = "ConnectionLoader";
    private Context appContext;
    private boolean connectionsInSharedPrefs = false;
    private String[] connectionPreferenceFiles;
    private String[] screenshotFiles;
    private String[] connectionLabels;
    private int numConnections = 0;
    private PermissionsManager permissionsManager;
    private FragmentManager fm;
    private Activity activity;

    public ConnectionLoader(Context appContext, Activity activity, FragmentManager fm, boolean connectionsInSharedPrefs) {
        this.appContext = appContext;
        this.connectionsInSharedPrefs = connectionsInSharedPrefs;
        this.permissionsManager = permissionsManager;
        this.fm = fm;
        this.activity = activity;
        permissionsManager = new PermissionsManager();
        permissionsManager.requestPermissions(activity);
        load();
    }

    public void load() {
        permissionsManager.requestPermissions(activity);
        if (connectionsInSharedPrefs) {
            loadFromSharedPrefs();
        } else {
            loadFromDatabase();
        }
    }

    private void loadFromDatabase() {
        android.util.Log.e(TAG, "Missing implementation of loadFromDatabase()");
    }

    private void loadFromSharedPrefs() {
        SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        android.util.Log.d(TAG, "Loading connections from this list: " + connections);
        if (connections != null && !connections.equals("")) {
            connectionPreferenceFiles = connections.split(" ");
            numConnections = connectionPreferenceFiles.length;
            screenshotFiles = new String[numConnections];
            connectionLabels = new String[numConnections];
            for (int i = 0; i < numConnections; i++) {
                Connection cs = new ConnectionSettings(connectionPreferenceFiles[i]);
                cs.load(appContext);
                connectionLabels[i] = cs.getVmname();
                android.util.Log.d(TAG, "Adding label: " + connectionLabels[i]);
                String location = cs.getFilename();
                screenshotFiles[i] = appContext.getFilesDir() + "/" + location + ".png";
            }
        }
    }

    public void importSettings() {
        if (connectionsInSharedPrefs) {
            importToSharedPrefs();
        } else {
            importToDatabase();
        }
    }

    private void importToSharedPrefs () {
        permissionsManager.requestPermissions(activity);

        String pathToFile = FileUtils.join(Environment.getExternalStorageDirectory().toString(),
                RemoteClientLibConstants.EXPORT_SETTINGS_FILE);

        try {
            String connections = ConnectionSettings.importPrefsFromFile(appContext, pathToFile);
            SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
            SharedPreferences.Editor editor = sp.edit();
            editor.putString("connections", connections);
            editor.apply();
            load();
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.info_dialog_title),
                    "Imported settings from " + pathToFile, "OK", null);
            message.show(fm, "successImportingSettings");
        } catch (IOException e) {
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.info_dialog_title),
                    "Could not read settings settings file " + pathToFile, "OK", null);
            message.show(fm, "errorImportingSettings");
            e.printStackTrace();
        } catch (JSONException e) {
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.info_dialog_title),
                    "Could not parse JSON from settings file " + pathToFile, "OK", null);
            message.show(fm, "errorImportingSettings");
            e.printStackTrace();
        }

    }

    private void importToDatabase() {
        android.util.Log.e(TAG, "Missing implementation of importToDatabase()");
    }

    public void exportSettings() {
        if (connectionsInSharedPrefs) {
            exportFromSharedPreferences();
        } else {
            exportFromDatabase();
        }
    }

    private void exportFromSharedPreferences () {
        permissionsManager.requestPermissions(activity);

        String pathToFile = FileUtils.join(Environment.getExternalStorageDirectory().toString(),
                RemoteClientLibConstants.EXPORT_SETTINGS_FILE);
        SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        try {
            ConnectionSettings.exportPrefsToFile(appContext, connections, pathToFile);
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.info_dialog_title),
                    "Exported settings to " + pathToFile, "OK", null);
            message.show(fm, "successExportingSettings");
        } catch (JSONException e) {
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.error_dialog_title),
                    "Could not convert settings to JSON", "OK", null);
            message.show(fm, "errorExportingSettings");
            e.printStackTrace();
        } catch (IOException e) {
            MessageFragment message = MessageFragment.newInstance(appContext.getString(R.string.error_dialog_title),
                    "Could write to settings file " + pathToFile, "OK", null);
            message.show(fm, "errorExportingSettings");
            e.printStackTrace();
        }
    }

    private void exportFromDatabase() {
        android.util.Log.e(TAG, "Missing implementation of exportFromDatabase()");
    }

    public boolean isConnectionsInSharedPrefs() {
        return connectionsInSharedPrefs;
    }

    public String[] getConnectionPreferenceFiles() {
        return connectionPreferenceFiles;
    }

    public String[] getScreenshotFiles() {
        return screenshotFiles;
    }

    public String[] getConnectionLabels() {
        return connectionLabels;
    }

    public int getNumConnections() {
        return numConnections;
    }
}
