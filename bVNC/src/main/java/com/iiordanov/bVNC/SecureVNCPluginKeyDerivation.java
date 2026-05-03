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

import org.bouncycastle.crypto.digests.SHA1Digest;
import org.bouncycastle.crypto.generators.PKCS5S2ParametersGenerator;
import org.bouncycastle.crypto.params.KeyParameter;


/**
 * PBKDF2-HMAC-SHA1 key derivation for SecureVNCPlugin.
 * <p>
 * Uses BouncyCastle's PKCS5S2ParametersGenerator because Java's built-in
 * SecretKeyFactory may reject empty salts.
 */
public final class SecureVNCPluginKeyDerivation {

    private SecureVNCPluginKeyDerivation() {
    }

    /**
     * Derive key bytes using PBKDF2-HMAC-SHA1.
     *
     * @param password   password bytes (UTF-8 encoded passphrase)
     * @param salt       salt bytes (may be empty)
     * @param iterations iteration count
     * @param keyBytes   desired output length in bytes
     * @return derived key
     */
    public static byte[] pbkdf2HmacSha1(byte[] password, byte[] salt, int iterations, int keyBytes) {
        PKCS5S2ParametersGenerator gen = new PKCS5S2ParametersGenerator(new SHA1Digest());
        gen.init(password, salt, iterations);
        return ((KeyParameter) gen.generateDerivedParameters(keyBytes * 8)).getKey();
    }

    /**
     * Derive the single-AES session keys (SV and VS) from key material.
     * <p>
     * The key material is split into two equal halves; the IV is the last 16 raw bytes.
     * SV key = PBKDF2(first_half, empty_salt, 4097, keyBytes)
     * VS key = PBKDF2(second_half, empty_salt, 4097, keyBytes)
     * IV     = last 16 bytes of keyMaterial (raw, not PBKDF2-derived)
     *
     * @param keyMaterial random bytes from RSA decryption (length = RSA_size - 12)
     * @param keyBytes    AES key size in bytes (16 or 32)
     * @return DerivedSessionKeys containing svKey, vsKey, iv
     */
    public static DerivedSessionKeys deriveSingleAesKeys(byte[] keyMaterial, int keyBytes) {
        int materialLen = keyMaterial.length - SecureVNCPluginFlags.SVNC_IV_BYTES;
        int half = materialLen / 2;

        byte[] firstHalf = new byte[half];
        byte[] secondHalf = new byte[half];
        System.arraycopy(keyMaterial, 0, firstHalf, 0, half);
        System.arraycopy(keyMaterial, half, secondHalf, 0, half);

        byte[] iv = new byte[SecureVNCPluginFlags.SVNC_IV_BYTES];
        System.arraycopy(keyMaterial, keyMaterial.length - SecureVNCPluginFlags.SVNC_IV_BYTES, iv, 0, SecureVNCPluginFlags.SVNC_IV_BYTES);

        byte[] svKey = getPbkdf2HmacSha1ForSection(firstHalf, keyBytes);
        byte[] vsKey = getPbkdf2HmacSha1ForSection(secondHalf, keyBytes);

        return new DerivedSessionKeys(svKey, vsKey, iv);
    }

    /**
     * Derive the triple-AES session keys (SV1/VS1/SV2/VS2/SV3/VS3) from key material.
     * <p>
     * The key material (minus last 16 IV bytes) is split into 6 equal sections.
     * Sections alternate: [0]=sv1, [1]=vs1, [2]=sv2, [3]=vs2, [4]=sv3, [5]=vs3.
     * Each section is PBKDF2-derived to produce one key. IV = last 16 bytes raw.
     *
     * @param keyMaterial random bytes from RSA decryption
     * @param keyBytes    AES key size in bytes (16 or 32)
     * @return DerivedTripleAesKeys with all 6 keys and the IV
     */
    public static DerivedTripleAesKeys deriveTripleAesKeys(byte[] keyMaterial, int keyBytes) {
        int materialLen = keyMaterial.length - SecureVNCPluginFlags.SVNC_IV_BYTES;
        int sectionLen = materialLen / 6;

        byte[][] sections = new byte[6][sectionLen];
        for (int i = 0; i < 6; i++) {
            System.arraycopy(keyMaterial, i * sectionLen, sections[i], 0, sectionLen);
        }

        byte[] iv = new byte[SecureVNCPluginFlags.SVNC_IV_BYTES];
        System.arraycopy(keyMaterial, keyMaterial.length - SecureVNCPluginFlags.SVNC_IV_BYTES, iv, 0, SecureVNCPluginFlags.SVNC_IV_BYTES);

        byte[] sv1 = getPbkdf2HmacSha1ForSection(sections[0], keyBytes);
        byte[] vs1 = getPbkdf2HmacSha1ForSection(sections[1], keyBytes);
        byte[] sv2 = getPbkdf2HmacSha1ForSection(sections[2], keyBytes);
        byte[] vs2 = getPbkdf2HmacSha1ForSection(sections[3], keyBytes);
        byte[] sv3 = getPbkdf2HmacSha1ForSection(sections[4], keyBytes);
        byte[] vs3 = getPbkdf2HmacSha1ForSection(sections[5], keyBytes);

        return new DerivedTripleAesKeys(sv1, vs1, sv2, vs2, sv3, vs3, iv);
    }

    private static byte[] getPbkdf2HmacSha1ForSection(byte[] section, int keyBytes) {
        byte[] emptySalt = new byte[0];
        return pbkdf2HmacSha1(section, emptySalt, SecureVNCPluginFlags.SVNC_PBKDF2_ITERATIONS, keyBytes);
    }

    /**
     * Holds the two session keys and IV for single-AES mode.
     *
     * @param svKey Server-to-client key (server encrypts, client decrypts).
     * @param vsKey Client-to-server key (client encrypts, server decrypts).
     * @param iv    Initial IV (raw bytes from end of key material).
     */
    public record DerivedSessionKeys(byte[] svKey, byte[] vsKey, byte[] iv) {
    }

    /**
     * Holds the six session keys and IV for triple-AES (EDE) mode.
     *
     * @param iv Initial IV (raw bytes from end of key material).
     */
    public record DerivedTripleAesKeys(byte[] sv1, byte[] vs1, byte[] sv2, byte[] vs2, byte[] sv3, byte[] vs3,
                                       byte[] iv) {
    }
}
