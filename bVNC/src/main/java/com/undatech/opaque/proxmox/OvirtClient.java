package com.undatech.opaque.proxmox;

import android.os.Handler;
import android.util.Log;

import com.undatech.opaque.Connection;

import org.apache.http.HttpException;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.net.HttpURLConnection;

import javax.security.auth.login.LoginException;

public class OvirtClient extends RestClient {
    private static final String TAG = "OvirtClient";
    private String baseUrl;
    private String accessToken;
    private String tokenType;

    /**
     * Initializes a connection to oVirt's API.
     *
     * @param address the address of oVirt
     */
    public OvirtClient(String address, Connection connection, Handler handler) {
        super(connection, handler);
        this.baseUrl = String.format("%s%s", address, "/ovirt-engine");
    }

    public void login(String user, String password)
            throws JSONException, IOException, HttpException, LoginException {
        String url = String.format("%s/sso/oauth/token?grant_type=password&username=%s&password=%s&scope=ovirt-app-api", baseUrl, user, password);
        resetState(url);
        addHeader("Accept", "application/json");
        execute(RequestMethod.GET);
        if (getResponseCode() == HttpURLConnection.HTTP_OK) {
            JSONObject data = new JSONObject(getResponse());
            accessToken = data.getString("access_token");
            tokenType = data.getString("token_type");
        } else if (getResponseCode() == HttpURLConnection.HTTP_UNAUTHORIZED) {
            throw new LoginException(getErrorMessage());
        } else {
            throw new HttpException(getErrorMessage());
        }
    }

    public void tryLogin(String user, String password) {
        try {
            login(user, password);
        } catch (JSONException | IOException | HttpException | LoginException e) {
            Log.d(TAG, "Ignoring exception during login: " + e);
        }
    }

    public String getAccessToken() {
        return accessToken;
    }

    public String getTokenType() {
        return tokenType;
    }
}
