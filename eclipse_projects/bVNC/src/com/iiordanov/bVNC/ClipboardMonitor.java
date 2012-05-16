package com.iiordanov.bVNC;

import java.io.IOException;
import java.util.TimerTask;
import android.content.Context;
import android.text.ClipboardManager;
import android.util.Log;

/*
 * This is a TimerTask which checks the clipboard for changes, and if
 * a change is detected, sends the new contents to the VNC server.
 */

public class ClipboardMonitor extends TimerTask {
	private String TAG = "ClipboardMonitor";
	private Context context;
	ClipboardManager clipboard;
	private String knownClipboardContents;
	VncCanvas vncCanvas;
	
	public ClipboardMonitor (Context c, VncCanvas vc) {
		context = c;
		vncCanvas = vc;
		clipboard = (ClipboardManager)context.getSystemService(Context.CLIPBOARD_SERVICE);
		knownClipboardContents = getClipboardContents ();
	}
	
	/*
	 * Grab the currrent clipboard contents.
	 */
	private String getClipboardContents () {
		if (clipboard != null && clipboard.getText() != null)
			return clipboard.getText().toString();
		else
			return null;
	}
	
	/*
	 * (non-Javadoc)
	 * @see java.util.TimerTask#run()
	 */
	@Override
	public void run() {
		String currentClipboardContents = getClipboardContents ();
		//Log.d(TAG, "Current clipboard contents: " + currentClipboardContents);
		//Log.d(TAG, "Previously known clipboard contents: " + knownClipboardContents);
		
		if (currentClipboardContents != null &&
			currentClipboardContents.compareTo(knownClipboardContents) != 0){
			try {
				if (vncCanvas.rfb != null && vncCanvas.rfb.inNormalProtocol)
					vncCanvas.rfb.writeClientCutText(currentClipboardContents);
					knownClipboardContents = new String(currentClipboardContents);
					//Log.d(TAG, "Wrote: " + knownClipboardContents + " to remote clipboard.");
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}			
		}
	}
}
