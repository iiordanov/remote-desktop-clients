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

package com.iiordanov.bVNC;

/**
 * Immutable settings for UltraVNC SecureVNCPlugin / MS Logon authentication.
 * Built by the caller and handed to the auth layer as one cohesive unit, rather
 * than configured field-by-field on RfbProto.
 */
public record SecureVncConfig(boolean enabled, boolean clientAuthEnabled, String clientAuthPrivKey, String passphrase) {

    /**
     * A disabled config (no SecureVNCPlugin, no client auth).
     */
    public static SecureVncConfig disabled() {
        return new SecureVncConfig(false, false, null, null);
    }
}
