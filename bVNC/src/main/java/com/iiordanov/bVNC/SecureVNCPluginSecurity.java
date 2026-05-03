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
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

import javax.crypto.Cipher;

/**
 * Orchestrates the SecureVNCPlugin authentication and encryption handshake.
 *
 * <p>Handles both the new format (security type 0x73) and the old format (0x72).
 *
 * <h3>New format (0x73) sequence:</h3>
 * <ol>
 *   <li>Read challenge (2-byte LE length + N bytes + 1-byte passphraseused)</li>
 *   <li>Decrypt RSA public key, verify SHA-1</li>
 *   <li>Generate key material, derive session keys</li>
 *   <li>Write response (2-byte LE length + response body)</li>
 *   <li>Install encrypted streams via {@code rfb.setStreams()}</li>
 *   <li>If passphraseused==0: send VNC password; if ==1: send passphrase; if ==2: skip</li>
 *   <li>Read 4-byte BE auth result (through encrypted channel)</li>
 * </ol>
 *
 * <h3>Old format (0x72) sequence:</h3>
 * <ol>
 *   <li>Read challenge (2-byte LE length + N bytes, no passphraseused byte)</li>
 *   <li>Same steps 2-4 as new format</li>
 *   <li>Read 4-byte BE auth result (PLAINTEXT, before encryption)</li>
 *   <li>Install encrypted streams via {@code rfb.setStreams()}</li>
 * </ol>
 */
public class SecureVNCPluginSecurity {
    private static final String TAG = "SVNCSecurity";

    /** Auth result value indicating success (4 bytes BE, value 0). */
    private static final int AUTH_RESULT_OK = 0;

    /** Auth result indicating more authentication rounds follow (e.g. MS Logon after plugin). */
    private static final int AUTH_RESULT_CONTINUE = 0xFFFFFFFF;

    /**
     * Perform the full SecureVNCPlugin handshake.
     *
     * @param channel           auth channel: supplies the raw socket streams and
     *                          receives the encrypted streams once built
     * @param challengePassphrase DSM passphrase for challenge decryption (key derivation)
     * @param password          VNC password sent after handshake when passphraseUsed==0
     * @param newFormat         true for security type 0x73, false for 0x72
     * @param clientAuthEnabled whether to perform client authentication
     * @param clientPrivKeyB64  base64 PKCS#8 private key for client auth (may be null)
     * @return true if the server requires an additional authentication round (rfbVncAuthContinue),
     *         false if authentication is complete (rfbVncAuthOK)
     * @throws IOException      on any network or crypto error
     * @throws AuthFailedException if the server rejects authentication
     */
    public static boolean authenticate(
            RfbAuthChannel channel,
            String challengePassphrase,
            String password,
            boolean newFormat,
            boolean clientAuthEnabled,
            String clientPrivKeyB64) throws IOException, AuthFailedException {

        Log.i(TAG, "Starting SecureVNCPlugin auth, newFormat=" + newFormat);

        // Raw (pre-encryption) socket streams; the encrypted streams are installed
        // back onto the channel once the handshake produces them.
        InputStream rawInputStream = channel.getRawInputStream();
        OutputStream rawOutputStream = channel.getRawOutputStream();

        // Step 1: Read and parse challenge
        SecureVNCPluginChallenge challenge =
                SecureVNCPluginChallenge.read(rawInputStream, newFormat);

        boolean serverRequiresClientAuth =
                (challenge.challengeFlags & SecureVNCPluginFlags.SVNC_CLIENT_AUTH_REQUIRED) != 0;
        Log.i(TAG, "Server requires client auth=" + serverRequiresClientAuth
                + ", client auth enabled=" + clientAuthEnabled
                + ", clientAuthKeyId=" + challenge.clientAuthKeyId);

        if (serverRequiresClientAuth && !clientAuthEnabled) {
            throw new SecureVNCPluginException(
                    R.string.error_securevncplugin_client_auth_required);
        }

        // Step 2: Decrypt RSA public key using DSM passphrase, verify hash
        challenge.decryptRsaPublicKey(challengePassphrase);

        // Step 3: Generate session key material and derive cipher keys
        SecureVNCPluginResponse response = new SecureVNCPluginResponse(challenge);

        // Step 4: Send response packet (plaintext — encryption not yet active)
        response.write(rawOutputStream, clientAuthEnabled, clientPrivKeyB64);

        // Build encrypted streams (response already holds all challenge-derived key material)
        SecureVNCPluginInStream encryptedIn = buildInStream(rawInputStream, response);
        SecureVNCPluginOutStream encryptedOut = buildOutStream(rawOutputStream, response);

        if (newFormat) {
            // Step 5 (new format): Install encrypted streams BEFORE reading auth result
            channel.setStreams(encryptedIn, encryptedOut);

            // Step 6: Send password/passphrase through encrypted channel.
            // The passphraseUsed byte from the server tells us what to send:
            //   0 (REQUIRED): No passphrase configured server-side → send VNC password.
            //   1 (OPTIONAL): Passphrase configured server-side → send passphrase.
            //                 Must always send (even if empty) because the server
            //                 unconditionally reads in this case.
            //   2 (NOT_USED): MS Logon or no password → don't send anything.
            if (challenge.passphraseUsed == SecureVNCPluginFlags.SVNC_PASSPHRASE_REQUIRED) {
                Log.i(TAG, "passphraseUsed=0, sending VNC password");
                sendPasswordEncrypted(encryptedOut, password != null ? password : "");
            } else if (challenge.passphraseUsed == SecureVNCPluginFlags.SVNC_PASSPHRASE_OPTIONAL) {
                String pp = challengePassphrase != null ? challengePassphrase : "";
                Log.i(TAG, "passphraseUsed=1, sending passphrase (len=" + pp.length() + ")");
                sendPasswordEncrypted(encryptedOut, pp);
            } else {
                Log.i(TAG, "passphraseUsed=2, skipping password send");
            }

            // Step 7: Read auth result through encrypted channel (4 bytes BE).
            int authResult = encryptedIn.readInt();
            Log.i(TAG, "auth result=" + authResult + " (0x" + Integer.toHexString(authResult) + ")");
            if (authResult == AUTH_RESULT_CONTINUE) {
                Log.i(TAG, "Server requests additional authentication round");
                return true;
            }
            checkAuthResult(authResult, challenge.passphraseUsed);
        } else {
            // Old format: Read auth result PLAINTEXT, then install encrypted streams.
            // Old format has no passphraseUsed byte — treat as generic failure.
            int authResult = readPlaintextAuthResult(rawInputStream);
            checkAuthResult(authResult, SecureVNCPluginFlags.SVNC_PASSPHRASE_NOT_USED);

            // Step 5 (old format): Install encrypted streams AFTER auth result
            channel.setStreams(encryptedIn, encryptedOut);
        }

        Log.i(TAG, "SecureVNCPlugin authentication succeeded");
        return false;
    }

    private static SecureVNCPluginInStream buildInStream(
            InputStream rawInputStream,
            SecureVNCPluginResponse response) {

        boolean useCfb8 = response.isCfb8();
        SecureVNCPluginCipherContext ctx;

        if (response.isTripleAes()) {
            SecureVNCPluginKeyDerivation.DerivedTripleAesKeys keys = response.getTripleAesKeys();
            // S→C direction: sv1/sv2/sv3; false = decrypting (InStream)
            ctx = new SecureVNCPluginCipherContext(
                    keys.sv1(), keys.sv2(), keys.sv3(), keys.iv(), false);
        } else {
            SecureVNCPluginKeyDerivation.DerivedSessionKeys keys = response.getSessionKeys();
            // S→C direction: svKey
            ctx = new SecureVNCPluginCipherContext(
                    keys.svKey(), keys.iv(), Cipher.DECRYPT_MODE, useCfb8);
        }

        return new SecureVNCPluginInStream(rawInputStream, ctx);
    }

    private static SecureVNCPluginOutStream buildOutStream(
            OutputStream rawOutputStream,
            SecureVNCPluginResponse response) {

        boolean useCfb8 = response.isCfb8();
        SecureVNCPluginCipherContext ctx;

        if (response.isTripleAes()) {
            SecureVNCPluginKeyDerivation.DerivedTripleAesKeys keys = response.getTripleAesKeys();
            // C→S direction: vs1/vs2/vs3; true = encrypting (OutStream)
            ctx = new SecureVNCPluginCipherContext(
                    keys.vs1(), keys.vs2(), keys.vs3(), keys.iv(), true);
        } else {
            SecureVNCPluginKeyDerivation.DerivedSessionKeys keys = response.getSessionKeys();
            // C→S direction: vsKey
            ctx = new SecureVNCPluginCipherContext(
                    keys.vsKey(), keys.iv(), Cipher.ENCRYPT_MODE, useCfb8);
        }

        return new SecureVNCPluginOutStream(rawOutputStream, ctx);
    }

    private static void sendPasswordEncrypted(SecureVNCPluginOutStream out, String passphrase)
            throws IOException {
        byte[] pwBytes = passphrase.getBytes(StandardCharsets.UTF_8);

        // Write 2-byte LE length + password bytes through encrypted channel
        byte[] lenBytes = {
                (byte) (pwBytes.length & 0xFF),
                (byte) ((pwBytes.length >> 8) & 0xFF)
        };
        out.writeBytes(lenBytes);
        out.writeBytes(pwBytes);
        out.flush();
    }

    private static int readPlaintextAuthResult(InputStream rawInputStream) throws IOException {
        byte[] buf = new byte[4];
        int offset = 0;
        while (offset < 4) {
            int n = rawInputStream.read(buf, offset, 4 - offset);
            if (n < 0) throw new IOException("SecureVNCPlugin: EOF reading auth result");
            offset += n;
        }
        // Big-endian 4-byte integer
        return ((buf[0] & 0xFF) << 24) | ((buf[1] & 0xFF) << 16) | ((buf[2] & 0xFF) << 8) | (buf[3] & 0xFF);
    }

    /**
     * Check the auth result from the server. When passphraseUsed indicates
     * the server expected a passphrase (OPTIONAL=1), throws SecureVNCPluginException
     * so the caller can prompt for passphrase re-entry.
     */
    private static void checkAuthResult(int authResult, int passphraseUsed)
            throws AuthFailedException, SecureVNCPluginException {
        if (authResult != AUTH_RESULT_OK) {
            if (passphraseUsed == SecureVNCPluginFlags.SVNC_PASSPHRASE_OPTIONAL) {
                throw new SecureVNCPluginException(
                        R.string.error_securevncplugin_wrong_passphrase);
            }
            throw new AuthFailedException(
                    "SecureVNCPlugin authentication failed (result=" + authResult + ")");
        }
    }

    /** Thrown when the server rejects client authentication. */
    public static class AuthFailedException extends IOException {
        public AuthFailedException(String message) {
            super(message);
        }
    }
}
