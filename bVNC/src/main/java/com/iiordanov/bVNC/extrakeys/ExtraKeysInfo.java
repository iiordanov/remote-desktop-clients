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

import android.view.View;

import androidx.annotation.NonNull;

import com.google.android.material.button.MaterialButton;
import com.iiordanov.bVNC.extrakeys.ExtraKeysConstants.EXTRA_KEY_DISPLAY_MAPS;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A {@link Class} that defines the info needed by {@link ExtraKeysView} to display the extra key
 * views.
 * <p>
 * The {@code propertiesInfo} passed to the constructors of this class must be json array of arrays.
 * Each array element of the json array will be considered a separate row of keys.
 * Each key can either be simple string that defines the name of the key or a json dict that defines
 * advance info for the key. The syntax can be `'KEY'` or `{key: 'KEY'}`.
 * For example `HOME` or `{key: 'HOME', ...}.
 * <p>
 * In advance json dict mode, the key can also be a sequence of space separated keys instead of one
 * key. This can be done by replacing `key` key/value pair of the dict with a `macro` key/value pair.
 * The syntax is `{macro: 'KEY COMBINATION'}`. For example {macro: 'HOME RIGHT', ...}.
 * <p>
 * In advance json dict mode, you can define a nested json dict with the `popup` key which will be
 * used as the popup key and will be triggered on swipe up. The syntax can be
 * `{key: 'KEY', popup: 'POPUP_KEY'}` or `{key: 'KEY', popup: {macro: 'KEY COMBINATION', display: 'Key combo'}}`.
 * For example `{key: 'HOME', popup: {KEY: 'END', ...}, ...}`.
 * <p>
 * In advance json dict mode, the key can also have a custom display name that can be used as the
 * text to display on the button by defining the `display` key. The syntax is `{display: 'DISPLAY'}`.
 * For example {display: 'Custom name', ...}.
 * <p>
 * Aliases are also allowed for the keys that you can pass as {@code extraKeyAliasMap}. Check
 * {@link ExtraKeysConstants#CONTROL_CHARS_ALIASES}.
 * <p>
 * Its up to the {@link ExtraKeysView.IExtraKeysView} client on how to handle individual key values
 * of an {@link ExtraKeyButton}. They are sent as is via
 * {@link ExtraKeysView.IExtraKeysView#onExtraKeyButtonClick(View, ExtraKeyButton, MaterialButton)}. The
 * {@link RemoteExtraKeysHandler} which is an implementation of the interface,
 * checks if the key is one of {@link ExtraKeysConstants#PRIMARY_KEY_CODES_FOR_STRINGS} and generates
 * a {@link android.view.KeyEvent} for it, and if its not, then sends the key as a literal character.
 */
public class ExtraKeysInfo {

    /**
     * Matrix of buttons to be displayed in {@link ExtraKeysView}.
     */
    private final ExtraKeyButton[][] mButtons;

    /**
     * Initialize {@link ExtraKeysInfo}.
     *
     * @param propertiesInfo The {@link String} containing the info to create the {@link ExtraKeysInfo}.
     *                       Check the class javadoc for details.
     * @param style The style to pass to {@link #getCharDisplayMapForStyle(String)} to get the
     *              {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the display text
     *              mapping for the keys if a custom value is not defined by
     *              {@link ExtraKeyButton#KEY_DISPLAY_NAME} for a key.
     * @param extraKeyAliasMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           aliases for the actual key names. You can create your own or
     *                           optionally pass {@link ExtraKeysConstants#CONTROL_CHARS_ALIASES}.
     */
    public ExtraKeysInfo(@NonNull String propertiesInfo, String style,
                         @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap) throws JSONException {
        mButtons = initExtraKeysInfo(propertiesInfo, getCharDisplayMapForStyle(style), extraKeyAliasMap);
    }

    /**
     * Initialize {@link ExtraKeysInfo}.
     *
     * @param propertiesInfo The {@link String} containing the info to create the {@link ExtraKeysInfo}.
     *                       Check the class javadoc for details.
     * @param extraKeyDisplayMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           display text mapping for the keys if a custom value is not defined
     *                           by {@link ExtraKeyButton#KEY_DISPLAY_NAME} for a key. You can create
     *                           your own or optionally pass one of the values defined in
     *                           {@link #getCharDisplayMapForStyle(String)}.
     * @param extraKeyAliasMap The {@link ExtraKeysConstants.ExtraKeyDisplayMap} that defines the
     *                           aliases for the actual key names. You can create your own or
     *                           optionally pass {@link ExtraKeysConstants#CONTROL_CHARS_ALIASES}.
     */
    public ExtraKeysInfo(@NonNull String propertiesInfo,
                         @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyDisplayMap,
                         @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap) throws JSONException {
        mButtons = initExtraKeysInfo(propertiesInfo, extraKeyDisplayMap, extraKeyAliasMap);
    }

    private ExtraKeyButton[][] initExtraKeysInfo(@NonNull String propertiesInfo,
                                                 @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyDisplayMap,
                                                 @NonNull ExtraKeysConstants.ExtraKeyDisplayMap extraKeyAliasMap) throws JSONException {
        // Convert String propertiesInfo to Array of Arrays
        JSONArray arr = new JSONArray(propertiesInfo);
        Object[][] matrix = new Object[arr.length()][];
        for (int i = 0; i < arr.length(); i++) {
            JSONArray line = arr.getJSONArray(i);
            matrix[i] = new Object[line.length()];
            for (int j = 0; j < line.length(); j++) {
                matrix[i][j] = line.get(j);
            }
        }

        // convert matrix to buttons
        ExtraKeyButton[][] buttons = new ExtraKeyButton[matrix.length][];
        for (int i = 0; i < matrix.length; i++) {
            buttons[i] = new ExtraKeyButton[matrix[i].length];
            for (int j = 0; j < matrix[i].length; j++) {
                Object key = matrix[i][j];

                JSONObject jobject = normalizeKeyConfig(key);

                ExtraKeyButton button;

                if (!jobject.has(ExtraKeyButton.KEY_POPUP)) {
                    // no popup
                    button = new ExtraKeyButton(jobject, extraKeyDisplayMap, extraKeyAliasMap);
                } else {
                    // a popup
                    JSONObject popupJobject = normalizeKeyConfig(jobject.get(ExtraKeyButton.KEY_POPUP));
                    ExtraKeyButton popup = new ExtraKeyButton(popupJobject, extraKeyDisplayMap, extraKeyAliasMap);
                    button = new ExtraKeyButton(jobject, popup, extraKeyDisplayMap, extraKeyAliasMap);
                }

                buttons[i][j] = button;
            }
        }

        return buttons;
    }

    /**
     * Convert "value" -> {"key": "value"}. Required by
     * {@link ExtraKeyButton#ExtraKeyButton(JSONObject, ExtraKeyButton, ExtraKeysConstants.ExtraKeyDisplayMap, ExtraKeysConstants.ExtraKeyDisplayMap)}.
     */
    private static JSONObject normalizeKeyConfig(Object key) throws JSONException {
        JSONObject jobject;
        if (key instanceof String) {
            jobject = new JSONObject();
            jobject.put(ExtraKeyButton.KEY_KEY_NAME, key);
        } else if (key instanceof JSONObject) {
            jobject = (JSONObject) key;
        } else {
            throw new JSONException("An key in the extra-key matrix must be a string or an object");
        }
        return jobject;
    }

    public ExtraKeyButton[][] getMatrix() {
        return mButtons;
    }

    @NonNull
    public static ExtraKeysConstants.ExtraKeyDisplayMap getCharDisplayMapForStyle(String style) {
        return switch (style) {
            case "arrows-only" -> EXTRA_KEY_DISPLAY_MAPS.ARROWS_ONLY_CHAR_DISPLAY;
            case "arrows-all" -> EXTRA_KEY_DISPLAY_MAPS.LOTS_OF_ARROWS_CHAR_DISPLAY;
            case "all" -> EXTRA_KEY_DISPLAY_MAPS.FULL_ISO_CHAR_DISPLAY;
            case "none" -> new ExtraKeysConstants.ExtraKeyDisplayMap();
            default -> EXTRA_KEY_DISPLAY_MAPS.DEFAULT_CHAR_DISPLAY;
        };
    }

}
