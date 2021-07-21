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


package com.undatech.opaque.dialogs;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

import com.undatech.opaque.ConnectionSettings;
import com.undatech.opaque.util.HttpsFileDownloader;
import com.undatech.remoteClientUi.R;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import androidx.fragment.app.DialogFragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

public class ManageCustomCaFragment extends DialogFragment
                                    implements HttpsFileDownloader.OnDownloadFinishedListener {
    public static String TAG = "ManageCustomCaFragment";
    public static int TYPE_OVIRT = 0;
    public static int TYPE_SPICE = 1;

    @Override
    public void onDownload(String contents) {
        Log.d(TAG, "onDownload");
        caTextContents = contents;
        handler.post(setCaText);
    }

    public interface OnFragmentDismissedListener {
        void onFragmentDismissed(ConnectionSettings currentConnection);
    }
    
    private OnFragmentDismissedListener dismissalListener;
    private int caPurpose;
    private ConnectionSettings currentConnection;
    private Handler handler;
    private String caTextContents = "";
    private Runnable setCaText = new Runnable() {
        @Override
        public void run() {
            caCert.setText(caTextContents);
        }
    };

    private EditText caCertPath;
    private EditText caCert;
    private Button importButton;
    private Button downloadButton;
    private Button helpButton;

    public ManageCustomCaFragment () {}
    
    public void setOnFragmentDismissedListener (OnFragmentDismissedListener dismissalListener) {
        this.dismissalListener = dismissalListener;
    }
    
    public static ManageCustomCaFragment newInstance(int caPurpose, ConnectionSettings currentConnection) {
        ManageCustomCaFragment f = new ManageCustomCaFragment();

        // Supply the CA purpose as an argument.
        Bundle args = new Bundle();
        args.putInt("caPurpose", caPurpose);
        args.putSerializable("currentConnection", currentConnection);
        f.setArguments(args);

        return f;
    }
    
    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        try {
            dismissalListener = (OnFragmentDismissedListener) activity;
            android.util.Log.e(TAG, "onAttach: assigning OnFragmentDismissedListener");
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString() + " must implement OnFragmentDismissedListener");
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        handler = new Handler();
        caPurpose = getArguments().getInt("caPurpose");
        currentConnection = (ConnectionSettings)getArguments().getSerializable("currentConnection");
    }
    
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = inflater.inflate(R.layout.manage_custom_ca, container, false);
        
        // Set title for this dialog
        getDialog().setTitle(R.string.manage_custom_ca_title);

        caCert      = (EditText) v.findViewById(R.id.caCert);
        caCertPath  = (EditText) v.findViewById(R.id.caCertPath);
        
        // Set up the import button.
        importButton = (Button) v.findViewById(R.id.importButton);
        importButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                importCaCertFromFile();
            }
        });

        // Set up the import from server button.
        downloadButton = (Button) v.findViewById(R.id.downloadButton);
        downloadButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                downloadFromServer();
            }
        });

        // Set up the help button.
        helpButton = (Button) v.findViewById(R.id.helpButton);
        helpButton.setOnClickListener(new View.OnClickListener() {
            
            @Override
            public void onClick(View v) {
                //TODO: Show help
            }
        });
        
        // Set the widgets' state appropriately.
        setWidgetStateAppropriately ();
        return v;
    }

    private void downloadFromServer() {
        Log.d(TAG, "downloadFromServer");
        String address = currentConnection.getAddress();
        if (!currentConnection.getAddress().startsWith("http")) {
            address = "https://" + address;
        }
        address += "/ovirt-engine/services/pki-resource?resource=ca-certificate&format=X509-PEM-CA";
        new HttpsFileDownloader(address, false,
                ManageCustomCaFragment.this).initiateDownload();
    }

    private void importCaCertFromFile () {
        Log.d(TAG, "importCaCertFromFile");
        Context context = getActivity();
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
                    Toast.makeText(context, R.string.ca_file_error_reading, Toast.LENGTH_LONG).show();
                }
            } while (line != null);
            caCert.setText(buf.toString());
        } catch (FileNotFoundException e) {
            Toast.makeText(context, R.string.ca_file_not_found, Toast.LENGTH_LONG).show();
        }
    }
    
    private void setWidgetStateAppropriately () {
        if (caPurpose == ManageCustomCaFragment.TYPE_OVIRT) {
            caCert.setText(currentConnection.getOvirtCaData());
            caCertPath.setText(getExternalSDCardDirectory());
        }
    }
    
    @Override
    public void onDismiss (DialogInterface dialog) {
        android.util.Log.e(TAG, "dismiss: sending back data to Activity");
        // Depending on the value of caPurpose, assign the certs in currentConnection.
        if (caPurpose == ManageCustomCaFragment.TYPE_OVIRT) {
            android.util.Log.e(TAG, "Setting custom oVirt CA");
            currentConnection.setOvirtCaData(caCert.getText().toString());
        }

        dismissalListener.onFragmentDismissed(currentConnection);
    }
    
    public String getExternalSDCardDirectory() {
        File dir = Environment.getExternalStorageDirectory();
        return dir.getAbsolutePath() + "/";
    }

}
