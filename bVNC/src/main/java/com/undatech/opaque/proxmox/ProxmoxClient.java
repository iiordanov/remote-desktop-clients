package com.undatech.opaque.proxmox;

import android.os.Handler;
import android.util.Log;

import com.undatech.opaque.Connection;
import com.undatech.opaque.proxmox.pojo.PveRealm;
import com.undatech.opaque.proxmox.pojo.PveResource;
import com.undatech.opaque.proxmox.pojo.SpiceDisplay;
import com.undatech.opaque.proxmox.pojo.VmStatus;
import com.undatech.opaque.proxmox.pojo.VncDisplay;

import org.apache.http.HttpException;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.util.HashMap;
import java.util.Map;

import javax.security.auth.login.LoginException;

public class ProxmoxClient extends RestClient {
    private static final int DEFAULT_PROXMOX_PORT = 8006;
    private final String baseUrl;
    private String ticket;
    private boolean perUserNeedTfa;
    private String csrfToken;
    private final String additionalCertificateAuthorities;

    /**
     * Initializes a connection to PVE's API.
     */
    public ProxmoxClient(Connection connection, Handler handler, String additionalCertificateAuthorities) {
        super(connection, handler, DEFAULT_PROXMOX_PORT);
        this.baseUrl = String.format("%s%s", getApiUrl(), "/api2/json");
        this.additionalCertificateAuthorities = additionalCertificateAuthorities;
    }

    public HashMap<String, PveRealm> getAvailableRealms()
            throws JSONException, IOException, HttpException {
        resetState(baseUrl + "/access/domains");
        execute(RestClient.RequestMethod.GET);

        HashMap<String, PveRealm> result;
        if (getResponseCode() == HttpURLConnection.HTTP_OK) {
            JSONArray array = new JSONObject(getResponse()).getJSONArray("data");
            result = PveRealm.getRealmsFromJsonArray(array);
        } else {
            throw new HttpException(getErrorMessage());
        }

        return result;
    }

    public void login(String user, String realm, String password, String otp)
            throws JSONException, IOException, HttpException, LoginException {
        resetState(baseUrl + "/access/ticket");

        if (!perUserNeedTfa && otpObtained(otp)) {
            addRegularAuthParams(user, password, realm);
            addParam("otp", otp);
        } else if (perUserNeedTfa && otpObtained(otp)) {
            String otpUsername = user + "@" + realm;
            String otpPassword = "totp:" + otp;
            addUserAndPassword(otpUsername, otpPassword);
            addParam("tfa-challenge", ticket);
        } else {
            addRegularAuthParams(user, password, realm);
        }

        execute(RestClient.RequestMethod.POST);

        if (getResponseCode() == HttpURLConnection.HTTP_OK) {
            JSONObject data = new JSONObject(getResponse()).getJSONObject("data");
            setPerUserNeedTfa(getNeedTfaIfAvailable(data));
            ticket = data.getString("ticket");
            csrfToken = data.getString("CSRFPreventionToken");
        } else if (getResponseCode() == HttpURLConnection.HTTP_UNAUTHORIZED) {
            throw new LoginException(getErrorMessage());
        } else {
            throw new HttpException(getErrorMessage());
        }
    }

    private void addRegularAuthParams(String user, String password, String realm) {
        addUserAndPassword(user, password);
        addParam("realm", realm);
    }

    private void addUserAndPassword(String user, String password) {
        addParam("username", user);
        addParam("password", password);
    }

    private static boolean otpObtained(String otp) {
        return otp != null && !otp.isEmpty();
    }

    private boolean getNeedTfaIfAvailable(JSONObject data) {
        boolean res = false;
        try {
            res = "1".equals(data.getString("NeedTFA"));
        } catch (JSONException e) {
            Log.d(TAG, "getNeedTfaIfAvailable: could not find NeedTFA, no TFA for user");
        }
        return res;
    }

    /**
     * Actually performs a request to PVE.
     *
     * @param resource    the REST resource to affect
     * @param method      the method (GET, POST, etc)
     * @param requestData the data to send
     * @return the data returned by PVE as a result of the request.
     * @noinspection SameParameterValue
     */
    private JSONObject request(String resource, RestClient.RequestMethod method, Map<String, String> requestData)
            throws IOException, JSONException, LoginException, HttpException {

        resetState(baseUrl + resource);

        addHeader("Cookie", "PVEAuthCookie=" + ticket);

        if (!method.equals(RestClient.RequestMethod.GET)) {
            addHeader("CSRFPreventionToken", csrfToken);
        }

        if (requestData != null)
            for (Map.Entry<String, String> p : requestData.entrySet()) {
                addParam(p.getKey(), p.getValue());
            }

        execute(method);

        if (isSuccessfulCode(getResponseCode())) {
            return new JSONObject(getResponse());
        } else if (getResponseCode() == HttpURLConnection.HTTP_UNAUTHORIZED) {
            throw new LoginException(getErrorMessage());
        } else {
            // The API returned an unexpected 4xx or 5xx return code.
            throw new HttpException(getErrorMessage());
        }
    }

    /**
     * Checks if an HTTP code is successful (200 - 299) or not
     */
    boolean isSuccessfulCode(int code) {
        return (code / 100 == 2);
    }

    /**
     * Retrieves a representation of the VNC display of a node.
     *
     * @param node the name of the PVE node
     * @return the VNC display of the node
     */
    public VncDisplay vncNode(String node) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/vncshell", RestClient.RequestMethod.POST, null);
        return new VncDisplay(jObj.getJSONObject("data"));
    }

    /**
     * Retrieves a representation of the SPICE display of a node.
     *
     * @param node the name of the PVE node
     * @return the SPICE display of the node
     */
    public SpiceDisplay spiceNode(String node) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/spiceshell", RestClient.RequestMethod.POST, null);
        return new SpiceDisplay(jObj.getJSONObject("data"), additionalCertificateAuthorities);
    }

    /**
     * Retrieves a representation of the VNC display of a VM.
     *
     * @param node the name of the PVE node
     * @param type of VM, one of qemu, lxc, or openvz (deprecated)
     * @param vmid the numeric VM ID
     * @return The VNC display of the VM
     */
    public VncDisplay vncVm(String node, String type, int vmid) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/" + type + "/" + vmid + "/vncproxy", RestClient.RequestMethod.POST, null);
        return new VncDisplay(jObj.getJSONObject("data"));
    }

    /**
     * Retrieves a representation of the SPICE display of a VM.
     *
     * @param node the name of the PVE node
     * @param type of VM, one of qemu, lxc, or openvz (deprecated)
     * @param vmid the numeric VM ID
     * @return The SPICE display of the VM
     */
    public SpiceDisplay spiceVm(String node, String type, int vmid) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/" + type + "/" + vmid + "/spiceproxy", RestClient.RequestMethod.POST, null);
        return new SpiceDisplay(jObj.getJSONObject("data"), additionalCertificateAuthorities);
    }

    /**
     * Starts a VM.
     *
     * @param node the name of the PVE node
     * @param type of VM, one of qemu, lxc, or openvz (deprecated)
     * @param vmid the numeric VM ID
     * @return status string
     */
    public String startVm(String node, String type, int vmid) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/" + type + "/" + vmid + "/status/start", RestClient.RequestMethod.POST, null);
        return jObj.getString("data");
    }

    /**
     * Gets the current status of a VM.
     *
     * @param node the name of the PVE node
     * @param type of VM, one of qemu, lxc, or openvz (deprecated)
     * @param vmid the numeric VM ID
     * @return status of VM
     */
    public VmStatus getCurrentStatus(String node, String type, int vmid) throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/nodes/" + node + "/" + type + "/" + vmid + "/status/current", RestClient.RequestMethod.GET, null);
        return new VmStatus(jObj.getJSONObject("data"));
    }

    /**
     * Shows what resources are currently available on the PVE cluster
     *
     * @return object representing resources available
     */
    public Map<String, PveResource> getResources() throws LoginException, JSONException, IOException, HttpException {
        JSONObject jObj = request("/cluster/resources", RestClient.RequestMethod.GET, null);
        JSONArray jArr = jObj.getJSONArray("data");
        HashMap<String, PveResource> result = new HashMap<>();
        for (int i = 0; i < jArr.length(); i++) {
            PveResource r = new PveResource(jArr.getJSONObject(i));
            if (r.getName() != null && r.getNode() != null && r.getType() != null && r.getVmid() != null) {
                result.put(r.getVmid(), r);
            }
        }
        return result;
    }

    public boolean isPerUserNeedTfa() {
        return perUserNeedTfa;
    }

    public void setPerUserNeedTfa(boolean needPerUserTfa) {
        this.perUserNeedTfa = needPerUserTfa;
    }
}
