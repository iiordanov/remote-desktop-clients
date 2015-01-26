package com.iiordanov.bVNC;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Log;

import com.freerdp.freerdpcore.services.LibFreeRDP.UIEventListener;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RemoteSpicePointer;
import com.iiordanov.bVNC.Constants;
import com.gstreamer.*;

public class SpiceCommunicator implements RfbConnectable {
    private final static String TAG = "SpiceCommunicator";
    
    private HashMap<String, Integer> deviceToFdMap = new HashMap<String, Integer>();
    UsbManager mUsbManager = null;
    private final BroadcastReceiver mUsbReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (Constants.ACTION_USB_PERMISSION.equals(action)) {
                UsbDevice device = (UsbDevice)intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (device != null) {
                    int vid = device.getVendorId();
                    int pid = device.getProductId();
                    String mapKey = Integer.toString(vid)+":"+Integer.toString(pid);
                    synchronized (deviceToFdMap.get(mapKey)) {
                        deviceToFdMap.get(mapKey).notify();
                    }
                }
            }
        }
    };
    
    public native int  SpiceClientConnect (String ip, String port, String tport, String password, String ca_file, String cert_subj, boolean sound);
    public native void SpiceClientDisconnect ();
    public native void SpiceButtonEvent (int x, int y, int metaState, int pointerMask);
    public native void SpiceKeyEvent (boolean keyDown, int virtualKeyCode);
    public native void UpdateBitmap (Bitmap bitmap, int x, int y, int w, int h);
    public native void SpiceRequestResolution (int x, int y);
    
    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("spice");
    }
    
    int metaState = 0;
    int remoteMetaState = 0;
    
    private int width = 0;
    private int height = 0;
    
    boolean isInNormalProtocol = false;
    
    private SpiceThread spicethread = null;
    private static SpiceCommunicator myself = null;
    private Context context;
    private boolean usbEnabled = true;

    public SpiceCommunicator (Context context, RemoteCanvas canvas, ConnectionBean connection) {
        myself = this;
        this.context = context;
        mUsbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);

        if (connection.getEnableSound()) {
            try {
                GStreamer.init(context);
            } catch (Exception e) {
                e.printStackTrace();
                canvas.displayShortToastMessage(e.getMessage());
            }
        }
    }

    private static UIEventListener uiEventListener = null;
    private Handler handler = null;

    public void setHandler(Handler handler) {
        this.handler = handler;
    }
    
    public void setUIEventListener(UIEventListener ui) {
        uiEventListener = ui;
    }

    public Handler getHandler() {
        return handler;
    }

    public void connect(String ip, String port, String tport, String password, String cf, String cs, boolean sound) {
        //android.util.Log.e(TAG, ip + ", " + port + ", " + tport + ", " + password + ", " + cf + ", " + cs);
        spicethread = new SpiceThread(ip, port, tport, password, cf, cs, sound);
        spicethread.start();
    }
    
    public void disconnect() {
        SpiceClientDisconnect();
        try {spicethread.join(3000);} catch (InterruptedException e) {}
    }

    class SpiceThread extends Thread {
        private String ip, port, tport, password, cf, cs;
        boolean sound;

        public SpiceThread(String ip, String port, String tport, String password, String cf, String cs, boolean sound) {
            this.ip = ip;
            this.port = port;
            this.tport = tport;
            this.password = password;
            this.cf = cf;
            this.cs = cs;
            this.sound = sound;
        }

        public void run() {
            SpiceClientConnect (ip, port, tport, password, cf, cs, sound);
            android.util.Log.e(TAG, "SpiceClientConnect returned.");

            // If we've exited SpiceClientConnect, the connection was
            // interrupted or was never established.
            if (handler != null) {
                handler.sendEmptyMessage(Constants.SPICE_CONNECT_FAILURE);
            }
        }
    }
    
    public void sendMouseEvent (int x, int y, int metaState, int pointerMask) {
        SpiceButtonEvent(x, y, metaState, pointerMask);
    }

    public void sendKeyEvent (boolean keyDown, int scanCode) {
        android.util.Log.e("SpiceCommunicator", "Sending scanCode: " + scanCode + ". Is it down: " + keyDown);
        SpiceKeyEvent(keyDown, scanCode);
    }
    
    
    /* Callbacks from jni */
    
    public static int openUsbDevice(int vid, int pid) throws InterruptedException {
        Log.i(TAG, "Attempting to open a USB device and return a file descriptor.");
        
        if (Utils.isFree(myself.context) || !myself.usbEnabled || android.os.Build.VERSION.SDK_INT < 12) {
            return -1;
        }
        
        String mapKey = Integer.toString(vid)+":"+Integer.toString(pid);
        myself.deviceToFdMap.put(mapKey, 0);
        
        boolean deviceFound = false;
        UsbDevice device = null;
        HashMap<String, UsbDevice> stringDeviceMap = null;
        int timeout = Constants.usbDeviceTimeout;
        while (!deviceFound && timeout > 0) {
            stringDeviceMap = myself.mUsbManager.getDeviceList();
            Collection<UsbDevice> usbDevices = stringDeviceMap.values();
            
            Iterator<UsbDevice> usbDeviceIter = usbDevices.iterator();
            while (usbDeviceIter.hasNext()) {
                UsbDevice ud = usbDeviceIter.next();
                Log.i(TAG, "DEVICE: " + ud.toString());
                if (ud.getVendorId() == vid && ud.getProductId() == pid) {
                    Log.i(TAG, "USB device successfully matched.");
                    deviceFound = true;
                    device = ud;
                    break;
                }
            }
            timeout -= 100;
            SystemClock.sleep(100);
        }
        
        int fd = -1;
        // If the device was located in the Java layer, we try to open it, and failing that
        // we request permission and wait for it to be granted or denied, or for a timeout to occur.
        if (device != null) {
            UsbDeviceConnection deviceConnection = myself.mUsbManager.openDevice(device);
            if (deviceConnection != null) {
                fd = deviceConnection.getFileDescriptor();
            } else {
                // Request permission to access the device.
                synchronized (myself.deviceToFdMap.get(mapKey)) {
                    PendingIntent mPermissionIntent = PendingIntent.getBroadcast(myself.context, 0, new Intent(Constants.ACTION_USB_PERMISSION), 0);
                    
                    // TODO: Try putting this intent filter into the activity in the manifest file.
                    IntentFilter filter = new IntentFilter(Constants.ACTION_USB_PERMISSION);
                    myself.context.registerReceiver(myself.mUsbReceiver, filter);
                    
                    myself.mUsbManager.requestPermission(device, mPermissionIntent);
                    // Wait for permission with a timeout. 
                    myself.deviceToFdMap.get(mapKey).wait(Constants.usbDevicePermissionTimeout);
                    
                    deviceConnection = myself.mUsbManager.openDevice(device);
                    if (deviceConnection != null) {
                        fd = deviceConnection.getFileDescriptor();
                    }
                }
            }
        }
        return fd;
    }
    
    private static void OnSettingsChanged(int inst, int width, int height, int bpp) {
        if (uiEventListener != null)
            uiEventListener.OnSettingsChanged(width, height, bpp);
    }

    private static boolean OnAuthenticate(int inst, StringBuilder username, StringBuilder domain, StringBuilder password) {
        if (uiEventListener != null)
            return uiEventListener.OnAuthenticate(username, domain, password);
        return false;
    }

    private static boolean OnVerifyCertificate(int inst, String subject, String issuer, String fingerprint) {
        if (uiEventListener != null)
            return uiEventListener.OnVerifiyCertificate(subject, issuer, fingerprint);
        return false;
    }

    private static void OnGraphicsUpdate(int inst, int x, int y, int width, int height) {
        if (uiEventListener != null)
            uiEventListener.OnGraphicsUpdate(x, y, width, height);
    }

    private static void OnGraphicsResize(int inst, int width, int height, int bpp) {
        android.util.Log.e("Connector", "onGraphicsResize, width: " + width + " height: " + height);
        if (uiEventListener != null)
            uiEventListener.OnGraphicsResize(width, height, bpp);
    }
    
    @Override
    public int framebufferWidth() {
        return width;
    }

    @Override
    public int framebufferHeight() {
        return height;
    }

    public void setFramebufferWidth(int w) {
        width = w;
    }

    public void setFramebufferHeight(int h) {
        height = h;
    }
    
    @Override
    public String desktopName() {
        // TODO Auto-generated method stub
        return "";
    }

    @Override
    public void requestUpdate(boolean incremental) {
        // TODO Auto-generated method stub

    }

    @Override
    public void writeClientCutText(String text) {
        // TODO Auto-generated method stub

    }
    
    @Override
    public void setIsInNormalProtocol(boolean state) {
        isInNormalProtocol = state;        
    }
    
    @Override
    public boolean isInNormalProtocol() {
        return isInNormalProtocol;
    }

    @Override
    public String getEncoding() {
        // TODO Auto-generated method stub
        return "";
    }

    @Override
    public void writePointerEvent(int x, int y, int metaState, int pointerMask) {
        this.metaState = metaState; 
        if ((pointerMask & RemoteSpicePointer.PTRFLAGS_DOWN) != 0)
            sendModifierKeys(true, 0);
        sendMouseEvent(x, y, metaState, pointerMask);
        if ((pointerMask & RemoteSpicePointer.PTRFLAGS_DOWN) == 0)
            sendModifierKeys(false, 0);
    }

    private void sendModifierKeys(boolean keyDown, int key) {
        // Avoid sending meta keys down multiple times.
        if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
            if (!keyDown || key != RemoteKeyboard.SCAN_LEFTCTRL && (remoteMetaState & RemoteKeyboard.CTRL_MASK) == 0) {
                android.util.Log.e("SpiceCommunicator", "Sending CTRL: " + RemoteKeyboard.SCAN_LEFTCTRL + " down: " + keyDown);
                sendKeyEvent(keyDown, RemoteKeyboard.SCAN_LEFTCTRL);
            }
            if (keyDown) {
                remoteMetaState |= RemoteKeyboard.CTRL_MASK;
            } else {
                remoteMetaState &= ~RemoteKeyboard.CTRL_MASK;
            }
        }
        if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
            if (!keyDown || key != RemoteKeyboard.SCAN_LEFTALT && (remoteMetaState & RemoteKeyboard.ALT_MASK) == 0) {
                android.util.Log.e("SpiceCommunicator", "Sending ALT: " + RemoteKeyboard.SCAN_LEFTALT + " down: " + keyDown);
                sendKeyEvent(keyDown, RemoteKeyboard.SCAN_LEFTALT);
            }
            if (keyDown) {
                remoteMetaState |= RemoteKeyboard.ALT_MASK;
            } else {
                remoteMetaState &= ~RemoteKeyboard.ALT_MASK;
            }
        }
        if ((metaState & RemoteKeyboard.ALTGR_MASK) != 0) {
            if (!keyDown || key != RemoteKeyboard.SCAN_RIGHTALT && (remoteMetaState & RemoteKeyboard.ALTGR_MASK) == 0) {
                android.util.Log.e("SpiceCommunicator", "Sending ALTGR: " + RemoteKeyboard.SCAN_RIGHTALT + " down: " + keyDown);
                sendKeyEvent(keyDown, RemoteKeyboard.SCAN_RIGHTALT);
            }
            if (keyDown) {
                remoteMetaState |= RemoteKeyboard.ALTGR_MASK;
            } else {
                remoteMetaState &= ~RemoteKeyboard.ALTGR_MASK;
            }
        }
        if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
            if (!keyDown || key != RemoteKeyboard.SCAN_LEFTSUPER && (remoteMetaState & RemoteKeyboard.SUPER_MASK) == 0) {
                android.util.Log.e("SpiceCommunicator", "Sending SUPER: " + RemoteKeyboard.SCAN_LEFTSUPER + " down: " + keyDown);
                sendKeyEvent(keyDown, RemoteKeyboard.SCAN_LEFTSUPER);
            }
            if (keyDown) {
                remoteMetaState |= RemoteKeyboard.SUPER_MASK;
            } else {
                remoteMetaState &= ~RemoteKeyboard.SUPER_MASK;
            }
        }
        if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
            if (!keyDown || key != RemoteKeyboard.SCAN_LEFTSUPER && (remoteMetaState & RemoteKeyboard.SHIFT_MASK) == 0) {
                android.util.Log.e("SpiceCommunicator", "Sending SHIFT: " + RemoteKeyboard.SCAN_LEFTSHIFT + " down: " + keyDown);
                sendKeyEvent(keyDown, RemoteKeyboard.SCAN_LEFTSHIFT);
            }
            if (keyDown) {
                remoteMetaState |= RemoteKeyboard.SHIFT_MASK;
            } else {
                remoteMetaState &= ~RemoteKeyboard.SHIFT_MASK;
            }
        }
    }
    
    @Override
    public void writeKeyEvent(int key, int metaState, boolean keyDown) {
        if (keyDown) {
            this.metaState = metaState;
            sendModifierKeys (true, key);
        }
        
        sendKeyEvent(keyDown, key);
        
        if (!keyDown) {
            sendModifierKeys (false, key);
            this.metaState = metaState;
        }
    }

    @Override
    public void writeSetPixelFormat(int bitsPerPixel, int depth,
            boolean bigEndian, boolean trueColour, int redMax, int greenMax,
            int blueMax, int redShift, int greenShift, int blueShift,
            boolean fGreyScale) {
        // TODO Auto-generated method stub
    }
    
    @Override
    public void writeFramebufferUpdateRequest(int x, int y, int w, int h,
            boolean b) {
        // TODO Auto-generated method stub
        
    }
    
    @Override
    public void close() {
        disconnect();
    }
    
    @Override
    public void requestResolution(int x, int y) {
        SpiceRequestResolution (x, y);        
    }
}
