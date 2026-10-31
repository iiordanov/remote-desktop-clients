package com.undatech.opaque.proxmox.pojo;

import android.util.Log;

import com.iiordanov.bVNC.HostResolution;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class SpiceDisplay {
    public static final String TAG = "SpiceDisplay";

    private String password;
    private final int tlsPort;
    private String host;
    private String title;
    private String ca;
    private final String hostSubject;
    private final String proxy;
    private final int deleteThisFile;
    private final String secureAttention;
    private String type;
    private final String toggleFullscreen;
    private final String releaseCursor;
    private final String additionalCertificateAuthorities;
    private final String casCombined;

    public SpiceDisplay(JSONObject data, String additionalCertificateAuthorities) throws JSONException {
        password = data.getString("password");
        tlsPort = data.getInt("tls-port");
        host = data.getString("host");
        title = data.getString("title");
        ca = data.getString("ca");
        hostSubject = data.getString("host-subject");
        proxy = data.getString("proxy");
        deleteThisFile = data.getInt("delete-this-file");
        secureAttention = data.getString("secure-attention");
        type = data.getString("type");
        toggleFullscreen = data.getString("toggle-fullscreen");
        releaseCursor = data.getString("release-cursor");
        this.additionalCertificateAuthorities = additionalCertificateAuthorities;
        this.casCombined = ca + "\\n" + additionalCertificateAuthorities;
        logCasLengths();
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getHost() {
        return host;
    }

    public void setHost(String host) {
        this.host = host;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public String getCa() {
        return ca;
    }

    public void setCa(String ca) {
        this.ca = ca;
    }

    public String getType() {
        return type;
    }

    public void setType(String type) {
        this.type = type;
    }

    /**
     * Writes the virt-viewer connection file for this display.
     *
     * @param tempVvFile destination path for the generated file
     * @param apiHost    PVE API host used to work out the effective proxy
     * @throws IOException if the file cannot be written
     */
    public void outputToFile(String tempVvFile, String apiHost) throws IOException {
        String proxyValue = HostResolution.resolveProxy(proxy, apiHost);
        File file = new File(tempVvFile);
        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write("[virt-viewer]\n".getBytes(StandardCharsets.UTF_8));
            writeLine(fos, "tls-port", tlsPort);
            writeLine(fos, "ca", casCombined);
            writeLine(fos, "host", host);
            writeLine(fos, "host-subject", hostSubject);
            writeLine(fos, "password", password);
            writeLine(fos, "proxy", proxyValue);
            writeLine(fos, "title", title);
            writeLine(fos, "delete-this-file", deleteThisFile);
            writeLine(fos, "release-cursor", releaseCursor);
            writeLine(fos, "secure-attention", secureAttention);
            writeLine(fos, "toggle-fullscreen", toggleFullscreen);
            writeLine(fos, "type", type);
        }
    }

    private static void writeLine(FileOutputStream fos, String key, Object value) throws IOException {
        fos.write((key + "=" + value + "\n").getBytes(StandardCharsets.UTF_8));
    }

    private void logCasLengths() {
        Log.i(TAG, String.format("CA length: %d, additional CAs length: %d, combined CAs length: %d", ca.length(), additionalCertificateAuthorities.length(), casCombined.length()));
    }
}
