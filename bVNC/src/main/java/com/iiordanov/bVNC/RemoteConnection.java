/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * Copyright (C) 2004 Horizon Wimba.  All Rights Reserved.
 * Copyright (C) 2001-2003 HorizonLive.com, Inc.  All Rights Reserved.
 * Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 * Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC;

import android.app.ProgressDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import com.iiordanov.bVNC.exceptions.AnonCipherUnsupportedException;
import com.iiordanov.bVNC.input.KeyInputHandler;
import com.iiordanov.bVNC.input.RemoteCanvasHandler;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RemoteRdpKeyboard;
import com.iiordanov.bVNC.input.RemoteRdpPointer;
import com.iiordanov.bVNC.input.RemoteSpiceKeyboard;
import com.iiordanov.bVNC.input.RemoteSpicePointer;
import com.iiordanov.bVNC.input.RemoteVncKeyboard;
import com.iiordanov.bVNC.input.RemoteVncPointer;
import com.tigervnc.rfb.AuthFailureException;
import com.undatech.opaque.Connection;
import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RdpCommunicator;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.SpiceCommunicator;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.proxmox.OvirtClient;
import com.undatech.opaque.proxmox.ProxmoxClient;
import com.undatech.opaque.proxmox.pojo.PveRealm;
import com.undatech.opaque.proxmox.pojo.PveResource;
import com.undatech.opaque.proxmox.pojo.SpiceDisplay;
import com.undatech.opaque.proxmox.pojo.VmStatus;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.remoteClientUi.R;

import org.apache.http.HttpException;
import org.json.JSONException;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Timer;

import javax.security.auth.login.LoginException;

public class RemoteConnection implements KeyInputHandler, InputCarriable {

    private final static String TAG = "RemoteCanvas";

    // Connection parameters
    public Connection connection;
    public SSHConnection sshConnection = null;
    // VNC protocol connection
    public RfbConnectable rfbConn = null;
    public RfbProto rfb = null;
    public SpiceCommunicator spiceComm = null;
    public boolean maintainConnection = true;
    // Progress dialog shown at connection time.
    public ProgressDialog pd;
    public boolean serverJustCutText = false;
    public Runnable hideKeyboardAndExtraKeys;
    public boolean spiceUpdateReceived = false;
    /**
     * Handler for the dialogs that display the x509/RDP/SSH key signatures to the user.
     * Also shows the dialogs which show various connection failures.
     */
    public Handler handler;
    Map<String, String> vmNameToId = new HashMap<>();
    // RFB Decoder
    Decoder decoder = null;
    // The remote pointer and keyboard
    RemotePointer pointer;
    RemoteKeyboard keyboard;
    // Used to set the contents of the clipboard.
    ClipboardManager clipboard;
    Timer clipboardMonitorTimer;
    ClipboardMonitor clipboardMonitor;

    /*
     * This flag indicates whether this is the VNC client.
     */
    boolean isVnc;

    /*
     * This flag indicates whether this is the RDP client.
     */
    boolean isRdp;

    /*
     * This flag indicates whether this is the SPICE client.
     */
    boolean isSpice;

    /*
     * This flag indicates whether this is the Opaque client.
     */
    boolean isOpaque;
    boolean sshTunneled = false;
    String vvFileName;
    Context context;
    Viewable canvas;

    private RdpCommunicator rdpComm = null;

    /**
     * Constructor used by the inflation apparatus
     */
    public RemoteConnection(final Context context, Viewable canvas) {
        this.context = context;
        this.canvas = canvas;
        clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        isVnc = Utils.isVnc(context);
        isRdp = Utils.isRdp(context);
        isSpice = Utils.isSpice(context);
        isOpaque = Utils.isOpaque(context);

        // Startup the connection thread with a progress dialog
        pd = ProgressDialog.show(context, context.getString(R.string.info_progress_dialog_connecting),
                context.getString(R.string.info_progress_dialog_establishing),
                true, true, dialog -> {
                    closeConnection();
                    handler.post(() -> Utils.showFatalErrorMessage(context, context.getString(R.string.info_progress_dialog_aborted)));
                });

        // Make this dialog cancellable only upon hitting the Back button and not touching outside.
        pd.setCanceledOnTouchOutside(false);
        pd.setCancelable(false);
    }

    void init(final Connection settings, final Handler handler, final Runnable hideKeyboardAndExtraKeys, final String vvFileName) {
        this.connection = settings;
        this.handler = handler;
        this.hideKeyboardAndExtraKeys = hideKeyboardAndExtraKeys;
        this.vvFileName = vvFileName;
        checkNetworkConnectivity();
        spiceComm = new SpiceCommunicator(context, handler, canvas,
                settings.isRequestingNewDisplayResolution() || settings.getRdpResType() == Constants.RDP_GEOM_SELECT_CUSTOM,
                !Utils.isFree(context) && settings.isUsbEnabled(), App.debugLog);
        rfbConn = spiceComm;
        pointer = new RemoteSpicePointer(spiceComm, context, this, canvas, handler, App.debugLog);
        try {
            keyboard = new RemoteSpiceKeyboard(
                    context.getResources(), spiceComm, canvas, this,
                    handler, settings.getLayoutMap(), App.debugLog);
        } catch (Throwable e) {
            handleUncaughtException(e);
        }
        maintainConnection = true;
        if (vvFileName == null) {
            if (connection.getConnectionTypeString().equals(context.getResources().getString(R.string.connection_type_pve))) {
                startPve();
            } else {
                connection.setAddress(Utils.getHostFromUriString(connection.getAddress()));
                startOvirt();
            }
        } else {
            startFromVvFile(vvFileName);
        }
        initializeClipboardMonitor();
    }

    /**
     * Checks whether the device has networking and quits with an error if it doesn't.
     */
    private void checkNetworkConnectivity() {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
        if (activeNetwork == null || !activeNetwork.isAvailable() || !activeNetwork.isConnectedOrConnecting()) {
            disconnectAndShowMessage(R.string.error_not_connected_to_network, R.string.error_dialog_title);
        }
    }

    public void disconnectAndShowMessage(final int messageId, final int titleId) {
        closeConnection();
        handler.post(() -> MessageDialogs.displayMessageAndFinish(context, messageId, titleId));
    }

    public void disconnectAndShowMessage(final int messageId, final int titleId, final String textToAppend) {
        closeConnection();
        handler.post(() -> MessageDialogs.displayMessageAndFinish(context, messageId, titleId, textToAppend));
    }

    /**
     * Initializes the clipboard monitor.
     */
    @SuppressWarnings("ConstantConditions")
    private void initializeClipboardMonitor() {
        clipboardMonitor = new ClipboardMonitor(context, rfbConn);
        if (clipboardMonitor != null) {
            clipboardMonitorTimer = new Timer();
            try {
                clipboardMonitorTimer.schedule(clipboardMonitor, 0, 500);
            } catch (NullPointerException e) {
                e.printStackTrace();
                Log.d(TAG, "Ignored NullPointerException while initializing clipboard monitor");
            }
        }
    }

    /**
     * Reinitialize Canvas
     */
    public void reinitializeConnection() {
        Log.i(TAG, "Reinitializing remote canvas");
        initializeConnection(this.connection, this.hideKeyboardAndExtraKeys);
        handler.post(this.hideKeyboardAndExtraKeys);
    }

    /**
     * Reinitialize Opaque
     */
    public void reinitializeOpaque() {
        Log.i(TAG, "Reinitializing remote canvas opaque");
        init(this.connection, this.handler, this.hideKeyboardAndExtraKeys, this.vvFileName);
        handler.post(this.hideKeyboardAndExtraKeys);
    }

    /**
     * Create a view showing a remote desktop connection
     *
     * @param conn     Connection settings
     */
    public void initializeConnection(Connection conn, final Runnable hideKeyboardAndExtraKeys) {
        maintainConnection = true;
        this.hideKeyboardAndExtraKeys = hideKeyboardAndExtraKeys;
        connection = conn;
        sshTunneled = (connection.getConnectionType() == Constants.CONN_TYPE_SSH);
        handler = new RemoteCanvasHandler(context, (RemoteCanvas) canvas, this, connection);

        try {
            if (isSpice) {
                initializeSpiceConnection();
            } else if (isRdp) {
                initializeRdpConnection();
            } else {
                initializeVncConnection();
            }
        } catch (Throwable e) {
            handleUncaughtException(e);
        }
        initializeClipboardMonitor();

        Thread t = new Thread() {
            public void run() {
                try {
                    initSshTunnelIfNeeded();
                    if (isSpice) {
                        startSpiceConnection();
                    } else if (isRdp) {
                        startRdpConnection();
                    } else {
                        startVncConnection();
                    }
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        t.start();
    }

    private void initSshTunnelIfNeeded() {
        if (sshTunneled && sshConnection == null) {
            String targetAddress = getSshTunnelTargetAddress();
            sshConnection = new SSHConnection(targetAddress, connection, context, handler);
        }
    }

    private String getSshTunnelTargetAddress() {
        String address = connection.getAddress();
        if (isRdp && connection.getRdpGatewayEnabled()) {
            address = connection.getRdpGatewayHostname();
        }
        return address;
    }

    private void handleUncaughtException(Throwable e) {
        if (maintainConnection) {
            Log.e(TAG, e.toString());
            e.printStackTrace();
            // Ensure we dismiss the progress dialog before we finish
            if (pd.isShowing())
                pd.dismiss();

            if (e instanceof OutOfMemoryError) {
                canvas.disposeDrawable();
                showFatalMessageAndQuit(context.getString(R.string.error_out_of_memory));
            } else {
                String error = context.getString(R.string.error_connection_failed);
                String message = e.getMessage();
                if (message != null) {
                    if (!message.contains("SSH") &&
                            (message.contains("authentication") ||
                                    message.contains("Unknown security result") ||
                                    message.contains("password check failed"))
                    ) {
                        error = context.getString(R.string.error_vnc_authentication);
                    }
                    error = error + "<br>" + e.getLocalizedMessage();
                }
                showFatalMessageAndQuit(error);
            }
        }
    }

    /**
     * Initializes a SPICE connection.
     */
    private void initializeSpiceConnection() throws Exception {
        spiceComm = new SpiceCommunicator(context, handler, canvas, true,
                !Utils.isFree(context) && connection.isUsbEnabled(),
                App.debugLog);
        rfbConn = spiceComm;
        pointer = new RemoteSpicePointer(spiceComm, context, this, canvas, handler, App.debugLog);
        keyboard = new RemoteSpiceKeyboard(
                context.getResources(), spiceComm, canvas, this,
                handler, connection.getLayoutMap(), App.debugLog);
        spiceComm.setHandler(handler);
    }

    /**
     * Starts a SPICE connection using libspice.
     */
    private void startSpiceConnection() throws Exception {
        // Get the address and port (based on whether an SSH tunnel is being established or not).
        String address = getAddress();
        // To prevent an SSH tunnel being created when port or TLS port is not set, we only
        // getPort when port/tlsPort are positive.
        int port = connection.getPort();
        if (port > 0) {
            port = getRemoteProtocolPort(port);
        }

        int tlsPort = connection.getTlsPort();
        if (tlsPort > 0) {
            tlsPort = getRemoteProtocolPort(tlsPort);
        }

        spiceComm.connectSpice(address, Integer.toString(port), Integer.toString(tlsPort), connection.getPassword(),
                connection.getCaCertPath(), null, // TODO: Can send connection.getCaCert() here instead
                connection.getCertSubject(), connection.getEnableSound());
    }

    /**
     * Initializes an RDP connection.
     */
    private void initializeRdpConnection() {
        Log.i(TAG, "initializeRdpConnection: Initializing RDP connection.");

        rdpComm = new RdpCommunicator(context, handler, canvas,
                connection.getUserName(), connection.getRdpDomain(), connection.getPassword(),
                App.debugLog);
        rfbConn = rdpComm;
        pointer = new RemoteRdpPointer(rfbConn, context, this, canvas, handler, App.debugLog);
        keyboard = new RemoteRdpKeyboard(rdpComm, canvas, this, handler, App.debugLog,
                connection.getPreferSendingUnicode());
    }

    /**
     * Starts an RDP connection using the FreeRDP library.
     */
    private void startRdpConnection() throws Exception {
        Log.i(TAG, "startRdpConnection: Starting RDP connection.");

        // Get the address and port (based on whether an SSH tunnel is being established or not).
        String address = getAddress();
        String gatewayAddress = getGatewayAddress();
        int rdpPort = getRemoteProtocolPort(connection.getPort());
        int gatewayPort = getGatewayPort(connection.getRdpGatewayPort());
        canvas.waitUntilInflated();
        int remoteWidth = canvas.getRemoteWidth(canvas.getWidth(), canvas.getHeight());
        int remoteHeight = canvas.getRemoteHeight(canvas.getWidth(), canvas.getHeight());

        rdpComm.setConnectionParameters(
                address, rdpPort,
                connection.getRdpGatewayEnabled(), gatewayAddress, gatewayPort,
                connection.getRdpGatewayUsername(), connection.getRdpGatewayDomain(), connection.getRdpGatewayPassword(),
                connection.getNickname(), remoteWidth,
                remoteHeight, connection.getDesktopBackground(), connection.getFontSmoothing(),
                connection.getDesktopComposition(), connection.getWindowContents(),
                connection.getMenuAnimation(), connection.getVisualStyles(),
                connection.getRedirectSdCard(), connection.getConsoleMode(),
                connection.getRemoteSoundType(), connection.getEnableRecording(),
                connection.getRemoteFx(), connection.getEnableGfx(), connection.getEnableGfxH264(),
                connection.getRdpColor());
        rdpComm.connect();
        pd.dismiss();
    }

    /**
     * Initializes a VNC connection.
     */
    private void initializeVncConnection() {
        Log.i(TAG, "Initializing connection to: " + connection.getAddress() + ", port: " + connection.getPort());
        boolean sslTunneled = connection.getConnectionType() == Constants.CONN_TYPE_STUNNEL;
        decoder = new Decoder(canvas, this, connection.getUseLocalCursor() == Constants.CURSOR_FORCE_LOCAL);
        rfb = new RfbProto(decoder, canvas, this, handler, connection.getPrefEncoding(), connection.getViewOnly(),
                sslTunneled, connection.getIdHashAlgorithm(), connection.getIdHash(), connection.getX509KeySignature(),
                App.debugLog);

        rfbConn = rfb;
        pointer = new RemoteVncPointer(rfbConn, context, this, canvas, handler, App.debugLog);
        boolean rAltAsIsoL3Shift = Utils.querySharedPreferenceBoolean(this.context,
                Constants.rAltAsIsoL3ShiftTag);
        keyboard = new RemoteVncKeyboard(rfbConn, this, context, handler,
                rAltAsIsoL3Shift, App.debugLog);
    }

    /**
     * Starts a VNC connection
     */
    private void startVncConnection() throws Exception {

        try {
            String address = getAddress();
            int vncPort = getRemoteProtocolPort(connection.getPort());
            Log.i(TAG, "Establishing VNC session to: " + address + ", port: " + vncPort);
            String sslCert = connection.getX509KeySignature();
            rfb.initializeAndAuthenticate(address, vncPort, connection.getUserName(),
                    connection.getPassword(), connection.getUseRepeater(),
                    connection.getRepeaterId(), connection.getConnectionType(),
                    sslCert);
        } catch (AnonCipherUnsupportedException e) {
            showFatalMessageAndQuit(context.getString(R.string.error_anon_dh_unsupported));
        } catch (RfbProto.RfbPasswordAuthenticationException e) {
            Log.e(TAG, "Authentication failed, will prompt user for password");
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_VNC_PASSWORD);
            return;
        } catch (RfbProto.RfbUserPassAuthFailedOrUsernameRequiredException e) {
            Log.e(TAG, "Auth failed or username required, prompting for username and password");
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_VNC_CREDENTIALS);
            return;
        } catch (AuthFailureException e) {
            Log.e(TAG, "TigerVNC AuthFailureException: " + e.getLocalizedMessage());
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_VNC_CREDENTIALS);
            return;
        } catch (Exception e) {
            e.printStackTrace();
            throw new Exception(context.getString(R.string.error_vnc_unable_to_connect) +
                    Utils.messageAndStackTraceAsString(e));
        }

        rfb.writeClientInit();
        rfb.readServerInit();

        // Is custom resolution enabled?
        if (connection.getRdpResType() != Constants.VNC_GEOM_SELECT_DISABLED) {
            canvas.waitUntilInflated();
            rfb.setPreferredFramebufferSize(getVncRemoteWidth(canvas.getWidth(), canvas.getHeight()),
                    getVncRemoteHeight(canvas.getWidth(), canvas.getHeight()));
        }

        canvas.reallocateDrawable(rfb.framebufferWidth, rfb.framebufferHeight);
        decoder.setPixelFormat(rfb);

        handler.post(() -> pd.setMessage(context.getString(R.string.info_progress_dialog_downloading)));

        sendUnixAuth();
        canvas.postDrawableSetter();

        // Hide progress dialog
        if (pd.isShowing())
            pd.dismiss();

        try {
            rfb.processProtocol();
        } catch (RfbProto.RfbUltraVncColorMapException e) {
            Log.e(TAG, "UltraVnc supports only 24bpp. Switching color mode and reconnecting.");
            connection.setColorModel(COLORMODEL.C24bit.nameString());
            connection.save(context);
            handler.sendEmptyMessage(RemoteClientLibConstants.REINIT_SESSION);
        }
    }

    /**
     * Initialize the canvas to show the remote desktop
     */
    void startOvirt() {
        if (!pd.isShowing())
            pd.show();

        Thread cThread = new Thread() {
            @Override
            public void run() {
                try {
                    // Obtain user's password if necessary.
                    if (connection.getPassword().equals("")) {
                        Log.i(TAG, "Displaying a dialog to obtain user's password.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_PASSWORD);
                        synchronized (handler) {
                            handler.wait();
                        }
                    }

                    OvirtClient ovirtClient = new OvirtClient(connection, handler);
                    ovirtClient.trySsoLogin(connection.getUserName(), connection.getPassword());
                    String ssoToken = ovirtClient.getAccessToken();

                    String ovirtCaFile;
                    if (connection.isUsingCustomOvirtCa()) {
                        ovirtCaFile = connection.getOvirtCaFile();
                    } else {
                        ovirtCaFile = new File(context.getFilesDir(), "ssl/certs/ca-certificates.crt").getPath();
                    }

                    // If not VM name is specified, then get a list of VMs and let the user pick one.
                    if (connection.getVmname().equals("")) {
                        int success = spiceComm.fetchOvirtVmNames(
                                connection.getHostname(),
                                connection.getUserName(),
                                connection.getPassword(),
                                ovirtCaFile,
                                connection.isSslStrict(),
                                ssoToken
                        );
                        // VM retrieval was unsuccessful we do not continue.
                        ArrayList<String> vmNames = spiceComm.getVmNames();
                        if (success != 0 || vmNames.isEmpty()) {
                            return;
                        } else {
                            // If there is just one VM, pick it and skip the dialog.
                            if (vmNames.size() == 1) {
                                connection.setVmname(vmNames.get(0));
                                connection.save(context);
                            } else {
                                while (connection.getVmname().equals("")) {
                                    Log.i(TAG, "Displaying a dialog with VMs to the user.");
                                    // Populate the data structure that is used to convert VM names to IDs.
                                    for (String s : vmNames) {
                                        vmNameToId.put(s, s);
                                    }
                                    handler.sendMessage(RemoteCanvasHandler.getMessageStringList(RemoteClientLibConstants.DIALOG_DISPLAY_VMS,
                                            "vms", vmNames));
                                    synchronized (spiceComm) {
                                        spiceComm.wait();
                                    }
                                }
                            }
                        }
                    }
                    spiceComm.setHandler(handler);
                    spiceComm.connectOvirt(
                            connection.getHostname(),
                            connection.getVmname(),
                            connection.getUserName(),
                            connection.getPassword(),
                            ovirtCaFile,
                            connection.isAudioPlaybackEnabled(),
                            connection.isSslStrict(),
                            ssoToken
                    );

                    try {
                        synchronized (spiceComm) {
                            spiceComm.wait(35000);
                        }
                    } catch (InterruptedException e) {
                        Log.w(TAG, "Wait for SPICE connection interrupted.");
                    }

                    if (!spiceUpdateReceived && maintainConnection) {
                        handler.sendEmptyMessage(RemoteClientLibConstants.OVIRT_TIMEOUT);
                    }

                } catch (LoginException e) {
                    Log.e(TAG, "Failed to login to oVirt.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.OVIRT_AUTH_FAILURE);
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }

    String retrieveVvFileFromPve(final ProxmoxClient api, final String vmId,
                                 final String node, final String virt) {
        Log.i(TAG, String.format("Trying to connect to PVE host: " + api.getHost()));
        final String tempVvFile = context.getFilesDir() + "/tempfile.vv";
        FileUtils.deleteFile(tempVvFile);

        Thread cThread = new Thread() {
            @Override
            public void run() {
                try {
                    VmStatus status = api.getCurrentStatus(node, virt, Integer.parseInt(vmId));
                    if (status.getStatus().equals(VmStatus.STOPPED)) {
                        api.startVm(node, virt, Integer.parseInt(vmId));
                        while (!status.getStatus().equals(VmStatus.RUNNING)) {
                            status = api.getCurrentStatus(node, virt, Integer.parseInt(vmId));
                            SystemClock.sleep(500);
                        }
                    }
                    SpiceDisplay spiceData = api.spiceVm(node, virt, Integer.parseInt(vmId));
                    if (spiceData != null) {
                        spiceData.outputToFile(tempVvFile, api.getHost());
                    } else {
                        Log.e(TAG, "PVE returned null data for display.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.PVE_NULL_DATA);
                    }
                } catch (LoginException e) {
                    Log.e(TAG, "Failed to login to PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_AUTHENTICATE);
                } catch (JSONException e) {
                    Log.e(TAG, "Failed to parse json from PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_PARSE_JSON);
                } catch (NumberFormatException e) {
                    Log.e(TAG, "Error converting PVE ID to integer.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_VMID_NOT_NUMERIC);
                } catch (IOException e) {
                    Log.e(TAG, "IO Error communicating with PVE API: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasHandler.getMessageString(RemoteClientLibConstants.PVE_API_IO_ERROR,
                            "error", e.getMessage()));
                    e.printStackTrace();
                } catch (HttpException e) {
                    Log.e(TAG, "PVE API returned error code: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasHandler.getMessageString(RemoteClientLibConstants.PVE_API_UNEXPECTED_CODE,
                            "error", e.getMessage()));
                }
                // At this stage we have either retrieved display data or failed, so permit the UI thread to continue.
                synchronized (tempVvFile) {
                    tempVvFile.notify();
                }
            }
        };
        cThread.start();

        // Wait until a timeout or until we are notified the worker thread trying to retrieve display data is done.
        synchronized (tempVvFile) {
            try {
                tempVvFile.wait();
            } catch (InterruptedException e) {
                handler.sendEmptyMessage(RemoteClientLibConstants.PVE_TIMEOUT_COMMUNICATING);
                e.printStackTrace();
            }
        }

        File checkFile = new File(tempVvFile);
        if (!checkFile.exists() || checkFile.length() == 0) {
            return null;
        }
        return tempVvFile;
    }

    void startFromVvFile(final String vvFileName) {
        Thread cThread = new Thread() {
            @Override
            public void run() {
                try {
                    spiceComm.startSessionFromVvFile(vvFileName, connection.isAudioPlaybackEnabled());
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }

    /**
     * Function used to initialize an empty SSH HostKey for a new VNC over SSH connection.
     */
    public String retrieveVvFileName() {
        return this.vvFileName;
    }

    void startPve() {
        if (!pd.isShowing())
            pd.show();

        Thread cThread = new Thread() {
            @Override
            public void run() {
                try {
                    // Obtain user's password if necessary.
                    if (connection.getPassword().equals("")) {
                        Log.i(TAG, "Displaying a dialog to obtain user's password.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_PASSWORD);
                        synchronized (handler) {
                            handler.wait();
                        }
                    }

                    String user = connection.getUserName();
                    String realm = RemoteClientLibConstants.PVE_DEFAULT_REALM;

                    // Try to parse realm from user entered
                    int indexOfAt = connection.getUserName().indexOf('@');
                    if (indexOfAt != -1) {
                        realm = user.substring(indexOfAt + 1);
                        user = user.substring(0, indexOfAt);
                    }

                    // Connect to the API and obtain available realms
                    ProxmoxClient api = new ProxmoxClient(connection, handler);
                    HashMap<String, PveRealm> realms = api.getAvailableRealms();

                    // If selected realm has TFA enabled, then ask for the code
                    PveRealm pveRealm = realms.get(realm);
                    if (pveRealm != null && pveRealm.getTfa() != null) {
                        Log.i(TAG, "Displaying a dialog to obtain OTP/TFA.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_OTP_CODE);
                        synchronized (handler) {
                            handler.wait();
                        }
                    }

                    // Login with provided credentials
                    api.login(user, realm, connection.getPassword(), connection.getOtpCode());

                    // Get map of user parsable names to resources
                    Map<String, PveResource> nameToResources = api.getResources();

                    if (nameToResources.isEmpty()) {
                        Log.e(TAG, "No available VMs found for user in PVE cluster");
                        disconnectAndShowMessage(R.string.error_no_vm_found_for_user, R.string.error_dialog_title);
                        return;
                    }

                    String vmId = connection.getVmname();
                    if (vmId.matches("/")) {
                        vmId = connection.getVmname().replaceAll(".*/", "");
                        connection.setVmname(vmId);
                        connection.save(context);
                    }

                    String node = "";
                    String virt = "";

                    // If there is just one VM, pick it and ignore what is saved in settings.
                    if (nameToResources.size() == 1) {
                        Log.e(TAG, "A single VM was found, so picking it.");
                        String key = (String) nameToResources.keySet().toArray()[0];
                        PveResource pveResource = nameToResources.get(key);
                        if (pveResource != null) {
                            node = pveResource.getNode();
                            virt = pveResource.getType();
                            connection.setVmname(pveResource.getVmid());
                            connection.save(context);
                        }
                    } else {
                        while (connection.getVmname().isEmpty()) {
                            Log.i(TAG, "PVE: Displaying a dialog with VMs to the user.");
                            // Populate the data structure that is used to convert VM names to IDs.
                            for (String s : nameToResources.keySet()) {
                                PveResource pveResource = nameToResources.get(s);
                                if (pveResource != null) {
                                    vmNameToId.put(pveResource.getName() + " (" + s + ")", s);
                                }
                            }
                            // Get the user parsable names and display them
                            ArrayList<String> vms = new ArrayList<>(vmNameToId.keySet());
                            handler.sendMessage(RemoteCanvasHandler.getMessageStringList(
                                    RemoteClientLibConstants.DIALOG_DISPLAY_VMS, "vms", vms));
                            synchronized (spiceComm) {
                                spiceComm.wait();
                            }
                        }

                        // At this point, either the user selected a VM or there was an ID saved.
                        if (nameToResources.get(connection.getVmname()) != null) {
                            PveResource pveResource = nameToResources.get(connection.getVmname());
                            if (pveResource != null) {
                                node = pveResource.getNode();
                                virt = pveResource.getType();
                            }
                        } else {
                            Log.e(TAG, "No VM with the following ID was found: " + connection.getVmname());
                            disconnectAndShowMessage(R.string.error_no_such_vm_found_for_user, R.string.error_dialog_title);
                            return;
                        }
                    }

                    vmId = connection.getVmname();
                    // Only if we managed to obtain a VM name we try to get a .vv file for the display.
                    if (!vmId.isEmpty()) {
                        String vvFileName = retrieveVvFileFromPve(api, vmId, node, virt);
                        if (vvFileName != null) {
                            startFromVvFile(vvFileName);
                        }
                    }
                } catch (LoginException e) {
                    Log.e(TAG, "Failed to login to PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_AUTHENTICATE);
                } catch (JSONException e) {
                    Log.e(TAG, "Failed to parse json from PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_PARSE_JSON);
                } catch (IOException e) {
                    Log.e(TAG, "IO Error communicating with PVE API: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasHandler.getMessageString(RemoteClientLibConstants.PVE_API_IO_ERROR,
                            "error", e.getMessage()));
                    e.printStackTrace();
                } catch (HttpException e) {
                    Log.e(TAG, "PVE API returned error code: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasHandler.getMessageString(RemoteClientLibConstants.PVE_API_UNEXPECTED_CODE,
                            "error", e.getMessage()));
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }

    /**
     * Sends over the unix username and password if this is VNC over SSH connection and automatic sending of
     * UNIX credentials is enabled for AutoX (for the x11vnc "-unixpw" option).
     */
    void sendUnixAuth() {
        // If the type of connection is ssh-tunneled and we are told to send the unix credentials, then do so.
        if (sshTunneled && connection.getAutoXUnixAuth()) {
            keyboard.keyEvent(KeyEvent.KEYCODE_UNKNOWN, new KeyEvent(SystemClock.uptimeMillis(),
                    connection.getSshUser(), 0, 0));
            keyboard.keyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
            keyboard.keyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

            keyboard.keyEvent(KeyEvent.KEYCODE_UNKNOWN, new KeyEvent(SystemClock.uptimeMillis(),
                    connection.getSshPassword(), 0, 0));
            keyboard.keyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
            keyboard.keyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        }
    }

    /**
     * Determines the preferred remote width for VNC connections.
     */
    private int getVncRemoteWidth(int viewWidth, int viewHeight) {
        int remoteWidth = 0;
        int reqWidth = connection.getRdpWidth();
        int reqHeight = connection.getRdpHeight();
        if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_CUSTOM &&
                reqWidth >= 2 && reqHeight >= 2) {
            remoteWidth = reqWidth;
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_AUTOMATIC) {
            remoteWidth = viewWidth;
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_NATIVE_PORTRAIT) {
            remoteWidth = Math.min(viewWidth, viewHeight);
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_NATIVE_LANDSCAPE) {
            remoteWidth = Math.max(viewWidth, viewHeight);
        }
        // We make the resolution even if it is odd.
        if (remoteWidth % 2 == 1) remoteWidth--;
        return remoteWidth;
    }

    /**
     * Determines the preferred remote height for VNC connections.
     */
    private int getVncRemoteHeight(int viewWidth, int viewHeight) {
        int remoteHeight = 0;
        int reqWidth = connection.getRdpWidth();
        int reqHeight = connection.getRdpHeight();
        if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_CUSTOM &&
                reqWidth >= 2 && reqHeight >= 2) {
            remoteHeight = reqHeight;
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_AUTOMATIC) {
            remoteHeight = viewHeight;
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_NATIVE_PORTRAIT) {
            remoteHeight = Math.max(viewWidth, viewHeight);
        } else if (connection.getRdpResType() == Constants.VNC_GEOM_SELECT_NATIVE_LANDSCAPE) {
            remoteHeight = Math.min(viewWidth, viewHeight);
        }
        // We make the resolution even if it is odd.
        if (remoteHeight % 2 == 1) remoteHeight--;
        return remoteHeight;
    }

    /**
     * Closes the connection and shows a fatal message which ends the activity.
     *
     */
    public void showFatalMessageAndQuit(final String error) {
        closeConnection();
        handler.post(() -> Utils.showFatalErrorMessage(context, error));
    }

    /**
     * If necessary, initializes an SSH tunnel and returns local forwarded port, or
     * if SSH tunneling is not needed, returns the given port.
     *
     */
    int getRemoteProtocolPort(int port) throws Exception {
        int result;

        if (sshTunneled && !(isRdp && connection.getRdpGatewayEnabled())) {
            initSshTunnelIfNeeded();
            int newPort = sshConnection.initializeSSHTunnel();
            if (newPort > 0) {
                port = newPort;
            }
            result = sshConnection.createLocalPortForward(port);
        } else {
            if (isVnc && port <= 20) {
                result = Constants.DEFAULT_VNC_PORT + port;
            } else {
                result = port;
            }
        }
        return result;
    }

    /**
     * If necessary, initializes an SSH tunnel and returns local forwarded port, or
     * if SSH tunneling is not needed, returns the given port.
     */
    int getGatewayPort(int port) throws Exception {
        int result = port;

        if (sshTunneled) {
            initSshTunnelIfNeeded();
            sshConnection.initializeSSHTunnel();
            result = sshConnection.createLocalPortForward(port);
        }
        return result;
    }

    /**
     * Returns localhost if using SSH tunnel or saved address.
     */
    String getAddress() {
        if (sshTunneled && !(isRdp && connection.getRdpGatewayEnabled())) {
            return "127.0.0.1";
        } else {
            return connection.getAddress();
        }
    }

    /**
     * Returns localhost if using SSH tunnel or saved address.
     */
    String getGatewayAddress() {
        if (sshTunneled) {
            return "127.0.0.1";
        } else {
            return connection.getRdpGatewayHostname();
        }
    }

    /**
     * Requests an update of the entire remote desktop.
     */
    public void writeFullUpdateRequest(boolean incremental) {
        canvas.prepareFullUpdateRequest(incremental);
        rfbConn.writeFramebufferUpdateRequest(canvas.getXoffset(), canvas.getYoffset(),
                canvas.bmWidth(), canvas.bmHeight(), incremental);
    }

    /**
     * Set the device clipboard text with the string parameter.
     */
    public void setClipboardText(String s) {
        if (s != null && s.length() > 0) {
            try {
                clipboard.setPrimaryClip(ClipData.newPlainText(null, s));
            } catch (Exception e) {
                String error = context.getString(R.string.error) + ": " + e;
                canvas.displayShortToastMessage(error);
                Log.e(TAG, "setClipboardText: exception: " + e);
            }
        }
    }

    /**
     * Method that disconnects from the remote server.
     */
    public void closeConnection() {
        maintainConnection = false;

        if (keyboard != null) {
            // Tell the server to release any meta keys.
            keyboard.clearMetaState();
            keyboard.keyEvent(0, new KeyEvent(KeyEvent.ACTION_UP, 0));
        }
        // Close the rfb connection.
        if (rfbConn != null) {
            rfbConn.close();
        }

        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }

        // Close the SSH tunnel.
        if (sshConnection != null) {
            sshConnection.terminateSSHTunnel();
            sshConnection = null;
        }

        if (connection != null) {
            Log.d(TAG, "Saving screenshot to " + context.getFilesDir() + "/" + connection.getScreenshotFilename());
            canvas.writeScreenshotToFile(context.getFilesDir() + "/" + connection.getScreenshotFilename(), 720);
        }
        onDestroy();
    }

    /**
     * Cleans up resources after a disconnection.
     */
    public void onDestroy() {
        Log.v(TAG, "Cleaning up resources");

        removeCallbacksAndMessages();
        if (clipboardMonitorTimer != null) {
            clipboardMonitorTimer.cancel();
            clipboardMonitorTimer = null;
        }
        clipboardMonitor = null;
        clipboard = null;
        decoder = null;
    }

    public void removeCallbacksAndMessages() {
        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }
    }

    /**
     * Displays connection info in a toast message.
     */
    public void showConnectionInfo() {
        if (rfbConn == null)
            return;

        String msg;
        int idx = rfbConn.desktopName().indexOf("(");
        if (idx > 0) {
            // Breakup actual desktop name from IP addresses for improved
            // readability
            String dn = rfbConn.desktopName().substring(0, idx).trim();
            String ip = rfbConn.desktopName().substring(idx).trim();
            msg = dn + "\n" + ip;
        } else
            msg = rfbConn.desktopName();
        msg += "\n" + rfbConn.framebufferWidth() + "x" + rfbConn.framebufferHeight();
        String enc = rfbConn.getEncoding();
        // Encoding might not be set when we display this message
        if (decoder != null && decoder.getColorModel() != null) {
            if (enc != null && !enc.equals("")) {
                msg += ", " + rfbConn.getEncoding() + context.getString(R.string.info_encoding) + decoder.getColorModel().toString();
            }
            msg += ", " + decoder.getColorModel().toString();
        }
        Toast.makeText(context, msg, Toast.LENGTH_SHORT).show();
    }

    public RemotePointer getPointer() {
        return pointer;
    }

    public RemoteKeyboard getKeyboard() {
        return keyboard;
    }


    @Override
    public boolean onKeyDownEvent(int keyCode, KeyEvent e) {
        GeneralUtils.debugLog(App.debugLog, TAG, "onKeyDownEvent, e: " + e);
        return keyboard.keyEvent(keyCode, e);
    }

    @Override
    public boolean onKeyUpEvent(int keyCode, KeyEvent e) {
        GeneralUtils.debugLog(App.debugLog, TAG, "onKeyUpEvent, e: " + e);
        return keyboard.keyEvent(keyCode, e);
    }

    public boolean isColorModel(COLORMODEL cm) {
        if (isVnc && decoder != null) {
            return (decoder.getColorModel() != null) && decoder.getColorModel().equals(cm);
        } else {
            return false;
        }
    }

    public void setColorModel(COLORMODEL cm) {
        if (isVnc && decoder != null) {
            decoder.setColorModel(cm);
        }
    }
}
