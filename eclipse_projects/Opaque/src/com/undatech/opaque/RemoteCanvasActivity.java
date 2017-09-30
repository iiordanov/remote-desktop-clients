/**
 * Copyright (C) 2013-2017 Iordan Iordanov
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

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

import com.undatech.opaque.R;
import com.undatech.opaque.dialogs.GetTextFragment;
import com.undatech.opaque.dialogs.SelectTextElementFragment;
import com.undatech.opaque.input.*;
import com.undatech.opaque.util.OnTouchViewMover;

import android.app.Dialog;
import android.app.AlertDialog;
import android.support.v4.app.FragmentManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.Vibrator;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.View.OnKeyListener;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.Toast;
import android.support.v7.widget.ActionMenuView.OnMenuItemClickListener;
import android.support.v7.widget.Toolbar;


public class RemoteCanvasActivity extends AppCompatActivity implements OnKeyListener,
                                                                      SelectTextElementFragment.OnFragmentDismissedListener,
                                                                      GetTextFragment.OnFragmentDismissedListener {
	private final static String TAG = "RemoteCanvasActivity";
	
	public RemoteCanvas canvas;

	private InputHandler inputHandler;
	Map<Integer, InputHandler> inputHandlerIdMap;
	 
	private ConnectionSettings connection;
	
	RemoteCanvasActivityHandler handler;
	
    private Vibrator myVibrator;
	
	RelativeLayout layoutKeys;
    LinearLayout layoutArrowKeys;
	ImageButton keyStow;
	ImageButton keyCtrl;
	boolean keyCtrlToggled;
	ImageButton keySuper;
	boolean keySuperToggled;
	ImageButton keyAlt;
	boolean keyAltToggled;
	ImageButton keyTab;
	ImageButton keyEsc;
	ImageButton keyShift;
	boolean keyShiftToggled;
	ImageButton keyUp;
	ImageButton keyDown;
	ImageButton keyLeft;
	ImageButton keyRight;
	volatile boolean softKeyboardUp;
	boolean hardKeyboardExtended;
	boolean extraKeysHidden = false;
	int prevBottomOffset = 0;
	Toolbar toolbar;

    
    /**
     * Enables sticky immersive mode if supported.
     */
    private void enableImmersive() {
        android.util.Log.i(TAG, "Enabling immersive mode");
        if (Constants.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
            canvas.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }
    
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
            super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enableImmersive();
        }
    }
    
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);
        
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);
        
        setContentView(R.layout.canvas);
        
        canvas = (RemoteCanvas) findViewById(R.id.canvas);

		startConnection();
	}
	
	private void startConnection() {
        Intent i = getIntent();
        String vvFileName = retrieveVvFileFromIntent(i);
        if (vvFileName == null) {
            android.util.Log.d(TAG, "Initializing session from connection settings.");
            connection = (ConnectionSettings)i.getSerializableExtra("com.undatech.opaque.ConnectionSettings");
            handler = new RemoteCanvasActivityHandler(this, canvas, connection);
            
            if (connection.getConnectionType().equals(getResources().getString(R.string.connection_type_pve))) {
                canvas.initializePve(connection, handler);
            } else {
                canvas.initialize(connection, handler);
            }
        } else {
            android.util.Log.d(TAG, "Initializing session from vv file: " + vvFileName);
            File f = new File(vvFileName);
            if (!f.exists()) {
                // Quit with an error if the file does not exist.
                MessageDialogs.displayMessageAndFinish(this, R.string.vv_file_not_found, R.string.error_dialog_title);
                return;
            }
            connection = new ConnectionSettings(Constants.DEFAULT_SETTINGS_FILE);
            connection.loadFromSharedPreferences(getApplicationContext());
            handler = new RemoteCanvasActivityHandler(this, canvas, connection);
            canvas.initialize(vvFileName, connection, handler);
        }
        
        // If we still don't have settings after this, we cannot continue.
        if (connection == null) {
            return;
        }
        
        canvas.setOnKeyListener(this);
        canvas.setFocusableInTouchMode(true);
        canvas.setDrawingCacheEnabled(false);
        
        // If rotation is disabled, fix the orientation to the current one.
        if (!connection.isRotationEnabled()) {
            int orientation = getResources().getConfiguration().orientation;
            if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            } else {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            }
        }

        
        
		// This code detects when the soft keyboard is up and sets an appropriate visibleHeight in the canvas.
		// When the keyboard is gone, it resets visibleHeight and pans zero distance to prevent us from being
		// below the desktop image (if we scrolled all the way down when the keyboard was up).
		// TODO: Move this into a separate thread, and post the visibility changes to the handler.
		//       to avoid occupying the UI thread with this.
        final View rootView = ((ViewGroup)findViewById(android.R.id.content)).getChildAt(0);
        rootView.getViewTreeObserver().addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                    if (canvas == null) {
                        return;
                    }
                    
                    Rect r = new Rect();
                    
                    rootView.getWindowVisibleDisplayFrame(r);
                    
                    // To avoid setting the visible height to a wrong value after an screen unlock event
                    // (when r.bottom holds the width of the screen rather than the height due to a rotation)
                    // we make sure r.top is zero (i.e. there is no notification bar and we are in full-screen mode)
                    // It's a bit of a hack.
                    if (r.top == 0) {
                    	if (canvas.myDrawable != null) {
                        	canvas.setVisibleDesktopHeight(r.bottom);
                    		canvas.relativePan(0, 0);
                    	}
                    }
                    
                    // Enable/show the toolbar if the keyboard is gone, and disable/hide otherwise.
                    // We detect the keyboard if more than 19% of the screen is covered.
                    int topBottomOffset = 0;
                    int leftRightOffset = 0;
                    int rootViewHeight = rootView.getHeight();
                    int rootViewWidth = rootView.getWidth();
                    if (r.bottom > rootViewHeight*0.81) {
                        softKeyboardUp = false;
                        topBottomOffset = rootViewHeight - r.bottom;
                        leftRightOffset = rootViewWidth - r.right;

                        // Soft Kbd gone, shift the meta keys and arrows down.
                        if (layoutKeys != null) {
                            layoutKeys.offsetTopAndBottom(topBottomOffset);
                            layoutArrowKeys.offsetLeftAndRight(leftRightOffset);
                            keyStow.offsetTopAndBottom(topBottomOffset);
                            if (prevBottomOffset != topBottomOffset) {
                                prevBottomOffset = topBottomOffset;
                                setExtraKeysVisibility(View.GONE, false);
                                canvas.invalidate();
                            }
                        }
                    } else {
                        softKeyboardUp = true;
                        topBottomOffset = r.bottom - rootViewHeight;
                        leftRightOffset = r.right - rootViewWidth;

                        //  Soft Kbd up, shift the meta keys and arrows up.
                        if (layoutKeys != null) {
                            layoutKeys.offsetTopAndBottom(topBottomOffset);
                            layoutArrowKeys.offsetLeftAndRight(leftRightOffset);
                            keyStow.offsetTopAndBottom(topBottomOffset);
                            if (extraKeysHidden) {
                                extraKeysHidden = !extraKeysHidden;
                                setExtraKeysVisibility(View.VISIBLE, true);
                                extraKeysHidden = !extraKeysHidden;
                                setExtraKeysVisibility(View.GONE, false);
                            } else {
                                extraKeysHidden = !extraKeysHidden;
                                setExtraKeysVisibility(View.GONE, false);
                                extraKeysHidden = !extraKeysHidden;
                                setExtraKeysVisibility(View.VISIBLE, true);
                            }
                            canvas.invalidate();
                            prevBottomOffset = topBottomOffset;
                        }
                    }
                    setKeyStowDrawableAndVisibility();
                    enableImmersive();
             }
        });

        // Initialize and define actions for on-screen keys.
		initializeOnScreenKeys ();
		
	    myVibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);
	    
		// Initialize map from XML IDs to input handlers.
		inputHandlerIdMap = new HashMap<Integer, InputHandler>();
		inputHandlerIdMap.put(R.id.inputMethodDirectSwipePan, new InputHandlerDirectSwipePan(this, canvas, myVibrator));
		inputHandlerIdMap.put(R.id.inputMethodDirectDragPan,  new InputHandlerDirectDragPan (this, canvas, myVibrator));
		inputHandlerIdMap.put(R.id.inputMethodTouchpad,       new InputHandlerTouchpad      (this, canvas, myVibrator));
		inputHandlerIdMap.put(R.id.inputMethodSingleHanded,   new InputHandlerSingleHanded  (this, canvas, myVibrator));
		
		android.util.Log.e(TAG, "connection.getInputMethod(): " + connection.getInputMethod());
		inputHandler = idToInputHandler(connection.getInputMethod());
		
		toolbar = (Toolbar) findViewById(R.id.toolbar);
		toolbar.setTitle("");
        toolbar.getBackground().setAlpha(64);
		setSupportActionBar(toolbar);
	}
	
    
    /**
     * Gets called when a new intent comes in.
     */
    @Override
    protected void onNewIntent (Intent i) {
        android.util.Log.e(TAG, "onNewIntent called");
        canvas.disconnectAndCleanUp();
        setIntent(i);
        startConnection();
    }
    
    /**
     * Outputs the given InputStream to a file.
     * @param is
     * @param file
     * @throws IOException
     */
    void outputToFile (InputStream is, File file) throws IOException {
        BufferedInputStream bis = new BufferedInputStream(is);
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        
        byte[] data = new byte[Constants.URL_BUFFER_SIZE];
        int current = 0;
        
        while((current = bis.read(data, 0, data.length)) != -1){
            buffer.write(data, 0, current);
        }
        
        FileOutputStream fos = new FileOutputStream(file);
        fos.write(buffer.toByteArray());
        fos.close();
    }
    
    /**
     * Retrieves a vv file from the intent if possible and returns the path to it.
     * @param i
     * @return the vv file name or NULL if no file was discovered.
     */
    private String retrieveVvFileFromIntent(Intent i) {
        final Uri data = i.getData();
        String vvFileName = null;
        final String tempVvFile = getFilesDir() + "/tempfile.vv";
        int msgId = 0;

        android.util.Log.d(TAG, "Got intent: " + i.toString());

        if (data != null) {
            android.util.Log.d(TAG, "Got data: " + data.toString());
            final String dataString = data.toString();
            if (dataString.startsWith("http")) {
                android.util.Log.d(TAG, "Intent is with http scheme.");
                msgId = R.string.error_failed_to_download_vv_http;
                deleteMyFile(tempVvFile);
                
                // Spin up a thread to grab the file over the network.
                Thread t = new Thread () {
                    @Override
                    public void run () {
                        try {
                            // Download the file and write it out.
                            URL url = new URL (data.toString());
                            File file = new File(tempVvFile);
                            
                            URLConnection ucon = url.openConnection();
                            outputToFile(ucon.getInputStream(), new File(tempVvFile));
                            
                            synchronized(RemoteCanvasActivity.this) {
                                RemoteCanvasActivity.this.notify();
                            }
                        } catch (IOException e) {
                            int what = Constants.VV_OVER_HTTP_FAILURE;
                            if (dataString.startsWith("https")) {
                                what = Constants.VV_OVER_HTTPS_FAILURE;
                            }
                            // Quit with an error we could not download the .vv file.
                            handler.sendEmptyMessage(what);
                        }
                    }
                };
                t.start();
                
                synchronized (this) {
                    try {
                        this.wait(Constants.VV_GET_FILE_TIMEOUT);
                    } catch (InterruptedException e) {
                        vvFileName = null;
                        e.printStackTrace();
                    }
                    vvFileName = tempVvFile;
                }
            } else if (dataString.startsWith("file")) {
                android.util.Log.d(TAG, "Intent is with file scheme.");
                msgId = R.string.error_failed_to_obtain_vv_file;
                vvFileName = data.getPath();
            } else if (dataString.startsWith("content")) {
                android.util.Log.d(TAG, "Intent is with content scheme.");
                msgId = R.string.error_failed_to_obtain_vv_content;
                deleteMyFile(tempVvFile);
                
                try {
                    outputToFile(getContentResolver().openInputStream(data), new File(tempVvFile));
                    vvFileName = tempVvFile;
                } catch (IOException e) {
                    android.util.Log.e(TAG, "Could not write temp file out.");
                    e.printStackTrace();
                }
            }
            
            // Check if we were successful in obtaining a file and put up an error dialog if not.
            if (dataString.startsWith("http") || dataString.startsWith("file") || dataString.startsWith("content")) {
                if (vvFileName == null)
                    MessageDialogs.displayMessageAndFinish(this, msgId, R.string.error_dialog_title);
            }
            android.util.Log.d(TAG, "Got filename: " + vvFileName);
        }

        return vvFileName;
    }
	
    private void deleteMyFile (String path) {
        new File(path).delete();
    }
    
	
	private void setKeyStowDrawableAndVisibility() {
		Drawable replacer = null;
		if (layoutKeys.getVisibility() == View.GONE)
			replacer = getResources().getDrawable(R.drawable.showkeys);
		else
			replacer = getResources().getDrawable(R.drawable.hidekeys);
		keyStow.setBackgroundDrawable(replacer);

		if (connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_OFF)
			keyStow.setVisibility(View.GONE);
		else
			keyStow.setVisibility(View.VISIBLE);
	}
	
	/**
	 * Initializes the on-screen keys for meta keys and arrow keys.
	 */
	private void initializeOnScreenKeys () {
		
        layoutKeys = (RelativeLayout) findViewById(R.id.layoutKeys);
        layoutArrowKeys = (LinearLayout) findViewById(R.id.layoutArrowKeys);

		keyStow = (ImageButton)    findViewById(R.id.keyStow);
		setKeyStowDrawableAndVisibility();
		keyStow.setOnClickListener(new OnClickListener () {
			@Override
			public void onClick(View arg0) {
				if (layoutKeys.getVisibility() == View.VISIBLE) {
					extraKeysHidden = true;
					setExtraKeysVisibility(View.GONE, false);
				} else {
					extraKeysHidden = false;
					setExtraKeysVisibility(View.VISIBLE, true);
				}
    			layoutKeys.offsetTopAndBottom(prevBottomOffset);
    			setKeyStowDrawableAndVisibility();
			}
		});

		// Define action of tab key and meta keys.
		keyTab = (ImageButton) findViewById(R.id.keyTab);
		keyTab.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = KeyEvent.KEYCODE_TAB;
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyTab.setImageResource(R.drawable.tabon);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyTab.setImageResource(R.drawable.taboff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});

		keyEsc = (ImageButton) findViewById(R.id.keyEsc);
		keyEsc.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = 111; /* KEYCODE_ESCAPE */
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyEsc.setImageResource(R.drawable.escon);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyEsc.setImageResource(R.drawable.escoff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});
		
		keyCtrl = (ImageButton) findViewById(R.id.keyCtrl);
		keyCtrl.setOnClickListener(new OnClickListener () {
			@Override
			public void onClick(View arg0) {
				boolean on = canvas.getKeyboard().onScreenCtrlToggle();
				keyCtrlToggled = false;
				if (on)
					keyCtrl.setImageResource(R.drawable.ctrlon);
				else
					keyCtrl.setImageResource(R.drawable.ctrloff);
			}
		});
		
		keyCtrl.setOnLongClickListener(new OnLongClickListener () {
			@Override
			public boolean onLongClick(View arg0) {
				myVibrator.vibrate(Constants.SHORT_VIBRATION);
				boolean on = canvas.getKeyboard().onScreenCtrlToggle();
				keyCtrlToggled = true;
				if (on)
					keyCtrl.setImageResource(R.drawable.ctrlon);
				else
					keyCtrl.setImageResource(R.drawable.ctrloff);
				return true;
			}
		});
		
		keySuper = (ImageButton) findViewById(R.id.keySuper);
		keySuper.setOnClickListener(new OnClickListener () {
			@Override
			public void onClick(View arg0) {
				boolean on = canvas.getKeyboard().onScreenSuperToggle();
				keySuperToggled = false;
				if (on)
					keySuper.setImageResource(R.drawable.superon);
				else
					keySuper.setImageResource(R.drawable.superoff);
			}
		});
		
		keySuper.setOnLongClickListener(new OnLongClickListener () {
			@Override
			public boolean onLongClick(View arg0) {
				myVibrator.vibrate(Constants.SHORT_VIBRATION);
				boolean on = canvas.getKeyboard().onScreenSuperToggle();
				keySuperToggled = true;
				if (on)
					keySuper.setImageResource(R.drawable.superon);
				else
					keySuper.setImageResource(R.drawable.superoff);
				return true;
			}
		});
		
		keyAlt = (ImageButton) findViewById(R.id.keyAlt);
		keyAlt.setOnClickListener(new OnClickListener () {
			@Override
			public void onClick(View arg0) {
				boolean on = canvas.getKeyboard().onScreenAltToggle();
				keyAltToggled = false;
				if (on)
					keyAlt.setImageResource(R.drawable.alton);
				else
					keyAlt.setImageResource(R.drawable.altoff);
			}
		});
		
		keyAlt.setOnLongClickListener(new OnLongClickListener () {
			@Override
			public boolean onLongClick(View arg0) {
				myVibrator.vibrate(Constants.SHORT_VIBRATION);
				boolean on = canvas.getKeyboard().onScreenAltToggle();
				keyAltToggled = true;
				if (on)
					keyAlt.setImageResource(R.drawable.alton);
				else
					keyAlt.setImageResource(R.drawable.altoff);
				return true;
			}
		});
		
		keyShift = (ImageButton) findViewById(R.id.keyShift);
		keyShift.setOnClickListener(new OnClickListener () {
			@Override
			public void onClick(View arg0) {
				boolean on = canvas.getKeyboard().onScreenShiftToggle();
				keyShiftToggled = false;
				if (on)
					keyShift.setImageResource(R.drawable.shifton);
				else
					keyShift.setImageResource(R.drawable.shiftoff);
			}
		});
		
		keyShift.setOnLongClickListener(new OnLongClickListener () {
			@Override
			public boolean onLongClick(View arg0) {
				myVibrator.vibrate(Constants.SHORT_VIBRATION);
				boolean on = canvas.getKeyboard().onScreenShiftToggle();
				keyShiftToggled = true;
				if (on)
					keyShift.setImageResource(R.drawable.shifton);
				else
					keyShift.setImageResource(R.drawable.shiftoff);
				return true;
			}
		});
		
		// Define action of arrow keys.
		keyUp = (ImageButton) findViewById(R.id.keyUpArrow);
		keyUp.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = KeyEvent.KEYCODE_DPAD_UP;
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyUp.setImageResource(R.drawable.upon);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyUp.setImageResource(R.drawable.upoff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});
		
		keyDown = (ImageButton) findViewById(R.id.keyDownArrow);
		keyDown.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = KeyEvent.KEYCODE_DPAD_DOWN;
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyDown.setImageResource(R.drawable.downon);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyDown.setImageResource(R.drawable.downoff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});

		keyLeft = (ImageButton) findViewById(R.id.keyLeftArrow);
		keyLeft.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = KeyEvent.KEYCODE_DPAD_LEFT;
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyLeft.setImageResource(R.drawable.lefton);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyLeft.setImageResource(R.drawable.leftoff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});

		keyRight = (ImageButton) findViewById(R.id.keyRightArrow);
		keyRight.setOnTouchListener(new OnTouchListener () {
			@Override
			public boolean onTouch(View arg0, MotionEvent e) {
				RemoteKeyboard k = canvas.getKeyboard();
				int key = KeyEvent.KEYCODE_DPAD_RIGHT;
				if (e.getAction() == MotionEvent.ACTION_DOWN) {
					myVibrator.vibrate(Constants.SHORT_VIBRATION);
					keyRight.setImageResource(R.drawable.righton);
					k.repeatKeyEvent(key, new KeyEvent(e.getAction(), key));
					return true;	
				} else if (e.getAction() == MotionEvent.ACTION_UP) {
					keyRight.setImageResource(R.drawable.rightoff);
					resetOnScreenKeys (0);
					k.stopRepeatingKeyEvent();
					return true;
				}
				return false;
			}
		});
	}

	/**
	 * Resets the state and image of the on-screen keys.
	 */
	private void resetOnScreenKeys (int keyCode) {
		// Do not reset on-screen keys if keycode is SHIFT.
		switch (keyCode) {
		case KeyEvent.KEYCODE_SHIFT_LEFT:
		case KeyEvent.KEYCODE_SHIFT_RIGHT: return;
		}
		if (!keyCtrlToggled) {
			keyCtrl.setImageResource(R.drawable.ctrloff);
			canvas.getKeyboard().onScreenCtrlOff();
		}
		if (!keyAltToggled) {
			keyAlt.setImageResource(R.drawable.altoff);
			canvas.getKeyboard().onScreenAltOff();
		}
		if (!keySuperToggled) {
			keySuper.setImageResource(R.drawable.superoff);
			canvas.getKeyboard().onScreenSuperOff();
		}
		if (!keyShiftToggled) {
			keyShift.setImageResource(R.drawable.shiftoff);
			canvas.getKeyboard().onScreenShiftOff();
		}
	}

	
	/**
	 * Sets the visibility of the extra keys appropriately.
	 */
	private void setExtraKeysVisibility (int visibility, boolean forceVisible) {
		Configuration config = getResources().getConfiguration();
		boolean makeVisible = forceVisible;
		if (config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO)
			makeVisible = true;

		if (!extraKeysHidden && makeVisible && 
			connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_ON) {
			layoutKeys.setVisibility(View.VISIBLE);
			layoutKeys.invalidate();
			return;
		}
		
		if (visibility == View.GONE) {
			layoutKeys.setVisibility(View.GONE);
			layoutKeys.invalidate();
		}
	}
	
	/*
	 * TODO: REMOVE THIS AS SOON AS POSSIBLE.
	 * onPause: This is an ugly hack for the Playbook, because the Playbook hides the keyboard upon unlock.
	 * This causes the visible height to remain less, as if the soft keyboard is still up. This hack must go 
	 * away as soon as the Playbook doesn't need it anymore.
	 */
	@Override
	protected void onPause(){
		super.onPause();
		try {
			InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
			imm.hideSoftInputFromWindow(canvas.getWindowToken(), 0);
		} catch (NullPointerException e) { }
	}

	/*
	 * TODO: REMOVE THIS AS SOON AS POSSIBLE.
	 * onResume: This is an ugly hack for the Playbook which hides the keyboard upon unlock. This causes the visible
	 * height to remain less, as if the soft keyboard is still up. This hack must go away as soon
	 * as the Playbook doesn't need it anymore.
	 */
	@Override
	protected void onResume(){
		super.onResume();
		Log.i(TAG, "onResume called.");
		try {
			canvas.postInvalidateDelayed(600);
		} catch (NullPointerException e) { }
	}
	
	ConnectionSettings getConnection() {
		return connection;
	}
	
	@Override
	protected Dialog onCreateDialog(int dialogID) {
		switch (dialogID) {
		// TODO: Introduce the ability to send text collected from an EditText in the future.
		case R.id.menuHelpInputMethod:
			return createHelpDialog ();
		}
		return createHelpDialog ();
	}

	/**
	 * Creates the help dialog for this activity.
	 */
	private Dialog createHelpDialog() {
	    AlertDialog.Builder adb = new AlertDialog.Builder(this)
	    		.setMessage(R.string.input_method_help_text)
	    		.setPositiveButton(R.string.close,
	    				new DialogInterface.OnClickListener() {
	    					public void onClick(DialogInterface dialog,
	    							int whichButton) {
	    						// We don't have to do anything.
	    					}
	    				});
	    Dialog d = adb.setView(new ListView (this)).create();
	    WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
	    lp.copyFrom(d.getWindow().getAttributes());
	    lp.width = WindowManager.LayoutParams.FILL_PARENT;
	    lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
	    d.show();
	    d.getWindow().setAttributes(lp);
	    return d;
	}

	/**
	 * This runnable fixes things up after a rotation.
	 */
	private Runnable rotationCorrector = new Runnable() {
		public void run() {
			try { correctAfterRotation (); } catch (NullPointerException e) { }
		}
	};

	/**
	 * This function is called by the rotationCorrector runnable
	 * to fix things up after a rotation.
	 */
	private void correctAfterRotation () {
		// Its quite common to see NullPointerExceptions here when this function is called
		// at the point of disconnection. Hence, we catch and ignore the error.
		float oldScale = canvas.canvasZoomer.getZoomFactor();
		int x = canvas.absX;
		int y = canvas.absY;
		canvas.canvasZoomer.resetScaling();
		float newScale = canvas.canvasZoomer.getZoomFactor();
		canvas.canvasZoomer.changeZoom(oldScale/newScale);
		newScale = canvas.canvasZoomer.getZoomFactor();
		if (newScale <= oldScale) {
			canvas.absX = x;
			canvas.absY = y;
			canvas.relativePan(0, 0);
		}
		if (connection.isRequestingNewDisplayResolution()) {
			canvas.spicecomm.requestNewResolutionIfNeeded();
		}

	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		enableImmersive();
		if (connection.isRotationEnabled()) {
			try {
				setExtraKeysVisibility(View.GONE, false);
				// Correct a couple of times just in case. There is no visual effect.
				handler.postDelayed(rotationCorrector, 600);
				handler.postDelayed(rotationCorrector, 1200);
			} catch (NullPointerException e) { }
		}
	}
	
	@Override
	protected void onStart() {
		super.onStart();
		try {
			canvas.postInvalidateDelayed(800);
		} catch (NullPointerException e) { }
	}
	
	@Override
	protected void onStop() {
		super.onStop();
	}
	
	@Override
	protected void onRestart() {
		super.onRestart();
		try {
			canvas.postInvalidateDelayed(1000);
		} catch (NullPointerException e) { }
	}
	
	/**
	 * Try to find an input handler by the specified string ID.
	 * @param inputHandlerId
	 * @return
	 */
	InputHandler idToInputHandler(String inputHandlerId) {
		Iterator<Integer> ids = inputHandlerIdMap.keySet().iterator();
		while (ids.hasNext()) {
			Integer id = ids.next();
			InputHandler h = inputHandlerIdMap.get(id);
			if (inputHandlerId.equals(h.getId())) {
				return h;
			}
		}
		return null;
	}
	
	/**
	 * If id corresponds to an input handler, return the XML id of the menu item corresponding
	 * to the input handler, otherwise return -1.
	 * @param inputHandlerId
	 * @return
	 */
	int inputHandlerIdToXmlId (String inputHandlerId) {
		Iterator<Integer> ids = inputHandlerIdMap.keySet().iterator();
		while (ids.hasNext()) {
			Integer id = ids.next();
			InputHandler h = inputHandlerIdMap.get(id);
			if (inputHandlerId.equals(h.getId())) {
				return id;
			}
		}
		return -1;
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		if (canvas != null)
			canvas.disconnectAndCleanUp();
		canvas = null;
		connection = null;
		inputHandler = null;
		System.gc();
	}
	
	@Override
	public boolean onKey(View v, int keyCode, KeyEvent evt) {
		boolean consumed = false;
		
		if (keyCode == KeyEvent.KEYCODE_MENU) {
			if (evt.getAction() == KeyEvent.ACTION_DOWN)
				return super.onKeyDown(keyCode, evt);
			else
				return super.onKeyUp(keyCode, evt);
		}
		
		try {
			if (evt.getAction() == KeyEvent.ACTION_DOWN || evt.getAction() == KeyEvent.ACTION_MULTIPLE) {
				consumed = inputHandler.onKeyDown(keyCode, evt);
			} else if (evt.getAction() == KeyEvent.ACTION_UP){
				consumed = inputHandler.onKeyUp(keyCode, evt);
			}
			resetOnScreenKeys (keyCode);
		} catch (NullPointerException e) { }
		
		return consumed;
	}

	public void displayInputModeInfo(boolean showLonger) {
		if (showLonger) {
			final Toast t = Toast.makeText(this, inputHandler.getDescription(), Toast.LENGTH_LONG);
			TimerTask tt = new TimerTask () {
				@Override
				public void run() {
					t.show();
					try { Thread.sleep(2000); } catch (InterruptedException e) { }
					t.show();
				}};
			new Timer ().schedule(tt, 2000);
			t.show();
		} else {
			Toast t = Toast.makeText(this, inputHandler.getDescription(), Toast.LENGTH_SHORT);
			t.show();
		}
	}
	
	// Send touch events or mouse events like button clicks to be handled.
	@Override
	public boolean onTouchEvent(MotionEvent event) {
		try {
			return inputHandler.onTouchEvent(event);
		} catch (NullPointerException e) { }
		return false;
	}
	
	// Send e.g. mouse events like hover and scroll to be handled.
	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
        // Ignore TOOL_TYPE_FINGER events that come from the touchscreen with y == 0.0
        // which cause pointer jumping trouble for some users.
        int a = event.getAction();
        if (! ( (a == MotionEvent.ACTION_HOVER_ENTER ||
                 a == MotionEvent.ACTION_HOVER_EXIT  ||
                 a == MotionEvent.ACTION_HOVER_MOVE) &&
                event.getSource() == InputDevice.SOURCE_TOUCHSCREEN &&
                event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER
              ) ) {
            try {
                return inputHandler.onTouchEvent(event);
            } catch (NullPointerException e) { }
        }
        return super.onGenericMotionEvent(event);
	}
	
	public float getSensitivity() {
		return 2.0f;
	}
	
	public boolean getAccelerationEnabled() {
		return true;
	}
	
	final long hideToolbarDelay = 2500;
	ToolbarHiderRunnable toolbarHider = new ToolbarHiderRunnable();
	
	public void showToolbar() {
	    if (!softKeyboardUp) {
	        getSupportActionBar().show();
	        handler.removeCallbacks(toolbarHider);
	        handler.postAtTime(toolbarHider, SystemClock.uptimeMillis() + hideToolbarDelay);
	    }
	}
	
	private class ToolbarHiderRunnable implements Runnable {
		public void run() {
		    getSupportActionBar().hide();
		}
	}

    public void showKeyboard(MenuItem menuItem) {
        android.util.Log.i(TAG, "Showing keyboard and hiding action bar");
        InputMethodManager inputMgr = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        inputMgr.showSoftInput(canvas, 0);
        softKeyboardUp = true;
        getSupportActionBar().hide();
    }

	@Override
	public boolean onMenuOpened(int featureId, Menu menu) {
	    if(menu != null){
	        android.util.Log.i(TAG, "Menu opened, disabling hiding action bar");
	        handler.removeCallbacks(toolbarHider);
	    }
	    return super.onMenuOpened(featureId, menu);
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		Log.e (TAG, "onCreateOptionsMenu called");
		try {
			getMenuInflater().inflate(R.menu.connectedmenu, menu);
			
			// Check the proper input method item.
			Menu inputMenu = menu.findItem(R.id.menuInputMethod).getSubMenu();	
			inputMenu.findItem(inputHandlerIdToXmlId (connection.getInputMethod())).setChecked(true);
			
			// Set the text of the Extra Keys menu item appropriately.
			if (connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_ON)
				menu.findItem(R.id.menuExtraKeys).setTitle(R.string.extra_keys_disable);
			else
				menu.findItem(R.id.menuExtraKeys).setTitle(R.string.extra_keys_enable);

			OnTouchListener moveListener = new OnTouchViewMover(toolbar, handler, toolbarHider, hideToolbarDelay);
			ImageButton moveButton = new ImageButton(this);
			moveButton.setBackgroundResource(R.drawable.ic_btn_move);
			MenuItem moveToolbar = menu.findItem(R.id.moveToolbar);
			moveToolbar.setActionView(moveButton);
			moveToolbar.getActionView().setOnTouchListener(moveListener);
		} catch (NullPointerException e) {
			Log.e (TAG, "There was an error: " + e.getMessage());
		}
		return super.onCreateOptionsMenu(menu);
	}
	
	@Override
	public boolean onOptionsItemSelected(MenuItem menuItem) {
		int itemID = menuItem.getItemId();
		switch (itemID) {
		case R.id.menuExtraKeys:
			if (connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_ON) {
				connection.setExtraKeysToggleType(Constants.EXTRA_KEYS_OFF);
				menuItem.setTitle(R.string.extra_keys_enable);
				setExtraKeysVisibility(View.GONE, false);
			} else {
				connection.setExtraKeysToggleType(Constants.EXTRA_KEYS_ON);
				menuItem.setTitle(R.string.extra_keys_disable);
				setExtraKeysVisibility(View.VISIBLE, false);
				extraKeysHidden = false;
			}
			setKeyStowDrawableAndVisibility();
			connection.saveToSharedPreferences(getApplicationContext());
			return true;
		case R.id.menuDisconnect:
			canvas.disconnectAndCleanUp();
			finish();
			return true;
		case R.id.menuSendCAD:
			canvas.getKeyboard().keyEvent(112, new KeyEvent(KeyEvent.ACTION_DOWN, 112),
													  RemoteKeyboard.CTRL_ON_MASK|RemoteKeyboard.ALT_ON_MASK);
			canvas.getKeyboard().keyEvent(112, new KeyEvent(KeyEvent.ACTION_UP, 112));
			return true;
		case R.id.menuHelpInputMethod:
			showDialog(R.id.menuHelpInputMethod);
			return true;
		default:
			InputHandler newInputHandler = inputHandlerIdMap.get(menuItem.getItemId());
			if (newInputHandler != null) {
				inputHandler = newInputHandler;
				connection.setInputMethod(newInputHandler.getId());
				menuItem.setChecked(true);
				displayInputModeInfo(true);
				connection.saveToSharedPreferences(getApplicationContext());
				return true;
			}
		}
		return super.onOptionsItemSelected(menuItem);
	}

    @Override
    public void onTextSelected(String selectedString) {
        canvas.progressDialog.show();
        connection.setVmname(canvas.vmNameToId.get(selectedString));
        connection.saveToSharedPreferences(this);
        synchronized (canvas.spicecomm) {
            canvas.spicecomm.notify();
        }
    }
    
    @Override
    public void onTextObtained(String id, String obtainedString) {
        android.util.Log.i(TAG, "onTextObtained called with id: " + id);
        canvas.progressDialog.show();
        
        if (id.equals(Constants.GET_PASSWORD_ID)) {
            connection.setPassword(obtainedString);
        } else if (id.equals(Constants.GET_OTP_CODE_ID)) {
            connection.setOtpCode(obtainedString);
        }
        
        synchronized (canvas.spicecomm) {
            canvas.spicecomm.notify();
        }
        
        FragmentManager fm = this.getSupportFragmentManager();
        fm.beginTransaction().remove(fm.findFragmentByTag(id)).commit();
    }
}
