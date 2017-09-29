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

import android.app.Activity;
import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Message;
import android.text.Html;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.util.Linkify;
import android.widget.TextView;

public class MessageDialogs {

    /**
     * Converts a given sequence of bytes to a human-readable colon-separated
     * Hex format.
     * 
     * @param bytes
     * @return
     */
    public static String toHexString(byte[] bytes) {
        char[] hexArray = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                'A', 'B', 'C', 'D', 'E', 'F' };
        char[] hexChars = new char[bytes.length * 3];
        int v, j;
        for (j = 0; j < bytes.length - 1; j++) {
            v = bytes[j] & 0xFF;
            hexChars[j * 3] = hexArray[v / 16];
            hexChars[j * 3 + 1] = hexArray[v % 16];
            hexChars[j * 3 + 2] = ":".charAt(0);
        }
        v = bytes[j] & 0xFF;
        hexChars[j * 3] = hexArray[v / 16];
        hexChars[j * 3 + 1] = hexArray[v % 16];
        return new String(hexChars);
    }

    /**
     * Displays a generic dialog.
     * 
     * @param context
     * @param alertTitle
     * @param alert
     * @param ok
     */
    private static void displayDialog(final Context context, int alertTitleID,
            int alertID, String appendText, DialogInterface.OnClickListener ok) {
        boolean show = true;
        if (context instanceof Activity) {
            Activity activity = (Activity) context;
            if (activity.isFinishing()) {
                show = false;
            }
        }

        if (show) {
            Builder builder = new Builder((Activity) context);
            builder.setCancelable(false);
            builder.setTitle(alertTitleID);
            String displayText = context.getString(alertID);
            if (appendText != null) {
                displayText = displayText + " " + appendText;
            }
            Spanned text = Html.fromHtml(displayText);
            final TextView message = new TextView(context);
            message.setText(text);
            message.setMovementMethod(LinkMovementMethod.getInstance());
            message.setPaddingRelative(50, 50, 50, 50);
            builder.setView(message);
            builder.setPositiveButton("OK", ok);
            builder.show();
        }
    }

    /**
     * Displays an info dialog which is dismissed on pressing OK.
     * 
     * @param context
     * @param info
     */
    public static void displayMessage(final Context context, int infoId,
            int titleId) {
        displayDialog(context, titleId, infoId, null,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
    }

    /**
     * Displays an error dialog that dismisses the calling activity on pressing
     * OK.
     * 
     * @param context
     * @param error
     */
    public static void displayMessageAndFinish(final Context context,
            int messageId, int titleId) {
        displayDialog(context, titleId, messageId, null,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        ((Activity) context).finish();
                    }
                });
    }

    /**
     * Displays an error dialog that dismisses the calling activity on pressing
     * OK.
     * 
     * @param context
     * @param error
     */
    public static void displayMessageAndFinish(final Context context,
            int messageId, int titleId, String appendText) {
        displayDialog(context, titleId, messageId, appendText,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        ((Activity) context).finish();
                    }
                });
    }
}
