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
import android.util.Base64;
import android.util.Log;


public class SSHConnection {
	private final static String TAG = "SSHConnection";
	private Connection connection;
	private final int authWaitPeriod = 300;
	private final int numAuthWaits = 3;
	private final int numPortTries = 100;
	ServerHostKeyVerifier hostKeyVerifier;
	private ConnectionInfo connectionInfo;
	private String serverHostKey;
	private Session session;

	public SSHConnection(String host, int sshPort) {
		
		connection = new Connection(host, sshPort);
	}
	
	String getServerHostKey() {
		return serverHostKey;
	}
	
	public boolean connect () {
		try {
			connection.setTCPNoDelay(true);
			//connection.setCompression(false);
			
			// TODO: Start controlling timeouts.
			connectionInfo = connection.connect();
			
			// Store a base64 encoded string representing the HostKey
			serverHostKey = Base64.encodeToString(connectionInfo.serverHostKey, Base64.DEFAULT);
		
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
		try {
			return connection.isAuthMethodAvailable(user, "password");
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
	}
	
	public boolean authenticate (String user, String password) {
		int numWaited = 0;
		
		try {
			connection.authenticateWithPassword(user, password);
		} catch (IOException e) {
			e.printStackTrace();
			return false;
		}
		
		while (!connection.isAuthenticationComplete()) {
			if (numWaited >= numAuthWaits)
				return false;
			try {
				Thread.sleep(authWaitPeriod);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			numWaited++;
		}
		return true;
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
			// TODO Auto-generated catch block
			e.printStackTrace();
			return false;
		}

		return true;
	}
}
