/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
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

package com.iiordanov.tigervnc.rfb;

import javax.net.ssl.*;

import java.security.cert.*;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Collection;

import android.content.DialogInterface;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.VncCanvas;
import com.iiordanov.bVNC.VncCanvasActivity;
import com.iiordanov.bVNC.bVNC;
import com.iiordanov.tigervnc.rdr.*;
import org.apache.commons.*;

public class CSecurityTLS extends CSecurity {

  public static StringParameter x509ca
  = new StringParameter("x509ca",
                        "X509 CA certificate", ""); //"/system/etc/security/cacerts.bks");
  public static StringParameter x509crl
  = new StringParameter("x509crl",
                        "X509 CRL file", "");

  private void initGlobal() 
  {
    try {
      SSLSocketFactory sslfactory;
      SSLContext ctx = SSLContext.getInstance("TLS");

      if (anon) {
        ctx.init(null, null, null);
      } else {
        TrustManager[] myTM = new TrustManager[] { 
          new MyX509TrustManager() 
        };
        ctx.init (null, myTM, null);
      }
 
      sslfactory = ctx.getSocketFactory();
      try {
        ssl = (SSLSocket)sslfactory.createSocket(CConnection.sock,
						  CConnection.sock.getInetAddress().getHostName(),
						  CConnection.sock.getPort(), true);
        ssl.setTcpNoDelay(true);
      } catch (java.io.IOException e) { 
        throw new Exception(e.toString());
      }

      if (anon) {
        String[] supported;
        ArrayList<String> enabled = new ArrayList<String>();

        supported = ssl.getSupportedCipherSuites();

        for (int i = 0; i < supported.length; i++) {
          //Log.e("SUPPORTED CIPHERS", supported[i]);
          if (supported[i].matches("TLS_DH_anon.*"))
	          enabled.add(supported[i]);
        }

        if (enabled.size() == 0)
          	throw new Exception("Your device lacks support for ciphers necessary for this encryption mode " +
						"(Anonymous Diffie-Hellman ciphers). " +
						"This is a known issue with devices running Android 2.2.x and older. You can " +
						"work around this by using VeNCrypt with x509 certificates instead.");

        ssl.setEnabledCipherSuites(enabled.toArray(new String[0]));
      } else {
        ssl.setEnabledCipherSuites(ssl.getSupportedCipherSuites());
      }

      ssl.setEnabledProtocols(ssl.getSupportedProtocols());
      ssl.addHandshakeCompletedListener(new MyHandshakeListener());
    }
    catch (java.security.GeneralSecurityException e)
    {
      vlog.error ("TLS handshake failed " + e.toString ());
      return;
    }
  }

  public CSecurityTLS(boolean _anon, VncCanvas v) 
  {
	vncCanvas = v;
    anon = _anon;
    setDefaults();
    cafile = x509ca.getData(); 
    crlfile = x509crl.getData(); 
  }

  public static void setDefaults()
  {
	// TODO: Perhaps add x509ca and crl fields in database.
    String homeDir = null;
    
    if ((homeDir/*=UserPrefs.getHomeDir()*/) == null) {
      vlog.error("Could not obtain VNC home directory path");
      return;
    }

    String vnchomedir = null; /*homeDir+UserPrefs.getFileSeparator()+".vnc"+
                        UserPrefs.getFileSeparator();*/
    String caDefault = new String(vnchomedir+"x509_ca.pem");
    String crlDefault = new String(vnchomedir+"x509_crl.pem");

    if (new File(caDefault).exists())
      x509ca.setDefaultStr(caDefault);
    if (new File(crlDefault).exists())
      x509crl.setDefaultStr(crlDefault);
  }

  public boolean processMsg(CConnection cc) {
    is = cc.getInStream();

    initGlobal();

    if (!is.checkNoWait(1))
      return false;

    if (is.readU8() == 0) {
      int result = is.readU32();
      String reason;
      if (result == Security.secResultFailed ||
          result == Security.secResultTooMany)
        reason = is.readString();
      else
        reason = new String("Authentication failure (protocol error)");
      throw new AuthFailureException(reason);
    }

    // SSLSocket.getSession blocks until the handshake is complete
    session = ssl.getSession();
    if (!session.isValid())
      throw new Exception("TLS Handshake failed!");

    try {
      cc.setStreams(new JavaInStream(ssl.getInputStream()),
		                new JavaOutStream(ssl.getOutputStream()));
    } catch (java.io.IOException e) { 
      throw new Exception("Failed to set streams");
    }

    return true;
  }

  class MyHandshakeListener implements HandshakeCompletedListener {
   public void handshakeCompleted(HandshakeCompletedEvent e) {
     vlog.info("Handshake succesful!");
     vlog.info("Using cipher suite: " + e.getCipherSuite());
   }
  }

  class MyX509TrustManager implements X509TrustManager
  {
    X509TrustManager tm;

    MyX509TrustManager() throws java.security.GeneralSecurityException
    {
      TrustManagerFactory tmf =
        TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
      KeyStore ks = KeyStore.getInstance("BKS");
      CertificateFactory cf = CertificateFactory.getInstance("X.509");
      try {
        ks.load(null, null);
        File cacert = new File(cafile);
        if (!cacert.exists() || !cacert.canRead())
          return;
        InputStream caStream = new FileInputStream(cafile);
        X509Certificate ca = (X509Certificate)cf.generateCertificate(caStream);
        ks.setCertificateEntry("CA", ca);
        PKIXBuilderParameters params = new PKIXBuilderParameters(ks, new X509CertSelector());
        File crlcert = new File(crlfile);
        if (!crlcert.exists() || !crlcert.canRead()) {
          params.setRevocationEnabled(false);
        } else {
          InputStream crlStream = new FileInputStream(crlfile);
          Collection<? extends CRL> crls = cf.generateCRLs(crlStream);
          CertStoreParameters csp = new CollectionCertStoreParameters(crls);
          CertStore store = CertStore.getInstance("Collection", csp);
          params.addCertStore(store);
          params.setRevocationEnabled(true);
        }
        tmf.init(new CertPathTrustManagerParameters(params));
      } catch (java.io.FileNotFoundException e) { 
        vlog.error(e.toString());
      } catch (java.io.IOException e) {
        vlog.error(e.toString());
      }
      tm = (X509TrustManager)tmf.getTrustManagers()[0];
    }

    public void checkClientTrusted(X509Certificate[] chain, String authType) 
      throws CertificateException
    {
      	if (tm!=null)
    		tm.checkClientTrusted(chain, authType);
    }
  
    public void checkServerTrusted(X509Certificate[] chain, String authType)
    	      throws CertificateException
    {
    	try {
    		if (tm!=null)
    			tm.checkClientTrusted(chain, authType);
    		else {
    			throw new CertificateException ("The authenticity of the server's certificate could not be established.");
    		}
    	} catch (final java.lang.Exception e) {

    		// Send a message containing the certificate to our handler.
    		Message m = new Message();
    		m.setTarget(vncCanvas.handler);
    		// TODO: Think of how to best replace this numeric value with something sane.
    		m.what = 1;
    		m.obj = chain[0];
    		vncCanvas.handler.sendMessage(m);

			// Block while user decides whether to accept certificate.
			while (!vncCanvas.certificateAccepted) {
    			try {
					Thread.sleep(100);
				} catch (InterruptedException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
    		}
    	}
    }

    public X509Certificate[] getAcceptedIssuers ()
    {
    	if (tm!=null)
    		return tm.getAcceptedIssuers();
    	else
    		return null;
    }
  }

  public final int getType() { return anon ? Security.secTypeTLSNone : Security.secTypeX509None; }
  public final String description() 
    { return anon ? "TLS Encryption without VncAuth" : "X509 Encryption without VncAuth"; }


  //protected void setParam();
  //protected void checkSession();
  protected CConnection cc;

  private boolean anon;
  private SSLSession session;
  private String cafile, crlfile;
  private InStream is;
  private SSLSocket ssl;
  TrustManager[] trustAllCerts;
  VncCanvas vncCanvas;

  static LogWriter vlog = new LogWriter("CSecurityTLS");
}
