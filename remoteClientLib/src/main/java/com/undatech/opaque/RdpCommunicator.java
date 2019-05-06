package com.undatech.opaque;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.undatech.opaque.input.RemoteKeyboard;
import com.undatech.opaque.input.RdpKeyboardMapper;
import com.undatech.opaque.input.RemotePointer;

public class RdpCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener,
                                        LibFreeRDP.UIEventListener, LibFreeRDP.EventListener {
    static final String TAG = "RdpCommunicator";

    final static int VK_CONTROL = 0x11;
    final static int VK_LCONTROL = 0xA2;
    final static int VK_RCONTROL = 0xA3;
    final static int VK_LMENU = 0xA4;
    final static int VK_RMENU = 0xA5;
    final static int VK_LSHIFT = 0xA0;
    final static int VK_RSHIFT = 0xA1;
    final static int VK_LWIN = 0x5B;
    final static int VK_RWIN = 0x5C;
    final static int VK_EXT_KEY = 0x00000100;

    SessionState session;
    int metaState = 0;
    boolean isInNormalProtocol = false;

    private final RdpCommunicator myself;
    private final Handler handler;
    private final Viewable viewable;

    // This variable indicates whether or not the user has accepted an untrusted
    // security certificate. Used to control progress while the dialog asking the user
    // to confirm the authenticity of a certificate is displayed.
    private boolean certificateAccepted = false;

    public RdpCommunicator(SessionState session, Handler handler, Viewable viewable) {
        this.session = session;
        this.handler = handler;
        this.viewable = viewable;
        myself = this;
    }

    @Override
    public void setIsInNormalProtocol (boolean state) {
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
        if (down) {
            sendModifierKeys(true);
        }
        android.util.Log.d("RdpCommunicator", "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);
        try { Thread.sleep(5); } catch (InterruptedException e) {}
        LibFreeRDP.sendKeyEvent(session.getInstance(), virtualKeyCode, down);
        if (!down) {
            sendModifierKeys(false);
        }
    }

    @Override
    public void processUnicodeKey(int unicodeKey) {
        android.util.Log.e(TAG, "Unicode character: " + unicodeKey);
        sendModifierKeys(true);
        try { Thread.sleep(5); } catch (InterruptedException e) {}
        LibFreeRDP.sendUnicodeKeyEvent(session.getInstance(), unicodeKey);
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
        myself.setIsInNormalProtocol(true);
    }

    @Override
    public void OnConnectionFailure(long instance) {
        Log.v(TAG, "OnConnectionFailure");
        myself.setIsInNormalProtocol(false);
        handler.sendEmptyMessage(RemoteClientLibConstants.RDP_UNABLE_TO_CONNECT);
    }

    @Override
    public void OnDisconnecting(long instance) {
        myself.setIsInNormalProtocol(false);
        Log.v(TAG, "OnDisconnecting");
        handler.sendEmptyMessage(RemoteClientLibConstants.RDP_CONNECT_FAILURE);
    }

    @Override
    public void OnDisconnected(long instance) {
        Log.v(TAG, "OnDisconnected");
        if (!myself.isInNormalProtocol()) {
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_UNABLE_TO_CONNECT);
        } else {
            handler.sendEmptyMessage(RemoteClientLibConstants.RDP_CONNECT_FAILURE);
        }
    }

    //////////////////////////////////////////////////////////////////////////////////
    //  Implementation of LibFreeRDP.UIEventListener. Through the functions implemented
    //  below libspice and FreeRDP communicate remote desktop size and updates.
    //////////////////////////////////////////////////////////////////////////////////

    @Override
    public void OnSettingsChanged(int width, int height, int bpp) {
        android.util.Log.e(TAG, "OnSettingsChanged called, wxh: " + width + "x" + height);
        viewable.reallocateDrawable(width, height);
    }

    @Override
    public boolean OnAuthenticate(StringBuilder username, StringBuilder domain, StringBuilder password) {
        android.util.Log.e(TAG, "OnAuthenticate called.");
        handler.sendEmptyMessage(RemoteClientLibConstants.RDP_AUTH_FAILED);
        return false;
    }

    @Override
    public int OnVerifiyCertificate(String commonName, String subject,
                                    String issuer, String fingerprint, boolean mismatch) {
        android.util.Log.e(TAG, "OnVerifiyCertificate called.");

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
        android.util.Log.e(TAG, "OnGatewayAuthenticate called.");
        return this.OnAuthenticate(username, domain, password);
    }

    @Override
    public int OnVerifyChangedCertificate(String commonName, String subject,
                                          String issuer, String fingerprint, String oldSubject,
                                          String oldIssuer, String oldFingerprint) {
        android.util.Log.e(TAG, "OnVerifyChangedCertificate called.");
        return this.OnVerifiyCertificate(commonName, subject, issuer, fingerprint, true);
    }

    @Override
    public void OnGraphicsUpdate(int x, int y, int width, int height) {
        //android.util.Log.e(TAG, "OnGraphicsUpdate called: " + x +", " + y + " + " + width + "x" + height );
        if (viewable != null && session != null) {
            Bitmap bitmap = viewable.getBitmap();
            if (bitmap != null) {
                synchronized (viewable) {
                    LibFreeRDP.updateGraphics(session.getInstance(), bitmap, x, y, width, height);
                }
            }
            viewable.reDraw(x, y, width, height);
        }
    }

    @Override
    public void OnGraphicsResize(int width, int height, int bpp) {
        android.util.Log.e(TAG, "OnGraphicsResize called.");
        OnSettingsChanged(width, height, bpp);
    }

    @Override
    public void OnRemoteClipboardChanged(String data) {
        android.util.Log.e(TAG, "OnRemoteClipboardChanged called.");

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
