/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Timer;

import javax.security.auth.login.LoginException;

import org.apache.http.HttpException;
import org.json.JSONException;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.ClipboardManager;
import android.text.InputType;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.ImageView;
import android.widget.Toast;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;
import android.graphics.RectF;

import com.undatech.opaque.input.InputHandlerTouchpad;
import com.undatech.opaque.input.RemoteKeyboard;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.input.RemoteSpiceKeyboard;
import com.undatech.opaque.input.RemoteSpicePointer;
import com.undatech.opaque.proxmox.ProxmoxClient;
import com.undatech.opaque.proxmox.pojo.PveRealm;
import com.undatech.opaque.proxmox.pojo.PveResource;
import com.undatech.opaque.proxmox.pojo.SpiceDisplay;
import com.undatech.opaque.proxmox.pojo.VmStatus;

public class RemoteCanvas extends ImageView implements Viewable {
    private final static String TAG = "RemoteCanvas";
    
    public Handler handler;
    
    // Current connection parameters
    private ConnectionSettings settings;
    
    // Indicates whether we intend to maintain the connection.
    boolean stayConnected = true;
    
    // Variable indicating that we are currently moving the cursor in one of the input modes.
    public boolean cursorBeingMoved = false;
    
    // The drawable of this ImageView.
    public CanvasDrawableContainer myDrawable = null;
    
    // The class that provides zooming functions to the canvas.
    public CanvasZoomer canvasZoomer;
    
    // The remote pointer and keyboard
    private RemotePointer pointer;
    private RemoteKeyboard keyboard;
        
    // The class that abstracts communication with the SPICE backend.
    SpiceCommunicator spicecomm;
    
    // Map of VM names to IDs
    Map<String, String> vmNameToId;
    
    // Used to set the contents of the clipboard.
    ClipboardManager clipboard;
    Timer clipboardMonitorTimer;
    ClipboardMonitor clipboardMonitor;
    public boolean serverJustCutText = false;
        
    // Indicates that an update from the SPICE server was received.
    boolean spiceUpdateReceived = false;
    
    /*
     * These variables indicate how far the top left corner of the visible screen
     * has scrolled down and to the right of the top corner of the remote desktop.
     */
    int absX = 0;
    int absY = 0;
    
    /*
     * How much to shift coordinates over when converting from full to view coordinates.
     */
    float shiftX = 0;
    float shiftY = 0;

    /*
     * This variable holds the height of the visible rectangle of the screen. It is used to keep track
     * of how much of the screen is hidden by the soft keyboard if any.
     */
    int visibleHeight = -1;

    /*
     * These variables contain the width and height of the display in pixels
     */
    int displayWidth = 0;
    int displayHeight = 0;
    float displayDensity = 0;
    
    /*
     * Variable used for BB workarounds.
     */
    boolean bb = false;
    
    ProgressDialog progressDialog = null;
    
    
    public RemoteCanvas(final Context context, AttributeSet attrSet) {
        super(context, attrSet);
        clipboard = (ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        final Display display = ((Activity)context).getWindow().getWindowManager().getDefaultDisplay();
        displayWidth  = display.getWidth();
        displayHeight = display.getHeight();
        DisplayMetrics metrics = new DisplayMetrics();
        display.getMetrics(metrics);
        displayDensity = metrics.density;
        
        canvasZoomer = new CanvasZoomer (this);
        setScaleType(ImageView.ScaleType.MATRIX);
        
        if (android.os.Build.MODEL.contains("BlackBerry") ||
            android.os.Build.BRAND.contains("BlackBerry") || 
            android.os.Build.MANUFACTURER.contains("BlackBerry")) {
            bb = true;
        }
        progressDialog = new ProgressDialog(context);
        progressDialog.setMessage(context.getString(R.string.message_please_wait));
        progressDialog.setCancelable(false);
        
        vmNameToId = new HashMap<String, String>();
    }
    



    /**
     * Checks whether the device has networking and quits with an error if it doesn't.
     */
    private void checkNetworkConnectivity() {
        ConnectivityManager cm = (ConnectivityManager)getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
        if (activeNetwork == null || !activeNetwork.isAvailable() || !activeNetwork.isConnected()) {
            disconnectAndShowMessage(R.string.error_not_connected_to_network, R.string.error_dialog_title);
        }
    }
    
    
    /**
     * Initializes the clipboard monitor.
     */
    private void initializeClipboardMonitor() {
        clipboardMonitor = new ClipboardMonitor(getContext(), this);
        if (clipboardMonitor != null) {
            clipboardMonitorTimer = new Timer ();
            if (clipboardMonitorTimer != null) {
                clipboardMonitorTimer.schedule(clipboardMonitor, 0, 500);
            }
        }
    }

    void handleUncaughtException(Throwable e) {
        if (stayConnected) {
            e.printStackTrace();
            android.util.Log.e(TAG, e.toString());

            if (e instanceof OutOfMemoryError) {
                disposeDrawable ();
                disconnectAndShowMessage(R.string.error_out_of_memory, R.string.error_dialog_title);
            }

            disconnectAndShowMessage(R.string.error_dialog_title, R.string.error_dialog_title, e.toString());
        }
    }

    RemotePointer init(final ConnectionSettings settings, final RemoteCanvasActivityHandler handler) {
        this.settings = settings;
        this.handler = handler;
        checkNetworkConnectivity();
        initializeClipboardMonitor();
        spicecomm = new SpiceCommunicator (getContext(), handler, RemoteCanvas.this, settings.isRequestingNewDisplayResolution(), settings.isUsbEnabled());
        pointer = new RemoteSpicePointer (spicecomm, RemoteCanvas.this, handler);
        try {
            keyboard = new RemoteSpiceKeyboard(getResources(), spicecomm, RemoteCanvas.this, handler, settings.getLayoutMap());
        } catch (Throwable e) {
            handleUncaughtException(e);
        }
        return pointer;
    }

    /**
     * Initialize the canvas to show the remote desktop
     */
    void startFromVvFile(final String vvFileName) {
        Thread cThread = new Thread () {
            @Override
            public void run() {
                try {
                    spicecomm.startSessionFromVvFile(vvFileName, settings.isAudioPlaybackEnabled());
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }
    
    private void deleteMyFile (String path) {
        new File(path).delete();
    }
    
    /**
     * Initialize the canvas to show the remote desktop
     * @return 
     */
    // TODO: Switch away from writing out a file to initiating a connection directly.
    String retrieveVvFileFromPve(final String hostname, final ProxmoxClient api) {
        android.util.Log.i(TAG, String.format("Trying to connect to PVE host: " + hostname));
        final String tempVvFile = getContext().getFilesDir() + "/tempfile.vv";
        deleteMyFile(tempVvFile);

        // TODO: Improve error handling.
        Thread cThread = new Thread () {
            @Override
            public void run() {
                try {
                    String user = settings.getUser();

                    // Parse out node, virtualization type and VM ID
                    String node = RemoteClientLibConstants.PVE_DEFAULT_NODE;
                    String virt = RemoteClientLibConstants.PVE_DEFAULT_VIRTUALIZATION;
                    String vmname = settings.getVmname();
                    
                    int indexOfFirstSlash = settings.getVmname().indexOf('/');
                    if (indexOfFirstSlash != -1) {
                        // If we find at least one slash, then we need to parse out node for sure.
                        node = vmname.substring(0, indexOfFirstSlash);
                        vmname = vmname.substring(indexOfFirstSlash+1);
                        int indexOfSecondSlash = vmname.indexOf('/');
                        if (indexOfSecondSlash != -1) {
                            // If we find a second slash, we need to parse out virtualization type and vmname after node.
                            virt = vmname.substring(0, indexOfSecondSlash);
                            vmname = vmname.substring(indexOfSecondSlash+1);
                        }
                    }
                    
                    VmStatus status = api.getCurrentStatus(node, virt, Integer.parseInt(vmname));
                    if (status.getStatus().equals(VmStatus.STOPPED)) {
                        api.startVm(node, virt, Integer.parseInt(vmname));
                        while (!status.getStatus().equals(VmStatus.RUNNING)) {
                            status = api.getCurrentStatus(node, virt, Integer.parseInt(vmname));
                            SystemClock.sleep(500);
                        }
                    }
                    SpiceDisplay spiceData = api.spiceVm(node, virt, Integer.parseInt(vmname));
                    if (spiceData != null) {
                        spiceData.outputToFile(tempVvFile, hostname);
                    } else {
                        android.util.Log.e(TAG, "PVE returned null data for display.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.PVE_NULL_DATA);
                    }
                } catch (LoginException e) {
                    android.util.Log.e(TAG, "Failed to login to PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_AUTHENTICATE);
                } catch (JSONException e) {
                    android.util.Log.e(TAG, "Failed to parse json from PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_PARSE_JSON);
                } catch (NumberFormatException e) {
                    android.util.Log.e(TAG, "Error converting PVE ID to integer.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_VMID_NOT_NUMERIC);
                }  catch (IOException e) {
                    android.util.Log.e(TAG, "IO Error communicating with PVE API: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasActivityHandler.getMessageString(RemoteClientLibConstants.PVE_API_IO_ERROR,
                                        "error", e.getMessage()));
                    e.printStackTrace();
                } catch (HttpException e) {
                    android.util.Log.e(TAG, "PVE API returned error code: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasActivityHandler.getMessageString(RemoteClientLibConstants.PVE_API_UNEXPECTED_CODE,
                                        "error", e.getMessage()));
                }
                // At this stage we have either retrieved display data or failed, so permit the UI thread to continue.
                synchronized(tempVvFile) {
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
    
    /**
     * Initialize the canvas to show the remote desktop
     */
    void startPve() {
        if (!progressDialog.isShowing())
            progressDialog.show();

        Thread cThread = new Thread () {
            @Override
            public void run() {
                try {
                    // Obtain user's password if necessary.
                    if (settings.getPassword().equals("")) {
                        android.util.Log.i (TAG, "Displaying a dialog to obtain user's password.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_PASSWORD);
                        synchronized(spicecomm) {
                            spicecomm.wait();
                        }
                    }

                    String user = settings.getUser();
                    String realm = RemoteClientLibConstants.PVE_DEFAULT_REALM;

                    // Try to parse realm from user entered
                    int indexOfAt = settings.getUser().indexOf('@');
                    if (indexOfAt != -1) {
                        realm = user.substring(indexOfAt+1);
                        user = user.substring(0, indexOfAt);
                    }

                    // Connect to the API and obtain available realms
                    String uriToParse = settings.getHostname();
                    if (!uriToParse.startsWith("http://") && !uriToParse.startsWith("https://")) {
                        uriToParse = String.format("%s%s", "https://", uriToParse);
                    }
                    Uri uri = Uri.parse(uriToParse);
                    String protocol = uri.getScheme();
                    String host = uri.getHost();
                    int port = uri.getPort();
                    if (port < 0) {
                        port = 8006;
                    }
                    String pveUri = String.format("%s://%s:%d", protocol, host, port);

                    ProxmoxClient api = new ProxmoxClient(pveUri, settings, handler);
                    HashMap<String, PveRealm> realms = api.getAvailableRealms();
                    
                    // If selected realm has TFA enabled, then ask for the code
                    if (realms.get(realm).getTfa() != null) {
                        android.util.Log.i (TAG, "Displaying a dialog to obtain OTP/TFA.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_OTP_CODE);
                        synchronized(spicecomm) {
                            spicecomm.wait();
                        }
                    }
                    
                    // Login with provided credentials
                    api.login(user, realm, settings.getPassword(), settings.getOtpCode());

                    // If not VM name is specified, then get a list of VMs and let the user pick one.
                    if (settings.getVmname().isEmpty()) {
                        // Get map of user parseable names to resources
                        Map<String, PveResource> nameToResources = api.getResources();

                        if (nameToResources.isEmpty()) {
                            android.util.Log.e(TAG, "No available suitable resources in PVE cluster");
                            disconnectAndShowMessage(R.string.error_no_vm_found_for_user, R.string.error_dialog_title);
                        }

                        // If there is just one VM, pick it and skip the dialog.
                        if (nameToResources.size() == 1) {
                            android.util.Log.e(TAG, "A single VM was found, so picking it.");
                            String key = (String)nameToResources.keySet().toArray()[0];
                            PveResource a = nameToResources.get(key);
                            settings.setVmname(a.getNode() + "/" + a.getType() + "/" + a.getVmid());
                            settings.saveToSharedPreferences(getContext());
                        } else {
                            while (settings.getVmname().equals("")) {
                                android.util.Log.i (TAG, "PVE: Displaying a dialog with VMs to the user.");
                                // Populate the data structure that is used to convert VM names to IDs.
                                for (String s : nameToResources.keySet()) {
                                    vmNameToId.put(nameToResources.get(s).getName() + " (" + s + ")", s);
                                }
                                // Get the user parseable names and display them
                                ArrayList<String> vms = new ArrayList<String>(vmNameToId.keySet());
                                handler.sendMessage(RemoteCanvasActivityHandler.getMessageStringList(
                                        RemoteClientLibConstants.DIALOG_DISPLAY_VMS, "vms", vms));
                                synchronized(spicecomm) {
                                    spicecomm.wait();
                                }
                            }
                        }

                    }

                    // Only if we managed to obtain a VM name we try to get a .vv file for the display.
                    if (!settings.getVmname().isEmpty()) {
                        String vvFileName = retrieveVvFileFromPve(host, api);
                        if (vvFileName != null) {
                            startFromVvFile(vvFileName);
                        }
                    }
                } catch (LoginException e) {
                    android.util.Log.e(TAG, "Failed to login to PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_AUTHENTICATE);
                } catch (JSONException e) {
                    android.util.Log.e(TAG, "Failed to parse json from PVE.");
                    handler.sendEmptyMessage(RemoteClientLibConstants.PVE_FAILED_TO_PARSE_JSON);
                }  catch (IOException e) {
                    android.util.Log.e(TAG, "IO Error communicating with PVE API: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasActivityHandler.getMessageString(RemoteClientLibConstants.PVE_API_IO_ERROR,
                            "error", e.getMessage()));
                    e.printStackTrace();
                } catch (HttpException e) {
                    android.util.Log.e(TAG, "PVE API returned error code: " + e.getMessage());
                    handler.sendMessage(RemoteCanvasActivityHandler.getMessageString(RemoteClientLibConstants.PVE_API_UNEXPECTED_CODE,
                            "error", e.getMessage()));
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }
    
    
    /**
     * Initialize the canvas to show the remote desktop
     */
    void start() {
        if (!progressDialog.isShowing())
            progressDialog.show();

        Thread cThread = new Thread () {
            @Override
            public void run() {
                try {
                    // Obtain user's password if necessary.
                    if (settings.getPassword().equals("")) {
                        android.util.Log.i (TAG, "Displaying a dialog to obtain user's password.");
                        handler.sendEmptyMessage(RemoteClientLibConstants.GET_PASSWORD);
                        synchronized(spicecomm) {
                            spicecomm.wait();
                        }
                    }
                    
                    String ovirtCaFile = null;
                    if (settings.isUsingCustomOvirtCa()) {
                        ovirtCaFile = settings.getOvirtCaFile();
                    } else {
                        String caBundleFileName = new File(getContext().getFilesDir(), "ssl/certs/ca-certificates.crt").getPath();
                        ovirtCaFile = caBundleFileName;
                    }
                    
                    // If not VM name is specified, then get a list of VMs and let the user pick one.
                    if (settings.getVmname().equals("")) {
                        int success = spicecomm.fetchOvirtVmNames(settings.getHostname(), settings.getUser(),
                                                                    settings.getPassword(), ovirtCaFile,
                                                                    settings.isSslStrict());
                        // VM retrieval was unsuccessful we do not continue.
                        ArrayList<String> vmNames = spicecomm.getVmNames();
                        if (success != 0 || vmNames.isEmpty()) {
                            return;
                        } else {
                            // If there is just one VM, pick it and skip the dialog.
                            if (vmNames.size() == 1) {
                                settings.setVmname(vmNames.get(0));
                                settings.saveToSharedPreferences(getContext());
                            } else {
                                while (settings.getVmname().equals("")) {
                                    android.util.Log.i (TAG, "Displaying a dialog with VMs to the user.");
                                    // Populate the data structure that is used to convert VM names to IDs.
                                    for (String s : vmNames) {
                                        vmNameToId.put(s, s);
                                    }
                                    handler.sendMessage(RemoteCanvasActivityHandler.getMessageStringList(RemoteClientLibConstants.DIALOG_DISPLAY_VMS,
                                                                                                         "vms", vmNames));
                                    synchronized(spicecomm) {
                                        spicecomm.wait();
                                    }
                                }
                            }
                        }
                    }
                    
                    spicecomm.connectOvirt(settings.getHostname(),
                                            settings.getVmname(),
                                            settings.getUser(),
                                            settings.getPassword(),
                                            ovirtCaFile,
                                            settings.isAudioPlaybackEnabled(), settings.isSslStrict());
                    
                    try {
                        synchronized(spicecomm) {
                            spicecomm.wait(35000);
                        }
                    } catch (InterruptedException e) {}
                    
                    if (!spiceUpdateReceived && stayConnected) {
                        handler.sendEmptyMessage(RemoteClientLibConstants.OVIRT_TIMEOUT);
                    }
                    
                } catch (Throwable e) {
                    handleUncaughtException(e);
                }
            }
        };
        cThread.start();
    }
    
    
    /**
     * Retreives the requested remote width.
     */
    @Override
    public int getDesiredWidth() {
        int w = getWidth();
        android.util.Log.e(TAG, "Width requested: " + w);
        return w;
    }
    
    /**
     * Retreives the requested remote height.
     */
    @Override
    public int getDesiredHeight() {
        int h = getHeight();
        android.util.Log.e(TAG, "Height requested: " + h);
        return h;
    }
    
    public boolean getMouseFollowPan() {
        // TODO: Fix
        return true; //connection.getFollowPan();
    }
    
    public void displayShortToastMessage (final CharSequence message) {
        screenMessage = message;
        handler.removeCallbacks(showMessage);
        handler.post(showMessage);
    }

    public void displayShortToastMessage (final int messageID) {
        screenMessage = getResources().getText(messageID);
        handler.removeCallbacks(showMessage);
        handler.post(showMessage);
    }
    
    void disconnectAndShowMessage (final int messageId, final int titleId) {
        disconnectAndCleanUp();
        handler.post(new Runnable() {
            public void run() {
                MessageDialogs.displayMessageAndFinish(getContext(), messageId, titleId);
            }
        });
    }
    
    void disconnectAndShowMessage (final int messageId, final int titleId, final String textToAppend) {
        disconnectAndCleanUp();
        handler.post(new Runnable() {
            public void run() {
                MessageDialogs.displayMessageAndFinish(getContext(), messageId, titleId, textToAppend);
            }
        });
    }
    
    /**
     * Set the device clipboard text with the string parameter.
     * @param s set the device clipboard to the text in this parameter.
     */
    public void setClipboardText(String s) {
        if (s != null && s.length() > 0) {
            clipboard.setText(s);
        }
    }
    
    void disposeDrawable() {
        if (myDrawable != null)
            myDrawable.destroy();
        myDrawable = null;
        System.gc();
    }

    @Override
    public void reallocateDrawable(int width, int height) {
        disposeDrawable();
        try {
            myDrawable = new CanvasDrawableContainer(width, height);
        } catch (Throwable e) {
            disconnectAndShowMessage (R.string.error_out_of_memory, R.string.error_dialog_title);
        }
        // TODO: Implement cursor integration.
        initializeSoftCursor();
        // Set the drawable for the canvas, now that we have it (re)initialized.
        handler.post(drawableSetter);
        computeShiftFromFullToView ();
    }
    
    
    public void disconnectAndCleanUp() {
        stayConnected = false;
        
        if (keyboard != null) {
            // Tell the server to release any meta keys.
            keyboard.clearOnScreenMetaState();
            keyboard.keyEvent(0, new KeyEvent(KeyEvent.ACTION_UP, 0));
        }
        
        if (spicecomm != null)
            spicecomm.close();
        
        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
        }
        
        if (clipboardMonitorTimer != null) {
            clipboardMonitorTimer.cancel();
            // Occasionally causes a NullPointerException
            //clipboardMonitorTimer.purge();
            clipboardMonitorTimer = null;
        }
        
        clipboardMonitor = null;
        clipboard        = null;

        try {
            if (myDrawable != null && myDrawable.bitmap != null) {
                String location = settings.getFilename();
                FileOutputStream out = new FileOutputStream(getContext().getFilesDir() + "/" + location + ".png");
                Bitmap tmp = Bitmap.createScaledBitmap(myDrawable.bitmap, 360, 300, true);
                myDrawable.bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                out.close();
                tmp.recycle();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        disposeDrawable ();
    }
    
    /**
     * Make sure the remote pointer is visible
     */
    public void movePanToMakePointerVisible() {
        if (spicecomm != null) {
            int x = pointer.getX();
            int y = pointer.getY();
            int newAbsX = absX;
            int newAbsY = absY;

            int wthresh = 30;
            int hthresh = 30;
            int visibleWidth  = getVisibleDesktopWidth();
            int visibleHeight = getVisibleDesktopHeight();
            int desktopWidth  = getDesktopWidth();
            int desktopHeight = getDesktopHeight();
            if (x - absX >= visibleWidth - wthresh) {
                newAbsX = x - (visibleWidth - wthresh);
            } else if (x < absX + wthresh) {
                newAbsX = x - wthresh;
            }
            if (y - absY >= visibleHeight - hthresh) {
                newAbsY = y - (visibleHeight - hthresh);
            } else if (y < absY + hthresh) {
                newAbsY = y - hthresh;
            }
            if (newAbsX < 0) {
                newAbsX = 0;
            }
            if (newAbsX + visibleWidth > desktopWidth) {
                newAbsX = desktopWidth - visibleWidth;
            }
            if (newAbsY < 0) {
                newAbsY = 0;
            }
            if (newAbsY + visibleHeight > desktopHeight) {
                newAbsY = desktopHeight - visibleHeight;
            }
            absolutePan(newAbsX, newAbsY);
        }
    }
    
    /**
     * Relative pan.
     * @param dX
     * @param dY
     */
    public void relativePan(int dX, int dY) {
        double zoomFactor = getZoomFactor();
        absolutePan((int)(absX + dX/zoomFactor), (int)(absY + dY/zoomFactor));
    }
    
    /**
     * Absolute pan.
     * @param x
     * @param y
     */
    public void absolutePan(int x, int y) {
        if (canvasZoomer != null) {
            int vW = getVisibleDesktopWidth();
            int vH = getVisibleDesktopHeight();
            int w = getDesktopWidth();
            int h = getDesktopHeight();
            if (x + vW > w) x = w - vW;
            if (y + vH > h) y = h - vH;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            absX = x;
            absY = y;
            resetScroll();
        }
    }
    
    /**
     * Reset the canvas's scroll position.
     */
    void resetScroll() {
        float scale = getZoomFactor();
        scrollTo((int)((absX - shiftX) * scale),
                 (int)((absY - shiftY) * scale));
    }
    
    /**
     * Computes the X and Y offset for converting coordinates from full-frame coordinates to view coordinates.
     */
    public void computeShiftFromFullToView () {
        shiftX = (spicecomm.framebufferWidth()  - getWidth())  / 2;
        shiftY = (spicecomm.framebufferHeight() - getHeight()) / 2;
    }
    
    @Override
    protected void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
        if (myDrawable != null) {
            pointer.movePointerToMakeVisible();
        }
    }
    
    /**
     * This runnable displays a message on the screen.
     */
    CharSequence screenMessage;
    private Runnable showMessage = new Runnable() {
            public void run() { Toast.makeText( getContext(), screenMessage, Toast.LENGTH_SHORT).show(); }
    };
    
    /**
     * This runnable sets the drawable for this ImageView.
     */
    private Runnable drawableSetter = new Runnable() {
        public void run() {
            if (myDrawable != null)
                setImageDrawable(myDrawable);
                canvasZoomer.resetScaling();
            }
    };

    @Override
    public Bitmap getBitmap() {
        return myDrawable.bitmap;
    }
    
    /**
     * Causes a redraw of the bitmapData to happen at the indicated coordinates.
     */
    public void reDraw(int x, int y, int w, int h) {
        float scale = getZoomFactor();
        float shiftedX = x-shiftX;
        float shiftedY = y-shiftY;
        // Make the box slightly larger to avoid artifacts due to truncation errors.
        postInvalidate ((int)((shiftedX-1)*scale),   (int)((shiftedY-1)*scale),
                        (int)((shiftedX+w+1)*scale), (int)((shiftedY+h+1)*scale));
    }

    /**
     * This is a float-accepting version of reDraw().
     * Causes a redraw of the bitmapData to happen at the indicated coordinates.
     */
    public void reDraw(float x, float y, float w, float h) {
        float scale = getZoomFactor();
        float shiftedX = x-shiftX;
        float shiftedY = y-shiftY;
        // Make the box slightly larger to avoid artifacts due to truncation errors.
        postInvalidate ((int)((shiftedX-1.f)*scale),   (int)((shiftedY-1.f)*scale),
                        (int)((shiftedX+w+1.f)*scale), (int)((shiftedY+h+1.f)*scale));
    }

    /**
     * Redraws the location of the remote pointer.
     */
    public void reDrawRemotePointer(int x, int y) {
        if (myDrawable != null) {
            myDrawable.moveCursorRect(x, y);
            RectF r = myDrawable.getCursorRect();
            reDraw(r.left, r.top, r.width(), r.height());
        }
    }

    @Override
    public void setMousePointerPosition(int x, int y) {
        softCursorMove(x, y);
    }

    @Override
    public void mouseMode(boolean relative) {
        if (relative && !settings.getInputMethod().equals(InputHandlerTouchpad.ID)) {
            MessageDialogs.displayMessage(handler, getContext(), R.string.info_set_touchpad_input_mode, R.string.error_dialog_title);
        } else {
            this.pointer.setRelativeEvents(relative);
        }
    }

    /**
     * Moves soft cursor into a particular location.
     *
     * @param x
     * @param y
     */
    synchronized void softCursorMove(int x, int y) {
        if (myDrawable.isNotInitSoftCursor()) {
            initializeSoftCursor();
        }

        if (!cursorBeingMoved || pointer.isRelativeEvents()) {
            pointer.setX(x);
            pointer.setY(y);
            RectF prevR = new RectF(myDrawable.getCursorRect());
            // Move the cursor.
            myDrawable.moveCursorRect(x, y);
            // Show the cursor.
            RectF r = myDrawable.getCursorRect();
            reDraw(r.left, r.top, r.width(), r.height());
            reDraw(prevR.left, prevR.top, prevR.width(), prevR.height());
        }
    }
    
    void initializeSoftCursor () {
        Bitmap bm = BitmapFactory.decodeResource(getResources(), R.drawable.cursor);
        int w = bm.getWidth();
        int h = bm.getHeight();
        int [] tempPixels = new int[w*h];
        bm.getPixels(tempPixels, 0, w, 0, 0, w, h);
        // Set cursor rectangle as well.
        myDrawable.setCursorRect(pointer.getX(), pointer.getY(), w, h);
        // Set softCursor to whatever the resource is.
        myDrawable.setSoftCursor (tempPixels);
        bm.recycle();
    }
    
    public RemotePointer getPointer() {
        return pointer;
    }

    public RemoteKeyboard getKeyboard() {
        return keyboard;
    }
    
    public float getZoomFactor() {
        if (canvasZoomer == null)
            return 1;
        return canvasZoomer.getZoomFactor();
    }
    
    public int getVisibleDesktopWidth() {
        return (int)((double)getWidth() / getZoomFactor() + 0.5);
    }
    
    public int getVisibleDesktopHeight() {
        if (visibleHeight > 0)
            return (int)((double)visibleHeight / getZoomFactor() + 0.5);
        else
            return (int)((double)getHeight() / getZoomFactor() + 0.5);
    }
    
    public void setVisibleDesktopHeight(int newHeight) {
        visibleHeight = newHeight;
    }
    
    public int getDesktopWidth() {
        return spicecomm.framebufferWidth();
    }

    public int getDesktopHeight() {
        return spicecomm.framebufferHeight();
    }
    
    public float getMinimumScale() {
        if (myDrawable != null) {
            return myDrawable.getMinimumScale(getWidth(), getHeight());
        } else
            return 1.f;
    }
    
    public int getAbsX() {
        return absX;
    }
    
    public int getAbsY() {
        return absY;
    }
    
    public float getShiftX() {
        return shiftX;
    }
    
    public float getShiftY() {
        return shiftY;
    }
    
    public float getDisplayDensity() {
        return displayDensity;
    }
    
    public float getDisplayWidth() {
        return displayWidth;
    }
    
    public float getDisplayHeight() {
        return displayHeight;
    }
    
    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        android.util.Log.d(TAG, "onCreateInputConnection called");
        BaseInputConnection bic = new BaseInputConnection(this, false);
        outAttrs.actionLabel = null;
        outAttrs.inputType = InputType.TYPE_NULL;
        String currentIme = Settings.Secure.getString(getContext().getContentResolver(), Settings.Secure.DEFAULT_INPUT_METHOD);
        android.util.Log.d(TAG, "currentIme: " + currentIme);
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return bic;
    }
    
    /**
     * Used to wait until getWidth and getHeight return sane values.
     */
    @Override
    public void waitUntilInflated() {
        synchronized (this) {
            while (getWidth() == 0 || getHeight() == 0) {
                try {
                    this.wait();
                } catch (InterruptedException e) { e.printStackTrace(); }
            }
        }
    }
    
    /**
     * Used to detect when the view is inflated to a sane size other than 0x0.
     */
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (w > 0 && h > 0) {
            synchronized (this) {
                this.notify();
            }
        }
    }
}
