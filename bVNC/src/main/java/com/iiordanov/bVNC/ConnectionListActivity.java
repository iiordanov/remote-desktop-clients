/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2009-2010 Michael A. MacDonald
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import static androidx.core.content.pm.ShortcutManagerCompat.createShortcutResultIntent;

import android.app.ListActivity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.Intent.ShortcutIconResource;
import android.content.SharedPreferences;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.database.Cursor;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import com.undatech.remoteClientUi.R;

/**
 * @author Michael A. MacDonald
 */
public class ConnectionListActivity extends ListActivity {

    private static final String TAG = "ConnectionListActivity";
    Database database;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        database = new Database(this);

        if (isMasterPasswordEnabled()) {
            Utils.showFatalErrorMessage(
                    this, getResources().getString(R.string.master_password_error_shortcuts_not_supported));
            return;
        }

        // Put a managed wrapper around the retrieved cursor so we don't have to worry about
        // requerying or closing it as the activity changes state.
        Cursor mCursor = database.getReadableDatabase().query(
                ConnectionBean.GEN_TABLE_NAME, new String[]{
                        ConnectionBean.GEN_FIELD__ID,
                        ConnectionBean.GEN_FIELD_NICKNAME,
                        ConnectionBean.GEN_FIELD_USERNAME,
                        ConnectionBean.GEN_FIELD_ADDRESS,
                        ConnectionBean.GEN_FIELD_PORT,
                        ConnectionBean.GEN_FIELD_REPEATERID},
                ConnectionBean.GEN_FIELD_KEEPPASSWORD + " <> 0",
                null,
                null,
                null,
                ConnectionBean.GEN_FIELD_NICKNAME
        );

        startManagingCursor(mCursor);

        // Now create a new list adapter bound to the cursor. 
        // SimpleListAdapter is designed for binding to a Cursor.
        SimpleCursorAdapter adapter = new SimpleCursorAdapter(
                this, // Context.
                R.layout.connection_list,
                mCursor,                                    // Pass in the cursor to bind to.
                new String[]{
                        ConnectionBean.GEN_FIELD_NICKNAME,
                        ConnectionBean.GEN_FIELD_ADDRESS,
                        ConnectionBean.GEN_FIELD_PORT,
                        ConnectionBean.GEN_FIELD_REPEATERID}, // Array of cursor columns to bind to.
                new int[]{
                        R.id.list_text_nickname,
                        R.id.list_text_address,
                        R.id.list_text_port,
                        R.id.list_text_repeater
                });                                 // Parallel array of which template objects to bind to those columns.

        // Bind to our new adapter.
        setListAdapter(adapter);
    }

    /* (non-Javadoc)
     * @see android.app.ListActivity#onListItemClick(android.widget.ListView, android.view.View, int, long)
     */
    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        ConnectionBean connection = new ConnectionBean(this);
        if (connection.Gen_read(database.getReadableDatabase(), id)) {
            Log.d(TAG, "Got a readable database");
            Intent intent = getShortcutIntent(connection);
            setResult(RESULT_OK, intent);
            Log.d(TAG, "RESULT_OK");
        } else {
            setResult(RESULT_CANCELED);
            Log.d(TAG, "RESULT_CANCELED");
        }
        finish();
    }

    private Intent getShortcutIntent(ConnectionBean connection) {
        Intent intent;
        Context context = getApplicationContext();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            intent = setupNewShortcut(context, connection);
        } else {
            intent = setupLegacyShortcut(context, connection);
        }
        return intent;
    }

    private ShortcutIconResource getIcon(Context context) {
        ShortcutIconResource icon = ShortcutIconResource.fromContext(this, R.drawable.icon_bvnc);
        if (Utils.isRdp(context)) {
            icon = ShortcutIconResource.fromContext(this, R.drawable.icon_bvnc);
        } else if (Utils.isSpice(context)) {
            icon = ShortcutIconResource.fromContext(this, R.drawable.icon_aspice);
        }
        return icon;
    }

    private Intent setupLegacyShortcut(Context context, ConnectionBean connection) {
        Log.i(TAG, "Setting up a legacy style shortcut.");
        Intent intent = new Intent();
        Intent launchIntent = getLaunchIntent(connection);
        Log.d(TAG, "EXTRA_SHORTCUT_INTENT: " + launchIntent.getData());
        intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, launchIntent);
        Log.d(TAG, "EXTRA_SHORTCUT_NAME: " + getShortLabel(connection));
        intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, getShortLabel(connection));
        ShortcutIconResource icon = getIcon(context);
        Log.d(TAG, "EXTRA_SHORTCUT_ICON_RESOURCE: " + icon);
        intent.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE, icon);
        return intent;
    }

    @NonNull
    private Intent getLaunchIntent(ConnectionBean connection) {
        Intent launchIntent = new Intent(this, RemoteCanvasActivity.class);
        Uri.Builder builder = new Uri.Builder();
        builder.authority(Utils.getConnectionString(this) + ":" + connection.get_Id());
        builder.scheme(Utils.getConnectionScheme(this));
        launchIntent.setData(builder.build());
        launchIntent.setAction(Intent.ACTION_VIEW);
        return launchIntent;
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    private Icon getNewIcon(Context context) {
        Icon icon = Icon.createWithResource(context, R.drawable.icon_bvnc);
        if (Utils.isRdp(context)) {
            icon = Icon.createWithResource(context, R.drawable.icon_ardp);
        } else if (Utils.isSpice(context)) {
            icon = Icon.createWithResource(context, R.drawable.icon_aspice);
        }
        return icon;
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    private Intent setupNewShortcut(Context context, ConnectionBean connection) {
        Log.i(TAG, "Setting up a new style shortcut.");
        ShortcutManager shortcutManager =
                context.getSystemService(ShortcutManager.class);
        Intent launchIntent = getLaunchIntent(connection);
        ShortcutInfo pinShortcutInfo =
                new ShortcutInfo.Builder(context, String.valueOf(connection.get_Id()))
                        .setShortLabel(getShortLabel(connection))
                        .setIcon(getNewIcon(context))
                        .setIntent(launchIntent).build();
        return shortcutManager.createShortcutResultIntent(pinShortcutInfo);
    }

    private static String getShortLabel(ConnectionBean connection) {
        String label = connection.getNickname();
        if (label == null || "".equals(label)) {
            label = connection.getAddress() + ":" + connection.getPort();
        }
        return label;
    }

    /* (non-Javadoc)
     * @see android.app.Activity#onDestroy()
     */
    @Override
    protected void onDestroy() {
        if (database != null) {
            database.close();
        }
        super.onDestroy();
    }

    private boolean isMasterPasswordEnabled() {
        SharedPreferences sp = getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
        return sp.getBoolean(Constants.masterPasswordEnabledTag, false);
    }
}
