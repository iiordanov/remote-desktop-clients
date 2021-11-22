package com.iiordanov.bVNC;

import android.content.Context;
import androidx.multidex.MultiDex;
import androidx.multidex.MultiDexApplication;
import androidx.appcompat.app.AppCompatDelegate;

import java.lang.ref.WeakReference;

public class App extends MultiDexApplication {

    private Database database;
    private static WeakReference<Context> context;
    public static boolean debugLog = false;

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
        debugLog = Utils.querySharedPreferenceBoolean(getApplicationContext(), "moreDebugLoggingTag");
    }

    public Database getDatabase() {
        return database;
    }

    public static Context getContext() {
        return context.get();
    }
}
