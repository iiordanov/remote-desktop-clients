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

import android.util.Log;

import com.iiordanov.bVNC.exceptions.SecureVNCPluginException;
import com.undatech.remoteClientUi.R;

import org.bouncycastle.asn1.pkcs.RSAPublicKey;

import java.io.IOException;
import java.io.InputStream;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * Parses the SecureVNCPlugin challenge packet sent by the server.
 *
 * <p>Wire format (all multi-byte integers are little-endian):
 * <pre>
 *   New format (0x73):
 *     2 bytes LE  - total challenge length N
 *     N bytes     - challenge data:
 *       1 byte      - plugin ID
 *       1 byte      - version compat
 *       4 bytes LE  - server ID length
 *       serverIdLen bytes - server ID string
 *       4 bytes LE  - challenge flags
 *       2 bytes LE  - client auth key ID length
 *       clientAuthKeyIdLen bytes - client auth key ID
 *       8 bytes     - salt
 *       16 bytes    - initial IV
 *       2 bytes LE  - encrypted RSA pub key length
 *       encKeyLen bytes - AES-OFB-encrypted PKCS#1 RSA pub key
 *       20 bytes    - SHA-1(salt || plaintext RSA pub key)
 *     1 byte      - passphraseused flag
 *
 *   Old format (0x72):
 *     Same as new format but WITHOUT the trailing passphraseused byte.
 * </pre>
 */
public class SecureVNCPluginChallenge {
    private static final String TAG = "SVNCChallenge";

    public final int challengeFlags;
    public final byte[] salt;
    public final byte[] initialIv;
    public final byte[] encryptedRsaPubKeyBytes;
    public final byte[] rsaPubKeySha1;
    public final String clientAuthKeyId;

    /**
     * 0=send VNC password, 1=send passphrase, 2=don't send. Only valid for new format (0x73).
     */
    public final int passphraseUsed;

    /**
     * Decrypted PKCS#1 RSA public key (available after calling decryptRsaPublicKey).
     */
    public byte[] rsaPubKeyBytes;

    /**
     * Parsed RSA public key modulus and exponent (available after decryptRsaPublicKey).
     */
    public BigInteger rsaModulus;
    public BigInteger rsaPublicExponent;

    private SecureVNCPluginChallenge(
            int challengeFlags, byte[] salt, byte[] initialIv,
            byte[] encryptedRsaPubKeyBytes, byte[] rsaPubKeySha1,
            String clientAuthKeyId, int passphraseUsed
    ) {
        this.challengeFlags = challengeFlags;
        this.salt = salt;
        this.initialIv = initialIv;
        this.encryptedRsaPubKeyBytes = encryptedRsaPubKeyBytes;
        this.rsaPubKeySha1 = rsaPubKeySha1;
        this.clientAuthKeyId = clientAuthKeyId;
        this.passphraseUsed = passphraseUsed;
    }

    /**
     * Read and parse a challenge packet from the raw socket stream.
     *
     * @param stream    raw socket input stream (before any encryption)
     * @param newFormat true for security type 0x73, false for 0x72
     * @return parsed challenge
     * @throws IOException on read error or malformed packet
     */
    public static SecureVNCPluginChallenge read(InputStream stream, boolean newFormat) throws IOException {
        // Read 2-byte LE challenge length
        int challengeLen = readLeShort(stream);
        if (challengeLen <= 0) {
            throw new IOException("SecureVNCPlugin: invalid challenge length " + challengeLen);
        }

        byte[] challengeData = new byte[challengeLen];
        readFully(stream, challengeData);

        ByteBuffer buf = ByteBuffer.wrap(challengeData).order(ByteOrder.LITTLE_ENDIAN);

        // plugin ID and version compat
        @SuppressWarnings("unused")
        int pluginId = buf.get() & 0xFF;
        @SuppressWarnings("unused")
        int versionCompat = buf.get() & 0xFF;

        // server ID
        int serverIdLen = buf.getInt();
        byte[] serverIdBytes = new byte[serverIdLen];
        buf.get(serverIdBytes);

        // challenge flags
        int challengeFlags = buf.getInt();

        // client auth key ID
        int clientAuthKeyIdLen = buf.getShort() & 0xFFFF;
        byte[] clientAuthKeyIdBytes = new byte[clientAuthKeyIdLen];
        buf.get(clientAuthKeyIdBytes);
        String clientAuthKeyId = new String(clientAuthKeyIdBytes, StandardCharsets.UTF_8);

        // salt
        byte[] salt = new byte[SecureVNCPluginFlags.SVNC_SALT_BYTES];
        buf.get(salt);

        // initial IV: 8 bytes for Blowfish (LOW_KEY), 16 bytes for AES
        boolean lowKey = (challengeFlags & SecureVNCPluginFlags.SVNC_LOW_KEY) != 0;
        int ivLen = lowKey ? SecureVNCPluginFlags.SVNC_BF_IV_BYTES : SecureVNCPluginFlags.SVNC_IV_BYTES;
        byte[] initialIv = new byte[ivLen];
        buf.get(initialIv);

        // encrypted RSA pub key
        int encKeyLen = buf.getShort() & 0xFFFF;
        byte[] encryptedRsaPubKey = new byte[encKeyLen];
        buf.get(encryptedRsaPubKey);

        // SHA-1 hash
        byte[] sha1 = new byte[SecureVNCPluginFlags.SVNC_SHA1_BYTES];
        buf.get(sha1);

        // passphraseused (0x73 format only)
        int passphraseUsed = SecureVNCPluginFlags.SVNC_PASSPHRASE_NOT_USED;
        if (newFormat) {
            passphraseUsed = stream.read();
            if (passphraseUsed < 0) {
                throw new IOException("SecureVNCPlugin: EOF reading passphraseused byte");
            }
        }

        Log.d(TAG, "Challenge: flags=0x" + Integer.toHexString(challengeFlags)
                + " passphraseUsed=" + passphraseUsed
                + " encKeyLen=" + encryptedRsaPubKey.length
                + " newKey=" + ((challengeFlags & SecureVNCPluginFlags.SVNC_NEW_KEY) != 0)
                + " lowKey=" + ((challengeFlags & SecureVNCPluginFlags.SVNC_LOW_KEY) != 0)
                + " salt=" + bytesToHex(salt, 4)
                + " iv=" + bytesToHex(initialIv, 4)
                + " sha1=" + bytesToHex(sha1, 4));

        return new SecureVNCPluginChallenge(challengeFlags, salt, initialIv,
                encryptedRsaPubKey, sha1, clientAuthKeyId, passphraseUsed);
    }

    /**
     * Decrypt the RSA public key from the challenge using a passphrase-derived key.
     * When LOW_KEY is set, uses Blowfish-OFB with a 7-byte key; otherwise AES-256-OFB.
     * Also verifies the SHA-1 hash. After this call, rsaPubKeyBytes, rsaModulus, and
     * rsaPublicExponent are populated.
     *
     * @param passphrase VNC passphrase entered by the user
     * @throws IOException if decryption fails or hash verification fails
     */
    public void decryptRsaPublicKey(String passphrase) throws IOException {
        boolean newKey = (challengeFlags & SecureVNCPluginFlags.SVNC_NEW_KEY) != 0;
        boolean lowKey = (challengeFlags & SecureVNCPluginFlags.SVNC_LOW_KEY) != 0;

        Log.i(TAG, "decryptRsaPublicKey: newKey=" + newKey + " lowKey=" + lowKey
                + " passphraseLen=" + (passphrase == null ? "null" : passphrase.length())
                + " passphraseUsed=" + passphraseUsed);

        if (!newKey) {
            // Legacy servers (no svncNewKey) use EVP_BytesToKey with 11 iterations.
            // Modern servers always set svncNewKey; throw if we encounter the old path.
            throw new SecureVNCPluginException(R.string.error_securevncplugin_legacy_key_derivation);
        }

        int keyBytes = lowKey ? SecureVNCPluginFlags.SVNC_BF_KEY_BYTES
                : SecureVNCPluginFlags.SVNC_AES_256_KEY_BYTES;
        byte[] derivedKey = tryPasswordCandidates(passphrase, keyBytes, lowKey);

        try {
            Cipher cipher;
            if (lowKey) {
                cipher = Cipher.getInstance("Blowfish/OFB/NoPadding");
                cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(derivedKey, "Blowfish"),
                        new IvParameterSpec(initialIv));
            } else {
                cipher = Cipher.getInstance("AES/OFB/NoPadding");
                cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(derivedKey, "AES"),
                        new IvParameterSpec(initialIv));
            }
            rsaPubKeyBytes = cipher.doFinal(encryptedRsaPubKeyBytes);
            Log.i(TAG, "decryptRsaPublicKey: decrypted pubkey first bytes=" + bytesToHex(rsaPubKeyBytes, 6));
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: failed to decrypt RSA public key", e);
        }

        verifyRsaPublicKeyHash();
        parseRsaPublicKey();
    }

    /**
     * Try password candidates in order and return the derived key for the first one
     * whose PBKDF2 output decrypts and verifies the RSA public key hash.
     * Tries: full passphrase, each prefix length 8 down to 1, then DEFAULT_PASSWORD.
     */
    private byte[] tryPasswordCandidates(String passphrase, int keyBytes, boolean lowKey) throws IOException {
        // Always try the user passphrase when provided — passphraseUsed is absent in old
        // format (0x72) and set to NOT_USED as a placeholder, but the server may still have
        // derived the challenge key from the real passphrase.
        boolean hasUserPassphrase = passphrase != null && !passphrase.isEmpty();
        byte[] userBytes = hasUserPassphrase
                ? passphrase.getBytes(StandardCharsets.UTF_8)
                : null;

        // Build candidate list: full passphrase, lengths 8 down to 1, then DEFAULT_PASSWORD
        List<byte[]> candidates = new ArrayList<>();
        if (userBytes != null) {
            candidates.add(userBytes);  // full length
            int maxPrefix = Math.min(userBytes.length - 1, 8);
            for (int len = maxPrefix; len >= 1; len--) {
                candidates.add(Arrays.copyOf(userBytes, len));
            }
        }
        candidates.add(SecureVNCPluginFlags.DEFAULT_PASSWORD);

        for (byte[] pw : candidates) {
            byte[] key = SecureVNCPluginKeyDerivation.pbkdf2HmacSha1(
                    pw, salt, SecureVNCPluginFlags.SVNC_PBKDF2_ITERATIONS, keyBytes);
            if (checkPasswordCandidate(key, lowKey)) {
                Log.i(TAG, "tryPasswordCandidates: matched pwLen=" + pw.length
                        + " isDefault=" + (pw == SecureVNCPluginFlags.DEFAULT_PASSWORD));
                return key;
            }
        }
        throw new SecureVNCPluginException(R.string.error_securevncplugin_wrong_passphrase);
    }

    /**
     * Returns true if the given key correctly decrypts and SHA-1-verifies the RSA public key.
     */
    private boolean checkPasswordCandidate(byte[] key, boolean lowKey) {
        try {
            Cipher cipher;
            if (lowKey) {
                cipher = Cipher.getInstance("Blowfish/OFB/NoPadding");
                cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "Blowfish"),
                        new IvParameterSpec(initialIv));
            } else {
                cipher = Cipher.getInstance("AES/OFB/NoPadding");
                cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"),
                        new IvParameterSpec(initialIv));
            }
            byte[] candidate = cipher.doFinal(encryptedRsaPubKeyBytes);
            MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
            sha1.update(salt);
            sha1.update(candidate);
            return Arrays.equals(sha1.digest(), rsaPubKeySha1);
        } catch (Exception e) {
            return false;
        }
    }

    private void verifyRsaPublicKeyHash() throws IOException {
        try {
            MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
            sha1.update(salt);
            sha1.update(rsaPubKeyBytes);
            byte[] computed = sha1.digest();
            if (!Arrays.equals(computed, rsaPubKeySha1)) {
                throw new SecureVNCPluginException(R.string.error_securevncplugin_wrong_passphrase);
            }
        } catch (NoSuchAlgorithmException e) {
            throw new IOException("SHA-1 not available", e);
        }
    }

    private void parseRsaPublicKey() throws IOException {
        try {
            RSAPublicKey parsed = RSAPublicKey.getInstance(rsaPubKeyBytes);
            rsaModulus = parsed.getModulus();
            rsaPublicExponent = parsed.getPublicExponent();
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: failed to parse PKCS#1 RSA public key", e);
        }
    }

    /**
     * Return up to maxBytes of an array as lowercase hex (for logging).
     */
    private static String bytesToHex(byte[] bytes, int maxBytes) {
        StringBuilder sb = new StringBuilder();
        int n = Math.min(bytes.length, maxBytes);
        for (int i = 0; i < n; i++) {
            sb.append(String.format("%02x", bytes[i] & 0xFF));
        }
        if (bytes.length > maxBytes) sb.append("..");
        return sb.toString();
    }

    /**
     * Read a 2-byte little-endian unsigned short from stream.
     */
    private static int readLeShort(InputStream stream) throws IOException {
        int lo = stream.read();
        int hi = stream.read();
        if (lo < 0 || hi < 0) throw new IOException("SecureVNCPlugin: EOF reading LE short");
        return (hi << 8) | lo;
    }

    /**
     * Read exactly len bytes from stream into buf.
     */
    private static void readFully(InputStream stream, byte[] buf) throws IOException {
        int offset = 0;
        while (offset < buf.length) {
            int n = stream.read(buf, offset, buf.length - offset);
            if (n < 0) throw new IOException("SecureVNCPlugin: EOF reading challenge data");
            offset += n;
        }
    }
}
