/**
 * Copyright (C) 2012-2017 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
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

//
// CanvasView is the Activity for showing VNC Desktop.
//
package com.iiordanov.bVNC;

import static com.iiordanov.bVNC.dialogs.MetaKeyDialog.tryPopulateKeysInListWhereFieldMatchesValue;

import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.os.Vibrator;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.dialogs.EnterTextDialog;
import com.iiordanov.bVNC.dialogs.MetaKeyDialog;
import com.iiordanov.bVNC.input.IgnoringMouseInputListener;
import com.iiordanov.bVNC.input.MetaKeyBean;
import com.iiordanov.bVNC.input.Panner;
import com.iiordanov.bVNC.input.RemoteCanvasHandler;
import com.iiordanov.bVNC.input.RemoteClientsInputListener;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.TouchInputHandler;
import com.iiordanov.bVNC.input.TouchInputHandlerDirectDragPan;
import com.iiordanov.bVNC.input.TouchInputHandlerDirectSwipePan;
import com.iiordanov.bVNC.input.TouchInputHandlerSingleHanded;
import com.iiordanov.bVNC.input.TouchInputHandlerTouchpad;
import com.iiordanov.bVNC.protocol.GettingConnectionSettingsException;
import com.iiordanov.bVNC.protocol.RemoteConnection;
import com.iiordanov.bVNC.protocol.RemoteConnectionFactory;
import com.iiordanov.util.SamsungDexUtils;
import com.undatech.opaque.Connection;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.dialogs.SelectTextElementFragment;
import com.undatech.opaque.util.GeneralUtils;
import com.undatech.opaque.util.OnTouchViewMover;
import com.undatech.opaque.util.RemoteToolbar;
import com.undatech.remoteClientUi.R;

import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Timer;
import java.util.TimerTask;

@SuppressLint("ClickableViewAccessibility")
public class RemoteCanvasActivity extends AppCompatActivity implements
        SelectTextElementFragment.OnFragmentDismissedListener {

    public static final int[] inputModeIds = {R.id.itemInputTouchpad,
            R.id.itemInputTouchPanZoomMouse,
            R.id.itemInputDragPanZoomMouse,
            R.id.itemInputSingleHanded};
    public static final Map<Integer, String> inputModeMap;
    private final static String TAG = "RemoteCanvasActivity";
    private static final int[] scalingModeIds = {R.id.itemZoomable, R.id.itemFitToScreen,
            R.id.itemOneToOne};

    static {
        Map<Integer, String> temp = new HashMap<>();
        temp.put(R.id.itemInputTouchpad, TouchInputHandlerTouchpad.ID);
        temp.put(R.id.itemInputDragPanZoomMouse, TouchInputHandlerDirectDragPan.ID);
        temp.put(R.id.itemInputTouchPanZoomMouse, TouchInputHandlerDirectSwipePan.ID);
        temp.put(R.id.itemInputSingleHanded, TouchInputHandlerSingleHanded.ID);
        inputModeMap = Collections.unmodifiableMap(temp);
    }

    final long hideToolbarDelay = 2500;
    TouchInputHandler touchInputHandler;
    Panner panner;
    Handler handler;
    RelativeLayout layoutKeys;
    LinearLayout layoutArrowKeys;
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
    boolean extraKeysHidden = false;
    volatile boolean softKeyboardUp;
    RemoteToolbar toolbar;
    View rootView;
    ActionBarHider actionBarHider = new ActionBarHider();
    ActionBarShower actionBarShower = new ActionBarShower();
    KeyboardIconShower keyboardIconShower = new KeyboardIconShower();
    private Vibrator myVibrator;
    private RemoteCanvas canvas;
    private RemoteConnection remoteConnection;
    private MenuItem[] inputModeMenuItems;
    private MenuItem[] scalingModeMenuItems;
    private TouchInputHandler[] inputModeHandlers;
    private Connection connection;
    public RemoteClientsInputListener inputListener;
    private ImageButton keyboardIconForAndroidTv;
    float keyboardIconForAndroidTvX = Float.MAX_VALUE;
    IgnoringMouseInputListener ignoringMouseInputListener = new IgnoringMouseInputListener();

    OnTouchViewMover toolbarMover;
    ActionBarPositionSaver toolbarPositionSaver = new ActionBarPositionSaver();

    /**
     * This runnable enables immersive mode.
     */
    private final Runnable immersiveEnabler = new Runnable() {
        public void run() {
            try {
                if (Utils.querySharedPreferenceBoolean(RemoteCanvasActivity.this,
                        Constants.disableImmersiveTag)) {
                    return;
                }

                if (Constants.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
                    canvas.setSystemUiVisibility(
                            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
                }

            } catch (Exception e) {
                Log.d(TAG, "Ignored Exception while enabling immersive mode");
            }
        }
    };
    /**
     * This runnable disables immersive mode.
     */
    private final Runnable immersiveDisabler = new Runnable() {
        public void run() {
            try {
                if (Constants.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
                    canvas.setSystemUiVisibility(
                            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
                }

            } catch (Exception e) {
                Log.d(TAG, "Ignored Exception while disabling immersive mode");
            }
        }
    };
    /**
     * This runnable fixes things up after a rotation.
     */
    private final Runnable rotationCorrector = () -> {
        try {
            correctAfterRotation();
        } catch (Exception e) {
            Log.d(TAG, "Ignoring Exception on rotationCorrector run.");
        }
    };

    private void correctAfterRotation() throws Exception {
        Log.d(TAG, "correctAfterRotation");
        canvas.waitUntilInflated();

        // Its quite common to see NullPointerExceptions here when this function is called
        // at the point of disconnection. Hence, we catch and ignore the error.
        float oldScale = canvas.canvasZoomer.getZoomFactor();
        int x = canvas.absoluteXPosition;
        int y = canvas.absoluteYPosition;
        canvas.canvasZoomer.setScaleTypeForActivity(this);
        float newScale = canvas.canvasZoomer.getZoomFactor();
        canvas.canvasZoomer.changeZoom(this, oldScale / newScale, 0, 0);
        newScale = canvas.canvasZoomer.getZoomFactor();
        if (newScale <= oldScale &&
                canvas.canvasZoomer.getScaleType() != ImageView.ScaleType.FIT_CENTER) {
            canvas.absoluteXPosition = x;
            canvas.absoluteYPosition = y;
            canvas.resetScroll();
        }
        remoteConnection.correctAfterRotation();
    }

    private MetaKeyBean lastSentKey;

    /**
     * Enables sticky immersive mode if supported.
     */
    private void enableImmersive() {
        if (handler != null) {
            handler.removeCallbacks(immersiveEnabler);
            handler.postDelayed(immersiveEnabler, 200);
        }
    }

    /**
     * Disables sticky immersive mode.
     */
    private void disableImmersive() {
        handler.removeCallbacks(immersiveDisabler);
        handler.postDelayed(immersiveDisabler, 200);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enableImmersive();
        }
    }

    @SuppressLint("ObsoleteSdkInt")
    @Override
    public void onCreate(Bundle icicle) {
        Log.d(TAG, "OnCreate called");
        super.onCreate(icicle);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);

        Utils.showMenu(this);

        setContentView(R.layout.canvas);

        canvas = findViewById(R.id.canvas);
        keyboardIconForAndroidTv = findViewById(R.id.keyboardIconForAndroidTv);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            canvas.setDefaultFocusHighlightEnabled(false);
        }
        if (android.os.Build.VERSION.SDK_INT >= 9) {
            android.os.StrictMode.ThreadPolicy policy = new android.os.StrictMode.ThreadPolicy.Builder().permitAll().build();
            android.os.StrictMode.setThreadPolicy(policy);
        }

        myVibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);

        View decorView = getWindow().getDecorView();
        decorView.setOnSystemUiVisibilityChangeListener
                (visibility -> {
                    try {
                        remoteConnection.correctAfterRotation();
                    } catch (Exception e) {
                        Log.d(TAG, "Ignoring Exception during SystemUiVisibilityChangeListener execution");
                    }
                });

        Runnable setModes = () -> {
            try {
                setModes();
            } catch (NullPointerException e) {
                Log.d(TAG, "Ignored NullPointerException while running setModes: " + Log.getStackTraceString(e));
            }
        };
        Runnable hideKeyboardAndExtraKeys = () -> {
            try {
                hideKeyboardAndExtraKeys();
            } catch (NullPointerException e) {
                Log.d(TAG, "Ignoring NullPointerException while running hideKeyboardAndExtraKeys.");
            }
        };

        setApplicationSpecificSettings();

        try {
            connection = AbstractConnectionBean.getRemoteConnectionSettings(getIntent(), this, isMasterPasswordEnabled());
        } catch (GettingConnectionSettingsException e) {
            Utils.showFatalErrorMessage(this, getResources().getString(e.getErrorStringId()));
            return;
        }
        remoteConnection = new RemoteConnectionFactory(this, connection, canvas, hideKeyboardAndExtraKeys).build();

        if (connection != null && connection.isReadyForConnection()) {
            handler = new RemoteCanvasHandler(this, canvas, remoteConnection, connection, setModes);
            handler.sendEmptyMessage(RemoteClientLibConstants.REINIT_SESSION);
            continueConnecting();
        } else {
            showConnectionScreenOrExitIfNotReadyForConnecting(connection);
        }

        Log.d(TAG, "OnCreate complete");
    }

    private void setApplicationSpecificSettings() {
        if (Utils.isOpaque(this)) {
            setVolumeControlStream(AudioManager.STREAM_MUSIC);
        } else {
            if (Utils.querySharedPreferenceBoolean(this, Constants.keepScreenOnTag))
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            if (Utils.querySharedPreferenceBoolean(this, Constants.forceLandscapeTag))
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        }
    }

    private void showConnectionScreenOrExitIfNotReadyForConnecting(Connection connection) {
        // we need to save the connection to display the loading screen, so otherwise we should exit
        if (connection == null || !connection.isReadyForConnection()) {
            Toast.makeText(this, getString(R.string.error_uri_noinfo_nosave), Toast.LENGTH_LONG).show();
            if (connection.isReadyToBeSaved()) {
                Log.i(TAG, "Exiting - Insufficent information to connect and connection was not saved.");
            } else {
                Log.i(TAG, "Insufficent information to connect, showing connection dialog.");
                // launch appropriate activity
                Class cls = bVNC.class;
                if (Utils.isRdp(this)) {
                    cls = aRDP.class;
                } else if (Utils.isSpice(this)) {
                    cls = aSPICE.class;
                }
                Intent Intent = new Intent(this, cls);
                startActivity(Intent);
            }
            Utils.justFinish(this);
        }
    }

    @SuppressLint("RtlHardcoded")
    public void continueConnecting() {
        Log.d(TAG, "continueConnecting");
        // Initialize and define actions for on-screen keys.
        initializeOnScreenKeys();

        canvas.setFocusableInTouchMode(true);
        canvas.setDrawingCacheEnabled(false);

        // This code detects when the soft keyboard is up and sets an appropriate visibleHeight in vncCanvas.
        // When the keyboard is gone, it resets visibleHeight and pans zero distance to prevent us from being
        // below the desktop image (if we scrolled all the way down when the keyboard was up).
        // TODO: Move this into a separate thread, and post the visibility changes to the handler.
        //       to avoid occupying the UI thread with this.
        rootView = ((ViewGroup) findViewById(android.R.id.content)).getChildAt(0);
        rootView.getViewTreeObserver().addOnGlobalLayoutListener(() -> relayoutViews(rootView));

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT);

        if (Utils.querySharedPreferenceBoolean(this, Constants.leftHandedModeTag)) {
            params.gravity = Gravity.CENTER | Gravity.LEFT;
        } else {
            params.gravity = Gravity.CENTER | Gravity.RIGHT;
        }

        panner = new Panner(this, handler);

        toolbar = (RemoteToolbar) findViewById(R.id.toolbar);
        toolbar.setTitle("");
        toolbar.setOnGenericMotionListener(ignoringMouseInputListener);
        toolbar.getBackground().setAlpha(64);
        toolbar.setLayoutParams(params);
        setSupportActionBar(toolbar);
        showActionBar();
    }

    void relayoutViews(View rootView) {
        Log.d(TAG, "onGlobalLayout: start");
        if (canvas == null) {
            Log.d(TAG, "onGlobalLayout: canvas null, returning");
            return;
        }

        Rect r = new Rect();

        rootView.getWindowVisibleDisplayFrame(r);
        Log.d(TAG, "onGlobalLayout: getWindowVisibleDisplayFrame: " + r);

        // To avoid setting the visible height to a wrong value after an screen unlock event
        // (when r.bottom holds the width of the screen rather than the height due to a rotation)
        // we make sure r.top is zero (i.e. there is no notification bar and we are in full-screen mode)
        // It's a bit of a hack.
        // One additional situation that needed handling was that devices with notches / cutouts don't
        // ever have r.top equal to zero. so a special case for them.
        Rect re = new Rect();
        getWindow().getDecorView().getWindowVisibleDisplayFrame(re);
        if (r.top == 0 || re.top > 0) {
            if (canvas.getDrawable() != null) {
                Log.d(TAG, "onGlobalLayout: Setting VisibleDesktopHeight to: " + (r.bottom - re.top));
                canvas.setVisibleDesktopHeight(r.bottom - re.top);
                canvas.relativePan(0, 0);
            } else {
                Log.d(TAG, "onGlobalLayout: canvas.myDrawable is null");
            }
        } else {
            Log.d(TAG, "onGlobalLayout: Found r.top to be non-zero");
        }

        // Enable/show the toolbar if the keyboard is gone, and disable/hide otherwise.
        // We detect the keyboard if more than 19% of the screen is covered.
        // Use the visible display frame of the decor view to compute notch dimensions.
        int rootViewHeight = rootView.getHeight();

        if (keyboardIconForAndroidTvX == Float.MAX_VALUE) {
            keyboardIconForAndroidTvX = keyboardIconForAndroidTv.getX();
        }

        int layoutKeysBottom = layoutKeys.getBottom();
        int toolbarBottom = toolbar.getBottom();
        int rootViewBottom = layoutKeys.getRootView().getBottom();
        int diffArrowKeysPosition = r.right - re.left - layoutArrowKeys.getRight();
        int diffLayoutKeysPosition = r.bottom - re.top - layoutKeysBottom;
        int diffToolbarPosition = r.bottom - re.top - toolbarBottom - r.bottom / 2;
        int standardToolbarPositionX = r.right - toolbar.getWidth();
        int standardToolbarPositionY = r.bottom - re.top - toolbar.getHeight() - r.bottom / 2;
        Log.d(TAG, "onGlobalLayout: before: r.bottom: " + r.bottom +
                " rootViewHeight: " + rootViewHeight + " re.top: " + re.top + " re.bottom: " + re.bottom +
                " layoutKeysBottom: " + layoutKeysBottom + " rootViewBottom: " + rootViewBottom + " toolbarBottom: " + toolbarBottom +
                " diffLayoutKeysPosition: " + diffLayoutKeysPosition + " diffToolbarPosition: " + diffToolbarPosition);

        if (r.bottom > rootViewHeight * 0.81) {
            Log.d(TAG, "onGlobalLayout: Less than 19% of screen is covered");
            String direction = "down";
            // Soft Kbd gone, shift the meta keys and arrows down.
            if (layoutKeys != null) {
                shiftLayoutArrowAndToolbar(r, diffArrowKeysPosition, diffLayoutKeysPosition, diffToolbarPosition, standardToolbarPositionX, standardToolbarPositionY, direction);
                if (softKeyboardUp) {
                    Log.d(TAG, "onGlobalLayout: softKeyboardUp was true, but keyboard is now hidden. Hiding on-screen buttons");
                    setExtraKeysVisibility(View.GONE, false);
                    canvas.invalidate();
                }
            }
            softKeyboardUp = false;
        } else {
            Log.d(TAG, "onGlobalLayout: More than 19% of screen is covered");
            softKeyboardUp = true;
            String direction = "up";
            //  Soft Kbd up, shift the meta keys and arrows up.
            if (layoutKeys != null) {
                shiftLayoutArrowAndToolbar(r, diffArrowKeysPosition, diffLayoutKeysPosition, diffToolbarPosition, standardToolbarPositionX, standardToolbarPositionY, direction);
                if (extraKeysHidden) {
                    Log.d(TAG, "onGlobalLayout: on-screen buttons should be hidden");
                    setExtraKeysVisibility(View.GONE, false);
                } else {
                    Log.d(TAG, "onGlobalLayout: on-screen buttons should be showing");
                    setExtraKeysVisibility(View.VISIBLE, true);
                }
                canvas.invalidate();
            }
        }
        if (layoutKeys != null) {
            layoutKeysBottom = layoutKeys.getBottom();
            rootViewBottom = layoutKeys.getRootView().getBottom();
        }
        Log.d(TAG, "onGlobalLayout: after: r.bottom: " + r.bottom +
                " rootViewHeight: " + rootViewHeight + " re.top: " + re.top + " re.bottom: " + re.bottom +
                " layoutKeysBottom: " + layoutKeysBottom + " rootViewBottom: " + rootViewBottom + " toolbarBottom: " + toolbarBottom +
                " diffLayoutKeysPosition: " + diffLayoutKeysPosition + " diffToolbarPosition: " + diffToolbarPosition);
    }

    private void shiftLayoutArrowAndToolbar(Rect r, int diffArrowKeysPosition, int diffLayoutKeysPosition, int diffToolbarPosition, int standardToolbarPositionX, int standardToolbarPositionY, String direction) {
        Log.d(TAG, String.format("onGlobalLayout: shifting on-screen buttons %s by: %d", direction, diffLayoutKeysPosition));
        layoutKeys.offsetTopAndBottom(diffLayoutKeysPosition);
        offsetOrRestoreSavedToolbarPosition(r, diffToolbarPosition, standardToolbarPositionX, standardToolbarPositionY);
        Log.d(TAG, "onGlobalLayout: shifting arrow keys by: " + diffArrowKeysPosition);
        layoutArrowKeys.offsetLeftAndRight(diffArrowKeysPosition);
    }

    private void offsetOrRestoreSavedToolbarPosition(Rect r, int diffToolbarPosition, int standardPositionX, int standardPositionY) {
        boolean useLastPosition = connection.getUseLastPositionToolbar();
        boolean toolbarMoved = connection.getUseLastPositionToolbarMoved();
        if (!useLastPosition || !toolbarMoved) {
            toolbar.offsetTopAndBottom(diffToolbarPosition);
        } else {
            toolbar.setPositionToMakeVisible(
                    connection.getUseLastPositionToolbarX(),
                    connection.getUseLastPositionToolbarY(),
                    r.right,
                    r.bottom,
                    standardPositionX,
                    standardPositionY);
        }
    }

    public void extraKeysToggle(MenuItem m) {
        if (layoutKeys.getVisibility() == View.VISIBLE) {
            extraKeysHidden = true;
            setExtraKeysVisibility(View.GONE, false);
        } else {
            extraKeysHidden = false;
            setExtraKeysVisibility(View.VISIBLE, true);
        }
        setKeyStowDrawableAndVisibility(m);
        relayoutViews(rootView);
    }

    private void setKeyStowDrawableAndVisibility(MenuItem m) {
        if (m == null) {
            return;
        }
        Drawable replacer;
        m.setVisible(connection.getExtraKeysToggleType() != Constants.EXTRA_KEYS_OFF);
        if (layoutKeys.getVisibility() == View.GONE)
            replacer = getResources().getDrawable(R.drawable.showkeys);
        else
            replacer = getResources().getDrawable(R.drawable.hidekeys);

        m.setIcon(replacer);
    }

    public void sendShortVibration() {
        if (myVibrator != null) {
            myVibrator.vibrate(Constants.SHORT_VIBRATION);
        } else {
            Log.i(TAG, "Device cannot vibrate, not sending vibration");
        }
    }

    /**
     * Initializes the on-screen keys for meta keys and arrow keys.
     */
    private void initializeOnScreenKeys() {
        layoutKeys = (RelativeLayout) findViewById(R.id.layoutKeys);
        layoutArrowKeys = (LinearLayout) findViewById(R.id.layoutArrowKeys);
        // Define action of tab key and modifier keys.
        keyTab = (ImageButton) findViewById(R.id.keyTab);
        keyTab.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyTab, R.drawable.tabon, R.drawable.taboff, KeyEvent.KEYCODE_TAB));
        keyTab.setOnGenericMotionListener(ignoringMouseInputListener);
        keyEsc = (ImageButton) findViewById(R.id.keyEsc);
        keyEsc.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyEsc, R.drawable.escon, R.drawable.escoff, 111 /* KEYCODE_ESCAPE */));
        keyEsc.setOnGenericMotionListener(ignoringMouseInputListener);
        keyCtrl = (ImageButton) findViewById(R.id.keyCtrl);
        keyCtrl.setOnClickListener(arg0 -> toggleCtrl(false, remoteConnection.getKeyboard().onScreenCtrlToggle()));
        keyCtrl.setOnLongClickListener(arg0 -> toggleCtrl(true, remoteConnection.getKeyboard().onScreenCtrlToggle()));
        keyCtrl.setOnGenericMotionListener(ignoringMouseInputListener);
        keySuper = (ImageButton) findViewById(R.id.keySuper);
        keySuper.setOnClickListener(arg0 -> toggleSuper(false, remoteConnection.getKeyboard().onScreenSuperToggle()));
        keySuper.setOnLongClickListener(arg0 -> toggleSuper(true, remoteConnection.getKeyboard().onScreenSuperToggle()));
        keySuper.setOnGenericMotionListener(ignoringMouseInputListener);
        keyAlt = (ImageButton) findViewById(R.id.keyAlt);
        keyAlt.setOnClickListener(arg0 -> toggleAlt(false, remoteConnection.getKeyboard().onScreenAltToggle()));
        keyAlt.setOnLongClickListener(arg0 -> toggleAlt(true, remoteConnection.getKeyboard().onScreenAltToggle()));
        keyAlt.setOnGenericMotionListener(ignoringMouseInputListener);
        keyShift = (ImageButton) findViewById(R.id.keyShift);
        keyShift.setOnClickListener(arg0 -> toggleShift(false, remoteConnection.getKeyboard().onScreenShiftToggle()));
        keyShift.setOnLongClickListener(arg0 -> toggleShift(true, remoteConnection.getKeyboard().onScreenShiftToggle()));
        keyShift.setOnGenericMotionListener(ignoringMouseInputListener);
        // Define action of arrow keys.
        keyUp = (ImageButton) findViewById(R.id.keyUpArrow);
        keyUp.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyUp, R.drawable.upon, R.drawable.upoff, KeyEvent.KEYCODE_DPAD_UP));
        keyUp.setOnGenericMotionListener(ignoringMouseInputListener);
        keyDown = (ImageButton) findViewById(R.id.keyDownArrow);
        keyDown.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyDown, R.drawable.downon, R.drawable.downoff, KeyEvent.KEYCODE_DPAD_DOWN));
        keyDown.setOnGenericMotionListener(ignoringMouseInputListener);
        keyLeft = (ImageButton) findViewById(R.id.keyLeftArrow);
        keyLeft.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyLeft, R.drawable.lefton, R.drawable.leftoff, KeyEvent.KEYCODE_DPAD_LEFT));
        keyLeft.setOnGenericMotionListener(ignoringMouseInputListener);
        keyRight = (ImageButton) findViewById(R.id.keyRightArrow);
        keyRight.setOnTouchListener((arg0, e) -> repeatableKeyAction(e, keyRight, R.drawable.righton, R.drawable.rightoff, KeyEvent.KEYCODE_DPAD_RIGHT));
        keyRight.setOnGenericMotionListener(ignoringMouseInputListener);
    }

    private boolean repeatableKeyAction(MotionEvent e, ImageButton button, int onRes, int offRes, int keyCode) {
        RemoteKeyboard k = remoteConnection.getKeyboard();
        if (e.getAction() == MotionEvent.ACTION_DOWN) {
            sendShortVibration();
            button.setImageResource(onRes);
            k.repeatKeyEvent(keyCode, new KeyEvent(e.getAction(), keyCode));
            return true;
        } else if (e.getAction() == MotionEvent.ACTION_UP) {
            button.setImageResource(offRes);
            resetOnScreenKeys(0);
            k.stopRepeatingKeyEvent();
            return true;
        }
        return false;
    }

    private boolean toggleSuper(boolean toggled, boolean isOn) {
        keySuperToggled = toggled;
        return toggleButtonImage(isOn, toggled, keySuper, R.drawable.superon, R.drawable.superoff);
    }

    private boolean toggleShift(boolean toggled, boolean isOn) {
        keyShiftToggled = toggled;
        return toggleButtonImage(isOn, toggled, keyShift, R.drawable.shifton, R.drawable.shiftoff);
    }

    private boolean toggleAlt(boolean toggled, boolean isOn) {
        keyAltToggled = toggled;
        return toggleButtonImage(isOn, toggled, keyAlt, R.drawable.alton, R.drawable.altoff);
    }

    private boolean toggleCtrl(boolean toggled, boolean isOn) {
        keyCtrlToggled = toggled;
        return toggleButtonImage(isOn, toggled, keyCtrl, R.drawable.ctrlon, R.drawable.ctrloff);
    }

    private boolean toggleButtonImage(
            boolean isOn, boolean isToggled, ImageButton button, int onResource, int offResource
    ) {
        if (isToggled) {
            sendShortVibration();
        }
        if (isOn) {
            button.setImageResource(onResource);
        } else {
            button.setImageResource(offResource);
        }
        return true;
    }

    /**
     * Resets the state and image of the on-screen keys.
     */
    private int resetOnScreenKeys(int keyCode) {
        // Do not reset on-screen keys if keycode is SHIFT.
        switch (keyCode) {
            case KeyEvent.KEYCODE_SHIFT_LEFT:
            case KeyEvent.KEYCODE_SHIFT_RIGHT:
                return keyCode;
        }
        if (!keyCtrlToggled) {
            keyCtrl.setImageResource(R.drawable.ctrloff);
            remoteConnection.getKeyboard().onScreenCtrlOff();
        }
        if (!keyAltToggled) {
            keyAlt.setImageResource(R.drawable.altoff);
            remoteConnection.getKeyboard().onScreenAltOff();
        }
        if (!keySuperToggled) {
            keySuper.setImageResource(R.drawable.superoff);
            remoteConnection.getKeyboard().onScreenSuperOff();
        }
        if (!keyShiftToggled) {
            keyShift.setImageResource(R.drawable.shiftoff);
            remoteConnection.getKeyboard().onScreenShiftOff();
        }
        return keyCode;
    }

    /**
     * Sets the visibility of the extra keys appropriately.
     */
    private void setExtraKeysVisibility(int visibility, boolean forceVisible) {
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

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause called.");
        try {
            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            imm.hideSoftInputFromWindow(canvas.getWindowToken(), 0);
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during onPause");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume called.");
        try {
            canvas.postInvalidateDelayed(600);
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during onResume");
        }
    }

    /**
     * Set modes on start to match what is specified in the ConnectionBean;
     * color mode (already done) scaling, input mode
     */
    void setModes() {
        Log.d(TAG, "setModes");
        setInputHandler(getInputHandlerByName(connection.getInputMode()));
        AbstractScaling.getByScaleType(connection.getScaleMode()).setScaleTypeForActivity(this);
        initializeOnScreenKeys();
        try {
            COLORMODEL cm = COLORMODEL.valueOf(connection.getColorModel());
            remoteConnection.setColorModel(cm);
        } catch (NullPointerException | IllegalArgumentException e) {
            Log.w(TAG, "Could not set color model");
        }
        canvas.setFocusableInTouchMode(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            canvas.setFocusedByDefault(true);
        }
        canvas.requestFocus();
        canvas.setDrawingCacheEnabled(false);

        SamsungDexUtils.INSTANCE.dexMetaKeyCapture(this);
    }

    /*
     * (non-Javadoc)
     *
     * @see android.app.Activity#onCreateDialog(int)
     */
    @Override
    protected Dialog onCreateDialog(int id) {
        if (id == R.layout.entertext) {
            return new EnterTextDialog(this);
        } else if (id == R.id.itemHelpInputMode) {
            return createHelpDialog();
        }

        // Default to meta key dialog
        return new MetaKeyDialog(this);
    }

    /**
     * Creates the help dialog for this activity.
     */
    private Dialog createHelpDialog() {
        AlertDialog.Builder adb = new AlertDialog.Builder(this)
                .setMessage(R.string.input_mode_help_text)
                .setPositiveButton(R.string.close,
                        (dialog, whichButton) -> {
                            // We don't have to do anything.
                        });
        Dialog d = adb.setView(new ListView(this)).create();
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
        Window window = d.getWindow();
        if (window != null) {
            lp.copyFrom(window.getAttributes());
        }
        lp.width = WindowManager.LayoutParams.MATCH_PARENT;
        lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
        d.show();
        d.getWindow().setAttributes(lp);
        return d;
    }

    /*
     * (non-Javadoc)
     *
     * @see android.app.Activity#onPrepareDialog(int, android.app.Dialog)
     */
    @SuppressWarnings("deprecation")
    @Override
    protected void onPrepareDialog(int id, Dialog dialog) {
        super.onPrepareDialog(id, dialog);
        if (dialog instanceof ConnectionSettable)
            ((ConnectionSettable) dialog).setConnection(connection);
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        enableImmersive();
        try {
            setExtraKeysVisibility(View.GONE, false);

            // Correct a few times just in case. There is no visual effect.
            handler.postDelayed(rotationCorrector, 300);
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during onConfigurationChanged");
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.i(TAG, "onStart called.");
        try {
            canvas.postInvalidateDelayed(800);
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during onStart");
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.i(TAG, "onStop called.");
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        Log.i(TAG, "onRestart called.");
        try {
            canvas.postInvalidateDelayed(1000);
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during onRestart");

        }
    }

    @Override
    public void onPanelClosed(int featureId, @NonNull Menu menu) {
        super.onPanelClosed(featureId, menu);
        showActionBar();
        enableImmersive();
    }

    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        if (menu != null) {
            android.util.Log.i(TAG, "Menu opened, disabling hiding action bar");
            handler.removeCallbacks(actionBarHider);
            updateScalingMenu();
            updateInputMenu();
            disableImmersive();
        }
        return super.onMenuOpened(featureId, menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        // Make sure extra keys stow item is gone if extra keys are disabled and vice versa.
        setKeyStowDrawableAndVisibility(menu.findItem(R.id.extraKeysToggle));
        menu.findItem(R.id.itemColorMode).setVisible(remoteConnection.canUpdateColorModelConnected());
        return true;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        Log.d(TAG, "OnCreateOptionsMenu called");
        try {
            getMenuInflater().inflate(R.menu.canvasactivitymenu, menu);

            Menu inputMenu = menu.findItem(R.id.itemInputMode).getSubMenu();
            inputModeMenuItems = new MenuItem[inputModeIds.length];
            for (int i = 0; i < inputModeIds.length; i++) {
                if (inputMenu != null) {
                    inputModeMenuItems[i] = inputMenu.findItem(inputModeIds[i]);
                }
            }
            updateInputMenu();

            Menu scalingMenu = menu.findItem(R.id.itemScaling).getSubMenu();
            scalingModeMenuItems = new MenuItem[scalingModeIds.length];
            for (int i = 0; i < scalingModeIds.length; i++) {
                if (scalingMenu != null) {
                    scalingModeMenuItems[i] = scalingMenu.findItem(scalingModeIds[i]);
                }
            }
            updateScalingMenu();

            // Set the text of the Extra Keys menu item appropriately.
            // TODO: Implement for Opaque
            if (connection != null && connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_ON)
                menu.findItem(R.id.itemExtraKeys).setTitle(R.string.extra_keys_disable);
            else
                menu.findItem(R.id.itemExtraKeys).setTitle(R.string.extra_keys_enable);

            toolbarMover = new OnTouchViewMover(toolbar, handler, toolbarPositionSaver, actionBarHider, hideToolbarDelay);
            ImageButton moveButton = new ImageButton(this);

            moveButton.setBackgroundResource(R.drawable.ic_all_out_gray_36dp);
            MenuItem moveToolbar = menu.findItem(R.id.moveToolbar);
            moveToolbar.setActionView(moveButton);
            moveButton.setOnTouchListener(toolbarMover);
        } catch (NullPointerException e) {
            e.printStackTrace();
        }
        Log.d(TAG, "OnCreateOptionsMenu complete");
        return true;
    }

    /**
     * Change the scaling mode sub-menu to reflect available scaling modes.
     */
    void updateScalingMenu() {
        try {
            for (MenuItem item : scalingModeMenuItems) {
                // If the entire framebuffer is NOT contained in the bitmap, fit-to-screen is meaningless.
                if (item.getItemId() == R.id.itemFitToScreen) {
                    item.setEnabled(canvas == null || canvas.myDrawable == null ||
                            (canvas.myDrawable.getBitmapHeight() == canvas.myDrawable.getFramebufferHeight() &&
                                    canvas.myDrawable.getBitmapWidth() == canvas.myDrawable.getFramebufferWidth()));
                } else {
                    item.setEnabled(true);
                }

                AbstractScaling scaling = AbstractScaling.getById(item.getItemId());
                if (scaling.scaleType == connection.getScaleMode()) {
                    item.setChecked(true);
                }
            }
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during updateScalingMenu");
        }
    }

    /**
     * Change the input mode sub-menu to reflect change in scaling
     */
    void updateInputMenu() {
        try {
            for (MenuItem item : inputModeMenuItems) {
                item.setEnabled(canvas.canvasZoomer.isValidInputMode(item.getItemId()));
                if (getInputHandlerById(item.getItemId()) == touchInputHandler)
                    item.setChecked(true);
            }
        } catch (NullPointerException e) {
            Log.d(TAG, "Ignoring NullPointerException during updateInputMenu");
        }
    }

    /**
     * If id represents an input handler, return that; otherwise return null
     */
    TouchInputHandler getInputHandlerById(int id) {
        myVibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);

        if (inputModeHandlers == null) {
            inputModeHandlers = new TouchInputHandler[inputModeIds.length];
        }
        for (int i = 0; i < inputModeIds.length; ++i) {
            if (inputModeIds[i] == id) {
                if (inputModeHandlers[i] == null) {
                    if (id == R.id.itemInputTouchPanZoomMouse) {
                        inputModeHandlers[i] = new TouchInputHandlerDirectSwipePan(this, canvas, remoteConnection, App.debugLog, getScrollSensitivitySetting());
                    } else if (id == R.id.itemInputDragPanZoomMouse) {
                        inputModeHandlers[i] = new TouchInputHandlerDirectDragPan(this, canvas, remoteConnection, App.debugLog, getScrollSensitivitySetting());
                    } else if (id == R.id.itemInputTouchpad) {
                        inputModeHandlers[i] = new TouchInputHandlerTouchpad(this, canvas, remoteConnection, App.debugLog, getScrollSensitivitySetting());
                    } else if (id == R.id.itemInputSingleHanded) {
                        inputModeHandlers[i] = new TouchInputHandlerSingleHanded(this, canvas, remoteConnection, App.debugLog, getScrollSensitivitySetting());
                    } else {
                        throw new IllegalStateException("Unexpected value: " + id);
                    }
                }
                return inputModeHandlers[i];
            }
        }
        return null;
    }

    TouchInputHandler getInputHandlerByName(String name) {
        TouchInputHandler result = null;
        for (int id : inputModeIds) {
            TouchInputHandler handler = getInputHandlerById(id);
            if (handler.getId().equals(name)) {
                result = handler;
                break;
            }
        }
        if (result == null) {
            result = getInputHandlerById(R.id.itemInputTouchPanZoomMouse);
        }
        return result;
    }

    int getModeIdFromHandler(TouchInputHandler handler) {
        for (int id : inputModeIds) {
            if (handler == getInputHandlerById(id))
                return id;
        }
        return R.id.itemInputTouchPanZoomMouse;
    }

    int getScrollSensitivitySetting() {
        // SeekBar starts at 0, so add 1 to not have 0 sensitivity
        return Utils.querySharedPreferencesInt(this, Constants.scrollSpeed, Constants.DEFAULT_SCROLL_SPEED) + 1;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        RemoteKeyboard k = remoteConnection.getKeyboard();
        if (k != null) {
            k.setAfterMenu(true);
        }
        int itemId = item.getItemId();
        if (itemId == R.id.itemInfo) {
            remoteConnection.showConnectionInfo();
            return true;
        } else if (itemId == R.id.itemSpecialKeys) {
            showDialog(R.layout.metakey);
            return true;
        } else if (itemId == R.id.itemColorMode) {
            selectColorModel();
            return true;
            // Following sets one of the scaling options
        } else if (itemId == R.id.itemZoomable || itemId == R.id.itemOneToOne || itemId == R.id.itemFitToScreen) {
            AbstractScaling.getById(item.getItemId()).setScaleTypeForActivity(this);
            item.setChecked(true);
            showPanningState(false);
            return true;
        } else if (itemId == R.id.itemCenterMouse) {
            remoteConnection.getPointer().movePointer(canvas.absoluteXPosition + canvas.getVisibleDesktopWidth() / 2,
                    canvas.absoluteYPosition + canvas.getVisibleDesktopHeight() / 2);
            return true;
        } else if (itemId == R.id.itemDisconnect) {
            disconnectAndFinishActivity();
            return true;
        } else if (itemId == R.id.itemEnterText) {
            showDialog(R.layout.entertext);
            return true;
        } else if (itemId == R.id.itemCtrlAltDel) {
            remoteConnection.getKeyboard().sendMetaKey(MetaKeyBean.keyCtrlAltDel);
            return true;
        } else if (itemId == R.id.itemSendKeyAgain) {
            sendSpecialKeyAgain();
            return true;
        } else if (itemId == R.id.itemExtraKeys) {
            if (connection.getExtraKeysToggleType() == Constants.EXTRA_KEYS_ON) {
                connection.setExtraKeysToggleType(Constants.EXTRA_KEYS_OFF);
                item.setTitle(R.string.extra_keys_enable);
                setExtraKeysVisibility(View.GONE, false);
            } else {
                connection.setExtraKeysToggleType(Constants.EXTRA_KEYS_ON);
                item.setTitle(R.string.extra_keys_disable);
                setExtraKeysVisibility(View.VISIBLE, false);
                extraKeysHidden = false;
            }
            invalidateOptionsMenu();
            connection.save(this);
            return true;
        } else if (itemId == R.id.itemHelpInputMode) {
            showDialog(R.id.itemHelpInputMode);
            return true;
        } else {
            boolean inputModeSet = setInputMode(item.getItemId());
            item.setChecked(inputModeSet);
            if (inputModeSet) {
                return true;
            }
        }
        return super.onOptionsItemSelected(item);
    }

    private void disconnectAndFinishActivity() {
        remoteConnection.closeConnection();
        Utils.justFinish(this);
    }

    public boolean setInputMode(int id) {
        TouchInputHandler input = getInputHandlerById(id);
        if (input != null) {
            setInputHandler(input);
            connection.setInputMode(input.getId());
            if (input.getId().equals(TouchInputHandlerTouchpad.ID)) {
                connection.setFollowMouse(true);
                connection.setFollowPan(true);
            } else {
                connection.setFollowMouse(false);
                connection.setFollowPan(false);
                remoteConnection.getPointer().setRelativeEvents(false);
            }

            showPanningState(true);
            connection.save(this);
            return true;
        }
        return false;
    }

    private void setInputHandler(TouchInputHandler input) {
        touchInputHandler = input;
        inputListener = new RemoteClientsInputListener(
                this,
                remoteConnection,
                remoteConnection,
                touchInputHandler,
                this::resetOnScreenKeys,
                connection.getUseDpadAsArrows()
        );
        canvas.setOnKeyListener(inputListener);
    }

    private void sendSpecialKeyAgain() {
        if (lastSentKey == null
                || lastSentKey.get_Id() != connection.getLastMetaKeyId()) {
            ArrayList<MetaKeyBean> keys = new ArrayList<>();
            Database database = new Database(this);
            tryPopulateKeysInListWhereFieldMatchesValue(
                    database, keys, MetaKeyBean.GEN_FIELD__ID, connection.getLastMetaKeyId()
            );
            database.close();
            if (keys.size() > 0) {
                lastSentKey = keys.get(0);
            } else {
                lastSentKey = null;
            }
        }
        if (lastSentKey != null)
            remoteConnection.getKeyboard().sendMetaKey(lastSentKey);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy called.");
        if (remoteConnection != null)
            remoteConnection.closeConnection();
        System.gc();
    }

    public void showPanningState(boolean showLonger) {
        if (showLonger) {
            final Toast t = Toast.makeText(this, touchInputHandler.getDescription(), Toast.LENGTH_LONG);
            TimerTask tt = new TimerTask() {
                @Override
                public void run() {
                    t.show();
                    try {
                        Thread.sleep(2000);
                    } catch (InterruptedException e) {
                        Log.d(TAG, "Ignored InterruptedException during showPanningState");
                    }
                    t.show();
                }
            };
            new Timer().schedule(tt, 2000);
            t.show();
        } else {
            Toast t = Toast.makeText(this, touchInputHandler.getDescription(), Toast.LENGTH_SHORT);
            t.show();
        }
    }

    /*
     * (non-Javadoc)
     *
     * @see android.app.Activity#onTrackballEvent(android.view.MotionEvent)
     */
    @Override
    public boolean onTrackballEvent(MotionEvent event) {
        boolean consumed = false;
        if (inputListener != null) {
            consumed = inputListener.onTrackballEvent(event);
        }
        if (!consumed) {
            consumed = super.onTrackballEvent(event);
        }
        return consumed;
    }

    // Send touch events or mouse events like button clicks to be handled.
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean consumed = false;
        if (inputListener != null) {
            consumed = inputListener.onTouchEvent(event);
        }
        if (!consumed) {
            consumed = super.onTrackballEvent(event);
        }
        return consumed;
    }

    // Send e.g. mouse events like hover and scroll to be handled.
    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        boolean consumed = false;
        if (inputListener != null) {
            consumed = inputListener.onGenericMotionEvent(event);
        }
        if (!consumed) {
            consumed = super.onGenericMotionEvent(event);
        }
        return consumed;
    }

    private void selectColorModel() {

        String[] choices = new String[COLORMODEL.values().length];
        int currentSelection = -1;
        for (int i = 0; i < choices.length; i++) {
            COLORMODEL cm = COLORMODEL.values()[i];
            choices[i] = cm.toString();
            if (remoteConnection.isColorModel(cm))
                currentSelection = i;
        }

        final Dialog dialog = new Dialog(this);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        ListView list = new ListView(this);
        list.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_list_item_checked, choices));
        list.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        list.setItemChecked(currentSelection, true);
        list.setOnItemClickListener((arg0, arg1, arg2, arg3) -> {
            dialog.dismiss();
            COLORMODEL cm = COLORMODEL.values()[arg2];
            remoteConnection.setColorModel(cm);
            connection.setColorModel(cm.nameString());
            connection.save(RemoteCanvasActivity.this);
            Toast.makeText(RemoteCanvasActivity.this, getString(R.string.info_update_color_model_to) + cm, Toast.LENGTH_SHORT).show();
        });
        dialog.setContentView(list);
        dialog.show();
    }

    public void showActionBar() {
        handler.removeCallbacks(actionBarShower);
        handler.postAtTime(actionBarShower, SystemClock.uptimeMillis() + 50);
        handler.removeCallbacks(actionBarHider);
        handler.postAtTime(actionBarHider, SystemClock.uptimeMillis() + hideToolbarDelay);
    }

    public void showKeyboardIcon() {
        handler.removeCallbacks(keyboardIconShower);
        handler.postAtTime(keyboardIconShower, SystemClock.uptimeMillis() + 50);
        handler.removeCallbacks(actionBarHider);
        handler.postAtTime(actionBarHider, SystemClock.uptimeMillis() + hideToolbarDelay);
    }

    @Override
    public void onTextSelected(String selectedString) {
        android.util.Log.i(TAG, "onTextSelected called with selectedString: " + selectedString);
        remoteConnection.pd.show();
        connection.setVmname(remoteConnection.vmNameToId.get(selectedString));
        connection.save(this);
        synchronized (remoteConnection.getRfbConn()) {
            remoteConnection.getRfbConn().notify();
        }
    }

    public void toggleKeyboard(MenuItem menuItem) {
        if (softKeyboardUp) {
            hideKeyboard();
        } else {
            showKeyboard();
        }
    }

    public void showKeyboard() {
        android.util.Log.i(TAG, "Showing keyboard and hiding action bar");
        canvas.requestFocus();
        Utils.showKeyboard(this, canvas);
        softKeyboardUp = true;
        Objects.requireNonNull(getSupportActionBar()).hide();
    }

    public void hideKeyboard() {
        android.util.Log.i(TAG, "Hiding keyboard and hiding action bar");
        canvas.requestFocus();
        Utils.hideKeyboard(this, getCurrentFocus());
        softKeyboardUp = false;
        Objects.requireNonNull(getSupportActionBar()).hide();
    }

    public void hideKeyboardAndExtraKeys() {
        hideKeyboard();
        if (layoutKeys.getVisibility() == View.VISIBLE) {
            extraKeysHidden = true;
            setExtraKeysVisibility(View.GONE, false);
        }
    }

    public Connection getConnection() {
        return connection;
    }

    public RemoteCanvas getCanvas() {
        return canvas;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putString("WORKAROUND_FOR_BUG_19917_KEY", "WORKAROUND_FOR_BUG_19917_VALUE");
        super.onSaveInstanceState(outState);
    }

    private boolean isMasterPasswordEnabled() {
        SharedPreferences sp = getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
        return sp.getBoolean(Constants.masterPasswordEnabledTag, false);
    }

    @Override
    public void onBackPressed() {
        if (GeneralUtils.isTv(this)) {
            disconnectAndFinishActivity();
        }
        if (inputListener != null) {
            inputListener.onKey(canvas, KeyEvent.KEYCODE_BACK, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
        }
    }

    public RemoteConnection getRemoteConnection() {
        return remoteConnection;
    }

    public Handler getHandler() { return handler; }

    private class ActionBarPositionSaver implements Runnable {
        public void run() {
            connection.setUseLastPositionToolbarX(toolbarMover.getLastX());
            connection.setUseLastPositionToolbarY(toolbarMover.getLastY());
            connection.setUseLastPositionToolbarMoved(true);
            connection.save(RemoteCanvasActivity.this);
        }
    }

    private class ActionBarHider implements Runnable {
        public void run() {
            if (GeneralUtils.isTv(RemoteCanvasActivity.this)) {
                keyboardIconForAndroidTv.setVisibility(View.GONE);
            } else {
                ActionBar actionBar = getSupportActionBar();
                if (actionBar != null) {
                    Log.d(TAG, "ActionBarHider: Hiding ActionBar");
                    actionBar.hide();
                }
            }
        }
    }

    private class ActionBarShower implements Runnable {
        public void run() {
            showActionBar();
        }

        private void showActionBar() {
            ActionBar actionBar = getSupportActionBar();
            if (actionBar != null) {
                Log.d(TAG, "ActionBarShower: Showing ActionBar");
                actionBar.show();
            }
        }
    }

    private class KeyboardIconShower implements Runnable {
        public void run() {
            if (GeneralUtils.isTv(RemoteCanvasActivity.this)) {
                animateKeyboardIconForAndroidTv();
            }
        }

        private void animateKeyboardIconForAndroidTv() {
            keyboardIconForAndroidTv.setVisibility(View.VISIBLE);
            Log.d(TAG, "ActionBarHider: keyboardIconForAndroidTv X position to: " + keyboardIconForAndroidTvX);
            keyboardIconForAndroidTv.setX(keyboardIconForAndroidTvX);
            ObjectAnimator animation = ObjectAnimator.ofFloat(keyboardIconForAndroidTv, "translationX", -100f);
            animation.setDuration(1000);
            animation.start();
        }
    }
}
