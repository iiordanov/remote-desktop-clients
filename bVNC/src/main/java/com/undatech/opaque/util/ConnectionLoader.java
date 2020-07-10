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

//For loading in Connections of bVNC
import java.util.ArrayList;
import java.util.Collections;
import net.sqlcipher.database.SQLiteDatabase;
import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.App;

import org.json.JSONException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class ConnectionLoader {
    private static String TAG = "ConnectionLoader";
    private Context appContext;
    private boolean connectionsInSharedPrefs = false;
    private Map<String, Connection> connectionsById;
    private String[] connectionPreferenceFiles;
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
        this.connectionsById = new HashMap<>();
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
        Database database = new Database(this.appContext);
        SQLiteDatabase db = database.getReadableDatabase();

        ArrayList<ConnectionBean> connections = new ArrayList<ConnectionBean>();
        ConnectionBean.getAll(db, ConnectionBean.GEN_TABLE_NAME, connections, ConnectionBean.newInstance);
        Collections.sort(connections);
        numConnections = connections.size();
        if (connections.size() == 0) {
            android.util.Log.e(TAG, "No connections in the database");
        }
        else {
            for (int i = 0; i < connections.size(); i++) {
                Connection connection = connections.get(i);
                connection.setRuntimeId(Integer.toString(i));
                connectionsById.put(Integer.toString(i), connection);
            }
        }
        database.close();
    }

    private void loadFromSharedPrefs() {
        SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connections = sp.getString("connections", null);
        android.util.Log.d(TAG, "Loading connections from this list: " + connections);
        if (connections != null && !connections.equals("")) {
            connectionPreferenceFiles = connections.split(" ");
            numConnections = connectionPreferenceFiles.length;
            for (int i = 0; i < numConnections; i++) {
                Connection cs = new ConnectionSettings(connectionPreferenceFiles[i]);
                cs.setRuntimeId(Integer.toString(i));
                cs.load(appContext);
                android.util.Log.d(TAG, "Adding label: " + cs.getLabel());
                connectionsById.put(Integer.toString(i), cs);
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

    public int getNumConnections() {
        return numConnections;
    }

    public Map<String, Connection> getConnectionsById() {
        return connectionsById;
    }
}
