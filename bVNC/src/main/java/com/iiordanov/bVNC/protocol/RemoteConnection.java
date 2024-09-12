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

package com.iiordanov.bVNC.protocol;

import static com.undatech.opaque.RemoteClientLibConstants.GET_FILE_TIMEOUT;

import android.annotation.SuppressLint;
import android.app.ProgressDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import com.iiordanov.bVNC.App;
import com.iiordanov.bVNC.COLORMODEL;
import com.iiordanov.bVNC.ClipboardMonitor;
import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Decoder;
import com.iiordanov.bVNC.SSHConnection;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.input.KeyInputHandler;
import com.iiordanov.bVNC.input.PointerInputHandler;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.util.UriIntentParser;
import com.undatech.opaque.Connection;
import com.undatech.opaque.ConnectionSettings;
import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.util.FileUtils;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.remoteClientUi.R;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;
import java.util.Timer;

;

abstract public class RemoteConnection implements PointerInputHandler, KeyInputHandler, InputCarriable {
    private final static String TAG = "RemoteConnection";

    // Connection parameters
    public Connection connection;
    public SSHConnection sshConnection = null;
    protected RfbConnectable rfbConn = null;
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
    public static Handler handler;
    public Map<String, String> vmNameToId = new HashMap<>();
    // RFB Decoder
    Decoder decoder = null;
    // The remote pointer and keyboard
    RemotePointer pointer;
    RemoteKeyboard keyboard;
    // Used to set the contents of the clipboard.
    ClipboardManager clipboard;
    Timer clipboardMonitorTimer;
    ClipboardMonitor clipboardMonitor;
    boolean sshTunneled;
    static String configFileName;
    Context context;
    Viewable canvas;

    static String configFileExtension = "rdp";
    static int failedToObtainConfigFileMessageId = R.string.error_failed_to_obtain_rdp_file;
    static int failedToObtainConfigFileAsContentMessageId = R.string.error_failed_to_obtain_rdp_content;
    static int failedToObtainConfigFileOverHttpMessageId = R.string.error_failed_to_download_rdp_http;
    static int failedToObtainConfigFileOverHttpsMessageId = R.string.error_failed_to_download_rdp_https;

    /**
     * Constructor used by the inflation apparatus
     */
    public RemoteConnection(
            final Context context,
            Connection connection,
            Viewable canvas,
            Runnable hideKeyboardAndExtraKeys
    ) {
        this.context = context;
        this.connection = connection;
        this.canvas = canvas;
        this.hideKeyboardAndExtraKeys = hideKeyboardAndExtraKeys;
        this.sshTunneled = connection.getConnectionType() == Constants.CONN_TYPE_SSH;
        this.clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        // Startup the connection thread with a progress dialog
        this.pd = ProgressDialog.show(context, context.getString(R.string.info_progress_dialog_connecting),
                context.getString(R.string.info_progress_dialog_establishing),
                true, true, dialog -> {
                    closeConnection();
                    handler.post(() -> Utils.showFatalErrorMessage(context, context.getString(R.string.info_progress_dialog_aborted)));
                });

        // Make this dialog cancellable only upon hitting the Back button and not touching outside.
        this.pd.setCanceledOnTouchOutside(false);
        this.pd.setCancelable(false);
    }


    public static Connection getRemoteConnectionSettings(Intent i, Context context, boolean masterPasswordEnabled) throws MasterPasswordNotSupportedForIntentsException {
        Connection connection;
        if (Utils.isOpaque(context)) {
            return getOpaqueConnection(i, context);
        }

        Uri data = i.getData();
        boolean isSupportedScheme = isSupportedScheme(data);
        if (isSupportedScheme || !Utils.isNullOrEmptry(i.getType())) {
            connection = handleSupportedUri(data, context, masterPasswordEnabled);
        } else {
            connection = loadSerializedConnection(i, context);
        }
        return connection;
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private static Connection getOpaqueConnection(Intent i, Context context) {
        Connection connection;
        if (configFileName == null) {
            Log.d(TAG, "Initializing session from connection settings.");
            connection = (ConnectionSettings) i.getSerializableExtra(Constants.opaqueConnectionSettingsClassPath);
        } else {
            connection = new ConnectionSettings(RemoteClientLibConstants.DEFAULT_SETTINGS_FILE);
            connection.load(context);
        }
        return connection;
    }

    private static boolean isSupportedScheme(Uri data) {
        boolean isSupportedScheme = false;
        if (data != null) {
            String s = data.getScheme();
            isSupportedScheme = "rdp".equals(s) || "spice".equals(s) || "vnc".equals(s);
        }
        return isSupportedScheme;
    }

    private static Connection handleSupportedUri(Uri data, Context context, boolean masterPasswordEnabled) throws MasterPasswordNotSupportedForIntentsException {
        Log.d(TAG, "Initializing classic connection from Intent.");
        if (masterPasswordEnabled) {
            throw new MasterPasswordNotSupportedForIntentsException();
        }

        return createConnectionFromUri(data, context);
    }

    private static Connection loadSerializedConnection(Intent i, Context context) {
        Log.d(TAG, "Initializing serialized connection");
        Connection connection = new ConnectionBean(context);
        Bundle extras = i.getExtras();

        if (extras != null) {
            Log.d(TAG, "Loading values from serialized connection");
            connection.populateFromContentValues((ContentValues) extras.getParcelable(Utils.getConnectionString(context)));
            connection.load(context);
        }
        return connection;
    }

    private static Connection createConnectionFromUri(Uri data, Context context) {
        Connection connection = UriIntentParser.loadFromUriOrCreateNew(data, context);
        String host = null;
        if (data != null) {
            host = data.getHost();
        }
        if (host != null && !host.startsWith(Utils.getConnectionString(context))) {
            UriIntentParser.parseFromUri(context, connection, data);
        }
        return connection;
    }

    public RfbConnectable getRfbConn() {
        return rfbConn;
    }

    /**
     * Checks whether the device has networking and quits with an error if it doesn't.
     */
    protected void checkNetworkConnectivity() {
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
    protected void initializeClipboardMonitor() {
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

    public void setHandler(Handler handler) {
        this.handler = handler;
    }

    /**
     * Create a view showing a remote desktop connection
     */
    public void initializeConnection() {
        Log.i(TAG, "Initializing remote connection");
        this.maintainConnection = true;
        handler.post(this.hideKeyboardAndExtraKeys);
    }

    protected void constructSshConnectionIfNeeded() throws Exception {
        if (sshTunneled && sshConnection == null) {
            String targetAddress = getSshTunnelTargetAddress();
            sshConnection = new SSHConnection(targetAddress, connection, context, handler);
            sshConnection.initializeSSHTunnel();
        }
    }

    abstract public String getSshTunnelTargetAddress();

    protected void handleUncaughtException(Throwable e, int resource) {
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
                        error = context.getString(resource);
                    }
                    error = error + "<br>" + e.getLocalizedMessage();
                }
                showFatalMessageAndQuit(error);
            }
        }
    }

    public String getConfigFileName() {
        return this.configFileName;
    }

    /**
     * Closes the connection and shows a fatal message which ends the activity.
     */
    public void showFatalMessageAndQuit(final String error) {
        closeConnection();
        handler.post(() -> Utils.showFatalErrorMessage(context, error));
    }

    /**
     * Returns localhost if using SSH tunnel or saved address.
     */
    String getAddress() {
        if (sshTunneled) {
            return "127.0.0.1";
        } else {
            return connection.getAddress();
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

    abstract public boolean isColorModel(COLORMODEL cm);

    abstract public void setColorModel(COLORMODEL cm);

    /**
     * This function is called by the rotationCorrector runnable
     * to fix things up after a rotation.
     */
    abstract public void correctAfterRotation() throws Exception;

    public boolean canUpdateColorModelConnected() {
        return false;
    }

    public boolean onKeyAsPointerEvent(int keyCode, KeyEvent event) {
        return pointer.hardwareButtonsAsMouseEvents(keyCode, event, 0);
    }

    /**
     * Retrieves a vv file from the intent if possible and returns the path to it.
     *
     * @param i intent that started the activity
     * @return the vv file name or NULL if no file was discovered.
     */
    public static int retrieveConfigFileFromIntent(Intent i, String filesDir, Context context, Object waitOn) {
        setFileExtensionAndErrorMessageStrings(context);
        final Uri data = i.getData();
        String configFileName = null;
        final String tempConfigFile = filesDir + "/tempfile." + configFileExtension;
        final int[] msgId = {failedToObtainConfigFileMessageId};

        Log.d(TAG, "Got intent: " + i);

        if (data != null) {
            Log.d(TAG, "Got data: " + data);
            final String dataString = data.toString();
            if (dataString.startsWith("http")) {
                android.util.Log.d(TAG, "Intent is with http scheme.");
                FileUtils.deleteFile(tempConfigFile);

                // Spin up a thread to grab the file over the network.
                Thread t = new Thread() {
                    @Override
                    public void run() {
                        try {
                            // Download the file and write it out.
                            URL url = new URL(data.toString());
                            File file = new File(tempConfigFile);
                            URLConnection ucon = url.openConnection();
                            FileUtils.outputToFile(ucon.getInputStream(), file);

                            synchronized (waitOn) {
                                waitOn.notify();
                            }
                        } catch (IOException e) {
                            msgId[0] = failedToObtainConfigFileOverHttpMessageId;
                            if (dataString.startsWith("https")) {
                                msgId[0] = failedToObtainConfigFileOverHttpsMessageId;
                            }
                        }
                    }
                };
                t.start();

                synchronized (waitOn) {
                    try {
                        waitOn.wait(GET_FILE_TIMEOUT);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    configFileName = tempConfigFile;
                }
            } else if (dataString.startsWith("file")) {
                Log.d(TAG, "Intent is with file scheme.");
                msgId[0] = failedToObtainConfigFileMessageId;
                configFileName = data.getPath();
            } else if (dataString.startsWith("content")) {
                Log.d(TAG, "Intent is with content scheme.");
                msgId[0] = failedToObtainConfigFileAsContentMessageId;
                FileUtils.deleteFile(tempConfigFile);

                try {
                    FileUtils.outputToFile(context.getContentResolver().openInputStream(data), new File(tempConfigFile));
                    configFileName = tempConfigFile;
                } catch (IOException e) {
                    Log.e(TAG, "Could not write temp file: IOException.");
                    e.printStackTrace();
                } catch (SecurityException e) {
                    Log.e(TAG, "Could not write temp file: SecurityException.");
                    e.printStackTrace();
                }
            }

            // Check if we were successful in obtaining a file and put up an error dialog if not.
            if (dataString.startsWith("http") || dataString.startsWith("file") || dataString.startsWith("content")
                    && configFileName == null) {
                return msgId[0];
            }
            android.util.Log.d(TAG, "Got filename: " + configFileName);
        }
        RemoteConnection.configFileName = configFileName;
        return 0;
    }

    private static void setFileExtensionAndErrorMessageStrings(Context context) {
        if (Utils.isOpaque(context)) {
            configFileExtension = "vv";
            failedToObtainConfigFileMessageId = R.string.error_failed_to_obtain_vv_file;
            failedToObtainConfigFileAsContentMessageId = R.string.error_failed_to_obtain_vv_content;
            failedToObtainConfigFileOverHttpMessageId = R.string.error_failed_to_download_vv_http;
            failedToObtainConfigFileOverHttpsMessageId = R.string.error_failed_to_download_vv_https;
        }
    }
}
