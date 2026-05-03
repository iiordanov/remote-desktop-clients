/**
 * Copyright (C) 2026 Iordan Iordanov
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

package com.iiordanov.bVNC.exceptions;

import androidx.annotation.StringRes;

import java.io.IOException;

/**
 * Thrown when SecureVNCPlugin encounters a configuration or protocol error
 * that should be presented to the user as a localized message.
 *
 * <p>Extends {@link IOException} so existing {@code throws IOException} declarations
 * in the SecureVNCPlugin classes do not need to change.
 */
public class SecureVNCPluginException extends IOException {

    @StringRes
    private final int resId;

    public SecureVNCPluginException(@StringRes int resId) {
        super();
        this.resId = resId;
    }

    @StringRes
    public int getResId() {
        return resId;
    }
}
