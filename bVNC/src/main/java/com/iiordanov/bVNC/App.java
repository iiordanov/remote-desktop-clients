package com.iiordanov.bVNC;

import android.app.Application;
import android.content.Context;

import java.lang.ref.WeakReference;

public class App extends Application {

    private Database database;
    private static WeakReference<Context> context;

    @Override
    public void onCreate() {
        super.onCreate();
        Constants.DEFAULT_PROTOCOL_PORT = Utils.getDefaultPort(this);
        database = new Database(this);
        context = new WeakReference<Context>(this);
    }

    public Database getDatabase() {
        return database;
    }

    public static Context getContext() {
        return context.get();
    }
}
