/**
 * Copyright (C) 2012 Iordan Iordanov
 * Copyright (C) 2010 Michael A. MacDonald
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.bVNC;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ActivityManager.MemoryInfo;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.net.Uri;
import android.text.Html;

public class Utils {

	public static void showYesNoPrompt(Context _context, String title, String message, OnClickListener onYesListener, OnClickListener onNoListener) {
		AlertDialog.Builder builder = new AlertDialog.Builder(_context);
		builder.setTitle(title);
		builder.setIcon(android.R.drawable.ic_dialog_info);
		builder.setMessage(message);
		builder.setCancelable(false);
		builder.setPositiveButton("Yes", onYesListener);
		builder.setNegativeButton("No", onNoListener);
		if (!(((Activity) _context).isFinishing())) {
			builder.show();
		}
	}
	
	private static final Intent docIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://code.google.com/p/android-vnc-viewer/wiki/Documentation")); 

	public static ActivityManager getActivityManager(Context context)
	{
		ActivityManager result = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
		if (result == null)
			throw new UnsupportedOperationException("Could not retrieve ActivityManager");
		return result;
	}
	
	public static MemoryInfo getMemoryInfo(Context _context) {
		MemoryInfo info = new MemoryInfo();
		getActivityManager(_context).getMemoryInfo(info);
		return info;
	}
	
	public static void showDocumentation(Context c) {
		c.startActivity(docIntent);
	}

	private static int nextNoticeID = 0;
	public static int nextNoticeID() {
		nextNoticeID++;
		return nextNoticeID;
	}

	public static void showErrorMessage(Context _context, String message) {
		showMessage(_context, "Error!", message, android.R.drawable.ic_dialog_alert, null);
	}

	public static void showFatalErrorMessage(final Context _context, String message) {
		showMessage(_context, "Error!", message, android.R.drawable.ic_dialog_alert, new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which) {
				((Activity) _context).finish();
			}
		});
	}
	
	public static void showMessage(Context _context, String title, String message, int icon, DialogInterface.OnClickListener ackHandler) {
		AlertDialog.Builder builder = new AlertDialog.Builder(_context);
		builder.setTitle(title);
		builder.setMessage(Html.fromHtml(message));
		builder.setCancelable(false);
		builder.setPositiveButton("Acknowledged", ackHandler);
		builder.setIcon(icon);
		if (!(((Activity) _context).isFinishing())) {
			builder.show();
		}
	}
	
	/**
	 * Converts a given sequence of bytes to a human-readable colon-separated Hex format. 
	 * @param bytes
	 * @return
	 */
    public static String toHexString(byte[] bytes) {
        char[] hexArray = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
        char[] hexChars = new char[bytes.length * 3];
        int v, j;
        for ( j = 0; j < bytes.length - 1; j++ ) {
            v = bytes[j] & 0xFF;
            hexChars[j*3] = hexArray[v/16];
            hexChars[j*3 + 1] = hexArray[v%16];
            hexChars[j*3 + 2] = ":".charAt(0);
        }
        v = bytes[j] & 0xFF;
        hexChars[j*3] = hexArray[v/16];
        hexChars[j*3 + 1] = hexArray[v%16];
        return new String(hexChars);
    }
}
