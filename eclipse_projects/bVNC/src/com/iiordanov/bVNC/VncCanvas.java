//  Copyright (C) 2012 Iordan Iordanov
//  Copyright (C) 2010 Michael A. MacDonald
//  Copyright (C) 2004 Horizon Wimba.  All Rights Reserved.
//  Copyright (C) 2001-2003 HorizonLive.com, Inc.  All Rights Reserved.
//  Copyright (C) 2001,2002 Constantin Kaplinsky.  All Rights Reserved.
//  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
//  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
//
//  This is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this software; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//

//
// VncCanvas is a subclass of android.view.SurfaceView which draws a VNC
// desktop on it.
//

package com.iiordanov.bVNC;

import java.io.IOException;
import java.util.zip.Inflater;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Paint.Style;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Display;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.ImageView;
import android.widget.Toast;
import android.graphics.BitmapFactory;
import android.graphics.Bitmap;

import com.iiordanov.android.bc.BCFactory;

public class VncCanvas extends ImageView {
	private final static String TAG = "VncCanvas";
	private final static boolean LOCAL_LOGV = true;
	
	// Variable holding the state of the Menu toggle buttons for meta keys (Ctrl, Alt...)
	private static int onScreenMetaState = 0;
	
	AbstractScaling scaling;
	
	// Available to activity
	int mouseX, mouseY;
	
	// Connection parameters
	ConnectionBean connection;
	private SSHConnection sshConnection;

	// Runtime control flags
	private boolean maintainConnection = true;
	private boolean showDesktopInfo = true;
	private boolean repaintsEnabled = true;

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
	private final static int SCAN_HOME = 102;
	private final static int SCAN_END = 107;
	
	/**
	 * Use camera button as meta key for right mouse button
	 */
	boolean cameraButtonDown = false;
	
	// Keep track when a seeming key press was the result of a menu shortcut
	int lastKeyDown;
	boolean afterMenu;

	// Color Model settings
	private COLORMODEL pendingColorModel = COLORMODEL.C24bit;
	private COLORMODEL colorModel = null;
	private int bytesPerPixel = 0;
	private int[] colorPalette = null;

	// VNC protocol connection
	public RfbProto rfb;

	// Internal bitmap data
	AbstractBitmapData bitmapData;
	boolean useFull = false;
	public Handler handler = new Handler();

	// VNC Encoding parameters
	private int preferredEncoding = -1;

	// Unimplemented VNC encoding parameters
	private boolean requestCursorUpdates = false;
	private boolean ignoreCursorUpdates = true;

	// Tight encoding parameters
	private int compressLevel = 9;
	private int jpegQuality = 7;

	// Used to determine if encoding update is necessary
	private int[] encodingsSaved = new int[20];
	private int nEncodingsSaved = 0;

	// Tight encoder's data.
	Inflater[] tightInflaters;
	private Paint handleTightRectPaint;
	byte[] solidColorBuf;
	byte[] tightPalette8;
	int[]  tightPalette24;
	byte[] colorBuf;
	byte[] uncompDataBuf;
	byte[] zlibData;
	byte[] inflBuf;

	// ZRLE encoder's data.
	private byte[] zrleBuf;
	private int[] zrleTilePixels;
	private ZlibInStream zrleInStream;

	// Zlib encoder's data.
	private byte[] zlibBuf;
	private Inflater zlibInflater;
	private MouseScrollRunnable scrollRunnable;
	
	private Paint handleRREPaint;
	
	private XKeySymCoverter xKeySymConv;
	
	/**
	 * Position of the top left portion of the <i>visible</i> part of the screen, in
	 * full-frame coordinates
	 */
	int absoluteXPosition = 0, absoluteYPosition = 0;

	/**
	 * This variable holds the height of the visible rectangle of the screen. It is used to keep track
	 * of how much of the screen is hidden by the soft keyboard if any.
	 */
	int visibleHeight = -1;
	
	/**
	 * Constructor used by the inflation apparatus
	 * @param context
	 */
	public VncCanvas(final Context context, AttributeSet attrs)
	{
		super(context, attrs);
		scrollRunnable = new MouseScrollRunnable();
		handleRREPaint = new Paint();
		handleRREPaint.setStyle(Style.FILL);
	    tightInflaters = new Inflater[4];
	    solidColorBuf = new byte[3];
		tightPalette8  = new byte[2];
		tightPalette24 = new int[256];
		handleTightRectPaint = new Paint();
		handleTightRectPaint.setStyle(Style.FILL);
		colorBuf = new byte[768];
		uncompDataBuf = new byte[RfbProto.TightMinToCompress*3];
		zlibData = new byte[4096];
		inflBuf = new byte[8192];
		xKeySymConv = new XKeySymCoverter();
	}
	
	/**
	 * Create a view showing a VNC connection
	 * @param context Containing context (activity)
	 * @param bean Connection settings
	 * @param setModes Callback to run on UI thread after connection is set up
	 */
	void initializeVncCanvas(ConnectionBean bean, final Runnable setModes) {

		connection = bean;
		this.pendingColorModel = COLORMODEL.valueOf(bean.getColorModel());

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
		Thread t = new Thread() {
			public void run() {
				try {
					connectAndAuthenticate(connection.getUserName(),connection.getPassword());
					doProtocolInitialisation(display.getWidth(), display.getHeight());
					handler.post(new Runnable() {
						public void run() {
							pd.setMessage("Downloading first frame.\nPlease wait...");
						}
					});
					processNormalProtocol(getContext(), pd, setModes);
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
									                "Try restarting the application and failing that, restart your device.");
						} else {
							String error = "Connection failed!";
							if (e.getMessage() != null) {
								if (e.getMessage().indexOf("authentication") > -1) {
									error = "VNC authentication failed! Check VNC password.";
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
	}
	
	void showFatalMessageAndQuit (final String error) {
		closeConnection();
		handler.post(new Runnable() {
			public void run() {
				Utils.showFatalErrorMessage(getContext(), error);
			}
		});
	}

	void connectAndAuthenticate(String us,String pw) throws Exception {
		Log.i(TAG, "Connecting to " + connection.getAddress() + ", port " + connection.getPort() + "...");

		// TODO: Switch from hard-coded numeric position to something better (at least an enumeration).
		if (connection.getConnectionType() == 1) {
			int localForwardedPort;
			sshConnection = new SSHConnection(connection.getSshServer(), connection.getSshPort());
		
			// Attempt to connect.
			if (!sshConnection.connect(connection.getSshUser()))
				throw new Exception("Failed to connect to SSH server. Please check network connection status, and SSH Server address and port.");
			
			// Verify host key against saved one.
			if (!sshConnection.verifyHostKey(connection.getSshHostKey()))
				throw new Exception("ERROR! The server host key has changed. If this is intentional, " +
									"please delete and recreate the connection. Otherwise, this may be " +
									"a man in the middle attack. Not continuing.");
			
			// Authenticate and set up port forwarding.
			if (sshConnection.canAuthWithPass(connection.getSshUser())) {
				if (sshConnection.authenticate(connection.getSshUser(), connection.getSshPassword())) {
					localForwardedPort = sshConnection.createPortForward(connection.getPort(),
																		 connection.getAddress(), connection.getPort());
					
					// If we got back a negative number, port forwarding failed.
					if (localForwardedPort < 0) {
						throw new Exception("Could not set up the port forwarding for tunneling VNC traffic over SSH." +
								"Please ensure your SSH server is configured to allow port forwarding and try again.");
					}
					
					// TODO: This is a proof of concept for remote command execution.
					//if (!sshConnection.execRemoteCommand("/usr/bin/x11vnc -N -forever -auth guess -localhost -display :0 1>/dev/null 2>/dev/null", 5000))
					//	throw new Exception("Could not execute remote command.");
				} else {
					throw new Exception("Failed to authenticate to SSH server. Please check your username and password.");
				}
			} else {
				throw new Exception("Remote server " + connection.getAddress() + " supports neither \"password\" nor " +
									"\"keyboard-interactive\" auth methods. Please configure it to allow at least one " +
									"of the two methods and try again.");
			}
			rfb = new RfbProto("localhost", localForwardedPort);
		} else {
			rfb = new RfbProto(connection.getAddress(), connection.getPort());
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
		int secType = rfb.negotiateSecurity(bitPref);
		int authType;
		if (secType == RfbProto.SecTypeTight) {
			rfb.initCapabilities();
			rfb.setupTunneling();
			authType = rfb.negotiateAuthenticationTight();
		} else if (secType == RfbProto.SecTypeUltra34) {
			rfb.prepareDH();
			authType = RfbProto.AuthUltra;
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

		Log.i(TAG, "Desktop name is " + rfb.desktopName);
		Log.i(TAG, "Desktop size is " + rfb.framebufferWidth + " x " + rfb.framebufferHeight);

		int capacity = BCFactory.getInstance().getBCActivityManager().getMemoryClass(Utils.getActivityManager(getContext()));
		if (connection.getForceFull() == BitmapImplHint.AUTO)
		{
			if (rfb.framebufferWidth * rfb.framebufferHeight * FullBufferBitmapData.CAPACITY_MULTIPLIER <= capacity * 1024 * 1024)
				useFull = true;
		}
		else
			useFull = (connection.getForceFull() == BitmapImplHint.FULL);

		if (! useFull)
			bitmapData=new LargeBitmapData(rfb,this,dx,dy,capacity);
		else
			bitmapData=new FullBufferBitmapData(rfb,this, capacity);

		mouseX=rfb.framebufferWidth/2;
		mouseY=rfb.framebufferHeight/2;

		setPixelFormat();
	}

	private void setPixelFormat() throws IOException {
		pendingColorModel.setPixelFormat(rfb);
		bytesPerPixel = pendingColorModel.bpp();
		colorPalette = pendingColorModel.palette();
		colorModel = pendingColorModel;
		pendingColorModel = null;
	}

	public void setColorModel(COLORMODEL cm) {
		// Only update if color model changes
		if (colorModel == null || !colorModel.equals(cm))
			pendingColorModel = cm;
	}

	public boolean isColorModel(COLORMODEL cm) {
		return (colorModel != null) && colorModel.equals(cm);
	}
	
	private void mouseFollowPan()
	{
		if (connection.getFollowPan() && scaling.isAbleToPan())
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

	public void processNormalProtocol(final Context context, ProgressDialog pd, final Runnable setModes) throws Exception {
		try {
			setEncodings(true);
			bitmapData.writeFullUpdateRequest(false);

			handler.post(setModes);
			//
			// main dispatch loop
			//
			while (maintainConnection) {
				bitmapData.syncScroll();
				// Read message type from the server.
				int msgType = rfb.readServerMessageType();
				bitmapData.doneWaiting();
				// Process the message depending on its type.
				switch (msgType) {
				case RfbProto.FramebufferUpdate:
					rfb.readFramebufferUpdate();

					for (int i = 0; i < rfb.updateNRects; i++) {
						rfb.readFramebufferUpdateRectHdr();
						int rx = rfb.updateRectX, ry = rfb.updateRectY;
						int rw = rfb.updateRectW, rh = rfb.updateRectH;

						if (rfb.updateRectEncoding == RfbProto.EncodingLastRect) {
							//Log.v(TAG, "rfb.EncodingLastRect");
							break;
						}

						if (rfb.updateRectEncoding == RfbProto.EncodingNewFBSize) {
							rfb.setFramebufferSize(rw, rh);
							// - updateFramebufferSize();
							//Log.v(TAG, "rfb.EncodingNewFBSize");
							break;
						}

						if (rfb.updateRectEncoding == RfbProto.EncodingXCursor || rfb.updateRectEncoding == RfbProto.EncodingRichCursor) {
							// - handleCursorShapeUpdate(rfb.updateRectEncoding,
							// rx,
							// ry, rw, rh);
							//Log.v(TAG, "rfb.EncodingCursor");
							continue;

						}

						if (rfb.updateRectEncoding == RfbProto.EncodingPointerPos) {
							// This never actually happens
							mouseX=rx;
							mouseY=ry;
							//Log.v(TAG, "rfb.EncodingPointerPos");
							continue;
						}

						rfb.startTiming();

						switch (rfb.updateRectEncoding) {
						case RfbProto.EncodingRaw:
							handleRawRect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingCopyRect:
							handleCopyRect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingRRE:
							handleRRERect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingCoRRE:
							handleCoRRERect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingHextile:
							handleHextileRect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingZRLE:
							handleZRLERect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingZlib:
							handleZlibRect(rx, ry, rw, rh);
							break;
						case RfbProto.EncodingTight:
							handleTightRect(rx, ry, rw, rh);
							break;
						default:
							Log.e(TAG, "Unknown RFB rectangle encoding " + rfb.updateRectEncoding + " (0x" + Integer.toHexString(rfb.updateRectEncoding) + ")");
						}

						rfb.stopTiming();

						// Hide progress dialog
						if (pd.isShowing())
							pd.dismiss();
					}

					boolean fullUpdateNeeded = false;

					if (pendingColorModel != null) {
						setPixelFormat();
						fullUpdateNeeded = true;
					}

					setEncodings(true);
					bitmapData.writeFullUpdateRequest(!fullUpdateNeeded);

					break;

				case RfbProto.SetColourMapEntries:
					throw new Exception("Can't handle SetColourMapEntries message");

				case RfbProto.Bell:
					handler.post( new Runnable() {
						public void run() { Toast.makeText( context, "VNC Beep", Toast.LENGTH_SHORT); }
					});
					break;

				case RfbProto.ServerCutText:
					String s = rfb.readServerCutText();
					if (s != null && s.length() > 0) {
						// TODO implement cut & paste
					}
					break;

				case RfbProto.TextChat:
					// UltraVNC extension
					String msg = rfb.readTextChatMsg();
					if (msg != null && msg.length() > 0) {
						// TODO implement chat interface
					}
					break;

				default:
					throw new Exception("Unknown RFB message type " + msgType);
				}
			}
		} catch (Exception e) {
			throw e;
		} finally {
			Log.v(TAG, "Closing VNC Connection");
			rfb.close();
		}
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
		bitmapData = null;
	}
	
	/**
	 * Warp the mouse to x, y in the RFB coordinates
	 * @param x
	 * @param y
	 */
	void warpMouse(int x, int y)
	{
		bitmapData.invalidateMousePosition();
		mouseX=x;
		mouseY=y;
		bitmapData.invalidateMousePosition();
		try
		{
			rfb.writePointerEvent(x, y, 0, MOUSE_BUTTON_NONE);
		}
		catch ( IOException ioe)
		{
			Log.w(TAG,ioe);
		}
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
		scrollTo((int)((absoluteXPosition + ((float)getWidth() - getImageWidth()) / 2 ) * scale),
				(int)((absoluteYPosition + ((float)getHeight() - getImageHeight()) / 2 ) * scale));
	}

	/**
	 * Make sure mouse is visible on displayable part of screen
	 */
	void panToMouse()
	{
		if (! connection.getFollowMouse())
			return;
		
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
		
		int newX = absoluteXPosition;
		int newY = absoluteYPosition;
		
		if (x - newX >= w - 5)
		{
			newX = x - w + 5;
			if (newX + w > iw)
				newX = iw - w;
		}
		else if (x < newX + 5)
		{
			newX = x - 5;
			if (newX < 0)
				newX = 0;
		}
		if ( newX != absoluteXPosition ) {
			absoluteXPosition = newX;
			panned = true;
		}
		if (y - newY >= h - 5)
		{
			newY = y - h + 5;
			if (newY + h > ih)
				newY = ih - h;
		}
		else if (y < newY + 5)
		{
			newY = y - 5;
			if (newY < 0)
				newY = 0;
		}
		if ( newY != absoluteYPosition ) {
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

	void handleRawRect(int x, int y, int w, int h) throws IOException {
		handleRawRect(x, y, w, h, true);
	}

	byte[] handleRawRectBuffer = new byte[128];
	void handleRawRect(int x, int y, int w, int h, boolean paint) throws IOException {
		boolean valid=bitmapData.validDraw(x, y, w, h);
		int[] pixels=bitmapData.bitmapPixels;
		if (bytesPerPixel == 1) {
			// 1 byte per pixel. Use palette lookup table.
		  if (w > handleRawRectBuffer.length) {
			  handleRawRectBuffer = new byte[w];
		  }
			int i, offset;
			for (int dy = y; dy < y + h; dy++) {
				rfb.readFully(handleRawRectBuffer, 0, w);
				if ( ! valid)
					continue;
				offset = bitmapData.offset(x, dy);
				for (i = 0; i < w; i++) {
					pixels[offset + i] = colorPalette[0xFF & handleRawRectBuffer[i]];
				}
			}
		} else {
			// 4 bytes per pixel (argb) 24-bit color
		  
			final int l = w * 4;
			if (l>handleRawRectBuffer.length) {
				handleRawRectBuffer = new byte[l];
			}
			int i, offset;
			for (int dy = y; dy < y + h; dy++) {
				rfb.readFully(handleRawRectBuffer, 0, l);
				if ( ! valid)
					continue;
				offset = bitmapData.offset(x, dy);
				for (i = 0; i < w; i++) {
				  final int idx = i*4;
					pixels[offset + i] = // 0xFF << 24 |
					(handleRawRectBuffer[idx + 2] & 0xff) << 16 | (handleRawRectBuffer[idx + 1] & 0xff) << 8 | (handleRawRectBuffer[idx] & 0xff);
				}
			}
		}
		
		if ( ! valid)
			return;

		bitmapData.updateBitmap( x, y, w, h);

		if (paint)
			reDraw();
	}

	private Runnable reDraw = new Runnable() {
		public void run() {
			if (showDesktopInfo) {
				// Show a Toast with the desktop info on first frame draw.
				showDesktopInfo = false;
				showConnectionInfo();
			}
			if (bitmapData != null)
				bitmapData.updateView(VncCanvas.this);
		}
	};
	
	private void reDraw() {
		if (repaintsEnabled)
			handler.post(reDraw);
	}
	
	// Toggles on-screen Ctrl mask. Returns true if result is Ctrl enabled, false otherwise.
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

	// Toggles on-screen Ctrl mask.  Returns true if result is Alt enabled, false otherwise.
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
	
	public void disableRepaints() {
		repaintsEnabled = false;
	}

	public void enableRepaints() {
		repaintsEnabled = true;
	}

	public void showConnectionInfo() {
		String msg = rfb.desktopName;
		int idx = rfb.desktopName.indexOf("(");
		if (idx > -1) {
			// Breakup actual desktop name from IP addresses for improved
			// readability
			String dn = rfb.desktopName.substring(0, idx).trim();
			String ip = rfb.desktopName.substring(idx).trim();
			msg = dn + "\n" + ip;
		}
		msg += "\n" + rfb.framebufferWidth + "x" + rfb.framebufferHeight;
		String enc = getEncoding();
		// Encoding might not be set when we display this message
		if (enc != null && !enc.equals(""))
			msg += ", " + getEncoding() + " encoding, " + colorModel.toString();
		else
			msg += ", " + colorModel.toString();
		Toast.makeText(getContext(), msg, Toast.LENGTH_LONG).show();
	}

	private String getEncoding() {
		switch (preferredEncoding) {
		case RfbProto.EncodingRaw:
			return "RAW";
		case RfbProto.EncodingTight:
			return "TIGHT";
		case RfbProto.EncodingCoRRE:
			return "CoRRE";
		case RfbProto.EncodingHextile:
			return "HEXTILE";
		case RfbProto.EncodingRRE:
			return "RRE";
		case RfbProto.EncodingZlib:
			return "ZLIB";
		case RfbProto.EncodingZRLE:
			return "ZRLE";
		}
		return "";
	}

    // Useful shortcuts for modifier masks.

    final static int CTRL_MASK  = KeyEvent.META_SYM_ON;
    final static int SHIFT_MASK = KeyEvent.META_SHIFT_ON;
    final static int ALT_MASK   = KeyEvent.META_ALT_ON;
    final static int META_MASK  = 0;
    
	private static final int MOUSE_BUTTON_NONE = 0;
	static final int MOUSE_BUTTON_LEFT = 1;
	static final int MOUSE_BUTTON_MIDDLE = 2;
	static final int MOUSE_BUTTON_RIGHT = 4;
	static final int MOUSE_BUTTON_SCROLL_UP = 8;
	static final int MOUSE_BUTTON_SCROLL_DOWN = 16;
//  ARE THESE VALUES CORRECT? TODO
//	static final int MOUSE_BUTTON_SCROLL_LEFT = 32;
//	static final int MOUSE_BUTTON_SCROLL_RIGHT = 64;
	
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
		
		// IF none of the conditions below are satisfied, clear the pointer mask.
		pointerMask = 0;
		
		if (rfb != null && rfb.inNormalProtocol) {
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
		    } else {
			    //Log.i(TAG,"Mouse button mask cleared");
		    	pointerMask = 0;
		    }
						
		    bitmapData.invalidateMousePosition();
		    mouseX = x;
		    mouseY = y;
		    if ( mouseX < 0) mouseX=0;
		    else if ( mouseX >= rfb.framebufferWidth ) mouseX = rfb.framebufferWidth - 1;
		    if ( mouseY < 0) mouseY=0;
		    else if ( mouseY >= rfb.framebufferHeight) mouseY = rfb.framebufferHeight - 1;
		    bitmapData.invalidateMousePosition();

		    try {
		    	rfb.writePointerEvent(mouseX, mouseY, modifiers|onScreenMetaState, pointerMask);
			} catch (Exception e) {
				e.printStackTrace();
			}
		    panToMouse();
			return true;
		}
		return false;		
	}
	
	/**
	 * Moves the scroll while the volume key is held down
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
			try
			{
				rfb.writePointerEvent(mouseX, mouseY, 0, scrollButton);
				rfb.writePointerEvent(mouseX, mouseY, 0, 0);
				
				handler.postDelayed(this, delay);
			}
			catch (IOException ioe)
			{
				
			}
		}		
	}

	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt) {
		//Log.d(TAG, "keycode = " + keyCode);

		if (keyCode == KeyEvent.KEYCODE_MENU)
			return true; 			              // Ignore menu key
		if (keyCode == KeyEvent.KEYCODE_CAMERA)
		{
			cameraButtonDown = (evt.getAction() != KeyEvent.ACTION_UP);
		}
		else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP)
		{
			int mouseChange = keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ? MOUSE_BUTTON_SCROLL_DOWN : MOUSE_BUTTON_SCROLL_UP;
			if (evt.getAction() == KeyEvent.ACTION_DOWN)
			{
				// If not auto-repeat
				if (scrollRunnable.scrollButton != mouseChange)
				{
					pointerMask |= mouseChange;
					scrollRunnable.scrollButton = mouseChange;
					handler.postDelayed(scrollRunnable,200);
				}
			}
			else
			{
				handler.removeCallbacks(scrollRunnable);
				scrollRunnable.scrollButton = 0;
				pointerMask &= ~mouseChange;
			}
			try
			{
				rfb.writePointerEvent(mouseX, mouseY, evt.getMetaState()|onScreenMetaState, pointerMask);
			}
			catch (IOException e)
			{
				e.printStackTrace();
			}
			return true;
		}
		if (rfb != null && rfb.inNormalProtocol) {
		   boolean down = (evt.getAction() == KeyEvent.ACTION_DOWN) ||
				   		  (evt.getAction() == KeyEvent.ACTION_MULTIPLE);
		   int key = 0;
		   int metaState = evt.getMetaState();

		   if (evt.getAction() == KeyEvent.ACTION_UP) {
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               key = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   onScreenMetaState &= ~CTRL_MASK;
				   return true;
			   }
			   // Handle Alt key up event.
			   if (keyCode == KeyEvent.KEYCODE_ALT_LEFT || keyCode == KeyEvent.KEYCODE_ALT_RIGHT) {
				   onScreenMetaState &= ~ALT_MASK;
				   return true;
			   }
		   }
	   
		   switch(keyCode) {
		   	  case KeyEvent.KEYCODE_BACK:         key = 0xff1b; break;
		      case KeyEvent.KEYCODE_DPAD_LEFT:    key = 0xff51; break;
		   	  case KeyEvent.KEYCODE_DPAD_UP:      key = 0xff52; break;
		   	  case KeyEvent.KEYCODE_DPAD_RIGHT:   key = 0xff53; break;
		   	  case KeyEvent.KEYCODE_DPAD_DOWN:    key = 0xff54; break;
		      case KeyEvent.KEYCODE_DEL: 		  key = 0xff08; break;
		      case KeyEvent.KEYCODE_ENTER:        key = 0xff0d; break;
		      case KeyEvent.KEYCODE_DPAD_CENTER:  key = 0xff0d; break;
		   	  case KeyEvent.KEYCODE_TAB:          key = 0xff09; break;
		   	  case KeyEvent.KEYCODE_ALT_LEFT:     key = 0xffe9; break;
		   	  case KeyEvent.KEYCODE_ALT_RIGHT:    key = 0xffea; break;
		   	  case 92 /* KEYCODE_PAGE_UP */:      key = 0xff55; break;
		   	  case 93 /* KEYCODE_PAGE_DOWN */:    key = 0xff56; break;
		   	  case 111 /* KEYCODE_ESCAPE */:      key = 0xff1b; break;
		   	  case 112 /* KEYCODE_FORWARD_DEL */: key = 0xffff; break;
		   	  case 113 /* KEYCODE_CTRL_LEFT */:   key = 0xffe3; break;
		   	  case 114 /* KEYCODE_CTRL_RIGHT */:  key = 0xffe4; break;
		   	  case 115 /* KEYCODE_CAPS_LOCK */:   key = 0xffe5; break;
		   	  case 116 /* KEYCODE_SCROLL_LOCK */: key = 0xff14; break;
		   	  case 120 /* KEYCODE_SYSRQ */:       key = 0xff61; break;
		   	  case 121 /* KEYCODE_BREAK */:       key = 0xff6b; break;
		   	  case 122 /* KEYCODE_MOVE_HOME */:   key = 0xff50; break;
		   	  case 123 /* KEYCODE_MOVE_END */:    key = 0xff57; break;
		   	  case 124 /* KEYCODE_INSERT */:      key = 0xff63; break;
		   	  case 131 /* KEYCODE_F1 */:          key = 0xffbe; break;
		   	  case 132 /* KEYCODE_F2 */:          key = 0xffbf; break;
		   	  case 133 /* KEYCODE_F3 */:          key = 0xffc0; break;
		   	  case 134 /* KEYCODE_F4 */:          key = 0xffc1; break;
		   	  case 135 /* KEYCODE_F5 */:          key = 0xffc2; break;
		   	  case 136 /* KEYCODE_F6 */:          key = 0xffc3; break;
		   	  case 137 /* KEYCODE_F7 */:          key = 0xffc4; break;
		   	  case 138 /* KEYCODE_F8 */:          key = 0xffc5; break;
		   	  case 139 /* KEYCODE_F9 */:          key = 0xffc6; break;
		   	  case 140 /* KEYCODE_F10 */:         key = 0xffc7; break;
		   	  case 141 /* KEYCODE_F11 */:         key = 0xffc8; break;
		   	  case 142 /* KEYCODE_F12 */:         key = 0xffc9; break;
		   	  case 143 /* KEYCODE_NUM_LOCK */:    key = 0xff7f; break;
		   	  case 0   /* KEYCODE_UNKNOWN */:
					// TODO: This should be sending all characters, not just the first one.
		   		  if (evt.getCharacters() != null)
		   			  key = evt.getCharacters().charAt(0);
	    		  break;
		      default: 							  
		    	  // Modifier handling is a bit tricky. Alt and Ctrl should be passed
		    	  // through to the VNC server so that they get handled there, but strip
		    	  // them from the character before retrieving the Unicode char from it.
		    	  // Don't clear Shift, we still want uppercase characters.
		    	  int vncEventMask = ( 0x7000    /* KeyEvent.META_CTRL_MASK */
		    			  			 | 0x0032 ); /* KeyEvent.META_ALT_MASK */
		    	  KeyEvent copy = new KeyEvent(evt.getDownTime(), evt.getEventTime(), evt.getAction(),
		    	                               evt.getKeyCode(),  evt.getRepeatCount(),
		    	                               metaState & ~vncEventMask, evt.getDeviceId(), evt.getScanCode());
	    		  key = copy.getUnicodeChar();
		    	  break;
		   }

		   if (evt.getAction() == KeyEvent.ACTION_DOWN ||
				   evt.getAction() == KeyEvent.ACTION_MULTIPLE) {
			   // Look for standard scan-codes from external keyboards
			   switch (evt.getScanCode()) {
			   case SCAN_ESC:               key = 0xff1b; break;
			   case SCAN_LEFTCTRL:
			   case SCAN_RIGHTCTRL:
				   onScreenMetaState |= CTRL_MASK;
				   return true;
			   case SCAN_F1:				key = 0xffbe;				break;
			   case SCAN_F2:				key = 0xffbf;				break;
			   case SCAN_F3:				key = 0xffc0;				break;
			   case SCAN_F4:				key = 0xffc1;				break;
			   case SCAN_F5:				key = 0xffc2;				break;
			   case SCAN_F6:				key = 0xffc3;				break;
			   case SCAN_F7:				key = 0xffc4;				break;
			   case SCAN_F8:				key = 0xffc5;				break;
			   case SCAN_F9:				key = 0xffc6;				break;
			   case SCAN_F10:				key = 0xffc7;				break;
			   }
			   // Handle Alt key down event.
			   if (keyCode == KeyEvent.KEYCODE_ALT_LEFT || keyCode == KeyEvent.KEYCODE_ALT_RIGHT) {
				   onScreenMetaState |= ALT_MASK;
				   return true;
			   }
		   }

		   try {
			   if (afterMenu)
			   {
				   afterMenu = false;
				   if (!down && key != lastKeyDown)
					   return true;
			   }
			   if (down)
				   lastKeyDown = key;
			   // Convert key to an X keysym code.
			   key = (int)xKeySymConv.ucs2keysym(key);
			   //Log.d(TAG,"key = " + key + " metastate = " + metaState + " keycode = " + keyCode);
			   rfb.writeKeyEvent(key, metaState|onScreenMetaState, down);
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
		// TODO: Switch from hard-coded numeric position to something better (at least an enumeration).
		if (connection.getConnectionType() == 1)
			sshConnection.disconnect();
	}
	
	void sendMetaKey(MetaKeyBean meta)
	{
		if (meta.isMouseClick())
		{
			try {
				rfb.writePointerEvent(mouseX, mouseY, meta.getMetaFlags()|onScreenMetaState, meta.getMouseButtons());
				rfb.writePointerEvent(mouseX, mouseY, meta.getMetaFlags()|onScreenMetaState, 0);
			}
			catch (IOException ioe)
			{
				ioe.printStackTrace();
			}
		}
		else {
			try {
				rfb.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState, true);
				rfb.writeKeyEvent(meta.getKeySym(), meta.getMetaFlags()|onScreenMetaState, false);
			}
			catch (IOException ioe)
			{
				ioe.printStackTrace();
			}
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
		int xoffset = (bitmapData.framebufferwidth - getWidth()) / 2;
		return xoffset;
	}

	public int getCenteredYOffset() {
		int yoffset = (bitmapData.framebufferheight - getHeight()) / 2;
		return yoffset;
	}

	/**
	 * Additional Encodings
	 * 
	 */

	private void setEncodings(boolean autoSelectOnly) {
		if (rfb == null || !rfb.inNormalProtocol)
			return;

		if (preferredEncoding == -1) {
			preferredEncoding = RfbProto.EncodingTight;
		} else {
			// Auto encoder selection is not enabled.
			if (autoSelectOnly)
				return;
		}

		int[] encodings = new int[20];
		int nEncodings = 0;

		encodings[nEncodings++] = preferredEncoding;
		encodings[nEncodings++] = RfbProto.EncodingCopyRect;
		if (preferredEncoding != RfbProto.EncodingTight)
			encodings[nEncodings++] = RfbProto.EncodingTight;
		if (preferredEncoding != RfbProto.EncodingZRLE)
			encodings[nEncodings++] = RfbProto.EncodingZRLE;
		if (preferredEncoding != RfbProto.EncodingHextile)
			encodings[nEncodings++] = RfbProto.EncodingHextile;
		if (preferredEncoding != RfbProto.EncodingZlib)
			encodings[nEncodings++] = RfbProto.EncodingZlib;
		if (preferredEncoding != RfbProto.EncodingCoRRE)
			encodings[nEncodings++] = RfbProto.EncodingCoRRE;
		if (preferredEncoding != RfbProto.EncodingRRE)
			encodings[nEncodings++] = RfbProto.EncodingRRE;
		
		if (compressLevel >= 0 && compressLevel <= 9)
			encodings[nEncodings++] = RfbProto.EncodingCompressLevel0 + compressLevel;
		if (jpegQuality >= 0 && jpegQuality <= 9)
			encodings[nEncodings++] = RfbProto.EncodingQualityLevel0 + jpegQuality;

		if (requestCursorUpdates) {
			encodings[nEncodings++] = RfbProto.EncodingXCursor;
			encodings[nEncodings++] = RfbProto.EncodingRichCursor;
			if (!ignoreCursorUpdates)
				encodings[nEncodings++] = RfbProto.EncodingPointerPos;
		}

		encodings[nEncodings++] = RfbProto.EncodingLastRect;
		encodings[nEncodings++] = RfbProto.EncodingNewFBSize;

		boolean encodingsWereChanged = false;
		if (nEncodings != nEncodingsSaved) {
			encodingsWereChanged = true;
		} else {
			for (int i = 0; i < nEncodings; i++) {
				if (encodings[i] != encodingsSaved[i]) {
					encodingsWereChanged = true;
					break;
				}
			}
		}

		if (encodingsWereChanged) {
			try {
				rfb.writeSetEncodings(encodings, nEncodings);
			} catch (Exception e) {
				e.printStackTrace();
			}
			encodingsSaved = encodings;
			nEncodingsSaved = nEncodings;
		}
	}

	//
	// Handle a CopyRect rectangle.
	//

	private void handleCopyRect(int x, int y, int w, int h) throws IOException {

		// Read the source coordinates.
		rfb.readCopyRect();
		
		if ( ! bitmapData.validDraw(x, y, w, h))
			return;
		
		// Source Coordinates
		int leftSrc = rfb.copyRectSrcX;
		int topSrc = rfb.copyRectSrcY;
		int rightSrc = leftSrc + w;
		int bottomSrc = topSrc + h;

		// Change
		int dx = x - rfb.copyRectSrcX;
		int dy = y - rfb.copyRectSrcY;

		// Destination Coordinates
		int leftDest = leftSrc + dx;
		int topDest = topSrc + dy;
		int rightDest = rightSrc + dx;
		int bottomDest = bottomSrc + dy;

		bitmapData.copyRect(new Rect(leftSrc, topSrc, rightSrc, bottomSrc), new Rect(leftDest, topDest, rightDest, bottomDest));

		reDraw();
	}
	
	byte[] bg_buf = new byte[4];
	byte[] rre_buf = new byte[128];
	//
	// Handle an RRE-encoded rectangle.
	//
	private void handleRRERect(int x, int y, int w, int h) throws IOException {
		boolean valid=bitmapData.validDraw(x, y, w, h);
		int nSubrects = rfb.is.readInt();

		rfb.readFully(bg_buf, 0, bytesPerPixel);
		int pixel;
		if (bytesPerPixel == 1) {
			pixel = colorPalette[0xFF & bg_buf[0]];
		} else {
			pixel = Color.rgb(bg_buf[2] & 0xFF, bg_buf[1] & 0xFF, bg_buf[0] & 0xFF);
		}
		handleRREPaint.setColor(pixel);
		if ( valid)
			bitmapData.drawRect(x, y, w, h, handleRREPaint);

		int len = nSubrects * (bytesPerPixel + 8);
		if (len > rre_buf.length)
			rre_buf = new byte[len];
		
		rfb.readFully(rre_buf, 0, len);
		if ( ! valid)
			return;

		int sx, sy, sw, sh;

		int i = 0;
		for (int j = 0; j < nSubrects; j++) {
			if (bytesPerPixel == 1) {
				pixel = colorPalette[0xFF & rre_buf[i++]];
			} else {
				pixel = Color.rgb(rre_buf[i + 2] & 0xFF, rre_buf[i + 1] & 0xFF, rre_buf[i] & 0xFF);
				i += 4;
			}
			sx = x + ((rre_buf[i] & 0xff) << 8) + (rre_buf[i+1] & 0xff); i+=2;
			sy = y + ((rre_buf[i] & 0xff) << 8) + (rre_buf[i+1] & 0xff); i+=2;
			sw = ((rre_buf[i] & 0xff) << 8) + (rre_buf[i+1] & 0xff); i+=2;
			sh = ((rre_buf[i] & 0xff) << 8) + (rre_buf[i+1] & 0xff); i+=2;

			handleRREPaint.setColor(pixel);
			bitmapData.drawRect(sx, sy, sw, sh, handleRREPaint);
		}

		reDraw();
	}

	//
	// Handle a CoRRE-encoded rectangle.
	//

	private void handleCoRRERect(int x, int y, int w, int h) throws IOException {
		boolean valid=bitmapData.validDraw(x, y, w, h);
		int nSubrects = rfb.is.readInt();

		rfb.readFully(bg_buf, 0, bytesPerPixel);
		int pixel;
		if (bytesPerPixel == 1) {
			pixel = colorPalette[0xFF & bg_buf[0]];
		} else {
			pixel = Color.rgb(bg_buf[2] & 0xFF, bg_buf[1] & 0xFF, bg_buf[0] & 0xFF);
		}
		handleRREPaint.setColor(pixel);
		if ( valid)
			bitmapData.drawRect(x, y, w, h, handleRREPaint);

		int len = nSubrects * (bytesPerPixel + 8);
		if (len > rre_buf.length)
			rre_buf = new byte[len];
		
		rfb.readFully(rre_buf, 0, len);
		if ( ! valid)
			return;

		int sx, sy, sw, sh;
		int i = 0;

		for (int j = 0; j < nSubrects; j++) {
			if (bytesPerPixel == 1) {
				pixel = colorPalette[0xFF & rre_buf[i++]];
			} else {
				pixel = Color.rgb(rre_buf[i + 2] & 0xFF, rre_buf[i + 1] & 0xFF, rre_buf[i] & 0xFF);
				i += 4;
			}
			sx = x + (rre_buf[i++] & 0xFF);
			sy = y + (rre_buf[i++] & 0xFF);
			sw = rre_buf[i++] & 0xFF;
			sh = rre_buf[i++] & 0xFF;

			handleRREPaint.setColor(pixel);
			bitmapData.drawRect(sx, sy, sw, sh, handleRREPaint);
		}

		reDraw();
	}

	//
	// Handle a Hextile-encoded rectangle.
	//

	// These colors should be kept between handleHextileSubrect() calls.
	private int hextile_bg, hextile_fg;

	private void handleHextileRect(int x, int y, int w, int h) throws IOException {

		hextile_bg = Color.BLACK;
		hextile_fg = Color.BLACK;

		for (int ty = y; ty < y + h; ty += 16) {
			int th = 16;
			if (y + h - ty < 16)
				th = y + h - ty;

			for (int tx = x; tx < x + w; tx += 16) {
				int tw = 16;
				if (x + w - tx < 16)
					tw = x + w - tx;

				handleHextileSubrect(tx, ty, tw, th);
			}

			// Finished with a row of tiles, now let's show it.
			reDraw();
		}
	}

	//
	// Handle one tile in the Hextile-encoded data.
	//

	Paint handleHextileSubrectPaint = new Paint();
	byte[] backgroundColorBuffer = new byte[4];
	private void handleHextileSubrect(int tx, int ty, int tw, int th) throws IOException {

		int subencoding = rfb.is.readUnsignedByte();

		// Is it a raw-encoded sub-rectangle?
		if ((subencoding & RfbProto.HextileRaw) != 0) {
			handleRawRect(tx, ty, tw, th, false);
			return;
		}

		boolean valid=bitmapData.validDraw(tx, ty, tw, th);
		// Read and draw the background if specified.
		if (bytesPerPixel > backgroundColorBuffer.length) {
		  throw new RuntimeException("impossible colordepth");
		}
		if ((subencoding & RfbProto.HextileBackgroundSpecified) != 0) {
			rfb.readFully(backgroundColorBuffer, 0, bytesPerPixel);
			if (bytesPerPixel == 1) {
				hextile_bg = colorPalette[0xFF & backgroundColorBuffer[0]];
			} else {
				hextile_bg = Color.rgb(backgroundColorBuffer[2] & 0xFF, backgroundColorBuffer[1] & 0xFF, backgroundColorBuffer[0] & 0xFF);
			}
		}
		handleHextileSubrectPaint.setColor(hextile_bg);
		handleHextileSubrectPaint.setStyle(Paint.Style.FILL);
		if ( valid )
			bitmapData.drawRect(tx, ty, tw, th, handleHextileSubrectPaint);

		// Read the foreground color if specified.
		if ((subencoding & RfbProto.HextileForegroundSpecified) != 0) {
			rfb.readFully(backgroundColorBuffer, 0, bytesPerPixel);
			if (bytesPerPixel == 1) {
				hextile_fg = colorPalette[0xFF & backgroundColorBuffer[0]];
			} else {
				hextile_fg = Color.rgb(backgroundColorBuffer[2] & 0xFF, backgroundColorBuffer[1] & 0xFF, backgroundColorBuffer[0] & 0xFF);
			}
		}

		// Done with this tile if there is no sub-rectangles.
		if ((subencoding & RfbProto.HextileAnySubrects) == 0)
			return;

		int nSubrects = rfb.is.readUnsignedByte();
		int bufsize = nSubrects * 2;
		if ((subencoding & RfbProto.HextileSubrectsColoured) != 0) {
			bufsize += nSubrects * bytesPerPixel;
		}
		if (rre_buf.length < bufsize)
			rre_buf = new byte[bufsize];
		rfb.readFully(rre_buf, 0, bufsize);

		int b1, b2, sx, sy, sw, sh;
		int i = 0;
		if ((subencoding & RfbProto.HextileSubrectsColoured) == 0) {

			// Sub-rectangles are all of the same color.
			handleHextileSubrectPaint.setColor(hextile_fg);
			for (int j = 0; j < nSubrects; j++) {
				b1 = rre_buf[i++] & 0xFF;
				b2 = rre_buf[i++] & 0xFF;
				sx = tx + (b1 >> 4);
				sy = ty + (b1 & 0xf);
				sw = (b2 >> 4) + 1;
				sh = (b2 & 0xf) + 1;
				if ( valid)
					bitmapData.drawRect(sx, sy, sw, sh, handleHextileSubrectPaint);
			}
		} else if (bytesPerPixel == 1) {

			// BGR233 (8-bit color) version for colored sub-rectangles.
			for (int j = 0; j < nSubrects; j++) {
				hextile_fg = colorPalette[0xFF & rre_buf[i++]];
				b1 = rre_buf[i++] & 0xFF;
				b2 = rre_buf[i++] & 0xFF;
				sx = tx + (b1 >> 4);
				sy = ty + (b1 & 0xf);
				sw = (b2 >> 4) + 1;
				sh = (b2 & 0xf) + 1;
				handleHextileSubrectPaint.setColor(hextile_fg);
				if ( valid)
					bitmapData.drawRect(sx, sy, sw, sh, handleHextileSubrectPaint);
			}

		} else {

			// Full-color (24-bit) version for colored sub-rectangles.
			for (int j = 0; j < nSubrects; j++) {
				hextile_fg = Color.rgb(rre_buf[i + 2] & 0xFF, rre_buf[i + 1] & 0xFF, rre_buf[i] & 0xFF);
				i += 4;
				b1 = rre_buf[i++] & 0xFF;
				b2 = rre_buf[i++] & 0xFF;
				sx = tx + (b1 >> 4);
				sy = ty + (b1 & 0xf);
				sw = (b2 >> 4) + 1;
				sh = (b2 & 0xf) + 1;
				handleHextileSubrectPaint.setColor(hextile_fg);
				if ( valid )
					bitmapData.drawRect(sx, sy, sw, sh, handleHextileSubrectPaint);
			}

		}
	}

	//
	// Handle a ZRLE-encoded rectangle.
	//

  Paint handleZRLERectPaint = new Paint();
  int[] handleZRLERectPalette = new int[128];
	private void handleZRLERect(int x, int y, int w, int h) throws Exception {

		if (zrleInStream == null)
			zrleInStream = new ZlibInStream();

		int nBytes = rfb.is.readInt();
		if (nBytes > 64 * 1024 * 1024)
			throw new Exception("ZRLE decoder: illegal compressed data size");

		if (zrleBuf == null || zrleBuf.length < nBytes) {
			zrleBuf = new byte[nBytes+4096];
		}

		rfb.readFully(zrleBuf, 0, nBytes);

		zrleInStream.setUnderlying(new MemInStream(zrleBuf, 0, nBytes), nBytes);
		
		boolean valid=bitmapData.validDraw(x, y, w, h);

		for (int ty = y; ty < y + h; ty += 64) {

			int th = Math.min(y + h - ty, 64);

			for (int tx = x; tx < x + w; tx += 64) {

				int tw = Math.min(x + w - tx, 64);

				int mode = zrleInStream.readU8();
				boolean rle = (mode & 128) != 0;
				int palSize = mode & 127;

				readZrlePalette(handleZRLERectPalette, palSize);

				if (palSize == 1) {
					int pix = handleZRLERectPalette[0];
					int c = (bytesPerPixel == 1) ? colorPalette[0xFF & pix] : (0xFF000000 | pix);
					handleZRLERectPaint.setColor(c);
					handleZRLERectPaint.setStyle(Paint.Style.FILL);
					if ( valid)
						bitmapData.drawRect(tx, ty, tw, th, handleZRLERectPaint);
					continue;
				}

				if (!rle) {
					if (palSize == 0) {
						readZrleRawPixels(tw, th);
					} else {
						readZrlePackedPixels(tw, th, handleZRLERectPalette, palSize);
					}
				} else {
					if (palSize == 0) {
						readZrlePlainRLEPixels(tw, th);
					} else {
						readZrlePackedRLEPixels(tw, th, handleZRLERectPalette);
					}
				}
				if ( valid )
					handleUpdatedZrleTile(tx, ty, tw, th);
			}
		}

		zrleInStream.reset();

		reDraw();
	}

	//
	// Handle a Zlib-encoded rectangle.
	//

	byte[] handleZlibRectBuffer = new byte[128];
	private void handleZlibRect(int x, int y, int w, int h) throws Exception {
		boolean valid = bitmapData.validDraw(x, y, w, h);
		int nBytes = rfb.is.readInt();

		if (zlibBuf == null || zlibBuf.length < nBytes) {
			zlibBuf = new byte[nBytes*2];
		}

		rfb.readFully(zlibBuf, 0, nBytes);

		if (zlibInflater == null) {
			zlibInflater = new Inflater();
		}
		zlibInflater.setInput(zlibBuf, 0, nBytes);
		
		int[] pixels=bitmapData.bitmapPixels;

		if (bytesPerPixel == 1) {
			// 1 byte per pixel. Use palette lookup table.
		  if (w > handleZlibRectBuffer.length) {
		    handleZlibRectBuffer = new byte[w];
		  }
			int i, offset;
			for (int dy = y; dy < y + h; dy++) {
				zlibInflater.inflate(handleZlibRectBuffer,  0, w);
				if ( ! valid)
					continue;
				offset = bitmapData.offset(x, dy);
				for (i = 0; i < w; i++) {
					pixels[offset + i] = colorPalette[0xFF & handleZlibRectBuffer[i]];
				}
			}
		} else {
			// 24-bit color (ARGB) 4 bytes per pixel.
		  final int l = w*4;
		  if (l > handleZlibRectBuffer.length) {
			  handleZlibRectBuffer = new byte[l];
		  }
			int i, offset;
			for (int dy = y; dy < y + h; dy++) {
				zlibInflater.inflate(handleZlibRectBuffer, 0, l);
				if ( ! valid)
					continue;
				offset = bitmapData.offset(x, dy);
				for (i = 0; i < w; i++) {
				  final int idx = i*4;
					pixels[offset + i] = (handleZlibRectBuffer[idx + 2] & 0xFF) << 16 | (handleZlibRectBuffer[idx + 1] & 0xFF) << 8 | (handleZlibRectBuffer[idx] & 0xFF);
				}
			}
		}
		if ( ! valid)
			return;
		bitmapData.updateBitmap(x, y, w, h);

		reDraw();
	}

	private int readPixel(InStream is) throws Exception {
		int pix;
		if (bytesPerPixel == 1) {
			pix = is.readU8();
		} else {
			int p1 = is.readU8();
			int p2 = is.readU8();
			int p3 = is.readU8();
			pix = (p3 & 0xFF) << 16 | (p2 & 0xFF) << 8 | (p1 & 0xFF);
		}
		return pix;
	}

	byte[] readPixelsBuffer = new byte[128];
	private void readPixels(InStream is, int[] dst, int count) throws Exception {
		if (bytesPerPixel == 1) {
		  if (count > readPixelsBuffer.length) {
		    readPixelsBuffer = new byte[count];
		  }
			is.readBytes(readPixelsBuffer, 0, count);
			for (int i = 0; i < count; i++) {
				dst[i] = (int) readPixelsBuffer[i] & 0xFF;
			}
		} else {
		  final int l = count * 3;
      if (l > readPixelsBuffer.length) {
			readPixelsBuffer = new byte[l];
      }
			is.readBytes(readPixelsBuffer, 0, l);
			for (int i = 0; i < count; i++) {
			  final int idx = i*3;
				dst[i] = ((readPixelsBuffer[idx + 2] & 0xFF) << 16 | (readPixelsBuffer[idx + 1] & 0xFF) << 8 | (readPixelsBuffer[idx] & 0xFF));
			}
		}
	}

	private void readZrlePalette(int[] palette, int palSize) throws Exception {
		readPixels(zrleInStream, palette, palSize);
	}

	private void readZrleRawPixels(int tw, int th) throws Exception {
		int len = tw * th;
		if (zrleTilePixels == null || len > zrleTilePixels.length)
			zrleTilePixels = new int[len];
		readPixels(zrleInStream, zrleTilePixels, tw * th); // /
	}

	private void readZrlePackedPixels(int tw, int th, int[] palette, int palSize) throws Exception {

		int bppp = ((palSize > 16) ? 8 : ((palSize > 4) ? 4 : ((palSize > 2) ? 2 : 1)));
		int ptr = 0;
		int len = tw * th;
		if (zrleTilePixels == null || len > zrleTilePixels.length)
			zrleTilePixels = new int[len];

		for (int i = 0; i < th; i++) {
			int eol = ptr + tw;
			int b = 0;
			int nbits = 0;

			while (ptr < eol) {
				if (nbits == 0) {
					b = zrleInStream.readU8();
					nbits = 8;
				}
				nbits -= bppp;
				int index = (b >> nbits) & ((1 << bppp) - 1) & 127;
				if (bytesPerPixel == 1) {
					if (index >= colorPalette.length)
						Log.e(TAG, "zrlePlainRLEPixels palette lookup out of bounds " + index + " (0x" + Integer.toHexString(index) + ")");
					zrleTilePixels[ptr++] = colorPalette[0xFF & palette[index]];
				} else {
					zrleTilePixels[ptr++] = palette[index];
				}
			}
		}
	}

	private void readZrlePlainRLEPixels(int tw, int th) throws Exception {
		int ptr = 0;
		int end = ptr + tw * th;
		if (zrleTilePixels == null || end > zrleTilePixels.length)
			zrleTilePixels = new int[end];
		while (ptr < end) {
			int pix = readPixel(zrleInStream);
			int len = 1;
			int b;
			do {
				b = zrleInStream.readU8();
				len += b;
			} while (b == 255);

			if (!(len <= end - ptr))
				throw new Exception("ZRLE decoder: assertion failed" + " (len <= end-ptr)");

			if (bytesPerPixel == 1) {
				while (len-- > 0)
					zrleTilePixels[ptr++] = colorPalette[0xFF & pix];
			} else {
				while (len-- > 0)
					zrleTilePixels[ptr++] = pix;
			}
		}
	}

	private void readZrlePackedRLEPixels(int tw, int th, int[] palette) throws Exception {

		int ptr = 0;
		int end = ptr + tw * th;
		if (zrleTilePixels == null || end > zrleTilePixels.length)
			zrleTilePixels = new int[end];
		while (ptr < end) {
			int index = zrleInStream.readU8();
			int len = 1;
			if ((index & 128) != 0) {
				int b;
				do {
					b = zrleInStream.readU8();
					len += b;
				} while (b == 255);

				if (!(len <= end - ptr))
					throw new Exception("ZRLE decoder: assertion failed" + " (len <= end - ptr)");
			}

			index &= 127;
			int pix = palette[index];

			if (bytesPerPixel == 1) {
				while (len-- > 0)
					zrleTilePixels[ptr++] = colorPalette[0xFF & pix];
			} else {
				while (len-- > 0)
					zrleTilePixels[ptr++] = pix;
			}
		}
	}

	//
	// Copy pixels from zrleTilePixels8 or zrleTilePixels24, then update.
	//

	private void handleUpdatedZrleTile(int x, int y, int w, int h) {
		int offsetSrc = 0;
		int[] destPixels=bitmapData.bitmapPixels;
		for (int j = 0; j < h; j++) {
			System.arraycopy(zrleTilePixels, offsetSrc, destPixels, bitmapData.offset(x, y + j), w);
			offsetSrc += w;
		}

		bitmapData.updateBitmap(x, y, w, h);
	}

	
	//
	// Handle a Tight-encoded rectangle.
	//
	void handleTightRect(int x, int y, int w, int h) throws Exception {
		
		boolean valid = bitmapData.validDraw(x, y, w, h);
		int[] pixels = bitmapData.bitmapPixels;

		int comp_ctl = rfb.is.readUnsignedByte();

		// Flush zlib streams if we are told by the server to do so.
		for (int stream_id = 0; stream_id < 4; stream_id++) {
			if ((comp_ctl & 1) != 0 && tightInflaters[stream_id] != null) {
				tightInflaters[stream_id] = null;
			}
			comp_ctl >>= 1;
		}

		// Check correctness of sub-encoding value.
		if (comp_ctl > RfbProto.TightMaxSubencoding) {
			throw new Exception("Incorrect tight subencoding: " + comp_ctl);
		}

		// Handle solid-color rectangles.
		if (comp_ctl == RfbProto.TightFill) {
			if (bytesPerPixel == 1) {
				int idx = rfb.is.readUnsignedByte();
				handleTightRectPaint.setColor(colorPalette[0xFF & idx]);
			} else {
				rfb.readFully(solidColorBuf, 0, 3);
				handleTightRectPaint.setColor(0xFF000000 | (solidColorBuf[0] & 0xFF) << 16 
														 | (solidColorBuf[1] & 0xFF) << 8 | (solidColorBuf[2] & 0xFF));
			}
			if (valid) {
				bitmapData.drawRect(x, y, w, h, handleTightRectPaint);
				reDraw();
			}
			return;
		}

		if (comp_ctl == RfbProto.TightJpeg) {
			// Read JPEG data.
			int jpegDataLen = rfb.readCompactLen();
			if (jpegDataLen > inflBuf.length) {
				inflBuf = new byte[2*jpegDataLen];
			}
			rfb.readFully(inflBuf, 0, jpegDataLen);
			
			if (!valid)
				return;

			// Decode JPEG data
			Bitmap tightBitmap = BitmapFactory.decodeByteArray(inflBuf, 0, jpegDataLen);

			// Copy JPEG data into bitmap.
			tightBitmap.getPixels(pixels, bitmapData.offset(x, y), bitmapData.bitmapwidth, 0, 0, w, h);

			bitmapData.updateBitmap(x, y, w, h);
			reDraw();
			return;
		}

		// Read filter id and parameters.
		int    numColors = 0, rowSize = w;
		boolean useGradient = false;

		if ((comp_ctl & RfbProto.TightExplicitFilter) != 0) {
			int filter_id = rfb.is.readUnsignedByte();

			if (filter_id == RfbProto.TightFilterPalette) {
				numColors = rfb.is.readUnsignedByte() + 1;
				
				if (bytesPerPixel == 1) {
					if (numColors != 2) {
						throw new Exception("Incorrect tight palette size: " + numColors);
					}
					rfb.readFully(tightPalette8, 0, 2);

				} else {
					rfb.readFully(colorBuf, 0, numColors*3);
					for (int i = 0; i < numColors; i++) {
						tightPalette24[i] = ((colorBuf[i * 3]     & 0xFF) << 16 |
											 (colorBuf[i * 3 + 1] & 0xFF) << 8  |
											 (colorBuf[i * 3 + 2] & 0xFF));
					}
				}

				if (numColors == 2)
					rowSize = (w + 7) / 8;

			} else if (filter_id == RfbProto.TightFilterGradient) {
				useGradient = true;
			} else if (filter_id != RfbProto.TightFilterCopy) {
				throw new Exception("Incorrect tight filter id: " + filter_id);
			}
		}

		if (numColors == 0 && bytesPerPixel == 4)
			rowSize *= 3;

		// Read, optionally uncompress and decode data.
		int dataSize = h * rowSize;

		if (dataSize < RfbProto.TightMinToCompress) {
			// Data size is small - not compressed with zlib.

			if (numColors != 0) {
				// Indexed colors.
				rfb.readFully(uncompDataBuf, 0, dataSize);
				if (!valid)
					return;

				if (numColors == 2) {
					// Two colors.
					if (bytesPerPixel == 1) {
						decodeMonoData(x, y, w, h, uncompDataBuf, tightPalette8);
					} else {
						decodeMonoData(x, y, w, h, uncompDataBuf, tightPalette24);
					}	
				} else {
					// 3..255 colors (assuming bytesPerPixel == 4).
					int i = 0, offset;
					for (int dy = y; dy < y + h; dy++) {
						offset = bitmapData.offset(x, dy);
						for (int dx = x; dx < x + w; dx++) {
							pixels[offset++] = tightPalette24[uncompDataBuf[i++] & 0xFF];
						}
					}
				}
			} else if (useGradient) {
				// "Gradient"-processed data
				rfb.readFully(uncompDataBuf, 0, dataSize);
				if (!valid)
					return;

				decodeGradientData(x, y, w, h, uncompDataBuf);
			} else {
				// Raw true-color data.
				int i, offset;
				if (bytesPerPixel == 1) {
					for (int dy = y; dy < y + h; dy++) {
						rfb.readFully(uncompDataBuf, 0, rowSize);
						if (!valid)
							continue;

						offset = bitmapData.offset(x, dy);
						for (i = 0; i < w; i++) {
							pixels[offset + i] = colorPalette[0xFF & uncompDataBuf[i]];
						}						
					}
				} else {
					for (int dy = y; dy < y + h; dy++) {
						rfb.readFully(uncompDataBuf, 0, rowSize);
						if (!valid)
							continue;
						
						offset = bitmapData.offset(x, dy);
						for (i = 0; i < w; i++) {
							pixels[offset + i] = (uncompDataBuf[i * 3]     & 0xFF) << 16 | 
												 (uncompDataBuf[i * 3 + 1] & 0xFF) << 8  |
												 (uncompDataBuf[i * 3 + 2] & 0xFF);
						}
					}
				}
			}

		} else {
			// Data was compressed with zlib.
			int zlibDataLen = rfb.readCompactLen();
			if (zlibDataLen > zlibData.length) {
				zlibData = new byte[zlibDataLen*2];
			}
			rfb.readFully(zlibData, 0, zlibDataLen);
			
			int stream_id = comp_ctl & 0x03;
			if (tightInflaters[stream_id] == null) {
				tightInflaters[stream_id] = new Inflater();
			}

			Inflater myInflater = tightInflaters[stream_id];
			myInflater.setInput(zlibData, 0, zlibDataLen);
			
			if (dataSize > inflBuf.length) {
				inflBuf = new byte[dataSize*2];
			}

			if (numColors != 0) {
				myInflater.inflate(inflBuf, 0, dataSize);
				if (!valid)
					return;

				// Indexed colors.
				if (numColors == 2) {
					// Two colors.
					if (bytesPerPixel == 1) {
						decodeMonoData(x, y, w, h, inflBuf, tightPalette8);
					} else {
						decodeMonoData(x, y, w, h, inflBuf, tightPalette24);
					}
				} else {
					// More than two colors (assuming bytesPerPixel == 4).
					int i = 0, offset;
					for (int dy = y; dy < y + h; dy++) {
						offset = bitmapData.offset(x, dy);
						for (int dx = x; dx < x + w; dx++) {
							pixels[offset++] = tightPalette24[inflBuf[i++] & 0xFF];
						}
					}
				}
			} else if (useGradient) {
				// Compressed "Gradient"-filtered data (assuming bytesPerPixel == 4).
				myInflater.inflate(inflBuf, 0, dataSize);
				if (!valid)
					return;

				decodeGradientData(x, y, w, h, inflBuf);
			} else {
				// Compressed true-color data.
				if (bytesPerPixel == 1) {
					for (int dy = y; dy < y + h; dy++) {
						myInflater.inflate(inflBuf, 0, rowSize);
						if (!valid)
							continue;

						int offset = bitmapData.offset(x, dy);
						for (int i = 0; i < w; i++) {
							pixels[offset + i] = colorPalette[0xFF & inflBuf[i]];
						}
					}
				} else {
					int offset;
					for (int dy = y; dy < y + h; dy++) {
						myInflater.inflate(inflBuf, 0, rowSize);
						if (!valid)
							continue;

						offset = bitmapData.offset(x, dy);
						for (int i = 0; i < w; i++) {
							final int idx = i*3;
							pixels[offset + i] = (inflBuf[idx] & 0xFF)     << 16 |
												 (inflBuf[idx + 1] & 0xFF) << 8  |
												 (inflBuf[idx + 2] & 0xFF);
						}
					}
				}
			}
		}
		if (!valid)
			return;
		bitmapData.updateBitmap(x, y, w, h);
		reDraw();
	}
	  
	//
	// Decode 1bpp-encoded bi-color rectangle (8-bit and 24-bit versions).
	//
	// TODO: Switch to bitmapData.offset rather than direct calculations
	void decodeMonoData(int x, int y, int w, int h, byte[] src, byte[] palette) {

		int dx, dy, n;
		int i = bitmapData.offset(x, y);
		int[] pixels = bitmapData.bitmapPixels;
		int rowBytes = (w + 7) / 8;
		byte b;

		for (dy = 0; dy < h; dy++) {
			for (dx = 0; dx < w / 8; dx++) {
				b = src[dy*rowBytes+dx];
				for (n = 7; n >= 0; n--) {
					pixels[i++] = colorPalette[0xFF & palette[b >> n & 1]];
				}
			}
			for (n = 7; n >= 8 - w % 8; n--) {
				pixels[i++] = colorPalette[0xFF & palette[src[dy*rowBytes+dx] >> n & 1]];
			}
			i += (bitmapData.bitmapwidth - w);
		}
	}

	// TODO: Switch to bitmapData.offset rather than direct calculations
	void decodeMonoData(int x, int y, int w, int h, byte[] src, int[] palette) {

		int dx, dy, n;
		int i = bitmapData.offset(x, y);
		int[] pixels = bitmapData.bitmapPixels;
		int rowBytes = (w + 7) / 8;
		byte b;

		for (dy = 0; dy < h; dy++) {
			for (dx = 0; dx < w / 8; dx++) {
				b = src[dy*rowBytes+dx];
				for (n = 7; n >= 0; n--) {
					pixels[i++] = palette[b >> n & 1];
				}
			}
			for (n = 7; n >= 8 - w % 8; n--) {
				pixels[i++] = palette[src[dy*rowBytes+dx] >> n & 1];
			}
			i += (bitmapData.bitmapwidth - w);
		}
	}

	//
	// Decode data processed with the "Gradient" filter.
	//
	// TODO: Switch to bitmapData.offset rather than direct calculations
	void decodeGradientData (int x, int y, int w, int h, byte[] buf) {

		int dx, dy, c;
		byte[] prevRow = new byte[w * 3];
		byte[] thisRow = new byte[w * 3];
		byte[] pix = new byte[3];
		int[] est = new int[3];
		int[] pixels = bitmapData.bitmapPixels;

		int offset = bitmapData.offset(x, y);

		for (dy = 0; dy < h; dy++) {

			/* First pixel in a row */
			for (c = 0; c < 3; c++) {
				pix[c] = (byte)(prevRow[c] + buf[dy * w * 3 + c]);
				thisRow[c] = pix[c];
			}
			pixels[offset++] = (pix[0] & 0xFF) << 16 | (pix[1] & 0xFF) << 8 | (pix[2] & 0xFF);

			/* Remaining pixels of a row */
			for (dx = 1; dx < w; dx++) {
				for (c = 0; c < 3; c++) {
					est[c] = ((prevRow[dx * 3 + c] & 0xFF) + (pix[c] & 0xFF) -
							  (prevRow[(dx-1) * 3 + c] & 0xFF));
					if (est[c] > 0xFF) {
						est[c] = 0xFF;
					} else if (est[c] < 0x00) {
						est[c] = 0x00;
					}
					pix[c] = (byte)(est[c] + buf[(dy * w + dx) * 3 + c]);
					thisRow[dx * 3 + c] = pix[c];
				}
				pixels[offset++] = (pix[0] & 0xFF) << 16 | (pix[1] & 0xFF) << 8 | (pix[2] & 0xFF);
			}

			System.arraycopy(thisRow, 0, prevRow, 0, w * 3);
			offset += (bitmapData.bitmapwidth - w);
		}
	}
}