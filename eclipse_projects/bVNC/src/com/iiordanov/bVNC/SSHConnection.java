/* 
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

//TODO: Document functions.

package com.iiordanov.bVNC;

import java.io.IOException;

import com.trilead.ssh2.Connection;
import com.trilead.ssh2.ConnectionInfo;
import com.trilead.ssh2.ServerHostKeyVerifier;
import com.trilead.ssh2.Session;
import com.trilead.ssh2.InteractiveCallback;
import com.trilead.ssh2.KnownHosts;

import android.content.Context;
import android.util.Base64;
import android.util.Log;


public class SSHConnection implements InteractiveCallback {
	private final static String TAG = "SSHConnection";
	private Connection connection;
	private final int numPortTries = 1000;
	ServerHostKeyVerifier hostKeyVerifier;
	private ConnectionInfo connectionInfo;
	private String serverHostKey;
	private Session session;
	boolean passwordAuth = false;
	boolean keyboardInteractiveAuth = false;
	
	// Connection parameters
	String host;
	String user;
	String password;
	String savedServerHostKey;
	String targetAddress;
	int sshPort;
	int targetPort;

	public SSHConnection(ConnectionBean conn) {
		host = conn.getSshServer();
		sshPort = conn.getSshPort();
		user = conn.getSshUser();
		password = conn.getSshPassword();
		savedServerHostKey = conn.getSshHostKey();
		targetPort = conn.getPort();
		targetAddress = conn.getAddress();
		
		connection = new Connection(host, sshPort);
	}
	
	String getServerHostKey() {
		return serverHostKey;
	}
	
	
	public int initializeSSHTunnel () throws Exception {
		int localForwardedPort;

		// Attempt to connect.
		if (!connect())
			throw new Exception("Failed to connect to SSH server. Please check network connection status, and SSH Server address and port.");
		
		// Verify host key against saved one.
		if (!verifyHostKey())
			throw new Exception("ERROR! The server host key has changed. If this is intentional, " +
								"please delete and recreate the connection. Otherwise, this may be " +
								"a man in the middle attack. Not continuing.");
		
		// Authenticate and set up port forwarding.
		if (canAuthWithPass()) {
			if (authenticate()) {
				localForwardedPort = createPortForward(targetPort, targetAddress, targetPort);
				
				// If we got back a negative number, port forwarding failed.
				if (localForwardedPort < 0) {
					throw new Exception("Could not set up the port forwarding for tunneling VNC traffic over SSH." +
							"Please ensure your SSH server is configured to allow port forwarding and try again.");
				}
				
				// TODO: This is a proof of concept for remote command execution.
				//if (!sshConnection.execRemoteCommand("/usr/bin/x11vnc -N -forever -auth guess -localhost -display :0 1>/dev/null 2>/dev/null", 5000))
				//	throw new Exception("Could not execute remote command.");
			} else {
				throw new Exception("Failed to authenticate to SSH server. Please check your SSH username and password.");
			}
		} else {
			throw new Exception("Remote server " + targetAddress + " supports neither \"password\" nor " +
								"\"keyboard-interactive\" auth methods. Please configure it to allow at least one " +
								"of the two methods and try again.");
		}
		return localForwardedPort;
	}
	
	
	public boolean connect () {
			
		try {
			connection.setTCPNoDelay(true);
			
			connectionInfo = connection.connect();
			
			// Store a base64 encoded string representing the HostKey
			serverHostKey = Base64.encodeToString(connectionInfo.serverHostKey, Base64.DEFAULT);

			// Get information on supported authentication methods we're interested in.
			passwordAuth            = connection.isAuthMethodAvailable(user, "password");
			keyboardInteractiveAuth = connection.isAuthMethodAvailable(user, "keyboard-interactive");
			
			return true;
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
	}

	/*
	 * Return a string holding a Hex representation of the signature of the remote host's key.
	 */
	public String getHostKeySignature () {
		return KnownHosts.createHexFingerprint(connectionInfo.serverHostKeyAlgorithm, connectionInfo.serverHostKey);
	}
	
	public boolean verifyHostKey () {
		// Because JSch returns the host key base64 encoded, and trilead ssh returns it not base64 encoded,
		// we compare savedHostKey to serverHostKey both base64 encoded and not.
		return savedServerHostKey.equals(serverHostKey) ||
				savedServerHostKey.equals(new String(Base64.decode(serverHostKey, Base64.DEFAULT)));
	}

	public void disconnect () {
		connection.close();
	}

	public boolean canAuthWithPass () {
		return passwordAuth || keyboardInteractiveAuth;
	}
	
	public boolean authenticate () {
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
	
	public int createPortForward (int localPortStart, String remoteHost, int remotePort) {
		int portsTried = 0;
		while (portsTried < numPortTries) {
			try {
				connection.createLocalPortForwarder(localPortStart + portsTried, remoteHost, remotePort);
				return localPortStart + portsTried;
			} catch (IOException e) {
				portsTried++;
			}			
		}
		return -1;
	}
	
	public boolean execRemoteCommand (String cmd, long sleepTime){
		try {
			session = connection.openSession();
			session.execCommand(cmd);
			Thread.sleep(sleepTime);
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}

		return true;
	}

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
