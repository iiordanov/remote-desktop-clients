package com.iiordanov.bVNC.input;

import android.view.KeyEvent;
import com.iiordanov.bVNC.MetaKeyBean;

public interface RemoteKeyboard {
    // Useful shortcuts for modifier masks.
    public final static int CTRL_MASK  = KeyEvent.META_SYM_ON;
    public final static int SHIFT_MASK = KeyEvent.META_SHIFT_ON;
    public final static int ALT_MASK   = KeyEvent.META_ALT_ON;
    public final static int SUPER_MASK = 8;
    public final static int META_MASK  = 0;
    
	public boolean processLocalKeyEvent(int keyCode, KeyEvent evt); 
	public void sendMetaKey(MetaKeyBean meta);
	public boolean onScreenCtrlToggle();
	public void onScreenCtrlOff();
	public boolean onScreenAltToggle();
	public void onScreenAltOff();
	public int getMetaState ();
	public void setAfterMenu(boolean value);
	public boolean getCameraButtonDown();
	public void clearMetaState ();
}
