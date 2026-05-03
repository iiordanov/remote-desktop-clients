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

import android.os.SystemClock;
import android.util.Log;

import com.iiordanov.bVNC.exceptions.SecureVNCPluginException;
import com.tigervnc.rdr.InStream;
import com.tigervnc.rdr.OutStream;
import com.tigervnc.rfb.AuthFailureException;
import com.undatech.remoteClientUi.R;

/**
 * Owns all UltraVNC SecureVNCPlugin / MS Logon II authentication: the two-phase
 * type-17 negotiation, the 0x73/0x72 plugin handshake, the post-plugin auth
 * round, and the Diffie-Hellman MS Logon II credential exchange. Talks to the
 * connection only through {@link RfbAuthChannel}, so this protocol-specific
 * concern lives outside RfbProto.
 */
class UltraVncAuthenticator {
    private static final String TAG = "UltraVncAuthenticator";

    private final RfbAuthChannel channel;

    private DH dh;
    private long dhResp;

    UltraVncAuthenticator(RfbAuthChannel channel) {
        this.channel = channel;
    }

    /**
     * Picks the security type for an UltraVNC connection from the server's offered
     * list. We mirror the official viewer: prefer type 17 (rfbUltraVNC) so we go
     * through UltraVNC's continue/re-negotiate round; see the type-17 refs in
     * UltraVNC's rfb/rfbproto.h and winvnc/winvnc/vncclient.cpp.
     * <p>Priority:
     * <ul>
     *   <li>SecureVNCPlugin enabled: UltraVnc1 &gt; SecureVNCPluginNew &gt; SecureVNCPlugin (ignore plain types)</li>
     *   <li>not enabled:             UltraVnc1 &gt; MsLogonII &gt; VncAuth &gt; None</li>
     * </ul>
     * Returns {@link RfbProto#SecTypeInvalid} if nothing suitable was offered (and
     * SecureVNCPlugin is not required); throws if it is required but unavailable.
     */
    int selectSecurityType(byte[] secTypes, int nSecTypes) throws Exception {
        SecureVncConfig config = channel.getSecureVncConfig();
        int secType = RfbProto.SecTypeInvalid;
        for (int i = 0; i < nSecTypes; i++) {
            int currentSecType = secTypes[i] & 0xff;
            Log.i(TAG, "Received security type: " + secTypes[i]);
            if (currentSecType == RfbProto.SecTypeUltraVnc1) {
                secType = currentSecType;
                break; // always go through the two-phase UltraVNC viewer flow
            } else if (config.enabled()) {
                // Also accept direct SecureVNCPlugin types when type 17 is not offered.
                if (currentSecType == RfbProto.SecTypeSecureVNCPluginNew && secType == RfbProto.SecTypeInvalid) {
                    secType = currentSecType;
                    // keep looking for type 17
                } else if (currentSecType == RfbProto.SecTypeSecureVNCPlugin && secType == RfbProto.SecTypeInvalid) {
                    secType = currentSecType;
                    // keep looking for 0x73 and type 17
                }
            } else if (currentSecType == RfbProto.SecTypeMsLogonII || currentSecType == RfbProto.SecTypeVncAuth
                    || currentSecType == RfbProto.SecTypeNone) {
                secType = currentSecType;
                break; // plain UltraVNC / basic auth; accept first offered
            }
        }

        // When SecureVNCPlugin encryption is required but the server offered no
        // plugin / type-17 type, give the specific "plugin not offered" reason.
        if (secType == RfbProto.SecTypeInvalid && config.enabled()) {
            throw new SecureVNCPluginException(R.string.error_securevncplugin_not_offered);
        }
        return secType;
    }

    /**
     * Performs SecureVNCPlugin or UltraVNC two-phase authentication for a
     * selected security type of UltraVnc1 (type 17), SecureVNCPluginNew (0x73),
     * or SecureVNCPlugin (0x72).
     *
     * <p>For type 17, reads the phase-2 type list. If the server offers 0x73/0x72,
     * performs SecureVNCPlugin. Otherwise dispatches to the standard auth handler.
     * For direct 0x73/0x72 selection, performs the SecureVNCPlugin handshake immediately.
     */
    void authenticate(int secType, String us, String pw) throws Exception, AuthFailureException {
        int pluginSecType = secType;
        if (secType == RfbProto.SecTypeUltraVnc1) {
            Log.i(TAG, "UltraVNC phase 1: type 17 selected, reading auth-continue");
            int phase1Result = channel.getInStream().readInt();
            if (phase1Result != RfbProto.VncAuthContinue) {
                throw new SecureVNCPluginException(
                        R.string.error_securevncplugin_handshake);
            }
            pluginSecType = selectUltraVncPhase2Type();
            Log.i(TAG, "UltraVNC phase 2 type: 0x" + Integer.toHexString(pluginSecType));

            if (pluginSecType != RfbProto.SecTypeSecureVNCPluginNew
                    && pluginSecType != RfbProto.SecTypeSecureVNCPlugin) {
                dispatchUltraVncAuth(pluginSecType, us, pw, "UltraVNC phase 2");
                return;
            }
        }
        boolean newFormat = (pluginSecType == RfbProto.SecTypeSecureVNCPluginNew);
        Log.i(TAG, "SecureVNCPlugin auth, newFormat=" + newFormat);

        SecureVncConfig config = channel.getSecureVncConfig();
        // Use DSM passphrase for challenge decryption; fall back to VNC password
        // (tryPasswordCandidates will also try DEFAULT_PASSWORD as a last resort).
        String svncPassphrase = config.passphrase();
        String challengePassphrase = (svncPassphrase != null && !svncPassphrase.isEmpty())
                ? svncPassphrase : pw;
        boolean authContinue;
        try {
            authContinue = SecureVNCPluginSecurity.authenticate(
                    channel,
                    challengePassphrase,
                    pw,
                    newFormat,
                    config.clientAuthEnabled(),
                    config.clientAuthPrivKey());
        } catch (SecureVNCPluginSecurity.AuthFailedException e) {
            throw new AuthFailureException(e.getMessage());
        }

        if (authContinue) {
            authenticatePostPlugin(us, pw);
        }
    }

    /**
     * Diffie-Hellman MS Logon II credential exchange. Entry point for the classic
     * (non-type-17) UltraVNC auth path where MsLogonII/Ultra34 was selected directly.
     */
    void authenticateMsLogon(String us, String pw) throws Exception {
        prepareDH();
        authenticateDH(us, pw);
    }

    /**
     * Handles the post-SecureVNCPlugin auth round. The server offers a new type list
     * through the encrypted channel (e.g. MsLogonII, VncAuth, NoAuth).
     */
    private void authenticatePostPlugin(String us, String pw) throws Exception {
        // After the plugin handshake these are the encrypted streams installed via setStreams.
        InStream is = channel.getInStream();
        OutStream os = channel.getOutStream();
        int nSecTypes = is.readUnsignedByte();
        if (nSecTypes == 0) {
            channel.readConnFailedReason();
            throw new Exception("Post-plugin auth: server sent empty security-type list");
        }
        byte[] secTypes = new byte[nSecTypes];
        is.readBytes(secTypes);

        int selectedType = selectBestUltraVncAuthType(secTypes, nSecTypes);

        os.writeU8(selectedType);
        os.flush();

        dispatchUltraVncAuth(selectedType, us, pw, "Post-plugin");
    }

    /**
     * Dispatch to the appropriate auth handler for an UltraVNC auth type.
     * Used by both phase-2 (type 17 without SecureVNCPlugin) and post-plugin auth.
     *
     * @param context descriptive label for log messages (e.g. "UltraVNC phase 2")
     */
    private void dispatchUltraVncAuth(int authType, String us, String pw, String context)
            throws Exception {
        switch (authType) {
            case RfbProto.SecTypeMsLogonII:
                Log.i(TAG, context + ": MS Logon II authentication");
                prepareDH();
                authenticateDH(us != null ? us : "", pw);
                break;
            case RfbProto.AuthVNC:
                Log.i(TAG, context + ": VNC password authentication");
                channel.authenticateVNC(pw);
                break;
            case RfbProto.AuthNone:
                Log.i(TAG, context + ": No authentication needed");
                channel.authenticateNone();
                break;
            default:
                throw new Exception(context + ": unsupported auth type 0x"
                        + Integer.toHexString(authType));
        }
    }

    /**
     * Select the best auth type from a UltraVNC type list.
     * Prefers MsLogonII > VncAuth > NoAuth.
     */
    private int selectBestUltraVncAuthType(byte[] secTypes, int count)
            throws Exception {
        final String context = "Post-plugin";
        int[] preferred = {RfbProto.SecTypeMsLogonII, RfbProto.AuthVNC, RfbProto.AuthNone};
        StringBuilder offered = new StringBuilder(context + " auth types: ");
        for (int i = 0; i < count; i++) {
            if (i > 0) offered.append(", ");
            offered.append("0x").append(Integer.toHexString(secTypes[i] & 0xFF));
        }
        Log.i(TAG, offered.toString());

        for (int pref : preferred) {
            for (int i = 0; i < count; i++) {
                if ((secTypes[i] & 0xFF) == pref) {
                    Log.i(TAG, context + ": selected 0x" + Integer.toHexString(pref));
                    return pref;
                }
            }
        }
        throw new Exception(context + ": no supported auth type offered");
    }

    /**
     * Read and select the phase-2 auth type after type-17 negotiation.
     * When SecureVNCPlugin is enabled: prefers 0x73 > 0x72.
     * When not: prefers non-plugin types, but warns the user if plugin
     * types were available.
     */
    private int selectUltraVncPhase2Type() throws Exception {
        InStream is = channel.getInStream();
        SecureVncConfig config = channel.getSecureVncConfig();
        int nSecTypes = is.readUnsignedByte();
        if (nSecTypes == 0) {
            channel.readConnFailedReason();
            throw new Exception("UltraVNC phase 2: server sent empty security-type list");
        }
        byte[] secTypes = new byte[nSecTypes];
        is.readBytes(secTypes);

        // Scan for both plugin types and non-plugin fallbacks.
        int pluginType = RfbProto.SecTypeInvalid;
        int fallback = RfbProto.SecTypeInvalid;
        for (int i = 0; i < nSecTypes; i++) {
            int t = secTypes[i] & 0xff;
            Log.i(TAG, "UltraVNC phase 2 offered type: 0x" + Integer.toHexString(t));
            if (t == RfbProto.SecTypeSecureVNCPluginNew) {
                pluginType = t;
                // 0x73 preferred over 0x72; stop looking for plugin types
            } else if (t == RfbProto.SecTypeSecureVNCPlugin && pluginType == RfbProto.SecTypeInvalid) {
                pluginType = t;
                // keep looking for 0x73
            } else if (fallback == RfbProto.SecTypeInvalid
                    && (t == RfbProto.SecTypeMsLogonII || t == RfbProto.AuthVNC || t == RfbProto.AuthNone)) {
                fallback = t;
            }
        }

        int chosen;
        if (config.enabled()) {
            // Encryption required: prefer plugin types
            chosen = (pluginType != RfbProto.SecTypeInvalid) ? pluginType : fallback;
        } else if (pluginType != RfbProto.SecTypeInvalid) {
            // Encryption NOT required but server offers it: warn the user
            if (fallback != RfbProto.SecTypeInvalid) {
                // Non-plugin fallback available: ask user whether to proceed unencrypted
                Log.i(TAG, "Server offers encryption (0x" + Integer.toHexString(pluginType)
                        + ") but SecureVNCPlugin is not enabled; prompting user");
                boolean proceedUnencrypted = channel.confirmProceedWithoutEncryption();
                if (proceedUnencrypted) {
                    Log.i(TAG, "User chose to proceed without encryption");
                    chosen = fallback;
                } else {
                    throw new Exception("User cancelled unencrypted connection");
                }
            } else {
                // Server ONLY offers plugin types with no fallback
                throw new SecureVNCPluginException(
                        R.string.error_securevncplugin_server_requires_encryption);
            }
        } else {
            chosen = fallback;
        }

        if (chosen == RfbProto.SecTypeInvalid) {
            throw new Exception("UltraVNC phase 2: no supported auth type offered");
        }

        // Enforce SecureVNCPlugin when required: if the server went through type 17 but
        // its phase-2 list doesn't include 0x73/0x72, reject the connection.
        if (config.enabled()
                && chosen != RfbProto.SecTypeSecureVNCPluginNew
                && chosen != RfbProto.SecTypeSecureVNCPlugin) {
            throw new SecureVNCPluginException(
                    R.string.error_securevncplugin_not_offered);
        }

        OutStream os = channel.getOutStream();
        os.writeU8(chosen);
        os.flush();
        Log.i(TAG, "UltraVNC phase 2 selected: 0x" + Integer.toHexString(chosen));
        return chosen;
    }

    private void prepareDH() throws Exception {
        InStream is = channel.getInStream();
        OutStream os = channel.getOutStream();
        long gen = is.readLong();
        long mod = is.readLong();
        dhResp = is.readLong();

        dh = new DH(gen, mod);
        long pub = dh.createInterKey();

        os.writeBytes(DH.longToBytes(pub));
        os.flush();
    }

    private void authenticateDH(String us, String pw) throws Exception {
        long key = dh.createEncryptionKey(dhResp);

        DesCipher des = new DesCipher(DH.longToBytes(key));

        byte[] user = new byte[256];
        byte[] passwd = new byte[64];
        int i;
        System.arraycopy(us.getBytes(), 0, user, 0, us.length());
        if (us.length() < 256) {
            for (i = us.length(); i < 256; i++) {
                user[i] = 0;
            }
        }
        System.arraycopy(pw.getBytes(), 0, passwd, 0, pw.length());
        if (pw.length() < 64) {
            for (i = pw.length(); i < 64; i++) {
                passwd[i] = 0;
            }
        }

        des.encryptText(user, user, DH.longToBytes(key));
        des.encryptText(passwd, passwd, DH.longToBytes(key));

        OutStream os = channel.getOutStream();
        os.writeBytes(user);
        os.writeBytes(passwd);
        os.flush();
        // Workaround for a race in UltraVNC's legacy ("New MS Logon" disabled)
        // that improves the odds of connecting to ~50% (same as UltraVNC viewer).
        // Not a fix — the real flakiness is server-side. Strongly prefer "New MS Logon".
        SystemClock.sleep(200);
        channel.readSecurityResult(true, "UltraVNC authentication");
    }
}
