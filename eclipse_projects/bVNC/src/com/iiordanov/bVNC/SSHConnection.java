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
import java.io.IOException;
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.security.KeyPair;
import java.security.PrivateKey;
import java.security.PublicKey;

import com.trilead.ssh2.Connection;
import com.trilead.ssh2.ConnectionInfo;
import com.trilead.ssh2.Session;
import com.trilead.ssh2.InteractiveCallback;
import com.trilead.ssh2.KnownHosts;
import com.iiordanov.pubkeygenerator.PubkeyUtils;

import android.util.Base64;
import android.util.Log;

/**
 * @author Iordan K Iordanov
 *
 */
public class SSHConnection implements InteractiveCallback {
	private final static String TAG = "SSHConnection";
	private final static int MAXTRIES = 3;
	
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
	private String targetAddress;
	private int sshPort;
	private int targetPort;
	private boolean usePubKey;
	private String sshPrivKey;
	private boolean useSshRemoteCommand;
	private int     sshRemoteCommandType;
	private int     sshRemoteCommandTimeout;
	private String  sshRemoteCommand;
	private BufferedInputStream remoteStdout;
	private BufferedOutputStream remoteStdin;
	private boolean autoXEnabled;
	private int     autoXType;
	private String  autoXCommand;
	private boolean autoXUnixpw;
	private String  autoXRandFileNm;

	public SSHConnection(ConnectionBean conn) {
		host = conn.getSshServer();
		sshPort = conn.getSshPort();
		user = conn.getSshUser();
		password = conn.getSshPassword();
		vncpassword = conn.getPassword();
		passphrase = conn.getSshPassPhrase();
		savedServerHostKey = conn.getSshHostKey();
		targetPort = conn.getPort();
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
	}
	
	String getServerHostKey() {
		return serverHostKey;
	}
	
	/**
	 * Initializes the SSH Tunnel
	 * @return
	 * @throws Exception
	 */
	public void initializeSSHTunnel () throws Exception {

		// Attempt to connect.
		if (!connect())
			throw new Exception("Failed to connect to SSH server. Please check network connection " +
								"status, and SSH Server address and port.");

		// Verify host key against saved one.
		if (!verifyHostKey())
			throw new Exception("ERROR! The server host key has changed. If this is intentional, " +
								"please delete and recreate the connection. Otherwise, this may be " +
								"a man in the middle attack. Not continuing.");

		if (!usePubKey && !canAuthWithPass())
			throw new Exception("Remote server " + targetAddress + " supports neither \"password\" nor " +
					"\"keyboard-interactive\" auth methods. Please configure it to allow at least one " +
					"of the two methods and try again.");

		if (usePubKey && !canAuthWithPubKey())
			throw new Exception("Remote server " + targetAddress + " does not support the \"publickey\" " +
					"auth method, and we are trying to use a key-pair to authenticate. Please configure it " +
					"to allow the publickey authentication and try again.");

		// Authenticate and set up port forwarding.
		if (!usePubKey) {
			if (!authenticateWithPassword())
				throw new Exception("Failed to authenticate to SSH server with a password. " +
						"Please check your SSH username, and SSH password.");
		} else {
			if (!authenticateWithPubKey())
				throw new Exception("Failed to authenticate to SSH server with a key-pair. " +
						"Please check your SSH username, and ensure your public key is in the " +
						"authorized_keys file on the remote side.");
		}

		// Run a remote command if commanded to.
		if (autoXEnabled) {
			// If we're not using unix credentials, protect access with a temporary password file.
			targetPort = -1;
			int tries = 0;
			while (targetPort < 0 && tries < MAXTRIES) {
				if (!autoXUnixpw) {
					writeStringToRemoteCommand(vncpassword, VncConstants.AUTO_X_CREATE_PASSWDFILE+
															VncConstants.AUTO_X_PWFILEBASENAME+autoXRandFileNm+
															VncConstants.AUTO_X_SYNC);
				}
				// Execute AutoX command.
				execRemoteCommand(autoXCommand, 1);
				
				// If we are looking for the greeter, we give the password to sudo's stdin.
				if (autoXType == VncConstants.AUTOX_SELECT_SUDO_FIND)
					writeStringToStdin (password+"\n");
				
				// Try to find PORT=
				targetPort = parseRemoteStdoutForPort();
				if (targetPort < 0) {
					session.close();
					tries++;
					// Wait a little for x11vnc to recover.
					if (tries < MAXTRIES)
						try { Thread.sleep(tries*3500); } catch (InterruptedException e1) { }
				}
			}

			if (targetPort < 0)
				throw new Exception ("Could not obtain remote VNC port from x11vnc. Please ensure x11vnc " +
						             "is installed. To be sure, try running x11vnc by hand on the command-line.");
		}
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
			throw new Exception("Could not set up the port forwarding for tunneling VNC traffic over SSH." +
					"Please ensure your SSH server is configured to allow port forwarding and try again.");
		}

		return localForwardedPort;
	}

	
	/**
	 * Connects to remote server.
	 * @return
	 */
	public boolean connect() {
			
		try {
			connection.setTCPNoDelay(true);
			connection.setCompression(false);

			// TODO: Try using the provided KeyVerifier instead of verifying keys myself.
			connectionInfo = connection.connect(null, 6000, 24000);

			// Store a base64 encoded string representing the HostKey
			serverHostKey = Base64.encodeToString(connectionInfo.serverHostKey, Base64.DEFAULT);

			// Get information on supported authentication methods we're interested in.
			passwordAuth            = connection.isAuthMethodAvailable(user, "password");
			pubKeyAuth              = connection.isAuthMethodAvailable(user, "publickey");
			keyboardInteractiveAuth = connection.isAuthMethodAvailable(user, "keyboard-interactive");
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
		// Because JSch returns the host key base64 encoded, and trilead ssh returns it not base64 encoded,
		// we compare savedHostKey to serverHostKey both base64 encoded and not.
		return savedServerHostKey.equals(serverHostKey) ||
				savedServerHostKey.equals(new String(Base64.decode(serverHostKey, Base64.DEFAULT)));
	}

	/**
	 * Returns whether the server can authenticate with a password.
	 */
	private boolean canAuthWithPass () {
		return passwordAuth || keyboardInteractiveAuth;
	}

	/**
	 * Returns whether the server can authenticate with a key.
	 */
	private boolean canAuthWithPubKey () {
		return pubKeyAuth;
	}

	/**
	 * Authenticates with a password.
	 */
	private boolean authenticateWithPassword () {
		boolean isAuthenticated = false;

		try {
			if (passwordAuth) {
				Log.i(TAG, "Trying SSH password authentication.");
				isAuthenticated = connection.authenticateWithPassword(user, password);
			}
			if (!isAuthenticated && keyboardInteractiveAuth) {
				Log.i(TAG, "Trying SSH keyboard-interactive authentication.");
				isAuthenticated = connection.authenticateWithKeyboardInteractive(user, this);				
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
		
		// Detect an empty key (not generated).
		if (sshPrivKey.length() == 0)
			throw new Exception ("SSH key-pair not generated yet! Please generate one and put (its public part) " +
					"in authorized_keys on the SSH server.");

		// Detect passphrase entered when key unencrypted and report error.
		if (passphrase.length() != 0 &&
			!PubkeyUtils.isEncrypted(sshPrivKey))
				throw new Exception ("Passphrase provided but key-pair not encrypted. Please delete passphrase.");
		
		// Try to decrypt and recover keypair, and failing that, report error.
		kp = PubkeyUtils.decryptAndRecoverKeyPair(sshPrivKey, passphrase);
		if (kp == null) 
			throw new Exception ("Failed to decrypt key-pair. Please ensure you've entered your passphrase " +
					"correctly in the 'SSH Passphrase' field on the main screen.");

		privateKey = kp.getPrivate();
		publicKey = kp.getPublic();
	}

	/**
	 * Authenticates with a public/private key-pair.
	 */
	private boolean authenticateWithPubKey () throws Exception {
		
		decryptAndRecoverKey();
		Log.i(TAG, "Trying SSH pubkey authentication.");
		return connection.authenticateWithPublicKey(user, PubkeyUtils.convertToTrilead(privateKey, publicKey));
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
			remoteStdin  = new BufferedOutputStream(session.getStdin());
			Thread.sleep(secTimeout*1000);
		} catch (Exception e) {
			e.printStackTrace();
			throw new Exception ("Could not execute remote command.");
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
			throw new Exception ("Could not send sudo password.");
		}
	}

	// TODO: Improve this function - it is rather "wordy".
	/**
	 * Parses the remote stdout for PORT=
	 */
	private int parseRemoteStdoutForPort () {

		Log.i (TAG, "Parsing remote stdout for PORT=");
		boolean foundP = false, foundPO = false, foundPOR = false,
				foundPORT = false, foundPORTEQ = false;
		int port = -1;

		try {
			// Look for PORT=
			int data = remoteStdout.read();
			while (data != -1) {
				if      (data == 'P')
					foundP = true;
				else if (data == 'O' && foundP)
					foundPO = true;
				else if (data == 'R' && foundPO)
					foundPOR = true;
				else if (data == 'T' && foundPOR)
					foundPORT = true;
				else if (data == '=' && foundPORT)
					foundPORTEQ = true;
				else {
					foundP = false;
					foundPO = false;
					foundPOR = false;
					foundPORT = false;
					foundPORTEQ = false;
				}
				if (foundPORTEQ)
					break;
				else
					data = remoteStdout.read();
			}

			if (foundPORTEQ) {
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

	/**
	 * Used for keyboard-interactive authentication.
	 */
	@Override
	public String[] replyToChallenge(String name, String instruction,
									int numPrompts, String[] prompt,
									boolean[] echo) throws Exception {
        String[] responses = new String[numPrompts];
        for (int x=0; x < numPrompts; x++) {
            responses[x] = password;
        }
        return responses;	
	}
}
