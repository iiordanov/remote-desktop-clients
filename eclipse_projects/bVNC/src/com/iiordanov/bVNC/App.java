package com.iiordanov.bVNC;

import android.app.Application;

public class App extends Application {

    private Database database;

    @Override
    public void onCreate() {
        super.onCreate();
        database = new Database(this);
    }

    public Database getDatabase() {
        return database;
    }
}
