package com.iiordanov.bVNC;

import com.freerdp.freerdpcore.application.SessionState;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.services.LibFreeRDP;
import com.iiordanov.bVNC.input.RemoteKeyboard;
import com.iiordanov.bVNC.input.RdpKeyboardMapper;
import com.iiordanov.bVNC.input.RemoteRdpPointer;

public class RdpCommunicator implements RfbConnectable, RdpKeyboardMapper.KeyProcessingListener {
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

    RdpCommunicator (SessionState session) {
        this.session = session;
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
    public void writePointerEvent(int x, int y, int metaState, int pointerMask) {
        this.metaState = metaState;
        if ((pointerMask & RemoteRdpPointer.PTRFLAGS_DOWN) != 0)
            sendModifierKeys(true);
        LibFreeRDP.sendCursorEvent(session.getInstance(), x, y, pointerMask);
        if ((pointerMask & RemoteRdpPointer.PTRFLAGS_DOWN) == 0)
            sendModifierKeys(false);
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
        int instance;
        
        public DisconnectThread (int i) {
            this.instance = i;
        }
        public void run () {
            LibFreeRDP.disconnect(instance);
            LibFreeRDP.freeInstance(instance);
        }
    }
    
    @Override
    public void close() {
        setIsInNormalProtocol(false);
        int instance = session.getInstance();
        DisconnectThread d = new DisconnectThread(instance);
        d.start();
        //session = null;
    }
    
    private void sendModifierKeys (boolean down) {
        if ((metaState & RemoteKeyboard.CTRL_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending LCTRL " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LCONTROL, down);
        }
        if ((metaState & RemoteKeyboard.RCTRL_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending RCTRL " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RCONTROL, down);
        }
        if ((metaState & RemoteKeyboard.ALT_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending LALT " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LMENU, down);
        }
        if ((metaState & RemoteKeyboard.RALT_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending RALT " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RMENU, down);
        }
        if ((metaState & RemoteKeyboard.SUPER_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending LSUPER " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LWIN | VK_EXT_KEY, down);
        }
        if ((metaState & RemoteKeyboard.RSUPER_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending RSUPER " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RWIN | VK_EXT_KEY, down);
        }
        if ((metaState & RemoteKeyboard.SHIFT_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending LSHIFT " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_LSHIFT, down);
        }
        if ((metaState & RemoteKeyboard.RSHIFT_MASK) != 0) {
            //android.util.Log.e("RdpCommunicator", "Sending RSHIFT " + down);
            LibFreeRDP.sendKeyEvent(session.getInstance(), VK_RSHIFT, down);
        }
    }
    
    // ****************************************************************************
    // KeyboardMapper.KeyProcessingListener implementation
    @Override
    public void processVirtualKey(int virtualKeyCode, boolean down) {
        if (down)
            sendModifierKeys(true);
        //android.util.Log.e("RdpCommunicator", "Sending VK key: " + virtualKeyCode + ". Is it down: " + down);
        LibFreeRDP.sendKeyEvent(session.getInstance(), virtualKeyCode, down);
        if (!down)
            sendModifierKeys(false);
    }

    @Override
    public void processUnicodeKey(int unicodeKey) {
        android.util.Log.e("SpiceCommunicator", "Unicode character: " + unicodeKey);
        sendModifierKeys(true);
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
}
