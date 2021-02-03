package com.undatech.opaque;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.undatech.opaque.input.RemoteKeyboard;
import com.undatech.opaque.input.RdpKeyboardMapper;
import com.undatech.opaque.input.RemotePointer;
import com.undatech.opaque.util.GeneralUtils;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadPoolExecutor;

public class RdpCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener,
                                        LibFreeRDP.UIEventListener, LibFreeRDP.EventListener {
    static final String TAG = "RdpCommunicator";

    private final static int VK_CONTROL = 0x11;
    private final static int VK_LCONTROL = 0xA2;
    private final static int VK_RCONTROL = 0xA3;
    private final static int VK_LMENU = 0xA4;
    private final static int VK_RMENU = 0xA5;
    private final static int VK_LSHIFT = 0xA0;
    private final static int VK_RSHIFT = 0xA1;
    private final static int VK_LWIN = 0x5B;
    private final static int VK_RWIN = 0x5C;
    private final static int VK_EXT_KEY = 0x00000100;

    private SessionState session;
    private BookmarkBase bookmark;
    // Keeps track of libFreeRDP instance
    private GlobalApp freeRdpApp;


    private Context context;
    private int metaState = 0;
    private boolean isInNormalProtocol = false;

    private final RdpCommunicator myself;
    private final Handler handler;
    private final Viewable viewable;

    // This variable indicates whether or not the user has accepted an untrusted
    // security certificate. Used to control progress while the dialog asking the user
    // to confirm the authenticity of a certificate is displayed.
    private boolean certificateAccepted = false;
    private boolean reattemptWithoutCredentials = true;
    private boolean authenticationAttempted = false;
    private boolean disconnectRequested = false;

    private String username, password, domain;

    private boolean debugLogging = false;

    public RdpCommunicator(Context context, Handler handler, Viewable viewable, String username,
                           String domain, String password, boolean debugLogging) {
        // This is necessary because it initializes a synchronizedMap referenced later.
        this.freeRdpApp = new GlobalApp();
        patchFreeRdpCore();
        // Create a manual bookmark and populate it from settings.
        this.bookmark = new ManualBookmark();
        this.context = context;
        this.handler = handler;
        this.viewable = viewable;
        this.myself = this;
        this.username = username;
        this.domain = domain;
        this.password = password;
        this.debugLogging = debugLogging;
        initSession(username, domain, password);
    }

    private void patchFreeRdpCore() {
        Class cls = this.freeRdpApp.getClass();
        try {
            Log.i(TAG, "Initializing sessionMap in GlobalApp");
            Field sessionMap = cls.getDeclaredField("sessionMap");
            sessionMap.setAccessible(true);
            sessionMap.set(this.freeRdpApp, Collections.synchronizedMap(new HashMap<Long, SessionState>()));
        } catch (NoSuchFieldException e) {
            Log.e(TAG, "There is no longer a sessionMap field in GlobalApp");
        } catch (IllegalAccessException e) {
            Log.e(TAG, "The field sessionMap in GlobalApp was not accessible despite our attempts");
        }
    }

    @Override
    public void setIsInNormalProtocol (boolean state) {
        android.util.Log.d(TAG, "setIsInNormalProtocol: " + state);
        isInNormalProtocol = state;
    }
    
    @Override
    public int framebufferWidth() {
        return session.getBookmark().getActiveScreenSettings().getWidth();
    }

    @Override
    public int framebufferHeight() {
        return session.getBookmark().getActiveScreenSettings().getHeight();
    }

    @Override
    public String desktopName() {
        return ((ManualBookmark)session.getBookmark()).getHostname();
    }

    @Override
    public void requestUpdate(boolean incremental) {
        // NOT USED for RDP.
    }

    @Override
    public void writeClientCutText(String text) {
        LibFreeRDP.sendClipboardData(session.getInstance(), text);
    }

    @Override
    public boolean isInNormalProtocol() {
        return isInNormalProtocol;
    }

    @Override
    public String getEncoding() {
        return "RDP";
    }

    @Override
    public void writePointerEvent(int x, int y, int metaState, int pointerMask, boolean rel) {
        this.metaState = metaState;
        if ((pointerMask & RemotePointer.POINTER_DOWN_MASK) != 0) {
            sendModifierKeys(true);
        }
        try { Thread.sleep(5); } catch (InterruptedException e) {}
        LibFreeRDP.sendCursorEvent(session.getInstance(), x, y, pointerMask);
        if ((pointerMask & RemotePointer.POINTER_DOWN_MASK) == 0) {
            sendModifierKeys(false);
        }
    }

    @Override
    public void writeKeyEvent(int key, int metaState, boolean down) {
        // Not used for actually sending keyboard events, but rather to record the current metastate.
        // The key event is sent to the KeyboardMapper from RemoteRdpKeyboard, and
        // when processed through the keyboard mapper, it ends up in one of the KeyProcessingListener
        // methods defined here.
        this.metaState = metaState;
    }

    @Override
    public void writeSetPixelFormat(int bitsPerPixel, int depth,
            boolean bigEndian, boolean trueColour, int redMax, int greenMax,
            int blueMax, int redShift, int greenShift, int blueShift,
            boolean fGreyScale) {
        // NOT USED for RDP.
    }

    @Override
    public void writeFramebufferUpdateRequest(int x, int y, int w, int h,
            boolean b) {
        // NOT USED for RDP.
    }

    public class DisconnectThread extends Thread {
        long instance;

        public DisconnectThread (long i) {
            this.instance = i;
        }
        public void run () {
            LibFreeRDP.disconnect(instance);
            //LibFreeRDP.freeInstance(instance);
        }
    }
    
    @Override
    public void close() {
        setIsInNormalProtocol(false);
        disconnectRequested = true;
        long instance = session.getInstance();
        DisconnectThread d = new DisconnectThread(instance);
        d.start();
    }

    @Override
    public boolean isCertificateAccepted() {
        return certificateAccepted;
    }

    @Override
    public void setCertificateAccepted(boolean certificateAccepted) {
        this.certificateAccepted = certificateAccepted;
    }

    private void sendModifierKeys (boolean down) {
        if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending LCTRL " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LCONTROL, down);
        }
        if ((metaState & RemoteKeyboard.RCTRL_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending RCTRL " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RCONTROL, down);
        }
        if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending LALT " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LMENU, down);
        }
        if ((metaState & RemoteKeyboard.RALT_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending RALT " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RMENU, down);
        }
        if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending LSUPER " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LWIN | VK_EXT_KEY, down);
        }
        if ((metaState & RemoteKeyboard.RSUPER_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending RSUPER " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RWIN | VK_EXT_KEY, down);
        }
        if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending LSHIFT " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LSHIFT, down);
        }
        if ((metaState & RemoteKeyboard.RSHIFT_MASK) != 0) {
            //android.util.Log.d("RdpCommunicator", "Sending RSHIFT " + down);
            try { Thread.sleep(5); } catch (InterruptedException e) {}
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RSHIFT, down);
        }
    }
    
    // ****************************************************************************
    // KeyboardMapper.KeyProcessingListener implementation
    @Override
    public void processVirtualKey(int virtualKeyCode, boolean down) {
        GeneralUtils.debugLog(this.debugLogging, TAG, "processVirtualKey: " +
                "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);

        if (down) {
            sendModifierKeys(true);
        }
        try { Thread.sleep(5); } catch (InterruptedException e) {}
        LibFreeRDP.sendKeyEvent(session.getInstance(), virtualKeyCode, down);
        if (!down) {
            sendModifierKeys(false);
        }
    }

    @Override
    public void processUnicodeKey(int unicodeKey) {
        //android.util.Log.d(TAG, "Unicode character: " + unicodeKey);
        sendModifierKeys(true);
        try { Thread.sleep(5); } catch (InterruptedException e) {}
        LibFreeRDP.sendUnicodeKeyEvent(session.getInstance(), unicodeKey, true);
        LibFreeRDP.sendUnicodeKeyEvent(session.getInstance(), unicodeKey, false);
        sendModifierKeys(false);
    }

    @Override
    public void switchKeyboard(int keyboardType) {
        // This is functionality specific to aFreeRDP.
    }

    @Override
    public void modifiersChanged() {
        // This is functionality specific to aFreeRDP.
    }

    @Override
    public void requestResolution(int x, int y) {
        // TODO Auto-generated method stub
    }

    private void initSession(String username, String domain, String password) {
        bookmark.setUsername(username);
        bookmark.setDomain(domain);
        bookmark.setPassword(password);
        session = GlobalApp.createSession(bookmark, context);
        session.setUIEventListener(this);
        LibFreeRDP.setEventListener(this);
    }

    public void setConnectionParameters(String address, int rdpPort, String nickname, int remoteWidth,
                                        int remoteHeight, boolean wallpaper, boolean fontSmoothing,
                                        boolean desktopComposition, boolean fullWindowDrag,
                                        boolean menuAnimations, boolean theming, boolean redirectSdCard,
                                        boolean consoleMode, int redirectSound, boolean enableRecording,
                                        boolean enableRemoteFx, boolean enableGfx, boolean enableGfxH264) {
        // Set a writable data directory
        //LibFreeRDP.setDataDirectory(session.getInstance(), getContext().getFilesDir().toString());
        // Get the address and port (based on whether an SSH tunnel is being established or not).
        bookmark.<ManualBookmark>get().setLabel(nickname);
        bookmark.<ManualBookmark>get().setHostname(address);
        bookmark.<ManualBookmark>get().setPort(rdpPort);

        BookmarkBase.DebugSettings debugSettings = bookmark.getDebugSettings();
        debugSettings.setDebugLevel("INFO");
        //debugSettings.setAsyncUpdate(false);
        //debugSettings.setAsyncInput(false);
        //debugSettings.setAsyncChannel(false);

        // Set screen settings to native res if instructed to, or if height or width are too small.
        BookmarkBase.ScreenSettings screenSettings = bookmark.getActiveScreenSettings();
        screenSettings.setWidth(remoteWidth);
        screenSettings.setHeight(remoteHeight);
        screenSettings.setColors(16);

        // Set performance flags.
        BookmarkBase.PerformanceFlags performanceFlags = bookmark.getPerformanceFlags();
        performanceFlags.setRemoteFX(enableRemoteFx);
        performanceFlags.setWallpaper(wallpaper);
        performanceFlags.setFontSmoothing(fontSmoothing);
        performanceFlags.setDesktopComposition(desktopComposition);
        performanceFlags.setFullWindowDrag(fullWindowDrag);
        performanceFlags.setMenuAnimations(menuAnimations);
        performanceFlags.setTheming(theming);
        performanceFlags.setGfx(enableGfx);
        performanceFlags.setH264(enableGfxH264);

        BookmarkBase.AdvancedSettings advancedSettings = bookmark.getAdvancedSettings();
        advancedSettings.setRedirectSDCard(redirectSdCard);
        advancedSettings.setConsoleMode(consoleMode);
        advancedSettings.setRedirectSound(redirectSound);
        advancedSettings.setRedirectMicrophone(enableRecording);
        advancedSettings.setSecurity(0); // Automatic negotiation
    }

    public void connect() {
        session.connect(context);
    }

    //////////////////////////////////////////////////////////////////////////////////
    //  Implementation of LibFreeRDP.EventListener.  Through the functions implemented
    //  below, FreeRDP communicates connection state information.
    //////////////////////////////////////////////////////////////////////////////////

    @Override
    public void OnPreConnect(long instance) {
        Log.v(TAG, "OnPreConnect");
    }

    @Override
    public void OnConnectionSuccess(long instance) {
        Log.v(TAG, "OnConnectionSuccess");
        reattemptWithoutCredentials = false;
        authenticationAttempted = false;
        myself.setIsInNormalProtocol(true);
    }

    @Override
    public void OnConnectionFailure(long instance) {
        Log.v(TAG, "OnConnectionFailure");
        myself.setIsInNormalProtocol(false);
    }

    @Override
    public void OnDisconnecting(long instance) {
        Log.v(TAG, "OnDisconnecting, reattemptWithoutCredentials: " + reattemptWithoutCredentials +
                         ", authenticationAttempted: " + authenticationAttempted +
                         ", disconnectRequested: " + disconnectRequested +
                         ", isInNormalProtocol: " + myself.isInNormalProtocol());
        if (reattemptWithoutCredentials && !myself.isInNormalProtocol()) {
            reattemptWithoutCredentials = false;
            // It could be bad credentials that caused the disconnection, so trying to connect
            // once again with no credentials before reporting the type of failure.
            initSession("", "", "");
            connect();
        } else if (authenticationAttempted && !myself.isInNormalProtocol()) {
            Log.v(TAG, "Sending message: RDP_AUTH_FAILED");
            handler.sendEmptyMessage(RemoteClientLibConstants.GET_RDP_CREDENTIALS);
        } else if (!disconnectRequested && !myself.isInNormalProtocol()) {
            Log.v(TAG, "Sending message: RDP_UNABLE_TO_CONNECT");
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_UNABLE_TO_CONNECT);
        } else if (!disconnectRequested) {
            myself.setIsInNormalProtocol(false);
            Log.v(TAG, "Sending message: RDP_CONNECT_FAILURE");
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_CONNECT_FAILURE);
        }
    }

    @Override
    public void OnDisconnected(long instance) {
        Log.v(TAG, "OnDisconnected");
        if (!myself.isInNormalProtocol()) {
            Log.v(TAG, "Sending message: RDP_UNABLE_TO_CONNECT");
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_UNABLE_TO_CONNECT);
        } else {
            Log.v(TAG, "Sending message: RDP_CONNECT_FAILURE");
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_CONNECT_FAILURE);
        }
    }

    //////////////////////////////////////////////////////////////////////////////////
    //  Implementation of LibFreeRDP.UIEventListener. Through the functions implemented
    //  below libspice and FreeRDP communicate remote desktop size and updates.
    //////////////////////////////////////////////////////////////////////////////////

    @Override
    public void OnSettingsChanged(int width, int height, int bpp) {
        android.util.Log.d(TAG, "OnSettingsChanged called, wxh: " + width + "x" + height);
        viewable.reallocateDrawable(width, height);
    }

    @Override
    public boolean OnAuthenticate(StringBuilder username, StringBuilder domain, StringBuilder password) {
        android.util.Log.d(TAG, "OnAuthenticate called.");
        authenticationAttempted = true;

        username.setLength(0);
        domain.setLength(0);
        password.setLength(0);

        username.append(this.username);
        domain.append(this.domain);
        password.append(this.password);

        return true;
    }

    @Override
    public int OnVerifiyCertificate(String commonName, String subject,
                                    String issuer, String fingerprint, boolean mismatch) {
        android.util.Log.d(TAG, "OnVerifiyCertificate called.");

        // Send a message containing the certificate to our handler.
        Message m = new Message();
        m.setTarget(handler);
        m.what = RemoteClientLibConstants.DIALOG_RDP_CERT;
        Bundle strings = new Bundle();
        strings.putString("subject", subject);
        strings.putString("issuer", issuer);
        strings.putString("fingerprint", fingerprint);
        m.obj = strings;
        handler.sendMessage(m);

        // Block while user decides whether to accept certificate or not.
        // The activity ends if the user taps "No", so we block indefinitely here.
        synchronized (this) {
            while (!certificateAccepted) {
                try {
                    this.wait();
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }

        return 1;
    }

    @Override
    public boolean OnGatewayAuthenticate(StringBuilder username,
                                         StringBuilder domain, StringBuilder password) {
        android.util.Log.d(TAG, "OnGatewayAuthenticate called.");
        return this.OnAuthenticate(username, domain, password);
    }

    @Override
    public int OnVerifyChangedCertificate(String commonName, String subject,
                                          String issuer, String fingerprint, String oldSubject,
                                          String oldIssuer, String oldFingerprint) {
        android.util.Log.d(TAG, "OnVerifyChangedCertificate called.");
        return this.OnVerifiyCertificate(commonName, subject, issuer, fingerprint, true);
    }

    @Override
    public void OnGraphicsUpdate(int x, int y, int width, int height) {
        //android.util.Log.v(TAG, "OnGraphicsUpdate called: " + x +", " + y + " + " + width + "x" + height );
        if (viewable != null && session != null) {
            Bitmap bitmap = viewable.getBitmap();
            if (bitmap != null) {
                LibFreeRDP.updateGraphics(session.getInstance(), bitmap, x, y, width, height);
                viewable.reDraw(x, y, width, height);
            }
        }
    }

    @Override
    public void OnGraphicsResize(int width, int height, int bpp) {
        android.util.Log.d(TAG, "OnGraphicsResize called.");
        OnSettingsChanged(width, height, bpp);
    }

    @Override
    public void OnRemoteClipboardChanged(String data) {
        android.util.Log.d(TAG, "OnRemoteClipboardChanged called.");

        // Send a message containing the text to our handler.
        Message m = new Message();
        m.setTarget(handler);
        m.what = RemoteClientLibConstants.SERVER_CUT_TEXT;
        Bundle strings = new Bundle();
        strings.putString("text", data);
        m.obj = strings;
        handler.sendMessage(m);
    }
}
