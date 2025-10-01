package com.undatech.opaque.proxmox;

import android.annotation.SuppressLint;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.util.Base64;
import android.util.Log;

import androidx.annotation.NonNull;

import com.undatech.opaque.Connection;
import com.undatech.opaque.RemoteClientLibConstants;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Locale;

import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

// Original examples (adapter to use HttpURLConnection) taken from:
// http://lukencode.com/2010/04/27/calling-web-services-in-android-using-httpclient\
public class RestClient {
    public static final String TAG = "RestClient";
    private static final int TIMEOUT_MS = 60 * 1000; // 60 seconds
    protected ArrayList<Parameter> params;
    private ArrayList<Parameter> headers;
    private int responseCode;
    private String message;
    private String response;
    protected Connection connection;
    protected final Handler handler;
    private String url;
    private final String host;
    private final Uri uri;
    private final int port;

    public RestClient(Connection connection, Handler handler, int defaultPort) {
        this.connection = connection;
        this.handler = handler;
        this.uri = Uri.parse(getUriToParse());
        this.port = getApiPort(defaultPort);
        this.host = uri.getHost();
    }

    private int getApiPort(int defaultPort) {
        int port = uri.getPort();
        if (port < 0) {
            port = defaultPort;
        }
        return port;
    }

    @NonNull
    public String getApiUrl() {
        return String.format(Locale.US, "%s://%s:%d", uri.getScheme(), host, port);
    }

    @NonNull
    private String getUriToParse() {
        String uriToParse = connection.getHostname();
        if (!uriToParse.startsWith("http://") && !uriToParse.startsWith("https://")) {
            uriToParse = String.format("%s%s", "https://", uriToParse);
        }
        return uriToParse;
    }

    public String getHost() {
        return host;
    }

    public int getPort() {
        return port;
    }

    private static String convertStreamToString(InputStream is) {
        BufferedReader reader = new BufferedReader(new InputStreamReader(is));
        StringBuilder sb = new StringBuilder();
        String line;
        try {
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
        } catch (IOException e) {
            logErrorStacktrace(e);
        } finally {
            try {
                is.close();
            } catch (IOException e) {
                logErrorStacktrace(e);
            }
        }
        return sb.toString();
    }

    private static void logErrorStacktrace(IOException e) {
        Log.e(TAG, Log.getStackTraceString(e));
    }

    public String getErrorMessage() {
        return message;
    }

    public String getResponse() {
        return response;
    }

    public int getResponseCode() {
        return responseCode;
    }

    public void resetState(String url) {
        this.url = url;
        setupSSLConfiguration();
        params = new ArrayList<>();
        headers = new ArrayList<>();
    }

    public void addHeader(String name, String value) {
        headers.add(new Parameter(name, value));
    }

    public void addParam(String name, String value) {
        params.add(new Parameter(name, value));
    }

    public void execute(RequestMethod method) throws IOException {
        switch (method) {
            case GET:
                executeGet();
                break;
            case POST:
                executePost();
                break;
        }
    }

    private void executeGet() throws IOException {
        StringBuilder queryString = new StringBuilder();
        if (!params.isEmpty()) {
            queryString.append("?");
            for (int i = 0; i < params.size(); i++) {
                Parameter param = params.get(i);
                if (i > 0) {
                    queryString.append("&");
                }
                queryString.append(param.getName())
                        .append("=")
                        .append(URLEncoder.encode(param.getValue(), "UTF-8"));
            }
        }

        URL requestUrl = new URL(url + queryString);
        HttpURLConnection connection = createConnection(requestUrl);
        connection.setRequestMethod("GET");
        addHeaders(connection);
        executeRequest(connection);
    }

    private void executePost() throws IOException {
        URL requestUrl = new URL(url);
        HttpURLConnection connection = createConnection(requestUrl);
        connection.setRequestMethod("POST");
        connection.setDoOutput(true);
        addHeaders(connection);

        if (!params.isEmpty()) {
            connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
            String postData = buildPostData();
            try (OutputStream os = connection.getOutputStream()) {
                os.write(postData.getBytes(StandardCharsets.UTF_8));
                os.flush();
            }
        }

        executeRequest(connection);
    }

    private HttpURLConnection createConnection(URL url) throws IOException {
        HttpURLConnection connection;
        if (url.getProtocol().equals("https")) {
            connection = (HttpsURLConnection) url.openConnection();
        } else {
            connection = (HttpURLConnection) url.openConnection();
        }

        connection.setConnectTimeout(TIMEOUT_MS);
        connection.setReadTimeout(TIMEOUT_MS);
        return connection;
    }

    private void addHeaders(HttpURLConnection connection) {
        for (Parameter header : headers) {
            connection.setRequestProperty(header.getName(), header.getValue());
        }
    }

    private String buildPostData() throws IOException {
        StringBuilder postData = new StringBuilder();
        for (int i = 0; i < params.size(); i++) {
            Parameter param = params.get(i);
            if (i > 0) {
                postData.append("&");
            }
            postData.append(param.getName())
                    .append("=")
                    .append(URLEncoder.encode(param.getValue(), "UTF-8"));
        }
        return postData.toString();
    }

    private void executeRequest(HttpURLConnection connection) throws IOException {
        try {
            responseCode = connection.getResponseCode();
            message = connection.getResponseMessage();

            InputStream inputStream;
            if (responseCode >= 200 && responseCode < 300) {
                inputStream = connection.getInputStream();
            } else {
                inputStream = connection.getErrorStream();
            }

            if (inputStream != null) {
                response = convertStreamToString(inputStream);
            }
        } finally {
            connection.disconnect();
        }
    }

    private void setupSSLConfiguration() {
        try {
            String certString = connection.getX509KeySignature().trim();
            CustomTrustManager trustManager = new CustomTrustManager(certString, handler);

            SSLContext sslContext = SSLContext.getInstance("TLS");
            sslContext.init(null, new TrustManager[]{trustManager}, null);

            HttpsURLConnection.setDefaultSSLSocketFactory(sslContext.getSocketFactory());
            HttpsURLConnection.setDefaultHostnameVerifier((hostname, session) -> {
                // Accept all hostnames for custom certificate handling
                return true;
            });
        } catch (Exception e) {
            Log.e(TAG, "Failed to setup SSL configuration", e);
        }
    }

    public enum RequestMethod {
        GET,
        POST
    }

    public static class Parameter {
        private final String name;
        private final String value;

        public Parameter(String name, String value) {
            this.name = name;
            this.value = value;
        }

        public String getName() {
            return name;
        }

        public String getValue() {
            return value;
        }
    }

    @SuppressLint("CustomX509TrustManager")
    private class CustomTrustManager implements X509TrustManager {
        private Certificate storedCert = null;
        private final Handler handler;

        public CustomTrustManager(String certString, Handler handler) {
            this.handler = handler;
            if (certString != null && !certString.isEmpty()) {
                try {
                    ByteArrayInputStream in = new ByteArrayInputStream(Base64.decode(certString, Base64.DEFAULT));
                    CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
                    storedCert = certFactory.generateCertificate(in);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to parse stored certificate", e);
                }
            }
        }

        @Override
        public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException {
            // Client certificates are not used in this implementation
            Log.e(TAG, "checkClientTrusted called unexpectedly");
            throw new CertificateException();
        }

        @Override
        public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {
            if (storedCert == null || !storedCert.equals(chain[0])) {
                synchronized (handler) {
                    Log.d(TAG, "Sending a message containing the certificate to our handler.");
                    Message m = new Message();
                    m.setTarget(handler);
                    m.what = RemoteClientLibConstants.DIALOG_X509_CERT;
                    m.obj = chain[0];
                    handler.sendMessage(m);
                    connection.setX509KeySignature("");
                    connection.setOvirtCaData("");
                    Log.d(TAG, "Blocking indefinitely until the x509 cert is accepted.");
                    while (connection.getX509KeySignature().isEmpty() ||
                            connection.getOvirtCaData().isEmpty()) {
                        try {
                            handler.wait();
                        } catch (InterruptedException e) {
                            Log.e(TAG, "The x509 cert was not accepted.", e);
                            throw new CertificateException("The x509 cert was not accepted.");
                        }
                    }
                    Log.d(TAG, "The x509 cert was accepted and X509KeySignature and OvirtCaData set.");
                }
            }
        }

        @Override
        public X509Certificate[] getAcceptedIssuers() {
            return new X509Certificate[0];
        }
    }
}
