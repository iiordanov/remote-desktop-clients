/** 
 * Copyright (C) 2012 Iordan Iordanov
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.security.KeyPair;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;

import android.content.Context;
import android.os.Handler;
import android.util.Base64;
import android.util.Log;
import android.widget.Toast;

import com.iiordanov.pubkeygenerator.PubkeyUtils;
import com.trilead.ssh2.Connection;
import com.trilead.ssh2.ConnectionInfo;
import com.trilead.ssh2.InteractiveCallback;
import com.trilead.ssh2.KnownHosts;
import com.trilead.ssh2.Session;
import com.iiordanov.bVNC.dialogs.GetTextFragment;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.remoteClientUi.*;

/**
 * @author Iordan K Iordanov
 *
 */
public class SSHConnection implements InteractiveCallback, GetTextFragment.OnFragmentDismissedListener {
    private final static String TAG = "SSHConnection";
    private final static int MAXTRIES = 3;
    private final static int MAX_AUTH_RETRIES = 3;
    private final static int MAX_DECRYPTION_ATTEMPTS = 3;

    private Connection connection;
    private final int numPortTries = 1000;
    private ConnectionInfo connectionInfo;
    private String serverHostKey;
    private Session session;
    private boolean passwordAuth = false;
    private boolean keyboardInteractiveAuth = false;
    private boolean pubKeyAuth = false;
    private KeyPair    kp;
    private PrivateKey privateKey;
    private PublicKey  publicKey;

    // Connection parameters
    private String host;
    private String user;
    private String password;
    private String vncpassword;
    private String passphrase;
    private String savedServerHostKey;
    private int idHashAlg;
    private String idHash; // URI alternative to key
    private String savedIdHash; // alternative to key
    private String targetAddress;
    private int sshPort;
    private boolean usePubKey;
    private String sshPrivKey;
    private boolean useSshRemoteCommand;
    private int     sshRemoteCommandType;
    private int     sshRemoteCommandTimeout;
    private String  sshRemoteCommand;
    private BufferedInputStream remoteStdout;
    private BufferedInputStream remoteStderr;
    private BufferedOutputStream remoteStdin;
    private boolean autoXEnabled;
    private int     autoXType;
    private String  autoXCommand;
    private boolean autoXUnixpw;
    private String  autoXRandFileNm;
    private Context context;
    private Handler handler;
    private int sshPasswordAuthAttempts = 0;
    private int sshKeyDecryptionAttempts = 0;
    private com.undatech.opaque.Connection conn;

    // Used to communicate the MFA verification code obtained.
    private String verificationCode;
    private CountDownLatch userInputLatch;

    public SSHConnection(com.undatech.opaque.Connection conn, Context cntxt, Handler handler) {
        host = conn.getSshServer();
        sshPort = conn.getSshPort();
        user = conn.getSshUser();
        password = conn.getSshPassword();
        vncpassword = conn.getPassword();
        passphrase = conn.getSshPassPhrase();
        savedServerHostKey = conn.getSshHostKey();
        idHashAlg = conn.getIdHashAlgorithm();
        savedIdHash = conn.getIdHash();
        targetAddress = conn.getAddress();
        usePubKey = conn.getUseSshPubKey();
        sshPrivKey = conn.getSshPrivKey();
        useSshRemoteCommand = conn.getUseSshRemoteCommand();
        sshRemoteCommandType = conn.getSshRemoteCommandType();
        sshRemoteCommand = conn.getSshRemoteCommand();
        autoXEnabled = conn.getAutoXEnabled();
        autoXType = conn.getAutoXType();
        autoXCommand = conn.getAutoXCommand();
        autoXUnixpw = conn.getAutoXUnixpw();
        connection = new Connection(host, sshPort);
        autoXRandFileNm = conn.getAutoXRandFileNm();
        context = cntxt;
        userInputLatch = new CountDownLatch(1);
        this.verificationCode = new String();
        this.handler = handler;
        this.conn = conn;
    }
    
    String getServerHostKey() {
        return serverHostKey;
    }
    String getIdHash() {
        return idHash;
    }

    public void setVerificationCode(String verificationCode) {
        this.verificationCode = verificationCode;
        this.userInputLatch.countDown();
    }

    public void setUserAndPassword(String sshUser, String sshPassword, boolean keep) {
        this.user = sshUser;
        this.password = sshPassword;
        this.conn.setSshUser(sshUser);
        this.conn.setSshPassword(sshPassword);
        this.conn.setKeepSshPassword(keep);
        this.userInputLatch.countDown();
    }

    public void setPassphrase(String sshPassphrase, boolean keep) {
        this.passphrase = sshPassphrase;
        this.conn.setSshPassPhrase(sshPassphrase);
        this.conn.setKeepSshPassword(keep);
        this.userInputLatch.countDown();
    }

    private void attemptSshPasswordAuthentication() throws Exception {
        Log.i(TAG, "attemptSshPasswordAuthentication");
        while(!authenticateWithPassword() && canAuthWithPass()) {
            sshPasswordAuthAttempts++;
            if (sshPasswordAuthAttempts > MAX_AUTH_RETRIES) {
                throw new Exception(context.getString(R.string.error_ssh_pwd_auth_fail));
            }
            userInputLatch = new CountDownLatch(1);
            Log.i(TAG, "Requesting SSH password from user");
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_SSH_CREDENTIALS);
            while (true) {
                try {
                    userInputLatch.await();
                    break;
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void attemptSshKeyDecryption() throws Exception {
        // Try to decrypt and recover keypair, and failing that, report error.
        kp = PubkeyUtils.decryptAndRecoverKeyPair(sshPrivKey, passphrase);
        while(kp == null) {
            sshKeyDecryptionAttempts++;
            if (sshKeyDecryptionAttempts > MAX_DECRYPTION_ATTEMPTS) {
                throw new Exception(context.getString(R.string.error_ssh_keypair_decryption_failure));
            }
            userInputLatch = new CountDownLatch(1);
            Log.i(TAG, "Requesting SSH passphrase from user");
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_SSH_PASSPHRASE);
            while (true) {
                try {
                    userInputLatch.await();
                    break;
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            kp = PubkeyUtils.decryptAndRecoverKeyPair(sshPrivKey, passphrase);
        }
    }


    /**
     * Initializes the SSH Tunnel
     * @return -1 if the target port was not determined, and the port obtained from x11vnc if it was
     *             determined with AutoX.
     * @throws Exception
     */
    public int initializeSSHTunnel () throws Exception {
        int port = -1;
        
        // Attempt to connect.
        if (!connect())
            throw new Exception(context.getString(R.string.error_ssh_unable_to_connect));
        
        // Verify host key against saved one.
        if (!verifyHostKey())
            throw new Exception(context.getString(R.string.error_ssh_hostkey_changed));
        
        // Authenticate and set up port forwarding.
        if (!usePubKey) {
            Log.i(TAG, "SSH tunnel not configured to use public key, trying password auth");
            if (!canAuthWithPass()) {
                Log.e(TAG, "SSH server does not support password authentication so throw an error");
                String authMethods = Arrays.toString(connection.getRemainingAuthMethods(user));
                throw new Exception(context.getString(R.string.error_ssh_kbd_auth_method_unavail) + " " + authMethods);
            }
            attemptSshPasswordAuthentication();
        } else {
            Log.i(TAG, "SSH tunnel is configured to use public key, will attempt");
            if (canAuthWithPubKey()) {
                Log.i(TAG, "SSH server supports pubkey authentication, continuing");
                // Pubkey auth method is allowed so try it.
                if (!authenticateWithPubKey()) {
                    if (!canAuthWithPubKey()) {
                        // If pubkey authentication is now no longer available, we know pubkey
                        // authentication succeeded but the server wants further authentication.
                        Log.i(TAG, "SSH server needs more than key auth, trying password auth in addition");
                        MessageDialogs.displayToast(context, handler,
                                context.getString(R.string.ssh_server_needs_password_in_addition_to_key),
                                Toast.LENGTH_LONG);
                        attemptSshPasswordAuthentication();
                    } else {
                        Log.e(TAG, "Failed to authenticate to SSH server with key");
                        throw new Exception(context.getString(R.string.error_ssh_key_auth_fail));
                    }
                }
            } else {
                // Pubkey authentication is not available, so try password if one was supplied.
                if (canAuthWithPass()) {
                    Log.i(TAG, "Key auth enabled, but server is asking for password, trying password auth");
                    MessageDialogs.displayToast(context, handler,
                            context.getString(R.string.ssh_server_needs_password_in_addition_to_key),
                            Toast.LENGTH_LONG);
                    attemptSshPasswordAuthentication();
                    Log.d(TAG, "isAuthenticationComplete: " + connection.isAuthenticationComplete());
                    Log.d(TAG, "isAuthenticationPartialSuccess: " + connection.isAuthenticationPartialSuccess());
                    if (!connection.isAuthenticationComplete()) {
                        Log.i(TAG, "Key auth enabled, password authenticated succeeded, and server is asking for key auth");
                        // If password authentication is now no longer available, we know password
                        // authentication succeeded but the server wants further authentication.
                        if (!authenticateWithPubKey()) {
                            Log.e(TAG, "Key authentication failed");
                            throw new Exception(context.getString(R.string.error_ssh_key_auth_fail));
                        }
                    } else if (!connection.isAuthenticationComplete()) {
                        Log.e(TAG, "Password authentication failed");
                        throw new Exception(context.getString(R.string.error_ssh_pwd_auth_fail));
                    }
                } else {
                    Log.e(TAG, "SSH server does not support key auth, but SSH tunnel is configured to use it");
                    String authMethods = Arrays.toString(connection.getRemainingAuthMethods(user));
                    throw new Exception(context.getString(R.string.error_ssh_pubkey_auth_method_unavail)
                            + " " + authMethods);
                }
            }
        }

        // Run a remote command if commanded to.
        if (autoXEnabled) {
            int tries = 0;
            while (port < 0 && tries < MAXTRIES) {
                // If we're not using unix credentials, protect access with a temporary password file.
                if (!autoXUnixpw) {
                    writeStringToRemoteCommand(vncpassword, Constants.AUTO_X_CREATE_PASSWDFILE+
                                                            Constants.AUTO_X_PWFILEBASENAME+autoXRandFileNm+
                                                            Constants.AUTO_X_SYNC);
                }
                // Execute AutoX command.
                execRemoteCommand(autoXCommand, 1);
                
                // If we are looking for the greeter, we give the password to sudo's stdin.
                if (autoXType == Constants.AUTOX_SELECT_SUDO_FIND)
                    writeStringToStdin (password+"\n");
                
                // Try to find PORT=
                port = parseRemoteStdoutForPort();
                if (port < 0) {
                    session.close();
                    tries++;
                    // Wait a little for x11vnc to recover.
                    if (tries < MAXTRIES)
                        try { Thread.sleep(tries*3500); } catch (InterruptedException e1) { }
                }
            }

            if (port < 0) {
                throw new Exception (context.getString(R.string.error_ssh_x11vnc_no_port_failure)
                        + "  \n\n" + context.getString(R.string.error) + ":  \n\n"
                        + bufferedInputStreamToString(remoteStderr));
            }
        }
        
        return port;
    }
    
    /**
     * Creates a port forward to the given port and returns the local port forwarded.
     * @return the local port forwarded to the given remote port
     * @throws Exception
     */
    int createLocalPortForward (int port) throws Exception {
        int localForwardedPort;

        // At this point we know we are authenticated.
        localForwardedPort = createPortForward(port, targetAddress, port);
        // If we got back a negative number, port forwarding failed.
        if (localForwardedPort < 0) {
            throw new Exception(context.getString(R.string.error_ssh_port_forwarding_failure));
        }

        return localForwardedPort;
    }

    
    /**
     * Connects to remote server.
     * @return
     */
    public boolean connect() {
            
        try {
            connection.setCompression(false);

            // TODO: Try using the provided KeyVerifier instead of verifying keys myself.
            connectionInfo = connection.connect(null, 6000, 24000);

            // Store a base64 encoded string representing the HostKey
            serverHostKey = Base64.encodeToString(connectionInfo.serverHostKey, Base64.DEFAULT);

            // Get information on supported authentication methods we're interested in.

            return true;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Return a string holding a Hex representation of the signature of the remote host's key.
     */
    public String getHostKeySignature () {
        return KnownHosts.createHexFingerprint(connectionInfo.serverHostKeyAlgorithm,
                                               connectionInfo.serverHostKey);
    }

    /**
     * Disconnects from remote server.
     */
    public void terminateSSHTunnel () {
        connection.close();
    }

    private boolean verifyHostKey () {
    	// first check data against URI hash
    	try {
    		byte[] rawKey = connectionInfo.serverHostKey;
    		boolean isValid = SecureTunnel.isSignatureEqual(idHashAlg, savedIdHash, rawKey);
    		if (isValid) {
    			Log.i(TAG, "Validated against provided hash.");
    			return true;
    		}
    	} catch (Exception ex) { }
        // Because JSch returns the host key base64 encoded, and trilead ssh returns it not base64 encoded,
        // we compare savedHostKey to serverHostKey both base64 encoded and not.
        return savedServerHostKey.equals(serverHostKey) ||
                savedServerHostKey.equals(new String(Base64.decode(serverHostKey, Base64.DEFAULT)));
    }

    /**
     * Returns whether the server can authenticate either with pass or keyboard-interactive methods.
     */
    private boolean canAuthWithPass() {
        return hasPasswordAuth() || hasKeyboardInteractiveAuth();
    }
    
    /**
     * Returns whether the server supports passworde
     * @return
     */
    private boolean hasPasswordAuth () {
        boolean passwordAuth = false;
        try {
            passwordAuth = connection.isAuthMethodAvailable(user, "password");
        } catch (IOException e) {
            e.printStackTrace();
        }
        return passwordAuth;
    }
    
    /**
     * Returns whether the server supports passworde
     * @return
     */
    private boolean hasKeyboardInteractiveAuth () {
        boolean keyboardInteractiveAuth = false;
        try {
            keyboardInteractiveAuth = connection.isAuthMethodAvailable(user, "keyboard-interactive");
        } catch (IOException e) {
            e.printStackTrace();
        }
        return keyboardInteractiveAuth;
    }

    /**
     * Returns whether the server can authenticate with a key.
     */
    private boolean canAuthWithPubKey () {
        boolean pubKeyAuth = false;
        try {
            pubKeyAuth = connection.isAuthMethodAvailable(user, "publickey");
        } catch (IOException e) {
            e.printStackTrace();
        }
        return pubKeyAuth;
    }

    /**
     * Authenticates with a password.
     */
    private boolean authenticateWithPassword () {
        boolean isAuthenticated = false;

        try {
            if (hasKeyboardInteractiveAuth()) {
                Log.i(TAG, "Trying SSH keyboard-interactive authentication.");
                isAuthenticated = connection.authenticateWithKeyboardInteractive(user, this);
            }
            if (!isAuthenticated && hasPasswordAuth()) {
                Log.i(TAG, "Trying SSH password authentication. " + user + " " + password);
                isAuthenticated = connection.authenticateWithPassword(user, password);
            }
            return isAuthenticated;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    /**
     * Decrypts and recovers the key pair.
     * @throws Exception 
     */
    private void decryptAndRecoverKey () throws Exception {
        Log.i(TAG, "decryptAndRecoverKey");

        // Detect an empty key (not generated).
        if (sshPrivKey.length() == 0) {
            Log.e(TAG, "SSH key not generated yet");
            throw new Exception(context.getString(R.string.error_ssh_keypair_missing));
        }
        
        // Detect passphrase entered when key unencrypted and report error.
        if (passphrase.length() != 0 && !PubkeyUtils.isEncrypted(sshPrivKey)) {
            Log.e(TAG, "SSH key not encrypted but passphrase was entered");
            throw new Exception(context.getString(R.string.error_ssh_passphrase_but_keypair_unencrypted));
        }
        attemptSshKeyDecryption();

        privateKey = kp.getPrivate();
        publicKey = kp.getPublic();
    }

    /**
     * Authenticates with a public/private key-pair.
     */
    private boolean authenticateWithPubKey () throws Exception {
        decryptAndRecoverKey();
        Log.i(TAG, "Trying SSH pubkey authentication.");
        return connection.authenticateWithPublicKey(user, kp);
    }
    
    private int createPortForward (int localPortStart, String remoteHost, int remotePort) {
        int portsTried = 0;
        while (portsTried < numPortTries) {
            try {
                connection.createLocalPortForwarder(new InetSocketAddress("127.0.0.1", localPortStart + portsTried),
                                                    remoteHost, remotePort);
                return localPortStart + portsTried;
            } catch (IOException e) {
                portsTried++;
            }            
        }
        return -1;
    }

    /**
     * Executes a remote command, and waits a certain amount of time.
     * @param command - the command to execute.
     * @param secTimeout - amount of time in seconds to wait afterward.
     * @throws Exception
     */
    private void execRemoteCommand (String command, int secTimeout) throws Exception {
        Log.i (TAG, "Executing remote command: " + command);

        try {
            session = connection.openSession();
            session.execCommand(command);
            remoteStdout = new BufferedInputStream(session.getStdout());
            remoteStderr = new BufferedInputStream(session.getStderr());
            remoteStdin  = new BufferedOutputStream(session.getStdin());
            Thread.sleep(secTimeout*1000);
        } catch (Exception e) {
            e.printStackTrace();
            throw new Exception (context.getString(R.string.error_ssh_could_not_exec_command)
                    + "  \n\n" + context.getString(R.string.error) + ":  \n\n"
                    + bufferedInputStreamToString(remoteStderr));
        }
    }
    
    /**
     * Writes the specified string to a stdin of a remote command.
     * @throws Exception
     */
    private void writeStringToRemoteCommand (String s, String cmd) throws Exception {
        Log.i(TAG, "Writing string to stdin of remote command: " + cmd);
        execRemoteCommand(cmd, 0);
        remoteStdin.write(s.getBytes());
        remoteStdin.flush();
        remoteStdin.close();
        session.close();
    }
    
    /**
     * Writes the specified string to a stdin of open session.
     * @throws Exception
     */
    private void writeStringToStdin (String s) throws Exception {
        Log.i(TAG, "Writing string to remote stdin.");
        remoteStdin.write(s.getBytes());
        remoteStdin.flush();
    }
    
    // TODO: This doesn't work at the moment.
    private void sendSudoPassword () throws Exception {
        Log.i (TAG, "Sending sudo password.");

        try {
            remoteStdin.write(new String (password + '\n').getBytes());
        } catch (IOException e) {
            e.printStackTrace();
            throw new Exception (context.getString(R.string.error_ssh_could_not_send_sudo_pwd)
                    + "  \n\n" + context.getString(R.string.error) + ":  \n\n"
                    + bufferedInputStreamToString(remoteStderr));
        }
    }

    /**
     * Parses the remote stdout for PORT=
     */
    private int parseRemoteStdoutForPort () {
        Log.i (TAG, "Parsing remote stdout for PORT=");

        String sought = "PORT=";
        int soughtLength = sought.length();
        int port = -1;
        try {
            int data = 0;
            int i = 0;
            while (data != -1 && i < soughtLength) {
                data = remoteStdout.read();
                if (data == (int)sought.charAt(i)) {
                    i = i + 1;
                } else {
                    i = 0;
                }
            }

            if (i == soughtLength) {
                // Read in 5 bytes after PORT=
                byte[] buffer = new byte[5];
                remoteStdout.read(buffer);
                // Get rid of any whitespace (e.g. if the port is less than 5 digits).
                buffer = new String(buffer).replaceAll("\\s","").getBytes();
                port = Integer.parseInt(new String(buffer));
                Log.i (TAG, "Found PORT=, set to: " + port);
            } else {
                Log.e (TAG, "Failed to find PORT= in remote stdout.");
                port = -1;
            }
        } catch (IOException e) {
            Log.e (TAG, "Failed to read from remote stdout.");
            e.printStackTrace();
            port = -1;
        } catch (NumberFormatException e) {
            Log.e (TAG, "Failed to parse integer.");
            e.printStackTrace();
            port = -1;
        }

        return port;
    }

    String bufferedInputStreamToString(BufferedInputStream remote) {
        int nRead;
        byte[] dataBuf = new byte[1024];
        ByteArrayOutputStream bufferOs = new ByteArrayOutputStream();

        try {
            while ((nRead = remote.read(dataBuf, 0, dataBuf.length)) != -1) {
                bufferOs.write(dataBuf, 0, nRead);
            }
            String output = bufferOs.toString("UTF-8");

            Log.d(TAG, "bufferedInputStreamToString:");
            Log.d(TAG, output);
            return output;
        } catch (IOException e) {
            Log.e (TAG, "Failed to read from remote stdout.");
            e.printStackTrace();
        }
        return "";
    }

    /**
     * Used for keyboard-interactive authentication.
     */
    @Override
    public String[] replyToChallenge(String name, String instruction,
                                    int numPrompts, String[] prompt,
                                    boolean[] echo) throws Exception {
        String[] responses = new String[numPrompts];
        for (int x=0; x < numPrompts; x++) {
            if (prompt[0].indexOf("Verification code:") != -1) {
                Log.i(TAG, prompt[x] + "  Will request verification code from user");
                if (Utils.isFree(context)) {
                    handler.sendEmptyMessage(RemoteClientLibConstants.PRO_FEATURE);
                    responses[x] = "";
                } else {
                    userInputLatch = new CountDownLatch(1);
                    Log.i(TAG, "Requesting verification code from user");
                    handler.sendEmptyMessage(RemoteClientLibConstants.GET_VERIFICATIONCODE);
                    while (true) {
                        try {
                            userInputLatch.await();
                            break;
                        } catch (InterruptedException e) { e.printStackTrace(); }
                    }
                    Log.i(TAG, prompt[0] + "  Sending verification code: " + verificationCode);
                    responses[x] = verificationCode;
                }
            } else {
                Log.i(TAG, prompt[0] + "  Sending SSH password");
                responses[x] = password;
            }
        }
        return responses;
    }
    
    @Override
    public void onTextObtained(String dialogId, String[] obtainedStrings, boolean dialogCancelled, boolean keep) {
        if (dialogCancelled) {
            handler.sendEmptyMessage(RemoteClientLibConstants.DISCONNECT_NO_MESSAGE);
            return;
        }

        switch (dialogId) {
            case GetTextFragment.DIALOG_ID_GET_VERIFICATIONCODE:
                android.util.Log.i(TAG, "Text obtained from DIALOG_ID_GET_VERIFICATIONCODE.");
                setVerificationCode(obtainedStrings[0]);
                break;
            case GetTextFragment.DIALOG_ID_GET_SSH_CREDENTIALS:
                android.util.Log.i(TAG, "Text obtained from DIALOG_ID_GET_SSH_CREDENTIALS.");
                setUserAndPassword(obtainedStrings[0], obtainedStrings[1], keep);
                break;
            case GetTextFragment.DIALOG_ID_GET_SSH_PASSPHRASE:
                android.util.Log.i(TAG, "Text obtained from DIALOG_ID_GET_SSH_PASSPHRASE.");
                setPassphrase(obtainedStrings[0], keep);
                break;
            default:
                android.util.Log.e(TAG, "Unknown dialog type.");
                break;
        }
    }
}
