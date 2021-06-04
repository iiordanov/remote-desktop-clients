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
    public static ClientAPISettings clientAPISettings;
    public static ApiClient apiClient;
    public static Gson gson;
    public static EventLiveDataBus eldb;
    public static SharedPreferences sharedPrefs;
    public static CertRepository certRepo;
    public static String node_id;
    public static NodeAndSettings nodeAndSettings;
    public static WireguardKeyRepository wireguardKeyRepo;
    public static Backend backend;

    public static Map<String, Action> actions;
    public static long actionsLastRetrieved = 0;

    public static CryptUtils cryptUtils;
    volatile public static String cookie;

    public static ArrayList<TunnelValues> tunnels;
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
        App.clientAPISettings = ClientAPISettings.getInstance();
        App.clientAPISettings.setContext(App.getContext());
        App.clientAPISettings.initUtility();

        App.gson = ClientAPISettings.gson;
        App.eldb = this.clientAPISettings.getBus();
        App.cryptUtils = ClientAPISettings.cryptUtils;

        ClientAPISettings.allowInsecure = Utils.querySharedPreferenceBoolean(
                getApplicationContext(), "allow_insecure");
        ClientAPISettings.apiServer = Utils.querySharedPreferenceString(getApplicationContext(),
                "api_server", "127.0.0.1");

        ClientAPISettings.getInstance().setApiClient(new ApiClient(this));


        App.sharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);
        com.trinity.android.apiclient.utils.Utils.setSharedPrefs(App.sharedPrefs);
        App.node_id = App.sharedPrefs.getString(getString(R.string.node_id_key), null);
        if (App.node_id == null) {
            App.node_id = java.util.UUID.randomUUID().toString();
            com.trinity.android.apiclient.utils.Utils.saveStringToSharedPrefs(getString(R.string.node_id_key), App.node_id);
        }
        App.certRepo = new CertRepository(
                getString(R.string.ssl_key_pref_key),
                getString(R.string.ssl_cert_pref_key),
                App.sharedPrefs);
        ClientAPISettings.getInstance().setCertRepo(App.certRepo);
        App.wireguardKeyRepo = new WireguardKeyRepository(
                getString(R.string.wg_priv_pref_key),
                getString(R.string.wg_pub_pref_key),
                getString(R.string.wg_priv_ind_pref_key),
                getString(R.string.wg_pub_ind_pref_key),
                App.sharedPrefs);
        ClientAPISettings.getInstance().setWireguardKeyRepo(App.wireguardKeyRepo);

        /* Start Load Actions */
        String actionsJson = App.sharedPrefs.getString("actions", null);
        android.util.Log.d(TAG, "App actions are " + actionsJson);
        App.actions = new LinkedHashTreeMap<String, Action>();
        Map<String, LinkedTreeMap> tempActions = App.gson.fromJson(actionsJson, Map.class);
        if (tempActions == null) {
            tempActions = new HashMap<>();
        }
        for (String key : tempActions.keySet()) {
            android.util.Log.d(TAG, "App action key is " + key);
            LinkedTreeMap tempMap = tempActions.get(key);
            String temp = App.gson.toJson(tempMap, LinkedTreeMap.class);
            Action action = App.gson.fromJson(temp, Action.class);
            android.util.Log.d(TAG, "Action is " + action.toString());
            //android.util.Log.d(TAG, action.toString());
            App.actions.put(key, action);
        }
        /* End Load Actions */

        if (App.tunnels == null) {
            App.tunnels = new ArrayList<>();
        }
        ClientAPISettings.getInstance().setTunnels(App.tunnels);
        App.backend = new GoBackend(this);
        ClientAPISettings.getInstance().setBackend(App.backend);
        //End variables for Morpheusly API
    }

    public Database getDatabase() {
        return database;
    }

    public static Context getContext() {
        return context.get();
    }
}
