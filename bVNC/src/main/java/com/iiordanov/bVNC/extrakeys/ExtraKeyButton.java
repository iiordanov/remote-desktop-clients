/**
 * Copyright (C) 2026 Iordan Iordanov
 * Copyright (C) 2021-2026 Termux App authors
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU GPL version 3 General Public License
 * as published by the Free Software Foundation.
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

package com.iiordanov.bVNC.extrakeys;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

public class ExtraKeyButton {

    /** The key name for the name of the extra key if using a dict to define the extra key. {key: name, ...} */
    public static final String KEY_KEY_NAME = "key";

    /** The key name for the macro value of the extra key if using a dict to define the extra key. {macro: value, ...} */
    public static final String KEY_MACRO = "macro";

    /** The key name for the alternate display name of the extra key if using a dict to define the extra key. {display: name, ...} */
    public static final String KEY_DISPLAY_NAME = "display";

    /** The key name for the nested dict to define popup extra key info if using a dict to define the extra key. {popup: {key: name, ...}, ...} */
    public static final String KEY_POPUP = "popup";


    /**
     * The key that will be sent to the remote session, either a control character, like defined in
     * {@link ExtraKeysConstants#PRIMARY_KEY_CODES_FOR_STRINGS} (LEFT, RIGHT, PGUP...) or some text.
     */
    private final String key;

    /**
     * If the key is a macro, i.e. a sequence of keys separated by space.
     */
    private final boolean macro;

    /**
     * The text that will be displayed on the button.
     */
    private final String display;

    /**
     * The {@link ExtraKeyButton} containing the information of the popup button (triggered by swipe up).
     */
    @Nullable
    private final ExtraKeyButton popup;


    /**
     * Initialize a {@link ExtraKeyButton}.
     *
     * @param config The {@link JSONObject} containing the info to create the {@link ExtraKeyButton}.
     * @param extraKeyDisplayMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           display text mapping for the keys if a custom value is not defined
     *                           by {@link #KEY_DISPLAY_NAME}.
     * @param extraKeyAliasMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           aliases for the actual key names.
     */
    public ExtraKeyButton(@NonNull JSONObject config,
                          @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyDisplayMap,
                          @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap) throws JSONException {
        this(config, null, extraKeyDisplayMap, extraKeyAliasMap);
    }

    /**
     * Initialize a {@link ExtraKeyButton}.
     *
     * @param config The {@link JSONObject} containing the info to create the {@link ExtraKeyButton}.
     * @param popup The {@link ExtraKeyButton} optional {@link #popup} button.
     * @param extraKeyDisplayMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           display text mapping for the keys if a custom value is not defined
     *                           by {@link #KEY_DISPLAY_NAME}.
     * @param extraKeyAliasMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           aliases for the actual key names.
     */
    public ExtraKeyButton(@NonNull JSONObject config, @Nullable ExtraKeyButton popup,
                          @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyDisplayMap,
                          @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap) throws JSONException {
        String keyFromConfig = getStringFromJson(config, KEY_KEY_NAME);
        String macroFromConfig = getStringFromJson(config, KEY_MACRO);
        String[] keys;
        if (keyFromConfig != null && macroFromConfig != null) {
            throw new JSONException("Both key and macro can't be set for the same key. key: \"" + keyFromConfig + "\", macro: \"" + macroFromConfig + "\"");
        } else if (keyFromConfig != null) {
            keys = new String[]{keyFromConfig};
            this.macro = false;
        } else if (macroFromConfig != null) {
            keys = macroFromConfig.split(" ");
            this.macro = true;
        } else {
            throw new JSONException("All keys have to specify either key or macro");
        }

        for (int i = 0; i < keys.length; i++) {
            keys[i] = replaceAlias(extraKeyAliasMap, keys[i]);
        }

        this.key = TextUtils.join(" ", keys);

        String displayFromConfig = getStringFromJson(config, KEY_DISPLAY_NAME);
        if (displayFromConfig != null) {
            this.display = displayFromConfig;
        } else {
            this.display = getDisplayString(extraKeyDisplayMap, keys);
        }

        this.popup = popup;
    }

    private static String getDisplayString(ExtraKeysConstants.ExtraKeyDisplayMap extraKeyDisplayMap, String[] keys) {
        List<String> parts = new ArrayList<>(keys.length);
        for (String key : keys) {
            parts.add(extraKeyDisplayMap.get(key, key));
        }
        return TextUtils.join(" ", parts);
    }

    private String getStringFromJson(@NonNull JSONObject config, @NonNull String key) {
        try {
            return config.getString(key);
        } catch (JSONException e) {
            return null;
        }
    }

    /** Get {@link #key}. */
    public String getKey() {
        return key;
    }

    /** Check whether a {@link #macro} is defined or not. */
    public boolean isMacro() {
        return macro;
    }

    /** Get {@link #display}. */
    public String getDisplay() {
        return display;
    }

    /** Get {@link #popup}. */
    @Nullable
    public ExtraKeyButton getPopup() {
        return popup;
    }

    /**
     * Replace the alias with its actual key name if found in extraKeyAliasMap.
     */
    public static String replaceAlias(@NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap, String key) {
        return extraKeyAliasMap.get(key, key);
    }

}
