/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * Copyright (C) 2004 Horizon Wimba.  All Rights Reserved.
 * Copyright (C) 2001-2003 HorizonLive.com, Inc.  All Rights Reserved.
 * Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 * Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

//
// VncCanvas is a subclass of android.view.SurfaceView which draws a VNC
// desktop on it.
//

package com.iiordanov.bVNC;

import java.io.IOException;
import java.net.Socket;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.Timer;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.text.ClipboardManager;
import android.util.AttributeSet;
import android.util.Base64;
import android.util.Log;
import android.view.Display;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.ImageView;
import android.widget.Toast;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;

import com.iiordanov.android.bc.BCFactory;

import com.iiordanov.tigervnc.rfb.UnicodeToKeysym;
import com.iiordanov.tigervnc.vncviewer.CConn;

public class VncCanvas extends ImageView {
	private final static String TAG = "VncCanvas";
	private final static boolean LOCAL_LOGV = true;
	
	// Variable holding the state of the Menu toggle buttons for meta keys (Ctrl, Alt...)
	private static int onScreenMetaState = 0;
	
	AbstractScaling scaling;
	
	// Available to activity
	int mouseX, mouseY;
	
	// Variable indicating that we are currently scrolling in simulated touchpad mode.
	boolean inScrolling = false;
	
	// Connection parameters
	ConnectionBean connection;
	VncDatabase database;
	private SSHConnection sshConnection;

	private final static int SCAN_ESC = 1;
	private final static int SCAN_LEFTCTRL = 29;
	private final static int SCAN_RIGHTCTRL = 97;
	private final static int SCAN_F1 = 59;
	private final static int SCAN_F2 = 60;
	private final static int SCAN_F3 = 61;
	private final static int SCAN_F4 = 62;
	private final static int SCAN_F5 = 63;
	private final static int SCAN_F6 = 64;
	private final static int SCAN_F7 = 65;
	private final static int SCAN_F8 = 66;
	private final static int SCAN_F9 = 67;
	private final static int SCAN_F10 = 68;
	//private final static int SCAN_HOME = 102;
	//private final static int SCAN_END = 107;
	
	/**
	 * Use camera button as meta key for right mouse button
	 */
	boolean cameraButtonDown = false;
	
	// Keep track when a seeming key press was the result of a menu shortcut
	int lastKeyDown;
	boolean afterMenu;

	// VNC protocol connection
	public RfbConnectable rfbconn = null;
	public RfbProto rfb = null;
	Socket sock = null;
	public CConn cc = null;
	boolean maintainConnection = true;
	
	// RFB Decoder
	Decoder decoder = null;

	// Internal bitmap data
	private int capacity;
	public AbstractBitmapData bitmapData;
	boolean useFull = false;

	// Handler for the dialog that displays the x509 key signatures to the user.
	public Handler handler = new Handler() {
	    @Override
	    public void handleMessage(Message msg) {
	        switch (msg.what) {
	        // TODO: Change this numeric message to something more sane.
	        case 1:
	        	final X509Certificate cert = (X509Certificate)msg.obj;

	        	if (connection.getSshHostKey().equals("")) {
	    			// Show a dialog with the key signature for approval.
	    			DialogInterface.OnClickListener signatureNo = new DialogInterface.OnClickListener() {
	    	            @Override
	    	            public void onClick(DialogInterface dialog, int which) {
	    	                // We were told not to continue, so stop the activity
	    	            	closeConnection();
	    	            	((Activity) getContext()).finish();
	    	            }
	    	        };
	    	        DialogInterface.OnClickListener signatureYes = new DialogInterface.OnClickListener() {
	    	            @Override
	    	            public void onClick(DialogInterface dialog, int which) {
	    	    			// We were told to go ahead with the connection, so save the key into the database.
	    	            	String certificate = null;
	    	            	try {
	    	            		certificate = Base64.encodeToString(cert.getEncoded(), Base64.DEFAULT);
							} catch (CertificateEncodingException e) {
								e.printStackTrace();
								showFatalMessageAndQuit("Certificate encoding could not be generated.");
							}
							connection.setSshHostKey(certificate);
			    			connection.save(database.getWritableDatabase());
			    			database.close();
			    			// Indicate the certificate was accepted.
	    	            	certificateAccepted = true;
	    	            }
	    	        };

					// Generate a sha1 signature of the certificate.
				    MessageDigest sha1;
				    MessageDigest md5;
					try {
						sha1 = MessageDigest.getInstance("SHA1");
						md5 = MessageDigest.getInstance("MD5");
			    	    sha1.update(cert.getEncoded());
		    			Utils.showYesNoPrompt(getContext(), "Continue connecting to " + connection.getAddress () + "?",
		    									"The x509 certificate signatures are:"   +
		    									"\nSHA1:  " + Utils.toHexString(sha1.digest()) +
		    									"\nMD5:  "  + Utils.toHexString(md5.digest())  + 
		    									"\nYou can ensure they are identical to the known signatures of the server certificate to prevent a man-in-the-middle attack.",
		    									signatureYes, signatureNo);
					} catch (NoSuchAlgorithmException e2) {
						e2.printStackTrace();
						showFatalMessageAndQuit("Could not generate SHA1 or MD5 signature of certificate. No SHA1/MD5 algorithm found.");
					} catch (CertificateEncodingException e) {
						e.printStackTrace();
						showFatalMessageAndQuit("Certificate encoding could not be generated.");
					}
	        	} else {
					// Compare saved with obtained certificate and quit if they don't match.
	        		try {
						if (!connection.getSshHostKey().equals(Base64.encodeToString(cert.getEncoded(), Base64.DEFAULT))) {
							showFatalMessageAndQuit("ERROR: The saved x509 certificate does not match the current server certificate! " +
									"This could be a man-in-the-middle attack. If you are aware of the key change, delete and recreate the connection.");
						} else {
							// TODO: In case we need to display information about the certificate, we can reconstruct it.
							//CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
							//ByteArrayInputStream in = new ByteArrayInputStream(Base64.decode(connection.getSshHostKey(), Base64.DEFAULT));
							//X509Certificate c = (X509Certificate)certFactory.generateCertificate(in);
				    	    //android.util.Log.e("  Subject ", c.getSubjectDN().toString());
				    	    //android.util.Log.e("   Issuer  ", c.getIssuerDN().toString());
							// The certificate matches, so we proceed.
	    	            	certificateAccepted = true;
						}
					} catch (CertificateEncodingException e) {
						e.printStackTrace();
						showFatalMessageAndQuit("Certificate encoding could not be generated.");
					}
	        	}
	            break;
	        }
	    }
	};

	// Used to set the contents of the clipboard.
	ClipboardManager clipboard;
	Timer clipboardMonitorTimer;
	ClipboardMonitor clipboardMonitor;
	public boolean serverJustCutText = false;
	
	private MouseScrollRunnable scrollRunnable;

	private Runnable setModes;
	
	// This variable indicates whether or not the user has accepted an untrusted
	// security certificate. Used to control progress while the dialog asking the user
	// to confirm the authenticity of a certificate is displayed.
	public boolean certificateAccepted = false;
	
	/*
	 * Position of the top left portion of the <i>visible</i> part of the screen, in
	 * full-frame coordinates
	 */
	int absoluteXPosition = 0, absoluteYPosition = 0;
	
	/*
	 * How much to shift coordinates over when converting from full to view coordinates.
	 */
	float shiftX = 0, shiftY = 0;

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

	/*
	 * Variable used for BB10 hacks.
	 */
	boolean bb10 = false;

	/**
	 * Constructor used by the inflation apparatus
	 * 
	 * @param context
	 */
	public VncCanvas(final Context context, AttributeSet attrs)
	{
		super(context, attrs);

		clipboard = (ClipboardManager)getContext().getSystemService(Context.CLIPBOARD_SERVICE);

		String s = android.os.Build.MODEL;
		if (s.contains("BlackBerry 10"))
			bb10 = true;

		decoder = new Decoder (this);
	}

	/**
	 * Create a view showing a VNC connection
	 * @param context Containing context (activity)
	 * @param bean Connection settings
	 * @param setModes Callback to run on UI thread after connection is set up
	 */
	void initializeVncCanvas(ConnectionBean bean, VncDatabase db, final Runnable setModes) {

		this.setModes = setModes;
		connection = bean;
		database = db;
		decoder.setColorModel(COLORMODEL.valueOf(bean.getColorModel()));

		// Startup the RFB thread with a nifty progress dialog
		final ProgressDialog pd = ProgressDialog.show(getContext(), 
													  "Connecting...", "Establishing handshake.\nPlease wait...", true,
													  true, new DialogInterface.OnCancelListener() {
			@Override
			public void onCancel(DialogInterface dialog) {
				closeConnection();
				handler.post(new Runnable() {
					public void run() {
						Utils.showFatalErrorMessage(getContext(), "VNC connection aborted!");
					}
				});
			}
		});
		
		// Make this dialog cancellable only upon hitting the Back button and not touching outside.
		pd.setCanceledOnTouchOutside(false);

		final Display display = pd.getWindow().getWindowManager().getDefaultDisplay();
		displayWidth  = display.getWidth();
		displayHeight = display.getHeight();
		Thread t = new Thread () {
			public void run() {
			    try {
			    	if (connection.getConnectionType() < 4) {
			    		connectAndAuthenticate(connection.getUserName(),connection.getPassword());
			    		rfbconn = rfb;
			    		doProtocolInitialisation(displayWidth, displayHeight);
			    		handler.post(new Runnable() {
			    			public void run() {
			    				pd.setMessage("Downloading first frame.\nPlease wait...");
			    			}
			    		});
			    		sendUnixAuth ();
			    		if (connection.getUseLocalCursor())
			    			initializeSoftCursor();
			    		processNormalProtocol(getContext(), pd, setModes);
			    	} else {
			    		cc = new CConn(VncCanvas.this, sock, null, false, connection);
			    		rfbconn = cc;
			    		initializeBitmap(displayWidth, displayHeight);
			    		processNormalProtocolSecure(getContext(), pd, setModes);
			    	}
				} catch (Throwable e) {
					if (maintainConnection) {
						Log.e(TAG, e.toString());
						e.printStackTrace();
						// Ensure we dismiss the progress dialog
						// before we fatal error finish
						if (pd.isShowing())
							pd.dismiss();
	
						if (e instanceof OutOfMemoryError) {
							System.gc();
							showFatalMessageAndQuit("A fatal error has occurred. The device appears to be out of free memory. " +
									"Try reconnecting, and if that fails, try killing and restarting the application. " +
									"As a last resort, you may try restarting your device.");
						} else {
							String error = "Connection failed!";
							if (e.getMessage() != null) {
								if (e.getMessage().indexOf("authentication") > -1 ||
										e.getMessage().indexOf("Unknown security result") > -1 ||
										e.getMessage().indexOf("password check failed") > -1) {
									error = "VNC authentication failed! Check VNC password (and user if applicable).";
								}
								error = error + "<br>" + e.getLocalizedMessage();
							}
							showFatalMessageAndQuit(error);
						}
					}
				}
			}
		};
		t.start();

		clipboardMonitor = new ClipboardMonitor(getContext(), this);
		clipboardMonitorTimer = new Timer ();
		clipboardMonitorTimer.schedule(clipboardMonitor, 0, 500);
	}
	
	/**
	 * Sends over the unix username and password if the appropriate option is enabled, in order to automatically
	 * authenticate to x11vnc when it asks for unix credentials (-unixpw).
	 */
	void sendUnixAuth () {
		// If the type of connection is ssh-tunneled and we are told to send the unix credentials, then do so.
		// Do not send the up event if this is a bb10 device, since the up-event hack in processLocalKeyEvent takes care of that...
		if (connection.getConnectionType() == 1 && connection.getAutoXUnixAuth()) {
			VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_UNKNOWN, new KeyEvent(SystemClock.uptimeMillis(),
					connection.getSshUser(), 0, 0));
			VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
			if (!bb10)
				VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
			VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_UNKNOWN, new KeyEvent(SystemClock.uptimeMillis(),
					connection.getSshPassword(), 0, 0));
			VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
			if (!bb10)
				VncCanvas.this.processLocalKeyEvent(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
		}
	}
	
	void showFatalMessageAndQuit (final String error) {
		closeConnection();
		handler.post(new Runnable() {
			public void run() {
				Utils.showFatalErrorMessage(getContext(), error);
			}
		});
	}

	// TODO: Move part of this over to RfbProto so there is no rfb.is + os crap.
	void connectAndAuthenticate(String us,String pw) throws Exception {
		Log.i(TAG, "Connecting to " + connection.getAddress() + ", port " + connection.getPort() + "...");
		// TODO: Switch from hard-coded numeric position to something better (at least an enumeration).
		boolean anontls = (connection.getConnectionType() == 3);

		if (connection.getConnectionType() == 1) {
			int localForwardedPort;
			sshConnection = new SSHConnection(connection);
			localForwardedPort = sshConnection.initializeSSHTunnel ();
			rfb = new RfbProto(decoder, "localhost", localForwardedPort);
		} else {
			rfb = new RfbProto(decoder, connection.getAddress(), connection.getPort());
		}

		if (LOCAL_LOGV) Log.v(TAG, "Connected to server");
	
		// <RepeaterMagic>
		if (connection.getUseRepeater() && connection.getRepeaterId() != null && connection.getRepeaterId().length()>0) {
			Log.i(TAG, "Negotiating repeater/proxy connection");
			byte[] protocolMsg = new byte[12];
			rfb.is.read(protocolMsg);
			byte[] buffer = new byte[250];
			System.arraycopy(connection.getRepeaterId().getBytes(), 0, buffer, 0, connection.getRepeaterId().length());
			rfb.os.write(buffer);
		}
		// </RepeaterMagic>

		rfb.readVersionMsg();
		Log.i(TAG, "RFB server supports protocol version " + rfb.serverMajor + "." + rfb.serverMinor);

		rfb.writeVersionMsg();
		Log.i(TAG, "Using RFB protocol version " + rfb.clientMajor + "." + rfb.clientMinor);

		int bitPref=0;
		if(connection.getUserName().length()>0)
		  bitPref|=1;
		Log.d("debug","bitPref="+bitPref);
		int secType = rfb.negotiateSecurity(bitPref, anontls);
		int authType;
		if (secType == RfbProto.SecTypeTight) {
			rfb.initCapabilities();
			rfb.setupTunneling();
			authType = rfb.negotiateAuthenticationTight();
		} else if (secType == RfbProto.SecTypeUltra34) {
			rfb.prepareDH();
			authType = RfbProto.AuthUltra;
		} else if (secType == RfbProto.SecTypeTLS) {
			rfb.performTLSHandshake ();
			authType = rfb.negotiateSecurity(bitPref, false);
		//} else if (secType == RfbProto.SecTypeVeNCrypt) {
		//	rfb.performTLSHandshake ();
		//	authType = rfb.selectVeNCryptSecurityType();
		} else {
			authType = secType;
		}

		switch (authType) {
		case RfbProto.AuthNone:
			Log.i(TAG, "No authentication needed");
			rfb.authenticateNone();
			break;
		case RfbProto.AuthVNC:
			Log.i(TAG, "VNC authentication needed");
			rfb.authenticateVNC(pw);
			break;
		case RfbProto.AuthUltra:
			rfb.authenticateDH(us,pw);
			break;
		default:
			throw new Exception("Unknown authentication scheme " + authType);
		}
	}

	void doProtocolInitialisation(int dx, int dy) throws IOException {
		rfb.writeClientInit();
		rfb.readServerInit();

		initializeBitmap (dx, dy);
		decoder.setPixelFormat(rfb);
	}

	void initializeBitmap (int dx, int dy) throws IOException {
		Log.i(TAG, "Desktop name is " + rfbconn.desktopName());
		Log.i(TAG, "Desktop size is " + rfbconn.framebufferWidth() + " x " + rfbconn.framebufferHeight());

		capacity = BCFactory.getInstance().getBCActivityManager().getMemoryClass(Utils.getActivityManager(getContext()));
		if (connection.getForceFull() == BitmapImplHint.AUTO)
		{
			if (rfbconn.framebufferWidth()* rfbconn.framebufferHeight() * FullBufferBitmapData.CAPACITY_MULTIPLIER <= capacity * 1024 * 1024)
				useFull = true;
		}
		else
			useFull = (connection.getForceFull() == BitmapImplHint.FULL);
		
		if (!useFull) {
			bitmapData=new LargeBitmapData(rfbconn, this, dx, dy, capacity);
			android.util.Log.i(TAG, "Using LargeBitmapData.");
		} else {
			//bitmapData=new FullBufferBitmapData(rfbconn, this, capacity);
			bitmapData=new CompactBitmapData(rfbconn, this);
			android.util.Log.i(TAG, "Using FullBufferBitmapData.");
		}
		
		decoder.setBitmapData(bitmapData);

		mouseX=rfbconn.framebufferWidth()/2;
		mouseY=rfbconn.framebufferHeight()/2;
	}

	public boolean isColorModel(COLORMODEL cm) {
		return (decoder.getColorModel() != null) && decoder.getColorModel().equals(cm);
	}
	
	public void setColorModel(COLORMODEL cm) {
		decoder.setColorModel(cm);
	}
	
	public void mouseFollowPan()
	{
		if (connection.getFollowPan() && scaling != null && scaling.isAbleToPan())
		{
			int scrollx = absoluteXPosition;
			int scrolly = absoluteYPosition;
			int width = getVisibleWidth();
			int height = getVisibleHeight();
			//Log.i(TAG,"scrollx " + scrollx + " scrolly " + scrolly + " mouseX " + mouseX +" Y " + mouseY + " w " + width + " h " + height);
			if (mouseX < scrollx || mouseX >= scrollx + width || mouseY < scrolly || mouseY >= scrolly + height)
			{
				//Log.i(TAG,"warp to " + scrollx+width/2 + "," + scrolly + height/2);
				warpMouse(scrollx + width/2, scrolly + height / 2);
			}
		}
	}

	public void updateFBSize () {
		try {
			bitmapData.frameBufferSizeChanged ();
		} catch (Throwable e) {
			// If we've run out of memory, try using an LBMD
			if (e instanceof OutOfMemoryError) {
				useFull = false;
				bitmapData = new LargeBitmapData(rfbconn, this, getWidth(), getHeight(), capacity);
				decoder.setBitmapData(bitmapData);
			}
		}
		handler.post(drawableSetter);
		handler.post(setModes);
		handler.post(desktopInfo);		
		bitmapData.syncScroll();
	}
	
	public void processNormalProtocolSecure (final Context context, final ProgressDialog pd, final Runnable setModes) throws Exception {
		try {
    		// Initialize the protocol before we dismiss the progress dialog and request for the right
    		// modes to be set.
    		for (int i = 0; i < 6; i++)
    			cc.processMsg();
    		
    		handler.post(new Runnable() {
    			public void run() {
    				pd.setMessage("Downloading first frame.\nPlease wait...");
    			}
    		});
    		
    		for (int i = 0; i < 3; i++)
    			cc.processMsg();
    		
			// Hide progress dialog
			if (pd.isShowing())
				pd.dismiss();

			cc.processProtocol();
		} catch (Exception e) {
			throw e;
		} finally {
			Log.v(TAG, "Closing VNC Connection");
			cc.close();
		}
	}

	public void showBeep () {
		handler.post( new Runnable() {
			public void run() { Toast.makeText( getContext(), "VNC Beep", Toast.LENGTH_SHORT); }
		});
	}

	public void doneWaiting () {
		bitmapData.doneWaiting();
	}
	
	public void syncScroll () {
		bitmapData.syncScroll();
	}

	public void writeFramebufferUpdateRequest (int x, int y, int w, int h, boolean incremental) throws IOException {
		bitmapData.prepareFullUpdateRequest(incremental);
		rfbconn.writeFramebufferUpdateRequest(x, y, w, h, incremental);
	}
	
	public void writeFullUpdateRequest (boolean incremental) {
		bitmapData.prepareFullUpdateRequest(incremental);
		rfbconn.writeFramebufferUpdateRequest(bitmapData.getXoffset(), bitmapData.getYoffset(),
											  bitmapData.bmWidth(),    bitmapData.bmHeight(), incremental);
	}
	
	public void processNormalProtocol(final Context context, ProgressDialog pd, final Runnable setModes) throws Exception {
		handler.post(drawableSetter);
		handler.post(setModes);
		handler.post(desktopInfo);

		// Hide progress dialog
		if (pd.isShowing())
			pd.dismiss();

		rfb.processProtocol(this, connection.getUseLocalCursor());
	}
	
	/**
	 * Set the device clipboard text with the string parameter.
	 * @param readServerCutText set the device clipboard to the text in this parameter.
	 */
	public void setClipboardText(String s) {
		if (s != null && s.length() > 0) {
			clipboard.setText(s);
		}
	}

	/**
	 * Computes the X and Y offset for converting coordinates from full-frame coordinates to view coordinates.
	 */
	public void computeShiftFromFullToView () {
		shiftX = (bitmapData.fbWidth()  - getWidth())  / 2;
		shiftY = (bitmapData.fbHeight() - getHeight()) / 2;
	}

	/**
	 * Apply scroll offset and scaling to convert touch-space coordinates to the corresponding
	 * point on the full frame.
	 * @param e MotionEvent with the original, touch space coordinates.  This event is altered in place.
	 * @return e -- The same event passed in, with the coordinates mapped
	 */
	MotionEvent changeTouchCoordinatesToFullFrame(MotionEvent e)
	{
		//Log.v(TAG, String.format("tap at %f,%f", e.getX(), e.getY()));
		float scale = getScale();
		
		// Adjust coordinates for Android notification bar.
		e.offsetLocation(0, -1f * getTop());
		e.setLocation(absoluteXPosition + e.getX() / scale, absoluteYPosition + e.getY() / scale);
		return e;
	}

	public void onDestroy() {
		Log.v(TAG, "Cleaning up resources");
		if ( bitmapData!=null) bitmapData.dispose();
		handler.removeCallbacksAndMessages(null);
		bitmapData = null;
		clipboardMonitorTimer.cancel();
		clipboardMonitorTimer.purge();
		clipboardMonitorTimer = null;
		clipboardMonitor = null;
	}
	
	/**
	 * Warp the mouse to x, y in the RFB coordinates
	 * 
	 * @param x
	 * @param y
	 */
	void warpMouse(int x, int y)
	{
		bitmapData.invalidateMousePosition();
		mouseX=x;
		mouseY=y;
		bitmapData.invalidateMousePosition();
		rfbconn.writePointerEvent(x, y, 0, MOUSE_BUTTON_NONE);
	}

	/*
	 * f(x,s) is a function that returns the coordinate in screen/scroll space corresponding
	 * to the coordinate x in full-frame space with scaling s.
	 * 
	 * This function returns the difference between f(x,s1) and f(x,s2)
	 * 
	 * f(x,s) = (x - i/2) * s + ((i - w)/2)) * s
	 *        = s (x - i/2 + i/2 + w/2)
	 *        = s (x + w/2)
	 * 
	 * 
	 * f(x,s) = (x - ((i - w)/2)) * s
	 * @param oldscaling
	 * @param scaling
	 * @param imageDim
	 * @param windowDim
	 * @param offset
	 * @return
	 */
	
	/**
	 * Change to Canvas's scroll position to match the absoluteXPosition
	 */
	void scrollToAbsolute()
	{
		float scale = getScale();
		scrollTo((int)((absoluteXPosition + ((float)getWidth()  - getImageWidth())  / 2 ) * scale),
				 (int)((absoluteYPosition + ((float)getHeight() - getImageHeight()) / 2 ) * scale));
	}

	/**
	 * Make sure mouse is visible on displayable part of screen
	 */
	void panToMouse()
	{
		boolean panX = true;
		boolean panY = true;

		// Don't pan in a certain direction if dimension scaled is already less 
		// than the dimension of the visible part of the screen.
		if (bitmapData.bmWidth() <= getVisibleWidth())
			panX = false;
		if (bitmapData.bmHeight() <= getVisibleHeight())
			panY = false;

		// We only pan if the current scaling is able to pan.
		if (scaling != null && ! scaling.isAbleToPan())
			return;

		int x = mouseX;
		int y = mouseY;
		boolean panned = false;
		int w = getVisibleWidth();
		int h = getVisibleHeight();
		int iw = getImageWidth();
		int ih = getImageHeight();
		int wthresh = 30;
		int hthresh = 30;

		int newX = absoluteXPosition;
		int newY = absoluteYPosition;

		if (x - absoluteXPosition >= w - wthresh)
		{
			newX = x - (w - wthresh);
			if (newX + w > iw)
				newX = iw - w;
		}
		else if (x < absoluteXPosition + wthresh)
		{
			newX = x - wthresh;
			if (newX < 0)
				newX = 0;
		}
		if ( panX && newX != absoluteXPosition ) {
			absoluteXPosition = newX;
			panned = true;
		}
		if (y - absoluteYPosition >= h - hthresh)
		{
			newY = y - (h - hthresh);
			if (newY + h > ih)
				newY = ih - h;
		}
		else if (y < absoluteYPosition + hthresh)
		{
			newY = y - hthresh;
			if (newY < 0)
				newY = 0;
		}
		if ( panY && newY != absoluteYPosition ) {
			absoluteYPosition = newY;
			panned = true;
		}
		if (panned)
		{
			scrollToAbsolute();
		}
	}
	
	/**
	 * Pan by a number of pixels (relative pan)
	 * @param dX
	 * @param dY
	 * @return True if the pan changed the view (did not move view out of bounds); false otherwise
	 */
	boolean pan(int dX, int dY) {

		// We only pan if the current scaling is able to pan.
		if (scaling != null && ! scaling.isAbleToPan())
			return false;
		
		double scale = getScale();
		
		double sX = (double)dX / scale;
		double sY = (double)dY / scale;
		
		if (absoluteXPosition + sX < 0)
			// dX = diff to 0
			sX = -absoluteXPosition;
		if (absoluteYPosition + sY < 0)
			sY = -absoluteYPosition;

		// Prevent panning right or below desktop image
		if (absoluteXPosition + getVisibleWidth() + sX > getImageWidth())
			sX = getImageWidth() - getVisibleWidth() - absoluteXPosition;
		if (absoluteYPosition + getVisibleHeight() + sY > getImageHeight())
			sY = getImageHeight() - getVisibleHeight() - absoluteYPosition;

		absoluteXPosition += sX;
		absoluteYPosition += sY;
		if (sX != 0.0 || sY != 0.0)
		{
			scrollToAbsolute();
			return true;
		}
		return false;
	}

	/* (non-Javadoc)
	 * @see android.view.View#onScrollChanged(int, int, int, int)
	 */
	@Override
	protected void onScrollChanged(int l, int t, int oldl, int oldt) {
		super.onScrollChanged(l, t, oldl, oldt);
		bitmapData.scrollChanged(absoluteXPosition, absoluteYPosition);
		mouseFollowPan();
	}

	/**
	 * This runnable sets the drawable (contained in bitmapData) for the VncCanvas (ImageView).
	 */
	private Runnable drawableSetter = new Runnable() {
		public void run() {
			if (bitmapData != null)
				bitmapData.setImageDrawable(VncCanvas.this);
			}
	};

	/**
	 * This runnable causes a toast with information about the current connection to be shown.
	 */
	private Runnable desktopInfo = new Runnable() {
		public void run() {
			showConnectionInfo();
		}
	};

	/**
	 * Causes a redraw of the bitmapData to happen at the indicated coordinates.
	 */
	public void reDraw(Rect r) { reDraw (r.left, r.top, r.width(), r.height()); }
	public void reDraw(int x, int y, int w, int h) {
		float scale = getScale();
		float shiftedX = x-shiftX;
		float shiftedY = y-shiftY;
		// Make the box slightly larger to avoid artifacts due to truncation errors.
		postInvalidate ((int)((shiftedX-1)*scale),   (int)((shiftedY-1)*scale),
						(int)((shiftedX+w+1)*scale), (int)((shiftedY+h+1)*scale));
	}
	
	/**
	 * Toggles on-screen Ctrl mask. Returns true if result is Ctrl enabled, false otherwise.
	 * @return true if on false otherwise.
	 */
	public boolean onScreenCtrlToggle()	{
		// If we find Ctrl on, turn it off. Otherwise, turn it on.
		if (onScreenMetaState == (onScreenMetaState | CTRL_MASK)) {
			onScreenMetaState = onScreenMetaState & ~CTRL_MASK;
			return false;
		}
		else {
			onScreenMetaState = onScreenMetaState | CTRL_MASK;
			return true;
		}
	}
	
	/**
	 * Turns off on-screen Ctrl.
	 */
	public void onScreenCtrlOff()	{
		onScreenMetaState = onScreenMetaState & ~CTRL_MASK;
	}
	
	/**
	 * Toggles on-screen Ctrl mask.  Returns true if result is Alt enabled, false otherwise.
	 * @return true if on false otherwise.
	 */
	public boolean onScreenAltToggle() {
		// If we find Alt on, turn it off. Otherwise, trurn it on.
		if (onScreenMetaState == (onScreenMetaState | ALT_MASK)) {
			onScreenMetaState = onScreenMetaState & ~ALT_MASK;
			return false;
		}
		else {
			onScreenMetaState = onScreenMetaState | ALT_MASK;
			return true;
		}
	}

	/**
	 * Turns off on-screen Alt.
	 */
	public void onScreenAltOff()	{
		onScreenMetaState = onScreenMetaState & ~ALT_MASK;
	}


	public void showConnectionInfo() {
		String msg = null;
		int idx = rfbconn.desktopName().indexOf("(");
		if (idx > 0) {
			// Breakup actual desktop name from IP addresses for improved
			// readability
			String dn = rfbconn.desktopName().substring(0, idx).trim();
			String ip = rfbconn.desktopName().substring(idx).trim();
			msg = dn + "\n" + ip;
		} else
			msg = rfbconn.desktopName();
		msg += "\n" + rfbconn.framebufferWidth() + "x" + rfbconn.framebufferHeight();
		String enc = rfbconn.getEncoding();
		// Encoding might not be set when we display this message
		if (decoder.getColorModel() != null) {
			if (enc != null && !enc.equals(""))
				msg += ", " + rfbconn.getEncoding() + " encoding, " + decoder.getColorModel().toString();
			else 
				msg += ", " + decoder.getColorModel().toString();
		}
		Toast.makeText(getContext(), msg, Toast.LENGTH_SHORT).show();
	}



    // Useful shortcuts for modifier masks.

    public final static int CTRL_MASK  = KeyEvent.META_SYM_ON;
    public final static int SHIFT_MASK = KeyEvent.META_SHIFT_ON;
    public final static int ALT_MASK   = KeyEvent.META_ALT_ON;
    public final static int META_MASK  = 0;
    
	private static final int MOUSE_BUTTON_NONE = 0;
	static final int MOUSE_BUTTON_LEFT = 1;
	static final int MOUSE_BUTTON_MIDDLE = 2;
	static final int MOUSE_BUTTON_RIGHT = 4;
	static final int MOUSE_BUTTON_SCROLL_UP = 8;
	static final int MOUSE_BUTTON_SCROLL_DOWN = 16;
	static final int MOUSE_BUTTON_SCROLL_LEFT = 32;
	static final int MOUSE_BUTTON_SCROLL_RIGHT = 64;
	
	/**
	 * Current state of "mouse" buttons
	 * Alt meta means use second mouse button
	 * 0 = none
	 * 1 = default button
	 * 2 = second button
	 */
	private int pointerMask = MOUSE_BUTTON_NONE;
	
	/**
	 * Convert a motion event to a format suitable for sending over the wire
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.  cameraButton flag is interpreted as second mouse
	 * button
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @return true if event was actually sent
	 */
	public boolean processPointerEvent(MotionEvent evt, boolean downEvent)
	{
		return processPointerEvent(evt, downEvent, cameraButtonDown);
	}

	/**
	 *  Overloaded processPointerEvent method which supports mouse scroll button.
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @param useRightButton If true, event is interpreted as happening with right mouse button
	 * @param useMiddleButton If true, event is interpreted as click happening with middle mouse button
	 * @param useScrollButton If true, event is interpreted as click happening with mouse scroll button
	 * @param direction Indicates the direction of the scroll event: 0 for up, 1 for down, 2 for left, 3 for right.
	 * @return true if event was actually sent
	 */
	public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction) {
		return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getActionMasked(), 
									evt.getMetaState(), downEvent, useRightButton, useMiddleButton, useScrollButton, direction);
	}
	
	/**
	 *  Overloaded processPointerEvent method which supports middle mouse button.
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @param useRightButton If true, event is interpreted as happening with right mouse button
	 * @param useMiddleButton If true, event is interpreted as click happening with middle mouse button
	 * @return true if event was actually sent
	 */
	public boolean processPointerEvent(MotionEvent evt, boolean downEvent, 
                                       boolean useRightButton, boolean useMiddleButton) {
		return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getActionMasked(), 
									evt.getMetaState(), downEvent, useRightButton, useMiddleButton, false, -1);
	}

	/**
	 * Convert a motion event to a format suitable for sending over the wire
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @param useRightButton If true, event is interpreted as happening with right mouse button
	 * @return true if event was actually sent
	 */
	public boolean processPointerEvent(MotionEvent evt, boolean downEvent, boolean useRightButton) {
		return processPointerEvent((int)evt.getX(),(int)evt.getY(), evt.getAction(), 
									evt.getMetaState(), downEvent, useRightButton, false, false, -1);
	}

	/**
	 *  Overloaded processPointerEvent method which supports right mouse button.
	 * @param evt motion event; x and y must already have been converted from screen coordinates
	 * to remote frame buffer coordinates.
	 * @param downEvent True if "mouse button" (touch or trackball button) is down when this happens
	 * @param useRightButton If true, event is interpreted as happening with right mouse button
	 * @return true if event was actually sent
	 */
	boolean processPointerEvent(int x, int y, int action, int modifiers, boolean mouseIsDown, boolean useRightButton) {
		return processPointerEvent(x, y, action, modifiers, mouseIsDown, useRightButton, false, false, -1);
	}
	
	boolean processPointerEvent(int x, int y, int action, int modifiers, 
			                    boolean mouseIsDown, boolean useRightButton, boolean useMiddleButton, boolean useScrollButton, int direction) {
		
		// If none of the conditions below are satisfied, clear the pointer mask.
		pointerMask = 0;
		
		if (rfbconn != null) {
			if (mouseIsDown && useRightButton) {
		    	//Log.i(TAG,"Right mouse button mask set");
		        pointerMask = MOUSE_BUTTON_RIGHT;
			} else if (mouseIsDown && useMiddleButton) {
			    //Log.i(TAG,"Middle mouse button mask set");
			    pointerMask = MOUSE_BUTTON_MIDDLE;
			} else if (mouseIsDown && (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_MOVE)) {
			    //Log.i(TAG,"Left mouse button mask set");
		        pointerMask = MOUSE_BUTTON_LEFT;
			} else if (!mouseIsDown && useScrollButton) {
				//Log.d(TAG, "Sending a Mouse Scroll event: " + direction);
				if      ( direction == 0 )
					pointerMask = MOUSE_BUTTON_SCROLL_UP;
				else if ( direction == 1 )
					pointerMask = MOUSE_BUTTON_SCROLL_DOWN;
				if      ( direction == 2 )
					pointerMask = MOUSE_BUTTON_SCROLL_LEFT;
				else if ( direction == 3 )
					pointerMask = MOUSE_BUTTON_SCROLL_RIGHT;
		    } else {
			    //Log.i(TAG,"Mouse button mask cleared");
		    	pointerMask = 0;
		    }
						
		    bitmapData.invalidateMousePosition();
		    mouseX = x;
		    mouseY = y;
		    if ( mouseX < 0) mouseX=0;
		    else if ( mouseX >= bitmapData.fbWidth())  mouseX = bitmapData.fbWidth() - 1;
		    if ( mouseY < 0) mouseY=0;
		    else if ( mouseY >= bitmapData.fbHeight()) mouseY = bitmapData.fbHeight() - 1;
		    bitmapData.invalidateMousePosition();

    		rfbconn.writePointerEvent(mouseX, mouseY, modifiers|onScreenMetaState, pointerMask);
		    panToMouse();
			return true;
		}
		return false;		
	}
	
	/**
	 * Moves the scroll while the volume key is held down
	 * 
	 * @author Michael A. MacDonald
	 */
	class MouseScrollRunnable implements Runnable
	{
		int delay = 100;
		
		int scrollButton = 0;
		
		/* (non-Javadoc)
		 * @see java.lang.Runnable#run()
		 */
		@Override
		public void run() {
			if (rfbconn != null && rfbconn.isInNormalProtocol()) {
				rfbconn.writePointerEvent(mouseX, mouseY, 0, scrollButton);
				rfbconn.writePointerEvent(mouseX, mouseY, 0, 0);				
				handler.postDelayed(this, delay);
			}
		}		
	}

	
    //
    // softCursorMove(). Moves soft cursor into a particular location.
    //

    synchronized void softCursorMove(int x, int y) {
    	if (bitmapData.isNotInitSoftCursor()) {
    		initializeSoftCursor();
    	}
    	
    	if (!inScrolling) {
    		mouseX = x;
    		mouseY = y;
	    	Rect prevCursorRect = new Rect(bitmapData.getCursorRect());
	    	// Move the cursor.
	    	bitmapData.moveCursorRect(x, y);
	    	// Show the cursor.
	    	prevCursorRect.union(bitmapData.getCursorRect());
	    	reDraw(prevCursorRect);
    	}
    }
    
    void initializeSoftCursor () {
		Bitmap bm = BitmapFactory.decodeResource(getResources(), R.drawable.cursor);
		int w = bm.getWidth();
		int h = bm.getHeight();
		int [] tempPixels = new int[w*h];
		bm.getPixels(tempPixels, 0, w, 0, 0, w, h);
    	// Set cursor rectangle as well.
    	bitmapData.setCursorRect(mouseX, mouseY, w, h, 0, 0);
    	// Set softCursor to whatever the resource is.
		bitmapData.setSoftCursor (tempPixels);
    }


	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt) {
		if (rfbconn != null && rfbconn.isInNormalProtocol()) {
			boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
						   (evt.getAction() == KeyEvent.ACTION_MULTIPLE);
			
			int metaState = 0, numchars = 1;
			int keyboardMetaState = evt.getMetaState();

		    // Add shift to metaState if necessary.
			if ((keyboardMetaState & KeyEvent.META_SHIFT_MASK) != 0)
				metaState |= SHIFT_MASK;
			
			// If the keyboardMetaState contains any hint of CTRL, add CTRL_MASK to metaState
			if ((keyboardMetaState & 0x7000)!=0)
				metaState |= CTRL_MASK;
			// If the keyboardMetaState contains left ALT, add ALT_MASK to metaState.
		    // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			if ((keyboardMetaState & (KeyEvent.META_ALT_RIGHT_ON|0x00030000)) !=0 )
				metaState |= ALT_MASK;
			
			if (keyCode == KeyEvent.KEYCODE_MENU)
				return true; 			              // Ignore menu key
			if (keyCode == KeyEvent.KEYCODE_CAMERA) {
				cameraButtonDown = down;
				pointerMask = MOUSE_BUTTON_RIGHT;
				rfbconn.writePointerEvent(mouseX, mouseY, metaState|onScreenMetaState, pointerMask);
				return true;
			} else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
				int mouseChange = keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ? MOUSE_BUTTON_SCROLL_DOWN : MOUSE_BUTTON_SCROLL_UP;
				if (evt.getAction() == KeyEvent.ACTION_DOWN) {
					// If not auto-repeat
					if (scrollRunnable.scrollButton != mouseChange) {
						pointerMask |= mouseChange;
						scrollRunnable.scrollButton = mouseChange;
						handler.postDelayed(scrollRunnable,200);
					}
				} else {
					handler.removeCallbacks(scrollRunnable);
					scrollRunnable.scrollButton = 0;
					pointerMask &= ~mouseChange;
				}
				rfbconn.writePointerEvent(mouseX, mouseY, metaState|onScreenMetaState, pointerMask);
				return true;
			}

		   int key = 0, keysym = 0;
		   
		   if (!down) {
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               key = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   onScreenMetaState &= ~CTRL_MASK;
				   break;
			   }

			   switch(keyCode) {
			      case KeyEvent.KEYCODE_DPAD_CENTER:  onScreenMetaState &= ~CTRL_MASK; break;
			      // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			   	  case KeyEvent.KEYCODE_ALT_RIGHT:    onScreenMetaState &= ~ALT_MASK; break;
			   }
		   }
	   
		   switch(keyCode) {
		   	  case KeyEvent.KEYCODE_BACK:         keysym = 0xff1b; break;
		      case KeyEvent.KEYCODE_DPAD_LEFT:    keysym = 0xff51; break;
		   	  case KeyEvent.KEYCODE_DPAD_UP:      keysym = 0xff52; break;
		   	  case KeyEvent.KEYCODE_DPAD_RIGHT:   keysym = 0xff53; break;
		   	  case KeyEvent.KEYCODE_DPAD_DOWN:    keysym = 0xff54; break;
		      case KeyEvent.KEYCODE_DEL: 		  keysym = 0xff08; break;
		      case KeyEvent.KEYCODE_ENTER:        keysym = 0xff0d; break;
		   	  case KeyEvent.KEYCODE_TAB:          keysym = 0xff09; break;
		   	  case 92 /* KEYCODE_PAGE_UP */:      keysym = 0xff55; break;
		   	  case 93 /* KEYCODE_PAGE_DOWN */:    keysym = 0xff56; break;
		   	  case 111 /* KEYCODE_ESCAPE */:      keysym = 0xff1b; break;
		   	  case 112 /* KEYCODE_FORWARD_DEL */: keysym = 0xffff; break;
		   	  case 113 /* KEYCODE_CTRL_LEFT */:   keysym = 0xffe3; break;
		   	  case 114 /* KEYCODE_CTRL_RIGHT */:  keysym = 0xffe4; break;
		   	  case 115 /* KEYCODE_CAPS_LOCK */:   keysym = 0xffe5; break;
		   	  case 116 /* KEYCODE_SCROLL_LOCK */: keysym = 0xff14; break;
		   	  case 120 /* KEYCODE_SYSRQ */:       keysym = 0xff61; break;
		   	  case 121 /* KEYCODE_BREAK */:       keysym = 0xff6b; break;
		   	  case 122 /* KEYCODE_MOVE_HOME */:   keysym = 0xff50; break;
		   	  case 123 /* KEYCODE_MOVE_END */:    keysym = 0xff57; break;
		   	  case 124 /* KEYCODE_INSERT */:      keysym = 0xff63; break;
		   	  case 131 /* KEYCODE_F1 */:          keysym = 0xffbe; break;
		   	  case 132 /* KEYCODE_F2 */:          keysym = 0xffbf; break;
		   	  case 133 /* KEYCODE_F3 */:          keysym = 0xffc0; break;
		   	  case 134 /* KEYCODE_F4 */:          keysym = 0xffc1; break;
		   	  case 135 /* KEYCODE_F5 */:          keysym = 0xffc2; break;
		   	  case 136 /* KEYCODE_F6 */:          keysym = 0xffc3; break;
		   	  case 137 /* KEYCODE_F7 */:          keysym = 0xffc4; break;
		   	  case 138 /* KEYCODE_F8 */:          keysym = 0xffc5; break;
		   	  case 139 /* KEYCODE_F9 */:          keysym = 0xffc6; break;
		   	  case 140 /* KEYCODE_F10 */:         keysym = 0xffc7; break;
		   	  case 141 /* KEYCODE_F11 */:         keysym = 0xffc8; break;
		   	  case 142 /* KEYCODE_F12 */:         keysym = 0xffc9; break;
		   	  case 143 /* KEYCODE_NUM_LOCK */:    keysym = 0xff7f; break;
		   	  case 0   /* KEYCODE_UNKNOWN */:
		   		  if (evt.getCharacters() != null) {
		   			  key = evt.getCharacters().charAt(0);
		   			  keysym = UnicodeToKeysym.translate(key);
		   			  numchars = evt.getCharacters().length();
		   		  }
	    		  break;
		      default: 							  
		    	  // Modifier handling is a bit tricky. Alt and Ctrl should be passed
		    	  // through to the VNC server so that they get handled there, but strip
		    	  // them from the character before retrieving the Unicode char from it.
		    	  // Don't clear Shift, we still want uppercase characters.
		    	  int vncEventMask = ( 0x7000 );   // KeyEvent.META_CTRL_MASK
		    	  if ((metaState & ALT_MASK) != 0)
		    		  vncEventMask |= 0x0032;      // KeyEvent.META_ALT_MASK
		    	  KeyEvent copy = new KeyEvent(evt.getDownTime(), evt.getEventTime(), evt.getAction(),
		    			  						evt.getKeyCode(),  evt.getRepeatCount(),
		    				  					keyboardMetaState & ~vncEventMask, evt.getDeviceId(), evt.getScanCode());
		    	  key = copy.getUnicodeChar();
	    		  keysym = UnicodeToKeysym.translate(key);
		    	  break;
		   }

		   if (down) {
			   // Look for standard scan-codes from external keyboards
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               keysym = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   onScreenMetaState |= CTRL_MASK;
				   break;
			   case SCAN_F1:				keysym = 0xffbe;				break;
			   case SCAN_F2:				keysym = 0xffbf;				break;
			   case SCAN_F3:				keysym = 0xffc0;				break;
			   case SCAN_F4:				keysym = 0xffc1;				break;
			   case SCAN_F5:				keysym = 0xffc2;				break;
			   case SCAN_F6:				keysym = 0xffc3;				break;
			   case SCAN_F7:				keysym = 0xffc4;				break;
			   case SCAN_F8:				keysym = 0xffc5;				break;
			   case SCAN_F9:				keysym = 0xffc6;				break;
			   case SCAN_F10:				keysym = 0xffc7;				break;
			   }
			   
			   switch(keyCode) {
			      case KeyEvent.KEYCODE_DPAD_CENTER:  onScreenMetaState |= CTRL_MASK; break;
			      // Leaving KeyEvent.KEYCODE_ALT_LEFT for symbol input on hardware keyboards.
			   	  case KeyEvent.KEYCODE_ALT_RIGHT:    onScreenMetaState |= ALT_MASK; break;
			   }
		   }

		   try {
			   if (afterMenu)
			   {
				   afterMenu = false;
				   if (!down && keysym != lastKeyDown)
					   return true;
			   }
			   if (down)
				   lastKeyDown = keysym;

			   if (numchars == 1) {
			       //Log.e(TAG,"action down? = " + down + " key = " + key + " keysym = " + keysym + " onscreen metastate = " + onScreenMetaState + " keyboard metastate = " + keyboardMetaState + " RFB metastate = " + metaState + " keycode = " + keyCode + " unicode = " + evt.getUnicodeChar());
				   rfbconn.writeKeyEvent(keysym, (onScreenMetaState|metaState), down);

				   // UGLY HACK for BB10 devices which never send the up-event
				   // for backspace and enter... so we send it instead. Remove as soon as possible!
				   if (bb10 && (keyCode == KeyEvent.KEYCODE_DEL || keyCode == KeyEvent.KEYCODE_ENTER))
					   rfbconn.writeKeyEvent(keysym, (onScreenMetaState | metaState), false);

			   } else if (numchars > 1) {
				   for (int i = 0; i < numchars; i++) {
					   key = evt.getCharacters().charAt(i);
					   //Log.e(TAG,"action down? = " + down + " key = " + key + " keysym = " + keysym + " onscreen metastate = " + onScreenMetaState + " keyboard metastate = " + keyboardMetaState + " RFB metastate = " + metaState + " keycode = " + keyCode + " unicode = " + evt.getUnicodeChar());
					   keysym = UnicodeToKeysym.translate(key);
					   rfbconn.writeKeyEvent(keysym, (onScreenMetaState|metaState), down);
				   }
			   }
		   } catch (Exception e) {
			   e.printStackTrace();
		   }
		   return true;
		}
		return false;
	}

	public void closeConnection() {
		maintainConnection = false;
		// Tell the server to release any meta keys.
		onScreenMetaState = 0;
		processLocalKeyEvent(0, new KeyEvent(KeyEvent.ACTION_UP, 0));

		// Close the rfb connection.
		if (rfbconn != null) {
			rfbconn.close();
			rfbconn = null;
			rfb = null;
			cc = null;
		}
		// Close the SSH tunnel.
		if (sshConnection != null) {
			sshConnection.terminateSSHTunnel();
			sshConnection = null;
		}
	}
	
	void sendMetaKey(MetaKeyBean meta)
	{
		if (meta.isMouseClick())
		{
			rfbconn.writePointerEvent(mouseX, mouseY, meta.getMetaFlags()|onScreenMetaState, meta.getMouseButtons());
			rfbconn.writePointerEvent(mouseX, mouseY, meta.getMetaFlags()|onScreenMetaState, 0);
		}
		else {
			rfbconn.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState, true);
			rfbconn.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState, false);
		}
	}
	
	float getScale()
	{
		if (scaling == null)
			return 1;
		return scaling.getScale();
	}
	
	public int getVisibleWidth() {
		return (int)((double)getWidth() / getScale() + 0.5);
	}

	public void setVisibleHeight(int newHeight){
		visibleHeight = newHeight;
	}
	
	public int getVisibleHeight() {
		if (visibleHeight > 0)
			return (int)((double)visibleHeight / getScale() + 0.5);
		else
			return (int)((double)getHeight() / getScale() + 0.5);
	}

	public int getImageWidth() {
		return bitmapData.framebufferwidth;
	}

	public int getImageHeight() {
		return bitmapData.framebufferheight;
	}
	
	public int getCenteredXOffset() {
		return (bitmapData.framebufferwidth - getWidth()) / 2;
	}

	public int getCenteredYOffset() {
		return (bitmapData.framebufferheight - getHeight()) / 2;
	}
}