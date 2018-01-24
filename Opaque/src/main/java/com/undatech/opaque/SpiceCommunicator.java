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

import java.net.URISyntaxException;
import java.sql.Connection;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

import android.R.integer;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;

import org.freedesktop.gstreamer.GStreamer;
import com.undatech.opaque.input.RemoteKeyboard;
import com.undatech.opaque.input.RemoteSpicePointer;

public class SpiceCommunicator {
    
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
    
	private final static String TAG = "SpiceCommunicator";

	public native int FetchVmNames(String URI, String user, String password, String sslCaFile, boolean sslStrict);
	
	public native int CreateOvirtSession(String uri,
											String user,
											String password,
											String sslCaFile,
											boolean sound, boolean sslStrict);
	
	public native int StartSessionFromVvFile(String fileName, boolean sound);
	
	public native int SpiceClientConnect(String ip,
											String port,
											String tport,
											String password,
											String ca_file,
											String ca_cert,
											String cert_subj,
											boolean sound);
	public native void SpiceClientDisconnect();
	public native void SpiceButtonEvent(int x, int y, int metaState, int pointerMask);
	public native void SpiceKeyEvent(boolean keyDown, int virtualKeyCode);
	public native void UpdateBitmap(Bitmap bitmap, int x, int y, int w, int h);
	public native void SpiceRequestResolution(int x, int y);
	
    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("spice");
    }
    
	final static int LCONTROL = 29;
	final static int RCONTROL = 285;
	final static int LALT = 56;
	final static int RALT = 312;
	final static int LSHIFT = 42;
	final static int RSHIFT = 54;
	final static int LWIN = 347;
	final static int RWIN = 348;

	int remoteMetaState = 0;
	
	private int width = 0;
	private int height = 0;
    
	boolean isInNormalProtocol = false;
	
	private Thread thread = null;

	public SpiceCommunicator (Context context, RemoteCanvas canvas, boolean res, boolean usb) {
	    this.context = context;
	    this.canvas = canvas;
		this.isRequestingNewDisplayResolution = res;
		this.usbEnabled = usb;
		myself = this;
        mUsbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        
        try {
            GStreamer.init(context);
        } catch (Exception e) {
            e.printStackTrace();
            canvas.displayShortToastMessage(e.getMessage());
        }
	}
	
	private static SpiceCommunicator myself = null;
	private RemoteCanvas canvas = null;
	private CanvasDrawableContainer canvasDrawable = null;
	private Handler handler = null;
	private ArrayList<String> vmNames = null;
    private boolean isRequestingNewDisplayResolution = false;
    private boolean usbEnabled = false;
	private Context context = null;
	
	public void setHandler(Handler handler) {
		this.handler = handler;
	}
	
	public Handler getHandler() {
		return handler;
	}
	
	public ArrayList<String> getVmNames() {
		return vmNames;
	}

	/**
	 * Launches a new thread which performs a plain SPICE connection.
	 */
	public void connectSpice(String ip, String port, String tport, String password, String cf, String ca, String cs, boolean sound) {
		android.util.Log.e(TAG, "connectSpice: " + ip + ", " + port + ", " + tport + ", " + cf + ", " + cs);
		thread = new SpiceThread(ip, port, tport, password, cf, ca, cs, sound);
		thread.start();
	}

	/**
	 * Launches a new thread which performs an oVirt/RHEV session connection
	 */
	public void connectOvirt(String ip, String vmname, 
							String user, String password,
							String sslCaFile,
							boolean sound, boolean sslStrict) {
		android.util.Log.e(TAG, "connectOvirt: " + ip + ", " + vmname + ", " + user);
		thread = new OvirtThread(ip, vmname, user, password, sslCaFile, sound, sslStrict);
		thread.start();
	}
	
    
    /**
     * Connects to an oVirt/RHEV server to fetch the names of all VMs available to the specified user.
     */
    public int startSessionFromVvFile (String vvFileName, boolean sound) {
        return StartSessionFromVvFile(vvFileName, sound);
    }
    
    
	/**
	 * Connects to an oVirt/RHEV server to fetch the names of all VMs available to the specified user.
	 */
	public int fetchOvirtVmNames (String ip, String user, String password, String sslCaFile, boolean sslStrict) {
		vmNames = new ArrayList<String>();
		return FetchVmNames("https://" + ip + "//api", user, password, sslCaFile, sslStrict);
	}

	public void disconnect() {
		if (isInNormalProtocol) {
			SpiceClientDisconnect();
		}
		if (thread != null && thread.isAlive()) {
			try {thread.join(3000);} catch (InterruptedException e) {}
		}
	}

	class SpiceThread extends Thread {
		private String ip, port, tport, password, cf, ca, cs;
		boolean sound;

		public SpiceThread(String ip, String port, String tport, String password, String cf, String ca, String cs, boolean sound) {
			this.ip = ip;
			this.port = port;
			this.tport = tport;
			this.password = password;
			this.cf = cf;
			this.ca = ca;
			this.cs = cs;
			this.sound = sound;
		}

		public void run() {
			SpiceClientConnect (ip, port, tport, password, cf, ca, cs, sound);
			android.util.Log.e(TAG, "SpiceClientConnect returned.");

			// If we've exited SpiceClientConnect, the connection is certainly
			// interrupted or was never established.
			if (handler != null) {
				handler.sendEmptyMessage(Constants.SPICE_CONNECT_FAILURE);
			}
		}
	}
	
	class OvirtThread extends Thread {
		private String ip, vmname, user, password, sslCaFile;
		boolean sound, sslStrict;

		public OvirtThread(String ip, String vmname,
							String user, String password,
							String sslCaFile,
							boolean sound, boolean sslStrict) {
			this.ip = ip;
			this.vmname = vmname;
			this.user = user;
			this.password = password;
			this.sslCaFile = sslCaFile;
			this.sound = sound;
			this.sslStrict = sslStrict;
		}

		public void run() {
			CreateOvirtSession ("ovirt://" + ip + "/" + vmname, user, password, sslCaFile, sound, sslStrict);
			android.util.Log.e(TAG, "CreateOvirtSession returned.");

			// If we've exited CreateOvirtSession, the connection is certainly
			// interrupted or was never established.
			if (handler != null) {
				handler.sendEmptyMessage(Constants.SPICE_CONNECT_FAILURE);
			}
		}
	}
	
	public void sendMouseEvent (int x, int y, int metaState, int pointerMask) {
		SpiceButtonEvent(x, y, metaState, pointerMask);
	}

	public void sendSpiceKeyEvent (boolean keyDown, int virtualKeyCode) {
		SpiceKeyEvent(keyDown, virtualKeyCode);
	}
	
	public int framebufferWidth() {
		return width;
	}

	public int framebufferHeight() {
		return height;
	}

	public void setFramebufferWidth(int w) {
		width = w;
	}

	public void setFramebufferHeight(int h) {
		height = h;
	}
	
	public String desktopName() {
		// TODO Auto-generated method stub
		return "";
	}

	public void requestUpdate(boolean incremental) {
		// TODO Auto-generated method stub

	}

	public void writeClientCutText(String text) {
		// TODO Auto-generated method stub

	}
	
	public void setIsInNormalProtocol(boolean state) {
		isInNormalProtocol = state;		
	}
	
	public boolean isInNormalProtocol() {
		return isInNormalProtocol;
	}

	public String getEncoding() {
		// TODO Auto-generated method stub
		return "";
	}

	public void sendPointerEvent(int x, int y, int metaState, int pointerMask) {
		remoteMetaState = metaState; 
		if ((pointerMask & RemoteSpicePointer.POINTER_DOWN_MASK) != 0)
			sendModifierKeys(true);
		sendMouseEvent(x, y, metaState, pointerMask);
		if ((pointerMask & RemoteSpicePointer.POINTER_DOWN_MASK) == 0)
			sendModifierKeys(false);
	}

	private void sendModifierKeys(boolean keyDown) {		
		if ((remoteMetaState & RemoteKeyboard.CTRL_ON_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending CTRL: " + LCONTROL + " down: " + keyDown);
			sendSpiceKeyEvent(keyDown, LCONTROL);
		}
		if ((remoteMetaState & RemoteKeyboard.ALT_ON_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending ALT: " + LALT + " down: " + keyDown);
			sendSpiceKeyEvent(keyDown, LALT);
		}
        if ((remoteMetaState & RemoteKeyboard.ALTGR_ON_MASK) != 0) {
            android.util.Log.e("SpiceCommunicator", "Sending ALTGR: " + RALT + " down: " + keyDown);
            sendSpiceKeyEvent(keyDown, RALT);
        }
        if ((remoteMetaState & RemoteKeyboard.SUPER_ON_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending SUPER: " + LWIN + " down: " + keyDown);
			sendSpiceKeyEvent(keyDown, LWIN);
		}
		if ((remoteMetaState & RemoteKeyboard.SHIFT_ON_MASK) != 0) {
			android.util.Log.e("SpiceCommunicator", "Sending SHIFT: " + LSHIFT + " down: " + keyDown);
			sendSpiceKeyEvent(keyDown, LSHIFT);
		}
	}
	
	public void writeKeyEvent(int key, int metaState, boolean keyDown) {
		if (keyDown) {
			remoteMetaState = metaState;
			sendModifierKeys (true);
		}
		
		android.util.Log.e("SpiceCommunicator", "Sending scanCode: " + key + ". Is it down: " + keyDown);
		sendSpiceKeyEvent(keyDown, key);
		
		if (!keyDown) {
			sendModifierKeys (false);
			remoteMetaState = 0;
		}
	}
	
	public void writeSetPixelFormat(int bitsPerPixel, int depth,
			boolean bigEndian, boolean trueColour, int redMax, int greenMax,
			int blueMax, int redShift, int greenShift, int blueShift,
			boolean fGreyScale) {
		// TODO Auto-generated method stub

	}
	
	public void writeFramebufferUpdateRequest(int x, int y, int w, int h, boolean b) {
		// TODO Auto-generated method stub
	}
	
	public void close() {
		disconnect();
	}
	
	public void requestNewResolutionIfNeeded() {
		if (isInNormalProtocol) {
			int currentWidth = this.width;
			int currentHeight = this.height;
			if (isRequestingNewDisplayResolution) {
			    canvas.waitUntilInflated();
				int desiredWidth  = canvas.getDesiredWidth();
				int desiredHeight = canvas.getDesiredHeight();
		    	if (currentWidth != desiredWidth || currentHeight != desiredHeight) {
		    		android.util.Log.e(TAG, "Requesting new res: " + desiredWidth + "x" + desiredHeight);
					SpiceRequestResolution (desiredWidth, desiredHeight);
		    	}
			}
		}
	}
	
	/* Callbacks from jni and corresponding non-static methods */

	
	public static int openUsbDevice(int vid, int pid) throws InterruptedException {
        Log.i(TAG, "Attempting to open a USB device and return a file descriptor.");
        
        if (!myself.usbEnabled || android.os.Build.VERSION.SDK_INT < 12) {
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
	
	public static void sendMessage (int message) {
		android.util.Log.d(TAG, "sendMessage called with message: " + message);
		myself.handler.sendEmptyMessage(message);
	}

    public static void sendMessageWithText (int message, String messageText) {
        android.util.Log.d(TAG, "sendMessage called with message: " + messageText);
        Bundle b = new Bundle();
        b.putString("message", messageText);
        Message m = new Message();
        m.what = message;
        m.setData(b);
        myself.handler.sendMessage(m);
    }
	
	public static void LaunchVncViewer (String address, String port, String password) {
		android.util.Log.d(TAG, "LaunchVncViewer called");
        Bundle b = new Bundle();
        b.putString("address", address);
        b.putString("port", port);
        b.putString("password", password);
		Message msg = new Message();
		msg.what = Constants.LAUNCH_VNC_VIEWER;
		msg.setData(b);
		myself.handler.sendMessage(msg);
	}
	
	private static void AddVm (String vmname) {
		android.util.Log.d(TAG, "Adding VM: " + vmname + "to list of VMs");
		myself.vmNames.add(vmname);
	}
	
	public void onSettingsChanged(int width, int height, int bpp) {
		android.util.Log.e(TAG, "onSettingsChanged called, wxh: " + width + "x" + height);
		
		setFramebufferWidth(width);
		setFramebufferHeight(height);
		
    	canvasDrawable = canvas.reallocateDrawable(width, height);
		
		setIsInNormalProtocol(true);
    	handler.sendEmptyMessage(Constants.SPICE_CONNECT_SUCCESS);

		if (isRequestingNewDisplayResolution) {
			requestNewResolutionIfNeeded();
		}
	}
	
	private static void OnSettingsChanged(int inst, int width, int height, int bpp) {
		myself.onSettingsChanged(width, height, bpp);
	}
/*	
	public void onGraphicsUpdate(int x, int y, int width, int height) {
		//android.util.Log.e(TAG, "onGraphicsUpdate called: " + x +", " + y + " + " + width + "x" + height );
		synchronized (canvasDrawable) {
			UpdateBitmap(canvasDrawable.bitmap, x, y, width, height);
		}	
		canvas.reDraw(x, y, width, height);
	}
*/	
	private static void OnGraphicsUpdate(int inst, int x, int y, int width, int height) {
		//android.util.Log.e(TAG, "onGraphicsUpdate called: " + x +", " + y + " + " + width + "x" + height );
		synchronized (myself.canvasDrawable) {
			myself.UpdateBitmap(myself.canvasDrawable.bitmap, x, y, width, height);
		}	
		myself.canvas.reDraw(x, y, width, height);
		//myself.onGraphicsUpdate(x, y, width, height);
	}
	/* END Callbacks from jni and corresponding non-static methods */
}
