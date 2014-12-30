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

import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Timer;

import android.app.Activity;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.ClipboardManager;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
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

import com.undatech.opaque.R;
import com.undatech.opaque.SpiceCommunicator;
import com.undatech.opaque.input.RemoteKeyboard;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.input.RemoteSpiceKeyboard;
import com.undatech.opaque.input.RemoteSpicePointer;
import com.undatech.opaque.dialogs.*;

public class RemoteCanvas extends ImageView implements SelectTextElementFragment.OnFragmentDismissedListener,
                                                           GetTextFragment.OnFragmentDismissedListener {
	private final static String TAG = "RemoteCanvas";
	
	public Handler handler = new Handler() {
	    @Override
	    public void handleMessage(Message msg) {
	    	FragmentManager fm;
	        switch (msg.what) {
            case Constants.VV_FILE_ERROR:
                disconnectAndShowMessage(R.string.vv_file_error, R.string.error_dialog_title);
                break;
	        case Constants.NO_VM_FOUND_FOR_USER:
	        	disconnectAndShowMessage(R.string.error_no_vm_found_for_user, R.string.error_dialog_title);
	        	break;
	        case Constants.OVIRT_SSL_HANDSHAKE_FAILURE:
	        	disconnectAndShowMessage(R.string.error_ovirt_ssl_handshake_failure, R.string.error_dialog_title);
	        	break;
	        case Constants.VM_LOOKUP_FAILED:
	        	disconnectAndShowMessage(R.string.error_vm_lookup_failed, R.string.error_dialog_title);
	        	break;
	        case Constants.OVIRT_AUTH_FAILURE:
		        disconnectAndShowMessage(R.string.error_ovirt_auth_failure, R.string.error_dialog_title);
		        break;
	        case Constants.VM_LAUNCHED:
	        	disconnectAndShowMessage(R.string.info_vm_launched_on_stand_by, R.string.info_dialog_title);
	        	break;
	        case Constants.LAUNCH_VNC_VIEWER:
/* TODO: Implement:
	    		android.util.Log.d(TAG, "Trying to launch VNC viewer");
	    		Intent intent = new Intent ();
	    	    String intentURI = "vnc://" + address + ":" + port + "?password=" + password;
	    	    try {
	    	        intent = Intent.parseUri(intentURI, Intent.URI_INTENT_SCHEME);
	    	    } catch (URISyntaxException e) {
	    	    }

	    	    myself.canvas.getContext().startActivity(intent);
	    	    disconnectAndCleanup();
	    	    finish();
*/
	        	disconnectAndShowMessage(R.string.error_no_vnc_support_yet, R.string.error_dialog_title);
	        	break;
	        case Constants.GET_PASSWORD:
	    		fm = ((FragmentActivity)getContext()).getSupportFragmentManager();
	    		GetTextFragment getPassword = GetTextFragment.newInstance(
	    												RemoteCanvas.this.getContext().getString(R.string.enter_password),
	    												RemoteCanvas.this, true);
	    		// TODO: Add OK button.
	    		//newFragment.setCancelable(false);
	    		getPassword.show(fm, "getPassword");
	        	break;
	        case Constants.DIALOG_DISPLAY_VMS:
	    		fm = ((FragmentActivity)getContext()).getSupportFragmentManager();
	    		SelectTextElementFragment displayVms = SelectTextElementFragment.newInstance(
	    												RemoteCanvas.this.getContext().getString(R.string.select_vm_title),
	    												spicecomm.getVmNames(), RemoteCanvas.this);
	    		displayVms.setCancelable(false);
	    		displayVms.show(fm, "selectVm");
	        	break;
	        case Constants.SPICE_CONNECT_SUCCESS:
	        	spiceUpdateReceived = true;
				synchronized(spicecomm) {
					spicecomm.notifyAll();
				}
	        	break;
	        case Constants.SPICE_CONNECT_FAILURE:
	        	// If we were intending to maintainConnection, and the connection failed, show an error message.
	        	if (stayConnected) {
		    		if (!spiceUpdateReceived) {
		    			disconnectAndShowMessage(R.string.error_ovirt_unable_to_connect, R.string.error_dialog_title);
		    		} else {
		    			disconnectAndShowMessage(R.string.error_connection_interrupted, R.string.error_dialog_title);
		    		}
	        	}
	        	break;
	        }
	    }
	};
	
	// Current connection parameters
	private ConnectionSettings settings;
	
	// Indicates whether we intend to maintain the connection.
	private boolean stayConnected = true;
	
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
    
    /**
     * Initialize the canvas to show the remote desktop
     */
    void initialize(final String vvFileName, final ConnectionSettings settings) {
        checkNetworkConnectivity();
        initializeClipboardMonitor();
        this.settings = settings;
        
        Thread cThread = new Thread () {
            @Override
            public void run() {
                try {
                    spicecomm = new SpiceCommunicator (getContext(), RemoteCanvas.this, settings.isRequestingNewDisplayResolution(), settings.isUsbEnabled());
                    pointer = new RemoteSpicePointer (spicecomm, RemoteCanvas.this, handler);
                    keyboard = new RemoteSpiceKeyboard (getResources(), spicecomm, RemoteCanvas.this, handler, settings.getLayoutMap());
                    spicecomm.setHandler(handler);
                    spicecomm.startSessionFromVvFile(vvFileName, settings.isAudioPlaybackEnabled());                    
                } catch (Throwable e) {
                    if (stayConnected) {
                        e.printStackTrace();
                        android.util.Log.e(TAG, e.toString());
                        if (e instanceof OutOfMemoryError) {
                            disposeDrawable ();
                            disconnectAndShowMessage(R.string.error_out_of_memory, R.string.error_dialog_title);
                        }
                    }
                }
            }
        };
        cThread.start();
    }
    
    
	/**
	 * Initialize the canvas to show the remote desktop
	 */
	void initialize(final ConnectionSettings settings) {
        checkNetworkConnectivity();
        initializeClipboardMonitor();
		this.settings = settings;

		Thread cThread = new Thread () {
			@Override
			public void run() {
				try {		
					spicecomm = new SpiceCommunicator (getContext(), RemoteCanvas.this, settings.isRequestingNewDisplayResolution(), settings.isUsbEnabled());
					pointer = new RemoteSpicePointer (spicecomm, RemoteCanvas.this, handler);
					keyboard = new RemoteSpiceKeyboard (getResources(), spicecomm, RemoteCanvas.this, handler, settings.getLayoutMap());
					spicecomm.setHandler(handler);
					
					// Obtain user's password if necessary.
					if (settings.getPassword().equals("")) {
						android.util.Log.i (TAG, "Displaying a dialog to obtain user's password.");
						handler.sendEmptyMessage(Constants.GET_PASSWORD);
						synchronized(spicecomm) {
							spicecomm.wait();
						}
					}
					
					String ovirtCaFile = null;
					if (settings.isUsingCustomOvirtCa()) {
						ovirtCaFile = settings.getOvirtCaFile();
					} else {
						String caBundleFileName = getContext().getFilesDir() + "/ca-bundle.crt";
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
									handler.sendEmptyMessage(Constants.DIALOG_DISPLAY_VMS);
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
							spicecomm.wait(32000);
						}
					} catch (InterruptedException e) {}

					if (!spiceUpdateReceived) {
						handler.sendEmptyMessage(Constants.SPICE_CONNECT_FAILURE);
					}
				} catch (Throwable e) {
					if (stayConnected) {
						e.printStackTrace();
						android.util.Log.e(TAG, e.toString());
						
						if (e instanceof OutOfMemoryError) {
							disposeDrawable ();
							disconnectAndShowMessage(R.string.error_out_of_memory, R.string.error_dialog_title);
						}
					}
				}
			}
		};
		cThread.start();
	}
	
	
	/**
	 * Retreives the requested remote width.
	 */
	int getDesiredWidth () {
		int w = getWidth();
		android.util.Log.e(TAG, "Width requested: " + w);
		return w;
	}
	
	/**
	 * Retreives the requested remote height.
	 */
	int getDesiredHeight () {
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
	
	/**
	 * Set the device clipboard text with the string parameter.
	 * @param readServerCutText set the device clipboard to the text in this parameter.
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
	
	CanvasDrawableContainer reallocateDrawable(int width, int height) {
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
		return myDrawable; 
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
	void resetScroll()	{
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
	public void reDrawRemotePointer() {
		if (myDrawable != null) {
			myDrawable.moveCursorRect(pointer.getX(), pointer.getY());
			RectF r = myDrawable.getCursorRect();
			reDraw(r.left, r.top, r.width(), r.height());
		}
	}
	
	/**
	 * Moves soft cursor into a particular location.
	 * @param x
	 * @param y
	 */

    synchronized void softCursorMove(int x, int y) {
    	if (myDrawable.isNotInitSoftCursor()) {
    		initializeSoftCursor();
    	}
    	
    	if (!cursorBeingMoved) {
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
		myDrawable.setCursorRect(pointer.getX(), pointer.getY(), w, h, 0, 0);
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
        int version = android.os.Build.VERSION.SDK_INT;
        BaseInputConnection bic = null;
        if (!bb && version >= Build.VERSION_CODES.JELLY_BEAN) {
            bic = new BaseInputConnection(this, false) {
                final static String junk_unit = "%%%%%%%%%%";
                final static int multiple = 1000;
                Editable e;
                @Override
                public Editable getEditable() {
                    if (e == null) {
                        int numTotalChars = junk_unit.length()*multiple;
                        String junk = new String();
                        for (int i = 0; i < multiple ; i++) {
                            junk += junk_unit;
                        }
                        e = Editable.Factory.getInstance().newEditable(junk);
                        Selection.setSelection(e, numTotalChars);
                        if (RemoteCanvas.this.keyboard != null) {
                            RemoteCanvas.this.keyboard.skippedJunkChars = false;
                        }
                    }
                    return e;
                }
            };
        } else {
            bic = new BaseInputConnection(this, false);
        }
        
        outAttrs.actionLabel = null;
        outAttrs.inputType = InputType.TYPE_NULL;
        /* TODO: If people complain about kbd not working, this is a possible workaround to
         * test and add an option for.
        // Workaround for IME's that don't support InputType.TYPE_NULL.
        if (version >= 11) {
            outAttrs.inputType = InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;
        }*/
        return bic;
    }

	@Override
	public void onTextSelected(String selectedString) {
		settings.setVmname(selectedString);
		settings.saveToSharedPreferences(getContext());
		synchronized (spicecomm) {
			spicecomm.notify();
		}
	}
	
	@Override
	public void onTextObtained(String obtainedString) {
		settings.setPassword(obtainedString);
		synchronized (spicecomm) {
			spicecomm.notify();
		}
	}
	
    /**
     * Used to wait until getWidth and getHeight return sane values.
     */
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
