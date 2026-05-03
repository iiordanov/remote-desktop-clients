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

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.KeyFactory;
import java.security.SecureRandom;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.RSAPublicKeySpec;

import javax.crypto.Cipher;

/**
 * Builds and sends the SecureVNCPlugin response packet, and derives stream cipher keys.
 *
 * <p>Response wire format (all multi-byte integers are little-endian):
 * <pre>
 *   2 bytes LE  - total response length M
 *   M bytes     - response data:
 *     4 bytes LE  - response flags
 *     2 bytes LE  - encrypted key material length
 *     encKeyLen bytes - RSA-encrypted key material (PKCS#1 v1.5)
 *     2 bytes LE  - client auth signature length (0 if no client auth)
 *     sigLen bytes - SHA1withRSA signature (empty if no client auth)
 * </pre>
 *
 * <p>After construction, {@link #getSessionKeys()} and {@link #getTripleAesKeys()} provide
 * the derived cipher keys for wrapping the stream.
 */
public class SecureVNCPluginResponse {
    private static final String TAG = "SVNCResponse";

    private final SecureVNCPluginChallenge challenge;
    private final byte[] sessionKeyMaterial;
    private final int responseFlags;

    private SecureVNCPluginKeyDerivation.DerivedSessionKeys sessionKeys;
    private SecureVNCPluginKeyDerivation.DerivedTripleAesKeys tripleAesKeys;

    /**
     * Generate key material and derive session keys from the parsed challenge.
     *
     * @param challenge parsed challenge (must have had decryptRsaPublicKey called on it)
     * @throws IOException on key generation failure
     */
    public SecureVNCPluginResponse(SecureVNCPluginChallenge challenge) throws IOException {
        this.challenge = challenge;

        this.responseFlags = SecureVNCPluginFlags.getBestResponseFlags(challenge.challengeFlags);
        if (this.responseFlags == 0) {
            throw new SecureVNCPluginException(R.string.error_securevncplugin_unsupported_cipher);
        }
        Log.i(TAG, "Challenge flags=0x" + Integer.toHexString(challenge.challengeFlags)
                + ", response flags=0x" + Integer.toHexString(responseFlags));

        boolean newKey = (challenge.challengeFlags & SecureVNCPluginFlags.SVNC_NEW_KEY) != 0;
        int rsaBytes = (challenge.rsaModulus.bitLength() + 7) / 8;
        int keyMaterialLen = newKey ? rsaBytes - 12
                : SecureVNCPluginFlags.getKeyBytes(responseFlags) * 2;
        sessionKeyMaterial = new byte[keyMaterialLen];
        new SecureRandom().nextBytes(sessionKeyMaterial);

        deriveSessionKeys();
    }

    private void deriveSessionKeys() {
        int keyBytes = SecureVNCPluginFlags.getKeyBytes(responseFlags);

        if (SecureVNCPluginFlags.isTripleAes(responseFlags)) {
            tripleAesKeys = SecureVNCPluginKeyDerivation.deriveTripleAesKeys(sessionKeyMaterial, keyBytes);
        } else {
            sessionKeys = SecureVNCPluginKeyDerivation.deriveSingleAesKeys(sessionKeyMaterial, keyBytes);
        }
    }

    /**
     * Write the response packet to the raw socket output stream.
     *
     * @param stream              raw socket output stream (before encryption is installed)
     * @param clientAuthEnabled   whether to include a client auth signature
     * @param clientPrivKeyBase64 base64 PKCS#8 private key for client auth (null if not used)
     * @throws IOException on write or crypto failure
     */
    public void write(OutputStream stream, boolean clientAuthEnabled,
                      String clientPrivKeyBase64) throws IOException {
        byte[] encryptedKeyMaterial = rsaEncryptKeyMaterial();

        // Response flags = chosen cipher + key-size flags (no separate client-auth bit).
        // Client auth is signalled by the signature length field being non-zero.

        byte[] clientAuthSig = new byte[0];
        if (clientAuthEnabled && clientPrivKeyBase64 != null && !clientPrivKeyBase64.isEmpty()) {
            Log.i(TAG, "Client auth enabled, signing with private key (base64 len="
                    + clientPrivKeyBase64.length() + ")");
            clientAuthSig = SecureVNCPluginClientAuth.sign(
                    challenge.rsaPubKeyBytes, sessionKeyMaterial,
                    clientPrivKeyBase64);
            Log.i(TAG, "Client auth signature produced, sigLen=" + clientAuthSig.length);
        } else {
            Log.i(TAG, "Client auth not included (enabled=" + clientAuthEnabled
                    + ", keyProvided=" + (clientPrivKeyBase64 != null && !clientPrivKeyBase64.isEmpty()) + ")");
        }

        // Build response data body
        int bodyLen = 4 + 2 + encryptedKeyMaterial.length + 2 + clientAuthSig.length;
        ByteBuffer body = ByteBuffer.allocate(bodyLen).order(ByteOrder.LITTLE_ENDIAN);
        body.putInt(this.responseFlags);
        body.putShort((short) (encryptedKeyMaterial.length & 0xFFFF));
        body.put(encryptedKeyMaterial);
        body.putShort((short) (clientAuthSig.length & 0xFFFF));
        if (clientAuthSig.length > 0) {
            body.put(clientAuthSig);
        }

        // Write 2-byte LE length prefix + body
        byte[] bodyBytes = body.array();
        byte[] lengthPrefix = new byte[]{
                (byte) (bodyLen & 0xFF),
                (byte) ((bodyLen >> 8) & 0xFF)
        };
        stream.write(lengthPrefix);
        stream.write(bodyBytes);
        stream.flush();
    }

    private byte[] rsaEncryptKeyMaterial() throws IOException {
        try {
            RSAPublicKeySpec spec = new RSAPublicKeySpec(challenge.rsaModulus, challenge.rsaPublicExponent);
            KeyFactory kf = KeyFactory.getInstance("RSA");
            RSAPublicKey pubKey = (RSAPublicKey) kf.generatePublic(spec);

            Cipher cipher = Cipher.getInstance("RSA/ECB/PKCS1Padding");
            cipher.init(Cipher.ENCRYPT_MODE, pubKey);
            return cipher.doFinal(sessionKeyMaterial);
        } catch (Exception e) {
            throw new IOException("SecureVNCPlugin: RSA encryption of key material failed", e);
        }
    }

    /**
     * @return derived session keys for single-AES mode, or null if triple-AES mode
     */
    public SecureVNCPluginKeyDerivation.DerivedSessionKeys getSessionKeys() {
        return sessionKeys;
    }

    /**
     * @return derived session keys for triple-AES (EDE) mode, or null if single-AES mode
     */
    public SecureVNCPluginKeyDerivation.DerivedTripleAesKeys getTripleAesKeys() {
        return tripleAesKeys;
    }

    /**
     * @return true if triple-AES (EDE) mode is active
     */
    public boolean isTripleAes() {
        return SecureVNCPluginFlags.isTripleAes(responseFlags);
    }

    /**
     * @return true if AES-CFB8 mode (vs OFB) is used for the session cipher
     */
    public boolean isCfb8() {
        return SecureVNCPluginFlags.isCfb8(responseFlags);
    }
}
