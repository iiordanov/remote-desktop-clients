package com.iiordanov.bVNC;

import android.content.Context;
import android.util.Log;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.multidex.MultiDex;
import androidx.multidex.MultiDexApplication;

import java.io.IOException;
import java.lang.ref.WeakReference;

import com.iiordanov.util.CustomClientConfigFileReader;

public class App extends MultiDexApplication {
    private final static String TAG = "App";

    public static boolean debugLog = false;
    private static WeakReference<Context> context;
    private Database database;

    public static CustomClientConfigFileReader configFileReader;

    public static Context getContext() {
        return context.get();
    }

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        MultiDex.install(getBaseContext());
    }

    @Override
    public void onCreate() {
        super.onCreate();
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
        Constants.DEFAULT_PROTOCOL_PORT = Utils.getDefaultPort(this);
        database = new Database(this);
        context = new WeakReference<Context>(this);
        if(Utils.isCustom(getApplicationContext())){
            try {
                configFileReader = new CustomClientConfigFileReader(
                        getApplicationContext().getAssets().open(Utils.pName(getApplicationContext()) + ".yaml"));
            } catch (IOException e) {
                Log.e(TAG, "Error opening config file from assets.");
            }
        }
        debugLog = Utils.querySharedPreferenceBoolean(getApplicationContext(), "moreDebugLoggingTag");
    }

    public Database getDatabase() {
        return database;
    }

    public static CustomClientConfigFileReader getConfigFileReader(){ return configFileReader;}
}
