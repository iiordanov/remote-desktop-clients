package com.undatech.opaque.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Database;
import com.undatech.opaque.Connection;
import com.undatech.opaque.ConnectionSettings;

import net.zetetic.database.sqlcipher.SQLiteDatabase;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

public class ConnectionLoader {
    private static final String TAG = "ConnectionLoader";
    private final Context appContext;
    private final boolean connectionsInSharedPrefs;
    private final Map<String, Connection> connectionsById;
    private int numConnections = 0;

    public ConnectionLoader(Context appContext, boolean connectionsInSharedPrefs) {
        this.appContext = appContext;
        this.connectionsInSharedPrefs = connectionsInSharedPrefs;
        this.connectionsById = new HashMap<>();
        loadConnectionsById();
    }

    public Map<String, Connection> loadConnectionsById() {
        if (connectionsInSharedPrefs) {
            loadFromSharedPrefs();
        } else {
            loadFromDatabase();
        }
        return connectionsById;
    }

    private void loadFromDatabase() {
        Database database = new Database(this.appContext);
        SQLiteDatabase db = database.getWritableDatabase();

        ArrayList<ConnectionBean> connections = new ArrayList<>();
        ConnectionBean.getAll(db, ConnectionBean.GEN_TABLE_NAME, connections, ConnectionBean.newInstance);
        Collections.sort(connections);
        numConnections = connections.size();
        if (connections.isEmpty()) {
            Log.i(TAG, "No connections in the database");
        } else {
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
        Log.d(TAG, "Loading connections from this list: " + connections);
        if (connections != null && !connections.isEmpty()) {
            String[] connectionPreferenceFiles = connections.split(" ");
            numConnections = connectionPreferenceFiles.length;
            for (int i = 0; i < numConnections; i++) {
                Connection cs = new ConnectionSettings(connectionPreferenceFiles[i]);
                cs.setRuntimeId(Integer.toString(i));
                cs.load(appContext);
                Log.d(TAG, "Adding label: " + cs.getLabel());
                connectionsById.put(Integer.toString(i), cs);
            }
        }
    }

    public Map<String, Connection> getConnectionsById() {
        return connectionsById;
    }
}
