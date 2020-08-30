package com.iiordanov.bVNC;

import android.app.Application;
import android.app.job.JobInfo;
import android.content.Context;
import android.support.multidex.MultiDex;
import android.support.multidex.MultiDexApplication;
import android.support.v7.app.AppCompatDelegate;

import java.lang.ref.WeakReference;
import org.acra.*;
import org.acra.annotation.*;
import org.acra.config.CoreConfigurationBuilder;
import org.acra.config.LimiterConfigurationBuilder;
import org.acra.config.MailSenderConfigurationBuilder;
import org.acra.data.StringFormat;

public class App extends MultiDexApplication {

    private Database database;
    private static WeakReference<Context> context;

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        MultiDex.install(getBaseContext());
        CoreConfigurationBuilder builder = new CoreConfigurationBuilder(this)
                .setBuildConfigClass(BuildConfig.class)
                .setReportFormat(StringFormat.JSON);
        builder.getPluginConfigurationBuilder(MailSenderConfigurationBuilder.class)
                .setMailTo(Constants.ERROR_EMAIL_ADDRESS)
                .setEnabled(true);
        /* Requires API 22
        builder.getPluginConfigurationBuilder(SchedulerConfigurationBuilder.class)
                .setRestartAfterCrash(true)
                .setEnabled(true);
        */
        builder.getPluginConfigurationBuilder(LimiterConfigurationBuilder.class)
                .setEnabled(true);
        ACRA.init(this, builder);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
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
