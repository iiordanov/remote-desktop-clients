package com.undatech.opaque.util;

import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.net.URL;
import java.net.URLConnection;
import java.nio.charset.Charset;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

public class HttpsFileDownloader {
    public static String TAG = HttpsFileDownloader.class.toString();

    String url;
    boolean verifySslCerts;
    OnDownloadFinishedListener listener;

    public interface OnDownloadFinishedListener {
        void onDownload(String contents);
    }

    public HttpsFileDownloader(String url, boolean verifySslCerts, OnDownloadFinishedListener listener) {
        this.url = url;
        this.verifySslCerts = verifySslCerts;
        this.listener = listener;
    }

    public static void initDefaultTrustManager(boolean verifySslCerts) {
        Log.d(TAG, "initDefaultTrustManager");
        if (verifySslCerts) {
            try {
                SSLContext sc = SSLContext.getInstance("SSL");
                HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
            } catch (Exception e) {
                e.printStackTrace();
            }

        } else {
            TrustManager[] trustAllCerts = new TrustManager[]{
                    new X509TrustManager() {
                        public java.security.cert.X509Certificate[] getAcceptedIssuers() { return null;}
                        public void checkClientTrusted(java.security.cert.X509Certificate[] certs, String authType) {}
                        public void checkServerTrusted(java.security.cert.X509Certificate[] certs, String authType) {}
                    }
            };

            try {
                SSLContext sc = SSLContext.getInstance("SSL");
                sc.init(null, trustAllCerts, new java.security.SecureRandom());
                HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public static void resetDefaultTrustManager() {
        Log.d(TAG, "resetDefaultTrustManager");
        try {
            SSLContext sc = SSLContext.getInstance("SSL");
            sc.init(null, null, new java.security.SecureRandom());
            HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void initiateDownload() {
        initDefaultTrustManager(this.verifySslCerts);
        // Spin up a thread to grab the file over the network.
        Thread t = new Thread () {
            @Override
            public void run () {
                try {
                    URL url = new URL (HttpsFileDownloader.this.url);
                    URLConnection ucon = url.openConnection();

                    StringBuilder textBuilder = new StringBuilder();
                    try (Reader reader = new BufferedReader(new InputStreamReader
                            (ucon.getInputStream(), Charset.forName("UTF-8")))) {
                        int c = 0;
                        while ((c = reader.read()) != -1) {
                            textBuilder.append((char) c);
                        }
                    }
                    resetDefaultTrustManager();
                    listener.onDownload(textBuilder.toString());
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        };
        t.start();
    }
}
