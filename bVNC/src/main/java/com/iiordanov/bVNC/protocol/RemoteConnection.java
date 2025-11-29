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

import android.app.ProgressDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;
import android.widget.Toast;

import com.iiordanov.bVNC.App;
import com.iiordanov.bVNC.COLORMODEL;
import com.iiordanov.bVNC.ClipboardMonitor;
import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Decoder;
import com.iiordanov.bVNC.SSHConnection;
import com.iiordanov.bVNC.Utils;
import com.iiordanov.bVNC.input.KeyInputHandler;
import com.iiordanov.bVNC.input.PointerInputHandler;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.undatech.opaque.Connection;
import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.MessageDialogs;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.remoteClientUi.R;

import java.util.HashMap;
import java.util.Map;
import java.util.Timer;

abstract public class RemoteConnection implements PointerInputHandler, KeyInputHandler, InputCarriable {
    private final static String TAG = "RemoteConnection";
    public static final int CLIPBOARD_INITIAL_DELAY = 0;
    public static final int CLIPBOARD_CHECK_PERIOD = 500;
    public static final int MAX_IN_DIALOG_ERROR_LENGTH = 300;

    // Connection parameters
    public Connection connection;
    public SSHConnection sshConnection = null;
    protected RfbConnectable rfbConn = null;
    public boolean maintainConnection = true;
    // Progress dialog shown at connection time.
    public ProgressDialog pd;
    public boolean serverJustCutText = false;
    public Runnable hideKeyboardAndExtraKeys;
    public boolean graphicsSettingsReceived = false;
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
    Context context;
    Viewable canvas;
    Thread connectionThread;
    boolean isLocalToRemoteClipboardIntegrationEnabled;
    boolean isRemoteToLocalClipboardIntegrationEnabled;

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
        this.pd = ProgressDialog.show(
                context,
                context.getString(R.string.info_progress_dialog_connecting),
                context.getString(R.string.info_progress_dialog_establishing),
                true,
                true,
                dialog -> handler.sendEmptyMessage(RemoteClientLibConstants.DISCONNECT_NO_MESSAGE));

        // Make this dialog cancellable only upon hitting the Back button and not touching outside.
        this.pd.setCanceledOnTouchOutside(false);
        this.isLocalToRemoteClipboardIntegrationEnabled = Utils.querySharedPreferenceBoolean(context, Constants.localToRemoteClipboardIntegration, true);
        this.isRemoteToLocalClipboardIntegrationEnabled = Utils.querySharedPreferenceBoolean(context, Constants.remoteToLocalClipboardIntegration, true);
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
        if (isLocalToRemoteClipboardIntegrationEnabled) {
            Log.i(TAG, "Local to remote clipboard integration enabled");
            clipboardMonitor = new ClipboardMonitor(canvas, context, rfbConn);
            if (clipboardMonitor != null) {
                clipboardMonitorTimer = new Timer();
                try {
                    clipboardMonitorTimer.schedule(clipboardMonitor, CLIPBOARD_INITIAL_DELAY, CLIPBOARD_CHECK_PERIOD);
                } catch (NullPointerException e) {
                    Log.d(TAG, "Ignored NullPointerException while initializing clipboard monitor: " + Log.getStackTraceString(e));
                }
            }
        } else {
            Log.i(TAG, "Local to remote clipboard integration enabled");
        }
    }

    public void setHandler(Handler handler) {
        RemoteConnection.handler = handler;
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
            Log.e(TAG, Log.getStackTraceString(e));
            // Ensure we dismiss the progress dialog before we finish
            if (pd.isShowing())
                pd.dismiss();

            if (e instanceof OutOfMemoryError) {
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

                    error = error + "<br><br>" + context.getString(R.string.error) + ":<br><br>" +
                            Log.getStackTraceString(e).substring(0, MAX_IN_DIALOG_ERROR_LENGTH) + " ...";
                }
                showFatalMessageAndQuit(error);
            }
        }
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
        if (s != null && !s.isEmpty()) {
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

        if (connectionThread != null) {
            connectionThread.interrupt();
        }

        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }

        // Close the SSH tunnel.
        if (sshConnection != null) {
            sshConnection.terminateSSHTunnel();
            sshConnection = null;
        }

        if (connection != null && !Utils.isDoNotShowDesktopThumbnails(context)) {
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
            if (enc != null && !enc.isEmpty()) {
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
}
