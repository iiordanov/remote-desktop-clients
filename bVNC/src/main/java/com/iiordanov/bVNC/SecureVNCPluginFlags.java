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
 * Protocol constants for UltraVNC SecureVNCPlugin (security types 0x72 and 0x73).
 *
 * <p>Flag values match UltraVNC's IntegratedSecureVNCPluginObject.h SecureVNCFlags enum exactly.
 * All multi-byte integers on the wire are little-endian.
 */
public final class SecureVNCPluginFlags {

    private SecureVNCPluginFlags() {
    }

    // Cipher type flags (bits 0-7, present in both challenge and response flags)
    public static final int SVNC_CIPHER_AES = 0x0001;  // Single AES-OFB
    public static final int SVNC_CIPHER_AESCFB = 0x0020;  // Single AES-CFB8
    public static final int SVNC_CIPHER_3AESOFB = 0x0040;  // Triple AES chained CFB8 (EDE-style, despite the "OFB" name)

    // Key size flags (bits 8-15, present in both challenge and response flags)
    public static final int SVNC_KEY_128 = 0x1000;
    public static final int SVNC_KEY_192 = 0x2000;
    public static final int SVNC_KEY_256 = 0x4000;

    // Option flags (bits 16-31, only in challenge flags — not echoed in response)
    public static final int SVNC_CLIENT_AUTH_REQUIRED = 0x00010000;
    public static final int SVNC_LOW_KEY = 0x00040000;
    /**
     * Use PBKDF2-HMAC-SHA1 for key derivation (svncNewKey). Always set on modern servers.
     */
    public static final int SVNC_NEW_KEY = 0x00800000;

    // passphraseused values (1 byte, new format only).
    // Despite the names, these indicate what the SERVER expects the client to send:
    //   0: No passphrase configured → server expects VNC password.
    //   1: Passphrase configured → server expects the DSM passphrase.
    //   2: MS Logon or no password → server does not expect anything.
    public static final int SVNC_PASSPHRASE_REQUIRED = 0;
    public static final int SVNC_PASSPHRASE_OPTIONAL = 1;
    public static final int SVNC_PASSPHRASE_NOT_USED = 2;

    // PBKDF2 parameters
    public static final int SVNC_PBKDF2_ITERATIONS = 4097;  // 0x1001

    /**
     * Hardcoded fallback password used by UltraVNC when no passphrase is configured.
     * Mirrors g_DefaultPassword[] from IntegratedSecureVNCPluginObject.cpp.
     */
    public static final byte[] DEFAULT_PASSWORD = {
            (byte) 0x69, (byte) 0xF4, (byte) 0xA4, (byte) 0x7C, (byte) 0xF8, (byte) 0xF1, (byte) 0xA6, (byte) 0x11,
            (byte) 0xC1, (byte) 0x05, (byte) 0x81, (byte) 0xC4, (byte) 0x95, (byte) 0x49, (byte) 0xAF, (byte) 0x4E,
            (byte) 0xB9, (byte) 0x55, (byte) 0x22, (byte) 0x69, (byte) 0x2F, (byte) 0x68, (byte) 0x32, (byte) 0xF4,
            (byte) 0xD5, (byte) 0x64, (byte) 0x5D, (byte) 0xF5, (byte) 0xE2, (byte) 0x37, (byte) 0x02, (byte) 0x70
    };

    // Fixed sizes
    public static final int SVNC_AES_256_KEY_BYTES = 32;
    public static final int SVNC_IV_BYTES = 16;
    public static final int SVNC_SALT_BYTES = 8;
    public static final int SVNC_SHA1_BYTES = 20;

    // Blowfish (LOW_KEY) sizes
    public static final int SVNC_BF_IV_BYTES = 8;  // Blowfish block size = 8 bytes
    public static final int SVNC_BF_KEY_BYTES = 7;  // 56-bit = 7 bytes

    /**
     * Compute the best cipher + key-size flags to send as response flags.
     * Mirrors UltraVNC's CheckBestSupportedFlags() preference order:
     * higher key size wins, and within a key size: 3AES > AESCFB > AES.
     *
     * @param challengeFlags flags received from the server challenge
     * @return response flags to send back, or 0 if no supported combination found
     */
    public static int getBestResponseFlags(int challengeFlags) {
        int[] keySizes = {SVNC_KEY_256, SVNC_KEY_192, SVNC_KEY_128};
        int[] ciphers = {SVNC_CIPHER_3AESOFB, SVNC_CIPHER_AESCFB, SVNC_CIPHER_AES};
        for (int key : keySizes) {
            if ((challengeFlags & key) != 0) {
                for (int cipher : ciphers) {
                    if ((challengeFlags & cipher) != 0) {
                        return key | cipher;
                    }
                }
            }
        }
        return 0;
    }

    /**
     * Returns the AES key length in bytes for a given set of flags.
     */
    public static int getKeyBytes(int flags) {
        if ((flags & SVNC_KEY_256) != 0) return 32;
        if ((flags & SVNC_KEY_192) != 0) return 24;
        if ((flags & SVNC_KEY_128) != 0) return 16;
        return 32;  // safe default
    }

    /**
     * Returns true if the flags indicate triple-AES (EDE) session cipher.
     */
    public static boolean isTripleAes(int responseFlags) {
        return (responseFlags & SVNC_CIPHER_3AESOFB) != 0;
    }

    /**
     * Returns true if the flags indicate single AES-CFB8 session cipher.
     * Triple-AES (svncCipher3AESOFB) uses OFB per stage and is NOT CFB8.
     */
    public static boolean isCfb8(int responseFlags) {
        return (responseFlags & SVNC_CIPHER_AESCFB) != 0;
    }
}
