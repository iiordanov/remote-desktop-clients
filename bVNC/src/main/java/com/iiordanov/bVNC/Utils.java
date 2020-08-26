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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.Reader;
import java.io.StringWriter;
import java.io.Writer;
import java.lang.reflect.Field;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.UUID;

import org.json.JSONException;
import org.xml.sax.SAXException;

import com.antlersoft.android.contentxml.SqliteElement;
import com.antlersoft.android.contentxml.SqliteElement.ReplaceStrategy;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ActivityManager.MemoryInfo;
import android.app.Dialog;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences.Editor;
import android.content.Intent;
import net.sqlcipher.database.SQLiteDatabase;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;
import android.text.Html;
import android.util.Log;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.widget.ListView;
import android.widget.ScrollView;

import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;
import com.iiordanov.CustomClientPackage.*;
import com.undatech.opaque.ConnectionSettings;
import com.undatech.opaque.ConnectionSetupActivity;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.dialogs.MessageFragment;
import com.undatech.opaque.util.FileUtils;
import com.undatech.remoteClientUi.*;

public class Utils {
    private final static String TAG = "Utils";
    private static AlertDialog alertDialog;

    public static void showYesNoPrompt(Context _context, String title, String message, OnClickListener onYesListener, OnClickListener onNoListener) {
        try {
            if (alertDialog != null && alertDialog.isShowing() && !isContextActivityThatIsFinishing(_context)) {
                alertDialog.dismiss();
            }
            AlertDialog.Builder builder = new AlertDialog.Builder(_context);
            builder.setTitle(title);
            builder.setIcon(android.R.drawable.ic_dialog_info);
            builder.setMessage(message);
            builder.setCancelable(false);
            builder.setPositiveButton(_context.getString(android.R.string.yes), onYesListener);
            builder.setNegativeButton(_context.getString(android.R.string.no), onNoListener);
            if (!(alertDialog != null && alertDialog.isShowing()) && !isContextActivityThatIsFinishing(_context)) {
                alertDialog = builder.create();
                alertDialog.show();
            }
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
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
        showMessage(_context, _context.getString(R.string.error) + "!", message, android.R.drawable.ic_dialog_alert, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
    }

    public static void showFatalErrorMessage(final Context _context, String message) {
        showMessage(_context, _context.getString(R.string.error) + "!", message, android.R.drawable.ic_dialog_alert, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
                Activity activity = Utils.getActivity(_context);
                if (activity != null) {
                    activity.finish();
                }
            }
        });
    }
    
    public static void showMessage(Context _context, String title, String message, int icon, DialogInterface.OnClickListener ackHandler) {
        try {
            if (alertDialog != null && alertDialog.isShowing() && !isContextActivityThatIsFinishing(_context)) {
                alertDialog.dismiss();
            }
            AlertDialog.Builder builder = new AlertDialog.Builder(_context);
            builder.setTitle(title);
            builder.setMessage(Html.fromHtml(message));
            builder.setCancelable(false);
            builder.setPositiveButton(_context.getString(android.R.string.ok), ackHandler);
            builder.setIcon(icon);
            if (!(alertDialog != null && alertDialog.isShowing()) && !isContextActivityThatIsFinishing(_context)) {
                alertDialog = builder.create();
                alertDialog.show();
            }
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        }
    }
    
    /**
     * Determine if a string is null or empty
     * @param s The string to comapare
     * @return true iff s is null or empty
     */
    public static boolean isNullOrEmptry(String s)
    {
    	if (s == null || s.equals(""))
    		return true;
    	return false;
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
    
    /**
     * Forces the appearance of a menu in the given context.
     * @param ctx
     */
    public static void showMenu (Context ctx) {
        try {
            ViewConfiguration config = ViewConfiguration.get(ctx);
            Field menuKeyField = ViewConfiguration.class.getDeclaredField("sHasPermanentMenuKey");

            if (menuKeyField != null) {
              menuKeyField.setAccessible(true);
              menuKeyField.setBoolean(config, false);
            }
          }
          catch (Exception e) {}
    }
    
    public static boolean isFree(Context ctx) {
        return ctx.getPackageName().contains("free");
    }
    
    public static String getConnectionString(Context ctx) {
        return ctx.getPackageName() + ".CONNECTION";
    }

    public static String[] standardPackageNames = {
                    "com.iiordanov.bVNC", "com.iiordanov.freebVNC",
                    "com.iiordanov.aRDP", "com.iiordanov.freeaRDP",
                    "com.iiordanov.aSPICE", "com.iiordanov.freeaSPICE"
    };

    public static boolean isCustom(String packageName) {
        for (String s: standardPackageNames) {
            if (packageName.equals(s)) {
                return false;
            }
        }
        return true;
    }

    public static boolean isVnc(String packageName) {
        return packageName.toLowerCase().contains("vnc");
    }
    
    public static boolean isRdp(String packageName) {
        return packageName.toLowerCase().contains("rdp");
    }
    
    public static boolean isSpice(String packageName) {
        return packageName.toLowerCase().contains("spice");
    }

    public static boolean isOpaque(String packageName) {
        return packageName.toLowerCase().contains("opaque");
    }

    public static Class getConnectionSetupClass(String packageName) {
        boolean custom = isCustom(packageName);
        if (isOpaque(packageName)) {
            return ConnectionSetupActivity.class;
        } else if (isVnc(packageName)) {
            if (custom) {
                return CustomVnc.class;
            } else {
                return bVNC.class;
            }
        } else if (isRdp(packageName)) {
            if (custom) {
                throw new IllegalArgumentException("Custom SPICE clients not supported yet.");
            } else {
                return aRDP.class;
            }
        } else if (isSpice(packageName)) {
            if (custom) {
                throw new IllegalArgumentException("Custom SPICE clients not supported yet.");
            } else {
                return aSPICE.class;
            }
        } else {
            throw new IllegalArgumentException("Could not find appropriate connection setup activity class for package " + packageName);
        }
    }

    public static String getConnectionScheme(Context ctx) {
        String packageName = ctx.getPackageName();
        String scheme = "unsupported";
        if (isVnc(packageName))
            scheme = "vnc";
        else if (isRdp(packageName))
            scheme = "rdp";
        else if (isSpice(packageName))
            scheme = "spice";
        return scheme;
    }
    
    public static int getDefaultPort(Context ctx) {
        int port = Constants.DEFAULT_PROTOCOL_PORT;
        if (ctx != null) {
            String packageName = ctx.getPackageName();
            if (isRdp(packageName))
                port = Constants.DEFAULT_RDP_PORT;
            else
                port = Constants.DEFAULT_VNC_PORT;
        }
        return port;
    }
    
    public static String getDonationPackageName(Context ctx) {
        return ctx.getPackageName().replace("free", "");
    }
    
    public static boolean isBlackBerry () {
        boolean bb = false;
        if (android.os.Build.MODEL.contains("BlackBerry") ||
            android.os.Build.BRAND.contains("BlackBerry") || 
            android.os.Build.MANUFACTURER.contains("BlackBerry")) {
            bb = true;
        }
        return bb;
    }
    
    public static void exportSettingsToXml (String file, SQLiteDatabase db) throws SAXException, IOException {
        File f = new File(file);
        Writer writer = new OutputStreamWriter(new FileOutputStream(f, false));
        SqliteElement.exportDbAsXmlToStream(db, writer);
        writer.close();
    }

    public static String getExportFileName(String packageName) {
        String res = "settings.xml";
        if (isVnc(packageName))
            res = "vnc_" + res;
        else if (isRdp(packageName))
            res = "rdp_" + res;
        else if (isSpice(packageName))
            res = "spice_" + res;
        else if (isOpaque(packageName))
            res = "opaque_settings.json";
        return res;
    }

    public static void importSettingsFromXml (String file, SQLiteDatabase db) throws SAXException, IOException {
        Reader reader = new InputStreamReader(new FileInputStream(file));
        SqliteElement.importXmlStreamToDb(db, reader, ReplaceStrategy.REPLACE_EXISTING);
    }
    
    public static boolean isValidIpv6Address(final String address) {
        try {
            return InetAddress.getByName(address) instanceof Inet6Address;
        } catch (final UnknownHostException ex) {
            return false;
        }
    }
    
    public static String messageAndStackTraceAsString (Exception e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        String localizedMessage = e.getLocalizedMessage();
        if (localizedMessage == null)
            localizedMessage = "";
            
        return "\n" + localizedMessage + "\n" + sw.toString();
    }
    
    public static boolean querySharedPreferenceBoolean(Context context, String key) {
        boolean result = false;
        if (context != null) {
            SharedPreferences sp = context.getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
            result = sp.getBoolean(key, false);
        }
        return result;
    }

    public static String querySharedPreferenceString(Context context, String key, String dftValue) {
        String result = dftValue;
        if (context != null) {
            SharedPreferences sp = context.getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
            result = sp.getString(key, dftValue);
        }
        return result;
    }

    public static void setSharedPreferenceString(Context context, String key, String value) {
        if (context != null) {
            SharedPreferences sp = context.getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
            Editor editor = sp.edit();
            editor.putString(key, value);
            editor.apply();
            Log.i(TAG, "Set: " + key + " to value: " + value);
        }
    }

    public static void setSharedPreferenceBoolean(Context context, String key, boolean value) {
        if (context != null) {
            SharedPreferences sp = context.getSharedPreferences(Constants.generalSettingsTag, Context.MODE_PRIVATE);
            Editor editor = sp.edit();
            editor.putBoolean(key, value);
            editor.apply();
            Log.i(TAG, "Set: " + key + " to value: " + value);
        }
    }


    public static void toggleSharedPreferenceBoolean(Context context, String key) {
        if (context != null) {
            SharedPreferences sp = context.getSharedPreferences(Constants.generalSettingsTag,
                    Context.MODE_PRIVATE);
            boolean state = sp.getBoolean(key, false);
            Editor editor = sp.edit();
            editor.putBoolean(key, !state);
            editor.apply();
            Log.i(TAG, "Toggled " + key + " " + String.valueOf(state));
        }
    }

    static boolean isContextActivityThatIsFinishing(Context _context) {
        boolean result = false;
        if (_context instanceof Activity) {
            Activity activity = (Activity)_context;
            if (activity.isFinishing()) {
                result = true;
            }
        }
        return result;
    }

    static void writeScreenshotToFile(Context context, AbstractBitmapData drawable,
                                      String filePath, int dstWidth, int dstHeight) {
        try {
            if (drawable != null && drawable.mbitmap != null) {
                // TODO: Add Filename to settings.
                FileOutputStream out = new FileOutputStream(filePath);
                Bitmap tmp = Bitmap.createScaledBitmap(drawable.mbitmap, dstWidth, dstHeight, true);
                drawable.mbitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                out.close();
                tmp.recycle();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    /**
     * Either returns the input of it's already a UUID or returns a random UUID.
     * @param string which if it's a string representation of a UUID will be returned unaltered
     * @return the input if it's a UUID or a random UUID as a string
     */
    static String getUuid(String string) {
        try {
            UUID.fromString(string);
            return string;
        } catch (IllegalArgumentException e) {
            return UUID.randomUUID().toString();
        }
    }

    /**
     * Creates a connection screen help dialog for each app.
     * @param context
     * @return
     */
    public static Dialog createMainScreenDialog(Context context) {
        int textId = R.string.main_screen_help_text;
        return createDialog(context, textId);
    }

    /**
     * Creates a connection screen help dialog for each app.
     * @param context
     * @return
     */
    public static Dialog createConnectionScreenDialog(Context context) {
        int textId = R.string.vnc_connection_screen_help_text;
        if (Utils.isRdp(context.getPackageName()))
            textId = R.string.rdp_connection_screen_help_text;
        else if (Utils.isSpice(context.getPackageName()))
            textId = R.string.spice_connection_screen_help_text;
        else if (Utils.isOpaque(context.getPackageName()))
            textId = R.string.opaque_connection_screen_help_text;
        return createDialog(context, textId);
    }

    public static Dialog createDialog(Context context, int textId) {
        AlertDialog.Builder adb = new AlertDialog.Builder(context)
                .setMessage(textId)
                .setPositiveButton(R.string.close,
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog,
                                                int whichButton) {
                                // We don't have to do anything.
                            }
                        });
        Dialog d = adb.setView(new ScrollView(context)).create();
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
        lp.copyFrom(d.getWindow().getAttributes());
        lp.width = WindowManager.LayoutParams.MATCH_PARENT;
        lp.height = WindowManager.LayoutParams.WRAP_CONTENT;
        d.show();
        d.getWindow().setAttributes(lp);
        return d;
    }

    public static String newScreenshotFileName() {
        return UUID.randomUUID().toString() + ".png";
    }

    public static String getHostFromUriString(String uriString) {
        if (!uriString.startsWith("http")) {
            uriString = "https://" + uriString;
        }
        Uri uri = Uri.parse(uriString);
        String host = uri.getHost();
        return host;
    }

    public static Activity getActivity(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity)context;
            }
            context = ((ContextWrapper)context).getBaseContext();
        }
        return null;
    }

    public static String getVersionAndCode(Context context) {
        String result = "";
        try {
            String packageName = context.getPackageName();
            PackageInfo pInfo = context.getPackageManager().getPackageInfo(packageName, 0);
            result = pInfo.versionName + "_" + pInfo.versionCode;
            android.util.Log.d(TAG, "Version of " + packageName + " is " + result);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return result;
    }
}
