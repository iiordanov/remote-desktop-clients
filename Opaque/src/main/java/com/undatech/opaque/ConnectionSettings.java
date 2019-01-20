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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.Serializable;
import java.util.Iterator;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class ConnectionSettings implements Serializable {
    private static final String TAG = "ConnectionSettings";
    private static final long serialVersionUID = 1L;
    
    private String filename = "";
    private String connectionType = "";
    private String hostname = "";
    private String vmname = "";
    private String user = "";
    private String password = "";
    private String otpCode = "";
    private String inputMethod = Constants.DEFAULT_INPUT_METHOD_ID;
    private boolean rotationEnabled = true;
    private boolean requestingNewDisplayResolution = true;
    private boolean audioPlaybackEnabled = false;
    private boolean usingCustomOvirtCa = false;
    private boolean sslStrict = true;
    private boolean usbEnabled = true;

    private String ovirtCaFile = "";
    private String ovirtCaData = "";
    private String layoutMap = "";
        
    private int extraKeysToggleType = Constants.EXTRA_KEYS_ON;
    
    public ConnectionSettings(String filename) {
        super();
        this.filename = filename;
    }
    
    public String getConnectionType() {
        return connectionType;
    }
    
    public void setConnectionType(String connectionType) {
        this.connectionType = connectionType;
    }
    
    public String getInputMethod() {
        return inputMethod;
    }
    
    public void setInputMethod(String inputMethod) {
        this.inputMethod = inputMethod;
    }
    
    public int getExtraKeysToggleType() {
        return extraKeysToggleType;
    }
    
    public void setExtraKeysToggleType(int extraKeysToggleType) {
        this.extraKeysToggleType = extraKeysToggleType;
    }
    
    public String getHostname() {
        return hostname;
    }
    
    public void setHostname(String hostname) {
        this.hostname = hostname;
    }
    
    public String getVmname() {
        return vmname;
    }
    
    public void setVmname(String vmname) {
        this.vmname = vmname;
    }
    
    public String getUser() {
        return user;
    }
    
    public void setUser(String user) {
        this.user = user;
    }
    
    public String getPassword() {
        return password;
    }
    
    public void setPassword(String password) {
        this.password = password;
    }
    
    public String getOtpCode() {
        return otpCode;
    }

    public void setOtpCode(String otpCode) {
        this.otpCode = otpCode;
    }

    public String getFilename() {
        return filename;
    }

    public void setFilename(String filename) {
        this.filename = filename;
    }

    public boolean isRotationEnabled() {
        return rotationEnabled;
    }

    public void setRotationEnabled(boolean rotationEnabled) {
        this.rotationEnabled = rotationEnabled;
    }

    public boolean isRequestingNewDisplayResolution() {
        return requestingNewDisplayResolution;
    }

    public void setRequestingNewDisplayResolution(
            boolean requestingNewDisplayResolution) {
        this.requestingNewDisplayResolution = requestingNewDisplayResolution;
    }

    public boolean isAudioPlaybackEnabled() {
        return audioPlaybackEnabled;
    }

    public void setAudioPlaybackEnabled(boolean audioPlaybackEnabled) {
        this.audioPlaybackEnabled = audioPlaybackEnabled;
    }

    public boolean isUsingCustomOvirtCa() {
        return usingCustomOvirtCa;
    }

    public void setUsingCustomOvirtCa(boolean useCustomCa) {
        this.usingCustomOvirtCa = useCustomCa;
    }

    public boolean isSslStrict() {
        return sslStrict;
    }

    public void setSslStrict(boolean sslStrict) {
        this.sslStrict = sslStrict;
    }
    
    public boolean isUsbEnabled() {
        return usbEnabled;
    }

    public void setUsbEnabled(boolean usbEnabled) {
        this.usbEnabled = usbEnabled;
    }

    public String getOvirtCaFile() {
        return ovirtCaFile;
    }

    public void setOvirtCaFile(String ovirtCaFile) {
        this.ovirtCaFile = ovirtCaFile;
    }

    public String getOvirtCaData() {
        return ovirtCaData;
    }

    public void setOvirtCaData(String ovirtCaData) {
        this.ovirtCaData = ovirtCaData;
    }

    public String getLayoutMap() {
        return layoutMap;
    }

    public void setLayoutMap(String layoutMap) {
        this.layoutMap = layoutMap;
    }

    public void saveToSharedPreferences(Context context) {
        android.util.Log.d(TAG, "Saving settings to file: " + filename);
        SharedPreferences sp = context.getSharedPreferences(filename, Context.MODE_PRIVATE);
        Editor editor = sp.edit();
        editor.putString("connectionType", connectionType);
        editor.putString("hostname", hostname);
        editor.putString("vmname", vmname);
        editor.putString("user", user);
        editor.putString("password", password);
        editor.putInt("extraKeysToggleType", extraKeysToggleType);
        editor.putString("inputMethod", inputMethod);
        editor.putBoolean("rotationEnabled", rotationEnabled);
        editor.putBoolean("requestingNewDisplayResolution", requestingNewDisplayResolution);
        editor.putBoolean("audioEnabled", audioPlaybackEnabled);
        editor.putBoolean("usingCustomCa", usingCustomOvirtCa);
        editor.putBoolean("sslStrict", sslStrict);
        editor.putBoolean("usbEnabled", usbEnabled);
        editor.putString("ovirtCaData", ovirtCaData);
        editor.putString("layoutMap", layoutMap);
        editor.apply();
        // Make sure the CA gets saved to a file if necessary.
        ovirtCaFile = saveCaToFile (context, ovirtCaData);
    }
    
    public void loadFromSharedPreferences(Context context) {
        android.util.Log.d(TAG, "Loading settings from file: " + filename);
        SharedPreferences sp = context.getSharedPreferences(filename, Context.MODE_PRIVATE);
        connectionType = sp.getString("connectionType", "").trim();
        hostname = sp.getString("hostname", "").trim();
        vmname = sp.getString("vmname", "").trim();
        user = sp.getString("user", "").trim();
        password = sp.getString("password", "");
        loadAdvancedSettings (context, filename);
    }
    
    public void loadAdvancedSettings (Context context, String file) {
        SharedPreferences sp = context.getSharedPreferences(file, Context.MODE_PRIVATE);
        extraKeysToggleType = sp.getInt("extraKeysToggleType", Constants.EXTRA_KEYS_ON);
        inputMethod = sp.getString("inputMethod", Constants.DEFAULT_INPUT_METHOD_ID).trim();
        audioPlaybackEnabled = sp.getBoolean("audioEnabled", false);
        rotationEnabled = sp.getBoolean("rotationEnabled", true);
        requestingNewDisplayResolution = sp.getBoolean("requestingNewDisplayResolution", true);
        usingCustomOvirtCa = sp.getBoolean("usingCustomCa", false);
        sslStrict = sp.getBoolean("sslStrict", true);
        usbEnabled = sp.getBoolean("usbEnabled", true);
        ovirtCaData = sp.getString("ovirtCaData", "").trim();
        layoutMap = sp.getString("layoutMap", Constants.DEFAULT_LAYOUT_MAP).trim();
        // Make sure the CAs get saved to files if necessary.
        ovirtCaFile = saveCaToFile (context, ovirtCaData);
    }
    
    /**
     * Saves provided CA to a file if it doesn't exist already, and returns file name
     * if it was saved or it already exists. Returns "" if caCertData is empty or an error
     * occurred.
     * @param caCertData
     * @return
     */
    private String saveCaToFile (Context context, String caCertData) {
        String fileName = "";
        if (!caCertData.equals("")) {
            // Write out CA to file if it doesn't exist.
            try {
                // Write out a unique file containing the cert and return the path.
                fileName = context.getFilesDir() + "/ca" + Integer.toString(caCertData.hashCode()) + ".crt";
                File file = new File(fileName);
                if (!file.exists()) {
                    android.util.Log.e(TAG, "Writing out CA to file: " + fileName);
                    PrintWriter fout = new PrintWriter(fileName);
                    fout.println(caCertData);
                    fout.close();
                } else {
                    android.util.Log.e(TAG, "File already exists: " + fileName);
                }
            } catch (FileNotFoundException e) {
                fileName = "";
            }
        }
        return fileName;
    }
    
    /**
     * Exports preferences to a file.
     * @param context
     * @param connections space separated list of connections
     * @param externalFileName file to save to
     * @return the full path to the file saved.
     * @throws JSONException 
     * @throws IOException 
     */
    public static String exportPrefsToFile(Context context, String connections, String externalFileName) throws JSONException, IOException {
        android.util.Log.d(TAG, "Exporting settings to file: " + externalFileName);
        connections += " " + Constants.DEFAULT_SETTINGS_FILE;
        String[] preferenceFiles = connections.split(" ");
        JSONObject allPrefs = new JSONObject();
        
        for (String file: preferenceFiles) {
            SharedPreferences sp = context.getSharedPreferences(file, Context.MODE_PRIVATE);
            JSONObject prefs = new JSONObject(sp.getAll());
            prefs.put("password", "");
            allPrefs.put(file, prefs);
        }

        File exportFile = new File(externalFileName);

        PrintWriter writer = new PrintWriter(new FileWriter(exportFile));
        writer.print(allPrefs.toString());
        writer.close();
        return exportFile.getPath();
    }


    /**
     * Imports preferences from a file.
     * @param context
     * @param externalFileName full path to file to load from
     * @return connection list as a space separated string
     * @throws IOException 
     * @throws JSONException 
     */
    public static String importPrefsFromFile(Context context, String externalFileName) throws IOException, JSONException {
        android.util.Log.d(TAG, "Importing settings from file: " + externalFileName);
        File importFile = new File(externalFileName);
        String connections = "";
        InputStream is = new FileInputStream(importFile);
        BufferedReader reader = new BufferedReader(new InputStreamReader(is));
        StringBuilder sb = new StringBuilder();
        String line = null;
        while ((line = reader.readLine()) != null) {
            sb.append(line);
        }
        reader.close();

        JSONObject allPrefs = new JSONObject(sb.toString());
        Iterator<String> allPrefsItr = allPrefs.keys();
        while(allPrefsItr.hasNext()) {
            String file = allPrefsItr.next();
            SharedPreferences sp = context.getSharedPreferences(file, Context.MODE_PRIVATE);
            Editor editor = sp.edit();

            JSONObject settings = allPrefs.getJSONObject(file);

            Iterator<String> keysItr = settings.keys();
            while(keysItr.hasNext()) {
                String key = keysItr.next();
                Object value = settings.get(key);

                if(value instanceof String) {
                    editor.putString(key, (String)value);
                } else if(value instanceof Integer) {
                    editor.putInt(key, (int)((Integer)value));
                } else if(value instanceof Boolean) {
                    editor.putBoolean(key, (boolean)((Boolean)value));
                } else if(value instanceof Float) {
                    editor.putFloat(key, (float)((Float)value));
                } else if(value instanceof Long) {
                    editor.putLong(key, (long)((Long)value));
                }
            }
            if (!file.equals(Constants.DEFAULT_SETTINGS_FILE)) {
                connections += " " + file;
            }
            editor.apply();
        }

        return connections.trim();
    }
}
