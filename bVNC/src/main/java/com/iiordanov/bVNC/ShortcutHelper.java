/**
 * Copyright (C) 2026- Iordan Iordanov
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

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;

import com.undatech.opaque.Connection;
import com.undatech.remoteClientUi.R;

/**
 * Builds home-screen shortcuts that open a specific saved connection directly in
 * {@link RemoteCanvasActivity}. Supports both the launcher-driven CREATE_SHORTCUT result
 * flow and the in-app request-to-pin flow.
 */
public final class ShortcutHelper {

    private ShortcutHelper() {
    }

    /**
     * Whether the global Master Password is enabled. Shortcut creation is disallowed in that
     * case, since there is currently no way to obtain master password to decrypt the DB for direct launch.
     */
    public static boolean isMasterPasswordEnabled(Context context) {
        return Utils.querySharedPreferenceBoolean(context, Constants.masterPasswordEnabledTag);
    }

    /**
     * Requests that the launcher pin a shortcut for the given connection.
     *
     * @return true if the launcher supports pinning and the request was issued.
     */
    public static boolean requestPinShortcut(Context context, Connection connection) {
        if (!ShortcutManagerCompat.isRequestPinShortcutSupported(context)) {
            return false;
        }
        return ShortcutManagerCompat.requestPinShortcut(context, buildShortcut(context, connection), null);
    }

    /**
     * Builds the result intent returned by a CREATE_SHORTCUT activity.
     */
    @NonNull
    public static Intent createShortcutResultIntent(Context context, Connection connection) {
        return ShortcutManagerCompat.createShortcutResultIntent(context, buildShortcut(context, connection));
    }

    @NonNull
    private static ShortcutInfoCompat buildShortcut(Context context, Connection connection) {
        return new ShortcutInfoCompat.Builder(context, connection.getId())
                .setShortLabel(shortLabel(connection))
                .setIcon(getIcon(context))
                .setIntent(buildLaunchIntent(context, connection.getId()))
                .build();
    }

    @NonNull
    private static Intent buildLaunchIntent(Context context, String connectionId) {
        Intent launchIntent = new Intent(context, RemoteCanvasActivity.class);
        Uri.Builder builder = new Uri.Builder();
        builder.authority(Utils.getConnectionString(context) + ":" + connectionId);
        builder.scheme(Utils.getConnectionScheme(context));
        launchIntent.setData(builder.build());
        launchIntent.setAction(Intent.ACTION_VIEW);
        return launchIntent;
    }

    @NonNull
    private static String shortLabel(Connection connection) {
        String nickname = connection.getNickname();
        if (nickname != null && !nickname.isEmpty()) {
            return nickname;
        }
        return connection.getAddress() + ":" + connection.getPort();
    }

    private static IconCompat getIcon(Context context) {
        int resId = R.drawable.icon_bvnc;
        if (Utils.isRdp(context)) {
            resId = R.drawable.icon_ardp;
        } else if (Utils.isSpice(context)) {
            resId = R.drawable.icon_aspice;
        }
        return IconCompat.createWithResource(context, resId);
    }
}
