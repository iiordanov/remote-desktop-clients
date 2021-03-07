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

package com.iiordanov.bVNC.dialogs;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import com.iiordanov.android.bc.BCFactory;
import com.iiordanov.bVNC.MainConfiguration;
import com.iiordanov.bVNC.Utils;

import java.io.File;
import java.io.IOException;
import java.nio.file.Paths;

import org.xml.sax.SAXException;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;
import com.iiordanov.CustomClientPackage.*;
import com.undatech.opaque.ConnectionSettings;
import com.undatech.remoteClientUi.*;

/**
 * @author Michael A. MacDonald
 *
 */
public class ImportExportDialog extends Dialog {
    public static final String TAG = "ImportExportDialog";
    private Activity activity;
    private EditText _textLoadUrl;
    private EditText _textSaveUrl;
    private Database database;
    private boolean connectionsInSharedPrefs;

    /**
     * @param context
     */
    public ImportExportDialog(Activity context, Database database, boolean connectionsInSharedPrefs) {
        super((Context)context);
        setOwnerActivity((Activity)context);
        activity = context;
        this.database = database;
        this.connectionsInSharedPrefs = connectionsInSharedPrefs;
    }

    /* (non-Javadoc)
     * @see android.app.Dialog#onCreate(android.os.Bundle)
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.importexport);
        setTitle(R.string.import_export_settings);
        _textLoadUrl = (EditText)findViewById(R.id.textImportUrl);
        _textSaveUrl = (EditText)findViewById(R.id.textExportPath);

        File f = new File(Environment.getExternalStorageDirectory().getPath());
        f = new File(f, Utils.getExportFileName(getContext().getPackageName()));

        String path = f.getAbsolutePath();
        _textSaveUrl.setText(path);
        _textLoadUrl.setText(path);
        
        Button export = (Button)findViewById(R.id.buttonExport);
        export.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                try {
                    if (connectionsInSharedPrefs) {
                        ConnectionSettings.exportSettingsFromSharedPrefsToJson(_textLoadUrl.getText().toString(), getContext());
                    } else {
                        Utils.exportSettingsToXml(_textSaveUrl.getText().toString(), database.getReadableDatabase());
                    }
                    dismiss();
                }
                catch (IOException ioe)
                {
                    errorNotify("I/O Exception exporting config", ioe);
                } catch (SAXException e) {
                    errorNotify("XML Exception exporting config", e);
                }
            }
            
        });
        
        ((Button)findViewById(R.id.buttonImport)).setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                try
                {
                    if (connectionsInSharedPrefs) {
                        ConnectionSettings.importSettingsFromJsonToSharedPrefs(_textSaveUrl.getText().toString(), getContext());
                    } else {
                        Utils.importSettingsFromXml(_textLoadUrl.getText().toString(), database.getWritableDatabase());
                    }
                    dismiss();
                    activity.recreate();
                }
                catch (IOException ioe)
                {
                    errorNotify("I/O error reading configuration", ioe);
                }
                catch (SAXException e)
                {
                    errorNotify("XML or format error reading configuration", e);
                }
            }
            
        });
    }
    
    private void errorNotify(String msg, Throwable t)
    {
        Log.i(TAG, msg, t);
        Utils.showErrorMessage(this.getContext(), msg + ":" + t.getMessage());
    }

}
