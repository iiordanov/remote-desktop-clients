/**
 * Copyright (C) 2012 Iordan Iordanov
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

package com.iiordanov.bVNC;

import android.content.Context;
import android.text.ClipboardManager;
import android.util.Log;

import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.Viewable;

import java.util.TimerTask;

/*
 * This is a TimerTask which checks the clipboard for changes, and if
 * a change is detected, sends the new contents to the VNC server.
 */

public class ClipboardMonitor extends TimerTask {
    ClipboardManager clipboard;
    private final String TAG = "ClipboardMonitor";
    private final Context context;
    private String knownClipboardContents;

    RfbConnectable rfbConnectable;
    Viewable viewable;

    public ClipboardMonitor(Viewable viewable, Context c, RfbConnectable rfbConnectable) {
        context = c;
        this.rfbConnectable = rfbConnectable;
        this.viewable = viewable;
        clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        knownClipboardContents = "";
    }

    /*
     * Grab the current clipboard contents.
     */
    private String getClipboardContents() {
        try {
            return clipboard.getText().toString();
        } catch (NullPointerException e) {
            Log.e(TAG, "NullPointerException obtaining clipboard string");
            return null;
        } catch (RuntimeException e) {
            Log.e(TAG, "RuntimeException obtaining clipboard string");
            return null;
        }
    }

    /*
     * (non-Javadoc)
     * @see java.util.TimerTask#run()
     */
    @Override
    public void run() {
        if (!viewable.isForegrounded()) {
            Log.v(TAG, "App backgrounded, not monitoring clipboard");
            return;
        }
        
        String currentClipboardContents = getClipboardContents();
        //Log.v(TAG, "Current clipboard contents: " + currentClipboardContents);
        //Log.v(TAG, "Previously known clipboard contents: " + knownClipboardContents);
        if (rfbConnectable != null) {
            if (!rfbConnectable.serverJustCutText && currentClipboardContents != null &&
                    !currentClipboardContents.equals(knownClipboardContents)) {
                if (rfbConnectable.isInNormalProtocol()) {
                    rfbConnectable.writeClientCutText(currentClipboardContents);
                    knownClipboardContents = currentClipboardContents;
                    //Log.v(TAG, "Wrote: " + knownClipboardContents + " to remote clipboard.");
                }
            } else if (rfbConnectable.serverJustCutText && currentClipboardContents != null) {
                knownClipboardContents = currentClipboardContents;
                rfbConnectable.serverJustCutText = false;
                //Log.v(TAG, "Set knownClipboardContents to equal whatever the server just sent over.");
            }
        }
    }
}
