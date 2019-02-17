/**
 * Copyright (C) 2012 Iordan Iordanov
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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

import com.iiordanov.bVNC.ConnectionBean;
import com.iiordanov.bVNC.Database;
import com.iiordanov.bVNC.aSPICE;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import android.widget.LinearLayout.LayoutParams;
import com.iiordanov.bVNC.*;
import com.iiordanov.freebVNC.*;
import com.iiordanov.aRDP.*;
import com.iiordanov.freeaRDP.*;
import com.iiordanov.aSPICE.*;
import com.iiordanov.freeaSPICE.*;

/**
 * @author Iordan K Iordanov
 *
 */
public class ImportTlsCaDialog extends AlertDialog {
    private aSPICE mainConfigPage;
    private ConnectionBean selected;
    private EditText certSubject;
    private EditText caCertPath;
    private EditText caCert;
    private Button importButton;
    private Button helpButton;
    private Database database;

    /**
     * @param context
     */
    public ImportTlsCaDialog(Context context, Database database) {
        super(context);
        setOwnerActivity((Activity)context);
        mainConfigPage = (aSPICE)context;
        selected = mainConfigPage.getCurrentConnection();
        this.database = database;
    }

    private static final Intent docIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://spice-space.org/page/SSLConnection")); 
    
    public static void showDocumentation(Context c) {
        c.startActivity(docIntent);
    }
    
    /* 
     * (non-Javadoc)
     * @see android.app.Dialog#onBackPressed()
     */
    @Override
    public void onBackPressed () {
        selected.setCaCert(caCert.getText().toString());
        selected.setCertSubject(certSubject.getText().toString());
        mainConfigPage.updateViewFromSelected();
        selected.saveAndWriteRecent(false, database);
        dismiss();
    }

    /* This function needs to be overloaded because the dialog does not get
     * destroyed and recreated every time the button on the main screen is
     * pressed, and so the widgets' state wasn't set correctly. This makes 
     * sure the widgets' state is set when the dialog gets displayed.
     * (non-Javadoc)
     * @see android.app.Dialog#onAttachedToWindow()
     */
    @Override
    public void onAttachedToWindow() {
        setWidgetStateAppropriately ();
    }

    private void setWidgetStateAppropriately () {
        selected = mainConfigPage.getCurrentConnection();
        certSubject.setText(selected.getCertSubject());
        caCert.setText(selected.getCaCert());
        caCertPath.setText("/sdcard/");
    }
    
    private void importCaCert () {
        File file = new File (caCertPath.getText().toString());
        FileReader freader;
        try {
            freader = new FileReader(file);
            BufferedReader reader = new BufferedReader(freader);
            StringBuffer buf = new StringBuffer();
            String line = null;
            do {
                try {
                    line = reader.readLine();
                    if (line != null)
                        buf.append(line + '\n');
                } catch (IOException e) {
                    Toast.makeText(getContext(), R.string.spice_ca_file_error_reading, Toast.LENGTH_LONG).show();
                }
            } while (line != null);
            caCert.setText(buf.toString());
        } catch (FileNotFoundException e) {
            Toast.makeText(getContext(), R.string.spice_ca_file_not_found, Toast.LENGTH_LONG).show();
        }
    }
    
    /* (non-Javadoc)
     * @see android.app.Dialog#onCreate(android.os.Bundle)
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.import_tls_ca_dialog);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|
                               WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
        WindowManager.LayoutParams lp = getWindow().getAttributes();
        lp.dimAmount = 1.0f;
        lp.width     = LayoutParams.FILL_PARENT;
        lp.height    = LayoutParams.WRAP_CONTENT;
        getWindow().setAttributes(lp);

        certSubject = (EditText) findViewById(R.id.certSubject);
        caCert      = (EditText) findViewById(R.id.caCert);
        caCertPath  = (EditText) findViewById(R.id.caCertPath);
        
        // Set up the import button.
        importButton = (Button) findViewById(R.id.importButton);
        importButton.setOnClickListener(new View.OnClickListener() {
            
            @Override
            public void onClick(View v) {
                importCaCert();
            }
        });
        
        // Set up the help button.
        helpButton = (Button) findViewById(R.id.helpButton);
        helpButton.setOnClickListener(new View.OnClickListener() {
            
            @Override
            public void onClick(View v) {
                showDocumentation(ImportTlsCaDialog.this.mainConfigPage);
            }
        });
        
        // Set the widgets' state appropriately.
        setWidgetStateAppropriately ();
    }
}
