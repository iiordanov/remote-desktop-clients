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
import android.util.Base64;
import android.util.Log;


public class SSHConnection implements InteractiveCallback {
	private final static String TAG = "SSHConnection";
	private Connection connection;
	private final int numPortTries = 100;
	ServerHostKeyVerifier hostKeyVerifier;
	private ConnectionInfo connectionInfo;
	private String serverHostKey;
	private Session session;
	boolean passwordAuth = false;
	boolean keyboardInteractiveAuth = false;
	String sshPassword = null;

	public SSHConnection(String host, int sshPort) {
		
		connection = new Connection(host, sshPort);
	}
	
	String getServerHostKey() {
		return serverHostKey;
	}
	
	public boolean connect (String user) {
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
	
	public boolean verifyHostKey (String savedHostKey) {
		// Because JSch returns the host key base64 encoded, and trilead ssh returns it not base64 encoded,
		// we compare savedHostKey to serverHostKey both base64 encoded and not.
		return savedHostKey.equals(serverHostKey) ||
				savedHostKey.equals(new String(Base64.decode(serverHostKey, Base64.DEFAULT)));
	}

	public void disconnect () {
		connection.close();
	}

	public boolean canAuthWithPass (String user) {
		return passwordAuth || keyboardInteractiveAuth;
	}
	
	public boolean authenticate (String user, String password) {
		sshPassword = new String(password);
		boolean isAuthenticated = false;

		try {
			if (passwordAuth) {
				Log.i(TAG, "Trying SSH password authentication.");
				isAuthenticated = connection.authenticateWithPassword(user, sshPassword);
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
            responses[x] = sshPassword;
        }
        return responses;	
	}
}
