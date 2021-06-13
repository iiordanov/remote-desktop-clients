package com.iiordanov.bVNC;

//Begin imports for Morpheusly API
import com.google.gson.internal.LinkedHashTreeMap;
import com.google.gson.internal.LinkedTreeMap;
import com.trinity.android.apiclient.utils.CertRepository;
import com.trinity.android.apiclient.utils.ClientAPISettings;
import com.trinity.android.apiclient.utils.EventLiveDataBus;
import com.google.gson.Gson;

import com.trinity.android.apiclient.models.Action;
import com.trinity.android.apiclient.models.Node;
import com.trinity.android.apiclient.models.NodeAndSettings;

import com.trinity.android.apiclient.ApiClient;
import com.trinity.android.apiclient.utils.CryptUtils;
import com.trinity.android.apiclient.utils.EventLiveDataBus;
import com.trinity.android.apiclient.utils.WireguardKeyRepository;
import com.trinity.android.apiclient.models.TunnelValues;
//End imports for Morpheusly API

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import androidx.multidex.MultiDex;
import androidx.multidex.MultiDexApplication;
import androidx.appcompat.app.AppCompatDelegate;

import com.undatech.remoteClientUi.R;
import com.wireguard.android.backend.Backend;
import com.wireguard.android.backend.GoBackend;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class App extends MultiDexApplication {
    public static final String TAG = "App";

    private Database database;
    private static WeakReference<Context> context;
    public static boolean debugLog = false;

    //Begin variables for Morpheusly API
    public static App app;
    public static ClientAPISettings clientAPISettings;
    //End variables for Morpheusly API

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        MultiDex.install(getBaseContext());
    }

    @Override
    public void onCreate() {
        android.util.Log.d(TAG, "App starting");
        super.onCreate();
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
        Constants.DEFAULT_PROTOCOL_PORT = Utils.getDefaultPort(this);
        database = new Database(this);
        context = new WeakReference<Context>(this);
        debugLog = Utils.querySharedPreferenceBoolean(getApplicationContext(), "moreDebugLoggingTag");

        //Begin variables for Morpheusly API
        App.clientAPISettings = ClientAPISettings.getInstance(this);

        /* Start Load Nodes */
        String nodesJson = ClientAPISettings.getInstance(null).getSharedPrefs().getString("nodes", null);
        android.util.Log.d(TAG, "App nodes are " + nodesJson);
        Map<String, Node> nodes = new LinkedHashTreeMap<String, Node>();
        Map<String, LinkedTreeMap> tempNodes = ClientAPISettings.getInstance(null).getGson().fromJson(nodesJson, Map.class);
        if (tempNodes == null) {
            tempNodes = new HashMap<>();
        }
        for (String key : tempNodes.keySet()) {
            android.util.Log.d(TAG, "App node key is " + key);
            LinkedTreeMap tempMap = tempNodes.get(key);
            String temp = ClientAPISettings.getInstance(null).getGson().toJson(tempMap, LinkedTreeMap.class);
            Node node = ClientAPISettings.getInstance(null).getGson().fromJson(temp, Node.class);
            android.util.Log.d(TAG, "Node is " + node.toString());
            //android.util.Log.d(TAG, action.toString());
            nodes.put(key, node);
        }
        ClientAPISettings.getInstance(null).setNodes(nodes);
        /* End Load Nodes*/

        /* Start Load Actions */
        String actionsJson = ClientAPISettings.getInstance(null).getSharedPrefs().getString("actions", null);
        android.util.Log.d(TAG, "App actions are " + actionsJson);
        Map<String, Action> actions = new LinkedHashTreeMap<String, Action>();
        Map<String, LinkedTreeMap> tempActions = ClientAPISettings.getInstance(null).getGson().fromJson(actionsJson, Map.class);
        if (tempActions == null) {
            tempActions = new HashMap<>();
        }
        for (String key : tempActions.keySet()) {
            android.util.Log.d(TAG, "App action key is " + key);
            LinkedTreeMap tempMap = tempActions.get(key);
            String temp = ClientAPISettings.getInstance(null).getGson().toJson(tempMap, LinkedTreeMap.class);
            Action action = ClientAPISettings.getInstance(null).getGson().fromJson(temp, Action.class);
            android.util.Log.d(TAG, "Action is " + action.toString());
            //android.util.Log.d(TAG, action.toString());
            actions.put(key, action);
        }
        ClientAPISettings.getInstance(null).setActions(actions);
        /* End Load Actions */

        //End variables for Morpheusly API
    }

    public Database getDatabase() {
        return database;
    }

    public static Context getContext() {
        return context.get();
    }
}
