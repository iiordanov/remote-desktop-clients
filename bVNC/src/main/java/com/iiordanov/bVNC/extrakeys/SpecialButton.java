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

import androidx.annotation.NonNull;

import java.util.HashMap;

/** The {@link Class} that implements special buttons for {@link ExtraKeysView}. */
public class SpecialButton {

    private static final HashMap<String, SpecialButton> map = new HashMap<>();

    public static final SpecialButton CTRL = new SpecialButton("CTRL");
    public static final SpecialButton ALT = new SpecialButton("ALT");
    public static final SpecialButton SHIFT = new SpecialButton("SHIFT");
    public static final SpecialButton FN = new SpecialButton("FN");
    public static final SpecialButton SUPER = new SpecialButton("SUPER");

    /** The special button key. */
    private final String key;

    /**
     * Initialize a {@link SpecialButton}.
     *
     * @param key The unique key name for the special button. The key is registered in {@link #map}
     *            with which the {@link SpecialButton} can be retrieved via a call to
     *            {@link #valueOf(String)}.
     */
    public SpecialButton(@NonNull final String key) {
        this.key = key;
        map.put(key, this);
    }

    /** Get {@link #key} for this {@link SpecialButton}. */
    public String getKey() {
        return key;
    }

    /**
     * Get the {@link SpecialButton} for {@code key}.
     *
     * @param key The unique key name for the special button.
     */
    public static SpecialButton valueOf(String key) {
        return map.get(key);
    }

    @NonNull
    @Override
    public String toString() {
        return key;
    }

}
