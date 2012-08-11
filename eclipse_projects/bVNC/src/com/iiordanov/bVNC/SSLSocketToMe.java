/*
 * SSLSocketToMe.java: add SSL encryption to Java VNC viewer.
 *
 * Copyright (c) 2006 Karl J. Runge <runge@karlrunge.com>
 * All rights reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 *
 */

package com.iiordanov.bVNC;

import java.net.*;
import java.io.*;
import javax.net.ssl.*;

import android.app.Activity;
import android.util.Base64;

import java.util.*;

import java.security.*;
import java.security.cert.*;
import java.security.spec.*;
import java.security.cert.Certificate;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateFactory;
import android.util.Base64;
import android.util.Log;


public class SSLSocketToMe {

	/* basic member data: */
	String host;
	int port;
	boolean debug = true;

	/* sockets */
	SSLSocket socket = null;
	SSLSocketFactory factory;

	/* fallback for Proxy connection */
	boolean proxy_in_use = false;
	boolean proxy_is_https = false;
	boolean proxy_failure = false;
	public DataInputStream is = null;
	public OutputStream os = null;

	String proxy_dialog_host = null;
	int proxy_dialog_port = 0;

	Socket proxySock;
	DataInputStream proxy_is;
	OutputStream proxy_os;

	/* trust contexts */
	SSLContext trustloc_ctx;
	SSLContext trustall_ctx;
	SSLContext trusturl_ctx;
	SSLContext trustone_ctx;

	TrustManager[] trustLocCerts;
	TrustManager[] trustAllCerts;
	TrustManager[] trustUrlCert;
	TrustManager[] trustOneCert;

	boolean use_url_cert_for_auth = true;
	boolean user_wants_to_see_cert = true;

	// TODO: quick hack to port SSL patch
	  boolean GET = false;
	  String CONNECT = null;
	  String urlPrefix = null;
	  String httpsPort = null;
	  String oneTimeKey = null;
	  boolean forceProxy = false;
	  boolean ignoreProxy = true;
	  boolean trustAllVncCerts = true;
	  boolean trustUrlVncCert = false;
	
	/* cert(s) we retrieve from VNC server */
	java.security.cert.Certificate[] trustallCerts = null;
	java.security.cert.Certificate[] trusturlCerts = null;

	byte[] hex2bytes(String s) {
		byte[] bytes = new byte[s.length()/2];
		for (int i=0; i<s.length()/2; i++) {
			int j = 2*i;
			try {
				int val = Integer.parseInt(s.substring(j, j+2), 16);
				if (val > 127) {
					val -= 256;
				}
				Integer I = new Integer(val);
				bytes[i] = Byte.decode(I.toString()).byteValue();
				
			} catch (Exception e) {
				;
			}
		}
		return bytes;
	}

	SSLSocketToMe(String h, int p, Socket sock) throws Exception {
		
	}
	
	SSLSocketToMe(String h, int p) throws Exception {
		host = h;
		port = p;

		/* we will first try default factory for certification: */

		factory = (SSLSocketFactory) SSLSocketFactory.getDefault();

		dbg("SSL startup: " + host + " " + port);

		/* create trust managers used if initial handshake fails: */

		trustLocCerts = new TrustManager[] {
				
			new X509TrustManager() {
				public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException {
				}

				public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {
				}

				public X509Certificate[] getAcceptedIssuers() {
					return null;
				}
			}
		};
		
		
		trustAllCerts = new TrustManager[] {
		    /*
		     * this one accepts everything.
		     */
		    new X509TrustManager() {
			public java.security.cert.X509Certificate[]
			    getAcceptedIssuers() {
				return null;
			}
			public void checkClientTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) {
				/* empty */
			}
			public void checkServerTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) {
				/* empty */
				dbg("ALL: an untrusted connect to grab cert.");
			}
		    }
		};

		trustUrlCert = new TrustManager[] {
		    /*
		     * this one accepts only the retrieved server cert
		     * by SSLSocket by this applet.
		     */
		    new X509TrustManager() {
			public java.security.cert.X509Certificate[]
			    getAcceptedIssuers() {
				return null;
			}
			public void checkClientTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) throws CertificateException {
				throw new CertificateException("No Clients");
			}
			public void checkServerTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) throws CertificateException {
				if (trusturlCerts == null) {
					throw new CertificateException(
					    "No Trust url Certs array.");
				}
				if (trusturlCerts.length < 1) {
					throw new CertificateException(
					    "No Trust url Certs.");
				}
				if (trusturlCerts.length > 1) {
					int i;
					boolean ok = true;
					for (i = 0; i < trusturlCerts.length - 1; i++)  {
						if (! trusturlCerts[i].equals(trusturlCerts[i+1])) {
							ok = false;
						}
					}
					if (! ok) {
						throw new CertificateException(
						    "Too many Trust url Certs: "
						    + trusturlCerts.length
						);
					}
				}
				if (certs == null) {
					throw new CertificateException(
					    "No this-certs array.");
				}
				if (certs.length < 1) {
					throw new CertificateException(
					    "No this-certs Certs.");
				}
				if (certs.length > 1) {
					int i;
					boolean ok = true;
					for (i = 0; i < certs.length - 1; i++)  {
						if (! certs[i].equals(certs[i+1])) {
							ok = false;
						}
					}
					if (! ok) {
						throw new CertificateException(
						    "Too many this-certs: "
						    + certs.length
						);
					}
				}
				if (! trusturlCerts[0].equals(certs[0])) {
					throw new CertificateException(
					    "Server Cert Changed != URL.");
				}
				dbg("URL: trusturlCerts[0] matches certs[0]");
			}
		    }
		};
		trustOneCert = new TrustManager[] {
		    /*
		     * this one accepts only the retrieved server cert
		     * by SSLSocket by this applet.
		     */
		    new X509TrustManager() {
			public java.security.cert.X509Certificate[]
			    getAcceptedIssuers() {
				return null;
			}
			public void checkClientTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) throws CertificateException {
				throw new CertificateException("No Clients");
			}
			public void checkServerTrusted(
			    java.security.cert.X509Certificate[] certs,
			    String authType) throws CertificateException {
				if (trustallCerts == null) {
					throw new CertificateException(
					    "No Trust All Server Certs array.");
				}
				if (trustallCerts.length < 1) {
					throw new CertificateException(
					    "No Trust All Server Certs.");
				}
				if (trustallCerts.length > 1) {
					int i;
					boolean ok = true;
					for (i = 0; i < trustallCerts.length - 1; i++)  {
						if (! trustallCerts[i].equals(trustallCerts[i+1])) {
							ok = false;
						}
					}
					if (! ok) {
						throw new CertificateException(
						    "Too many Trust All Server Certs: "
						    + trustallCerts.length
						);
					}
				}
				if (certs == null) {
					throw new CertificateException(
					    "No this-certs array.");
				}
				if (certs.length < 1) {
					throw new CertificateException(
					    "No this-certs Certs.");
				}
				if (certs.length > 1) {
					int i;
					boolean ok = true;
					for (i = 0; i < certs.length - 1; i++)  {
						if (! certs[i].equals(certs[i+1])) {
							ok = false;
						}
					}
					if (! ok) {
						throw new CertificateException(
						    "Too many this-certs: "
						    + certs.length
						);
					}
				}
				if (! trustallCerts[0].equals(certs[0])) {
					throw new CertificateException(
					    "Server Cert Changed != TRUSTALL.");
				}
				dbg("ONE: trustallCerts[0] matches certs[0]");
			}
		    }
		};

		/* 
		 * They are used:
		 *
		 * 1) to retrieve the server cert in case of failure to
		 *    display it to the user.
		 * 2) to subsequently connect to the server if user agrees.
		 */

		KeyManager[] mykey = null;

		if (this.oneTimeKey != null && this.oneTimeKey.equals("PROMPT")) {
			//ClientCertDialog d = new ClientCertDialog();
			this.oneTimeKey = null;
		}

		if (this.oneTimeKey != null && this.oneTimeKey.indexOf(",") > 0) {
			int idx = this.oneTimeKey.indexOf(",");

			String onetimekey = this.oneTimeKey.substring(0, idx);
			byte[] key = Base64.decode(onetimekey, Base64.DEFAULT);
			String onetimecert = this.oneTimeKey.substring(idx+1);
			byte[] cert = Base64.decode(onetimecert, Base64.DEFAULT);

			KeyFactory kf = KeyFactory.getInstance("RSA");
			PKCS8EncodedKeySpec keysp = new PKCS8EncodedKeySpec ( key );
			PrivateKey ff = kf.generatePrivate (keysp);
			//dbg("ff " + ff);
			String cert_str = new String(cert);

			CertificateFactory cf = CertificateFactory.getInstance("X.509");
			Collection c = cf.generateCertificates(new ByteArrayInputStream(cert));
			Certificate[] certs = new Certificate[c.toArray().length];
			if (c.size() == 1) {
				Certificate tmpcert = cf.generateCertificate(new ByteArrayInputStream(cert));
				//dbg("tmpcert" + tmpcert);
				certs[0] = tmpcert;
			} else {
				certs = (Certificate[]) c.toArray();
			}

			KeyStore ks = KeyStore.getInstance("BKS");
			ks.load(null, null);
			ks.setKeyEntry("onetimekey", ff, "".toCharArray(), certs);
			String da = KeyManagerFactory.getDefaultAlgorithm();
			KeyManagerFactory kmf = KeyManagerFactory.getInstance(da);
			kmf.init(ks, "".toCharArray());

			mykey = kmf.getKeyManagers();
		}


		/* trust loc certs: */
		try {
			trustloc_ctx = SSLContext.getInstance("TLS");
			trustloc_ctx.init(null, null, new java.security.SecureRandom());

		} catch (Exception e) {
			String msg = "SSL trustloc_ctx FAILED.";
			dbg(msg);
			throw new Exception(msg);
		}

		/* trust all certs: */
		try {
			trustall_ctx = SSLContext.getInstance("TLS");
			trustall_ctx.init(null, trustAllCerts, new
			    java.security.SecureRandom());

		} catch (Exception e) {
			String msg = "SSL trustall_ctx FAILED.";
			dbg(msg);
			throw new Exception(msg);
		}

		/* trust url certs: */
		try {
			trusturl_ctx = SSLContext.getInstance("TLS");
			trusturl_ctx.init(null, trustUrlCert, new
			    java.security.SecureRandom());

		} catch (Exception e) {
			String msg = "SSL trusturl_ctx FAILED.";
			dbg(msg);
			throw new Exception(msg);
		}

		/* trust the one cert from server: */
		try {
			trustone_ctx = SSLContext.getInstance("TLS");
			trustone_ctx.init(null, trustOneCert, new
			    java.security.SecureRandom());

		} catch (Exception e) {
			String msg = "SSL trustone_ctx FAILED.";
			dbg(msg);
			throw new Exception(msg);
		}

	}

	boolean browser_cert_match() {
		String msg = "Browser URL accept previously accepted cert";

		if (user_wants_to_see_cert) {
			return false;
		}

		if (trustallCerts != null && trusturlCerts != null) {
		    if (trustallCerts.length == 1 && trusturlCerts.length == 1) {
			if (trustallCerts[0].equals(trusturlCerts[0])) {
				System.out.println(msg);
				return true;
			}
		    }
		}
		return false;
	}

	public void check_for_proxy() {
		
		boolean result = false;

		trusturlCerts = null;
		proxy_in_use = false;
		if (this.ignoreProxy) {
			return;
		}

		String ustr = "https://" + host + ":";
		if (this.httpsPort != null) {
			ustr += this.httpsPort;
		} else {
			ustr += port;	// hmmm
		}
		ustr += this.urlPrefix + "/check.https.proxy.connection";
		dbg("ustr is: " + ustr);


		try {
			URL url = new URL(ustr);
			HttpsURLConnection https = (HttpsURLConnection)
			    url.openConnection();

			https.setUseCaches(false);
			https.setRequestMethod("GET");
			https.setRequestProperty("Pragma", "No-Cache");
			https.setRequestProperty("Proxy-Connection",
			    "Keep-Alive");
			https.setDoInput(true);

			https.connect();

			trusturlCerts = https.getServerCertificates();
			if (trusturlCerts == null) {
				dbg("set trusturlCerts to null...");
			} else {
				dbg("set trusturlCerts to non-null");
			}

			if (https.usingProxy()) {
				proxy_in_use = true;
				proxy_is_https = true;
				dbg("HTTPS proxy in use. There may be connection problems.");
			}
			Object output = https.getContent();
			https.disconnect();
			result = true;

		} catch(Exception e) {
			dbg("HttpsURLConnection: " + e.getMessage());
		}

		if (proxy_in_use) {
			return;
		}

		ustr = "http://" + host + ":" + port;
		ustr += this.urlPrefix + "/index.vnc";

		try {
			URL url = new URL(ustr);
			HttpURLConnection http = (HttpURLConnection)
			    url.openConnection();

			http.setUseCaches(false);
			http.setRequestMethod("GET");
			http.setRequestProperty("Pragma", "No-Cache");
			http.setRequestProperty("Proxy-Connection",
			    "Keep-Alive");
			http.setDoInput(true);

			http.connect();

			if (http.usingProxy()) {
				proxy_in_use = true;
				proxy_is_https = false;
				dbg("HTTP proxy in use. There may be connection problems.");
			}
			Object output = http.getContent();
			http.disconnect();

		} catch(Exception e) {
			dbg("HttpURLConnection: " + e.getMessage());
		}
	}

	public Socket connectSock(Socket preexistingSocket) throws Exception {

		/*
		 * first try a https connection to detect a proxy, and
		 * also grab the VNC server cert.
		 */
		check_for_proxy();
		
		if (this.trustAllVncCerts) {
			dbg("this.trustAllVncCerts-0 using trustall_ctx");
			factory = trustall_ctx.getSocketFactory();
		} else if (use_url_cert_for_auth && trusturlCerts != null) {
			dbg("using trusturl_ctx");
			factory = trusturl_ctx.getSocketFactory();
		} else {
			dbg("using trustloc_ctx");
			factory = trustloc_ctx.getSocketFactory();
		}

		socket = null;
		try {
			if (proxy_in_use && this.forceProxy) {
				throw new Exception("forcing proxy (forceProxy)");
			} else if (this.CONNECT != null) {
				throw new Exception("forcing CONNECT");
			}

			if (preexistingSocket != null) {
		      try {
		        socket = (SSLSocket)factory.createSocket(preexistingSocket, host, port, true);
		        socket.setTcpNoDelay(true);
		        socket.setSoTimeout(0);
		      } catch (java.io.IOException e) { 
		        throw new Exception(e.toString());
		      }
	          String[] supported;
	          ArrayList<String> enabled = new ArrayList<String>();
	          supported = socket.getSupportedCipherSuites();
	          for (int i = 0; i < supported.length; i++) {
	          	//Log.d("SUPPORTED CIPHER", supported[i]);
	            if (supported[i].matches("TLS_DH_anon.*"))
	  	          enabled.add(supported[i]);
	          }

	          if (enabled.size() == 0)
	          	throw new Exception("Your device lacks support for ciphers necessary for this encryption mode " +
	          						"(Anonymous Diffie-Hellman ciphers). " +
	          						"This is a known issue with devices running Android 2.2.x and older. You can " +
	          						"work around this by using VeNCrypt with x509 certificates instead.");
	          
	          socket.setEnabledCipherSuites(enabled.toArray(new String[0]));
	          socket.setEnabledProtocols(socket.getSupportedProtocols());
			} else {
				int timeout = 0;
				socket = (SSLSocket) factory.createSocket();
				InetSocketAddress inetaddr = new InetSocketAddress(host, port);
				dbg("Using timeout of " + timeout + " secs to: " + host + ":" + port);
		        socket.setTcpNoDelay(true);
				socket.connect(inetaddr, timeout * 1000);
			}
		} catch (Exception esock) {
			dbg("esock: " + esock.getMessage());
			if (proxy_in_use || this.CONNECT != null) {
				proxy_failure = true;
				if (proxy_in_use) {
					dbg("HTTPS proxy in use. Trying to go with it.");
				} else {
					dbg("this.CONNECT reverse proxy in use. Trying to go with it.");
				}
				try {
					socket = proxy_socket(factory);
				} catch (Exception e) {
					dbg("err proxy_socket: " + e.getMessage());
				}
			} else
				throw new Exception (esock);
		}

		try {
			socket.startHandshake();
			dbg("Server Connection Verified on 1st try.");

			java.security.cert.Certificate[] currentTrustedCerts;
			//BrowserCertsDialog bcd;

			SSLSession sess = socket.getSession();
			try {
				currentTrustedCerts = sess.getPeerCertificates();
			} catch (Exception e) {
				currentTrustedCerts = null;
			}

			if (this.trustAllVncCerts) {
				dbg("this.trustAllVncCerts-1");
			} else if (currentTrustedCerts == null || currentTrustedCerts.length < 1) {
				socket.close();
				socket = null;
				throw new SSLHandshakeException("no current certs");
			}

			String serv = "";
			try {
				CertInfo ci = new CertInfo(currentTrustedCerts[0]);
				serv = ci.get_certinfo("CN");
			} catch (Exception e) {
				;
			}

			if (this.trustAllVncCerts) {
				dbg("this.trustAllVncCerts-2");
				user_wants_to_see_cert = false;
			} else if (this.trustUrlVncCert) {
				dbg("this.trustUrlVncCert-1");
				user_wants_to_see_cert = false;
			} else {
				//bcd = new BrowserCertsDialog(serv, host + ":" + port);
				dbg("bcd START");
				//bcd.queryUser();
				dbg("bcd DONE");
				if (false) {//bcd.showCertDialog) {
					String msg = "user wants to see cert";
					dbg(msg);
					user_wants_to_see_cert = true;
					throw new SSLHandshakeException(msg);
				} else {
					user_wants_to_see_cert = false;
					dbg("bcd: user said yes, accept it");
				}
			}

		} catch (SSLHandshakeException eh)  {
			dbg("Could not automatically verify Server.");
			dbg("msg: " + eh.getMessage());

			socket.close();
			socket = null;

			/*
			 * Reconnect, trusting any cert, so we can grab
			 * the cert to show it to the user.  The connection
			 * is not used for anything else.
			 */
			factory = trustall_ctx.getSocketFactory();
			if (proxy_failure) {
				socket = proxy_socket(factory);
			} else {
				socket = (SSLSocket) factory.createSocket(host, port);
			}

			try {
				socket.startHandshake();
				dbg("TrustAll Server Connection Verified.");

				/* grab the cert: */
				try {
					SSLSession sess = socket.getSession();
					trustallCerts = sess.getPeerCertificates();
				} catch (Exception e) {
					throw new Exception("Could not get " + 
					    "Peer Certificate");	
				}

				if (this.trustAllVncCerts) {
					dbg("this.trustAllVncCerts-3");
				} else if (! browser_cert_match()) {
					/*
					 * close socket now, we will reopen after
					 * dialog if user agrees to use the cert.
					 */
					socket.close();
					socket = null;

					/* dialog with user to accept cert or not: */

					//TrustDialog td= new TrustDialog(host, port,
					//    trustallCerts);

					if (false) {//! td.queryUser()) {
						String msg = "User decided against it.";
						dbg(msg);
						throw new IOException(msg);
					}
				}

			} catch (Exception ehand2)  {
				dbg("** Could not TrustAll Verify Server.");

				throw new IOException(ehand2.getMessage());
			}

			/*
			 * Now connect a 3rd time, using the cert
			 * retrieved during connection 2 (that the user
			 * likely blindly agreed to).
			 */

			factory = trustone_ctx.getSocketFactory();
			if (proxy_failure) {
				socket = proxy_socket(factory);
			} else {
				socket = (SSLSocket) factory.createSocket(host, port);
			}

			try {
				socket.startHandshake();
				dbg("TrustAll Server Connection Verified #3.");

			} catch (Exception ehand3)  {
				dbg("** Could not TrustAll Verify Server #3.");

				throw new IOException(ehand3.getMessage());
			}
		}

		if (socket != null && this.GET) {
			String str = "GET ";
			str += this.urlPrefix;
			str += "/request.https.vnc.connection";
			str += " HTTP/1.0\r\n";
			str += "Pragma: No-Cache\r\n";
			str += "\r\n";
			System.out.println("sending GET: " + str);
    			OutputStream os = socket.getOutputStream();
			String type = "os";
			if (type == "os") {
				os.write(str.getBytes());
				os.flush();
				System.out.println("used OutputStream");
			} else if (type == "bs") {
				BufferedOutputStream bs = new BufferedOutputStream(os);
				bs.write(str.getBytes());
				bs.flush();
				System.out.println("used BufferedOutputStream");
			} else if (type == "ds") {
				DataOutputStream ds = new DataOutputStream(os);
				ds.write(str.getBytes());
				ds.flush();
				System.out.println("used DataOutputStream");
			}
			if (false) {
				String rep = "";
				DataInputStream is = new DataInputStream(
				    new BufferedInputStream(socket.getInputStream(), 16384));
				while (true) {
					rep += readline(is);
					if (rep.indexOf("\r\n\r\n") >= 0) {
						break;
					}
				}
				System.out.println("rep: " + rep);
			}
		}

		dbg("SSL returning socket to caller.");
		return (Socket) socket;
	}

	private void dbg(String s) {
		if (debug) {
			System.out.println(s);
		}
	}

	private int gint(String s) {
		int n = -1;
		try {
			Integer I = new Integer(s);
			n = I.intValue();
		} catch (Exception ex) {
			return -1;
		}
		return n;
	}

	public SSLSocket proxy_socket(SSLSocketFactory factory) {
		Properties props = null;
		String proxyHost = null;
		int proxyPort = 0;
		String proxyHost_nossl = null;
		int proxyPort_nossl = 0;
		String str;

		/* see if we can guess the proxy info from Properties: */
		try {
			props = System.getProperties();
		} catch (Exception e) {
			dbg("props failed: " + e.getMessage());
		}
		if (props != null) {
			dbg("\n---------------\nAll props:");
			props.list(System.out);
			dbg("\n---------------\n\n");

			for (Enumeration e = props.propertyNames(); e.hasMoreElements(); ) {
				String s = (String) e.nextElement();
				String v = System.getProperty(s);
				String s2 = s.toLowerCase();
				String v2 = v.toLowerCase();

				if (s2.indexOf("proxy") < 0 && v2.indexOf("proxy") < 0) {
					continue;
				}
				if (v2.indexOf("http") < 0) {
					continue;
				}

				if (s2.indexOf("proxy.https.host") >= 0) {
					proxyHost = v2;
					continue;
				}
				if (s2.indexOf("proxy.https.port") >= 0) {
					proxyPort = gint(v2);
					continue;
				}
				if (s2.indexOf("proxy.http.host") >= 0) {
					proxyHost_nossl = v2;
					continue;
				}
				if (s2.indexOf("proxy.http.port") >= 0) {
					proxyPort_nossl = gint(v2);
					continue;
				}

				String[] pieces = v.split("[,;]");
				for (int i = 0; i < pieces.length; i++) {
					String p = pieces[i];
					int j = p.indexOf("https");
					if (j < 0) {
						j = p.indexOf("http");
						if (j < 0) {
							continue;
						}
					}
					j = p.indexOf("=", j);
					if (j < 0) {
						continue;
					}
					p = p.substring(j+1);
					String [] hp = p.split(":");
					if (hp.length != 2) {
						continue;
					}
					if (hp[0].length() > 1 && hp[1].length() > 1) {

						proxyPort = gint(hp[1]);
						if (proxyPort < 0) {
							continue;
						}
						proxyHost = new String(hp[0]);
						break;
					}
				}
			}
		}
		if (proxyHost != null) {
			if (proxyHost_nossl != null && proxyPort_nossl > 0) {
				dbg("Using http proxy info instead of https.");
				proxyHost = proxyHost_nossl;
				proxyPort = proxyPort_nossl;
			}
		}

		if (proxy_in_use) {
			if (proxy_dialog_host != null && proxy_dialog_port > 0) {
				proxyHost = proxy_dialog_host;
				proxyPort = proxy_dialog_port;
			}
			if (proxyHost != null) {
				dbg("Lucky us! we figured out the Proxy parameters: " + proxyHost + " " + proxyPort);
			} else {
				/* ask user to help us: */
				//ProxyDialog pd = new ProxyDialog(proxyHost, proxyPort);
				//pd.queryUser();
				//proxyHost = pd.getHost(); 
				//proxyPort = pd.getPort();
				proxy_dialog_host = new String(proxyHost);
				proxy_dialog_port = proxyPort;
				//dbg("User said host: " + pd.getHost() + " port: " + pd.getPort());
			}

			dbg("proxy_in_use psocket:");
			proxySock = psocket(proxyHost, proxyPort);
			if (proxySock == null) {
				dbg("1-a sadly, returning a null socket");
				return null;
			}
			String hp = host + ":" + port;

			String req1 = "CONNECT " + hp + " HTTP/1.1\r\n"
			    + "Host: " + hp + "\r\n\r\n";

			dbg("requesting1: " + req1);

			try {
				proxy_os.write(req1.getBytes());
				String reply = readline(proxy_is);

				dbg("proxy replied1: " + reply.trim());

				if (reply.indexOf("HTTP/1.") < 0 && reply.indexOf(" 200") < 0) {
					proxySock.close();
					proxySock = psocket(proxyHost, proxyPort);
					if (proxySock == null) {
						dbg("2-a sadly, returning a null socket");
						return null;
					}
				}
			} catch(Exception e) {
				dbg("sock prob1: " + e.getMessage());
			}

			while (true) {
				String line = readline(proxy_is);
				dbg("proxy line1: " + line.trim());
				if (line.equals("\r\n") || line.equals("\n")) {
					break;
				}
			}
		} else if (this.CONNECT != null) {
			dbg("this.CONNECT psocket:");
			proxySock = psocket(host, port);
			if (proxySock == null) {
				dbg("1-b sadly, returning a null socket");
				return null;
			}
		}
		
		if (this.CONNECT != null) {
			String hp = this.CONNECT;
			String req2 = "CONNECT " + hp + " HTTP/1.1\r\n"
			    + "Host: " + hp + "\r\n\r\n";

			dbg("requesting2: " + req2);

			try {
				proxy_os.write(req2.getBytes());
				String reply = readline(proxy_is);

				dbg("proxy replied2: " + reply.trim());

				if (reply.indexOf("HTTP/1.") < 0 && reply.indexOf(" 200") < 0) {
					proxySock.close();
					proxySock = psocket(proxyHost, proxyPort);
					if (proxySock == null) {
						dbg("2-b sadly, returning a null socket");
						return null;
					}
				}
			} catch(Exception e) {
				dbg("sock prob2: " + e.getMessage());
			}

			while (true) {
				String line = readline(proxy_is);
				dbg("proxy line2: " + line.trim());
				if (line.equals("\r\n") || line.equals("\n")) {
					break;
				}
			}
			
		}

		Socket sslsock = null;
		try {
			sslsock = factory.createSocket(proxySock, host, port, true);
		} catch(Exception e) {
			dbg("sslsock prob: " + e.getMessage());
			dbg("3 sadly, returning a null socket");
		}

		return (SSLSocket) sslsock;
	}

	Socket psocket(String h, int p) {
		Socket psock = null;
		try {
			psock = new Socket(h, p);
			proxy_is = new DataInputStream(new BufferedInputStream(
			    psock.getInputStream(), 16384));
			proxy_os = psock.getOutputStream();
		} catch(Exception e) {
			dbg("psocket prob: " + e.getMessage());
			return null;
		}

		return psock;
	}

	String readline(DataInputStream i) {
		byte[] ba = new byte[1];
		String s = new String("");
		ba[0] = 0;
		try {
			while (ba[0] != 0xa) {
				ba[0] = (byte) i.readUnsignedByte();
				s += new String(ba);
			}
		} catch (Exception e) {
			;
		}
		return s;
	}
}

class CertInfo {
	String fields[] = {"CN", "OU", "O", "L", "C"};
	java.security.cert.Certificate cert;
	String certString = "";

	CertInfo(java.security.cert.Certificate c) {
		cert = c;
		certString = cert.toString();
	}
	
	String get_certinfo(String which) {
		int i;
		String cs = new String(certString);
		String all = "";

		/*
		 * For now we simply scrape the cert string, there must
		 * be an API for this... perhaps optionValue?
		 */
		for (i=0; i < fields.length; i++) {
			int f, t, t1, t2;
			String sub, mat = fields[i] + "=";
			
			f = cs.indexOf(mat, 0);
			if (f > 0) {
				t1 = cs.indexOf(", ", f);
				t2 = cs.indexOf("\n", f);
				if (t1 < 0 && t2 < 0) {
					continue;
				} else if (t1 < 0) {
					t = t2;
				} else if (t2 < 0) {
					t = t1;
				} else if (t1 < t2) {
					t = t1;
				} else {
					t = t2;
				}
				if (t > f) {
					sub = cs.substring(f, t);
					all = all + "        " + sub + "\n";
					if (which.equals(fields[i])) {
						return sub;
					}
				}
			}
		}
		if (which.equals("all")) {
			return all;
		} else {
			return "";
		}
	}
}
