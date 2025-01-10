/**
 * Copyright (C) 2013- Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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


package com.undatech.opaque;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Utils;
import com.undatech.remoteClientUi.R;

import java.util.Arrays;
import java.util.List;

public class ConnectionSetupActivity extends Activity {
    private static final String TAG = "ConnectionSetupActivity";

    private EditText hostname = null;
    private EditText vmname = null;
    private EditText user = null;
    private EditText password = null;
    private CheckBox keepPass = null;

    private Context appContext = null;
    private ConnectionSettings currentConnection = null;
    private String currentSelectedConnection = null;
    private String[] connectionsArray = null;
    private boolean newConnection = false;
    private Spinner spinnerConnectionType;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appContext = getApplicationContext();
        setContentView(R.layout.connection_setup_activity);

        hostname = findViewById(R.id.hostname);
        vmname = findViewById(R.id.vmname);
        user = findViewById(R.id.user);
        password = findViewById(R.id.password);
        keepPass = findViewById(R.id.checkboxKeepPassword);

        // Define what happens when one taps the Advanced Settings button.
        Button advancedSettingsButton = findViewById(R.id.advancedSettingsButton);
        advancedSettingsButton.setOnClickListener(arg0 -> {
            saveSelectedPreferences(false);

            Intent intent = new Intent(ConnectionSetupActivity.this, AdvancedSettingsActivity.class);
            intent.putExtra(Constants.opaqueConnectionSettingsClassPath, currentConnection);
            startActivityForResult(intent, RemoteClientLibConstants.ADVANCED_SETTINGS_REQUEST_CODE);
        });

        // Load any existing list of connection preferences.
        loadConnections();

        Intent i = getIntent();
        currentSelectedConnection = i.getStringExtra("com.undatech.opaque.connectionToEdit");
        Log.d(TAG, "currentSelectedConnection set to: " + currentSelectedConnection);

        // If no currentSelectedConnection was passed in, then generate one.
        if (currentSelectedConnection == null) {
            currentSelectedConnection = nextLargestNumber(connectionsArray);
            newConnection = true;
        }

        spinnerConnectionType = findViewById(R.id.spinnerConnectionType);
        spinnerConnectionType.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (view != null) {
                    Log.d(TAG, "Selected connection type: " +
                            position + " " + ((TextView) view).getText());
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        currentConnection = new ConnectionSettings(currentSelectedConnection);
        if (newConnection) {
            // Load advanced settings defaults from the saved default settings
            currentConnection.loadAdvancedSettings(this, RemoteClientLibConstants.DEFAULT_SETTINGS_FILE);
            // Save the empty connection preferences to override any values of a previously
            // deleted connection.
            saveSelectedPreferences(false);
        }

        // Finally, load the preferences for the currentSelectedConnection.
        loadSelectedPreferences();
    }

    /**
     * Returns the string representation of N+1 where N is the largest value
     * in the array "numbers" when converted to an integer.
     */
    private String nextLargestNumber(String[] numbers) {
        int maxValue = 0;
        if (numbers != null) {
            for (String num : numbers) {
                int currValue;
                try {
                    currValue = Integer.parseInt(num);
                    if (currValue >= maxValue) {
                        maxValue = currValue + 1;
                    }
                } catch (NumberFormatException e) {
                    Log.e(TAG, "NumberFormatException: " + Log.getStackTraceString(e));
                }
            }
        }
        Log.d(TAG, "nextLargestNumber determined: " + maxValue);
        return Integer.toString(maxValue);
    }

    /**
     * Loads the space-separated string representing the saved connections, splits them,
     * also setting the appropriate member variables.
     */
    private void loadConnections() {
        SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
        String connectionsList = sp.getString("connections", null);
        if (connectionsList != null && !connectionsList.trim().isEmpty()) {
            connectionsArray = connectionsList.split(" ");
        }
    }

    /**
     * Saves the space-separated string representing the saved connections,
     * and reloads the list to ensure the related member variables are consistent.
     */
    private void saveConnections() {
        // Only if this is a new connection do we need to add it to the list
        if (newConnection) {
            newConnection = false;

            StringBuilder newListOfConnections = new StringBuilder(currentSelectedConnection);
            if (connectionsArray != null) {
                for (String s : connectionsArray) {
                    newListOfConnections.append(" ").append(s);
                }
            }

            Log.d(TAG, "Saving list of connections: " + newListOfConnections);
            SharedPreferences sp = appContext.getSharedPreferences("generalSettings", Context.MODE_PRIVATE);
            Editor editor = sp.edit();
            editor.putString("connections", newListOfConnections.toString().trim());
            editor.apply();

            // Reload the list of connections from preferences for consistency.
            loadConnections();
        }
    }

    /**
     * This function is used to retrieve data returned by activities started with startActivityForResult.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(TAG, "onActivityResult");

        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == RemoteClientLibConstants.ADVANCED_SETTINGS_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK) {
                Bundle b = data.getExtras();
                if (b != null) {
                    currentConnection = (ConnectionSettings) b.get(Constants.opaqueConnectionSettingsClassPath);
                    saveSelectedPreferences(false);
                }
            } else {
                Log.i(TAG, "Error during AdvancedSettingsActivity.");
            }
        }
    }

    /**
     * Loads the preferences from shared preferences and populates the on-screen Views.
     */
    private void loadSelectedPreferences() {
        // We use the index as the file name to which to save the connection.
        Log.i(TAG, "Loading current settings from file: " + currentSelectedConnection);
        currentConnection.loadFromSharedPreferences(appContext);
    }

    private void updateViewsFromPreferences() {
        List<String> connectionTypes = Arrays.asList(getResources().getStringArray(R.array.connection_types));
        spinnerConnectionType.setSelection(connectionTypes.indexOf(currentConnection.getConnectionTypeString()));
        hostname.setText(currentConnection.getHostname());
        vmname.setText(currentConnection.getVmname());
        user.setText(currentConnection.getUser());
        password.setText(currentConnection.getPassword());
        keepPass.setChecked(currentConnection.getKeepPassword());
    }

    /**
     * Saves the preferences which are selected on-screen by the user into shared preferences.
     */
    private void saveSelectedPreferences(boolean saveInList) {
        Log.i(TAG, "Saving current settings to file: " + currentSelectedConnection);

        String u = user.getText().toString();
        String h = hostname.getText().toString();

        // Only if a username and a hostname were entered, save the connection to list of connections.
        if (saveInList && !(u.isEmpty() || h.isEmpty())) {
            saveConnections();
        }

        // Then, save the connection to a separate SharedPreferences file.
        currentConnection.setConnectionTypeString(spinnerConnectionType.getSelectedItem().toString());
        currentConnection.setUser(u);
        currentConnection.setHostname(h);
        currentConnection.setVmname(vmname.getText().toString());
        currentConnection.setPassword(password.getText().toString());
        currentConnection.setKeepPassword(keepPass.isChecked());
        currentConnection.saveToSharedPreferences(appContext);
    }

    @Override
    public void onStop() {
        super.onStop();
        Log.d(TAG, "onStop");
        //saveSelectedPreferences();
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.d(TAG, "onResume");
        loadSelectedPreferences();
        updateViewsFromPreferences();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.connection_setup_activity_actions, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem menuItem) {
        return true;
    }

    public void showConnectionScreenHelp(View view) {
        showConnectionScreenHelp();
    }

    public void showConnectionScreenHelp(MenuItem item) {
        showConnectionScreenHelp();
    }

    public void showConnectionScreenHelp() {
        Log.d(TAG, "Showing connection screen help.");
        Utils.createConnectionScreenDialog(this);
    }


    public void save(View view) {
        save();
    }

    public void save(MenuItem item) {
        save();
    }

    public void save() {
        String u = user.getText().toString();
        String h = hostname.getText().toString();

        // Only if a username and a hostname were entered, save the connection.
        if (!(u.isEmpty() || h.isEmpty())) {
            saveSelectedPreferences(true);
            finish();
            // Otherwise, let the user know that at least a user and hostname are required.
        } else {
            Toast toast = Toast.makeText(appContext, R.string.error_no_user_hostname, Toast.LENGTH_LONG);
            toast.show();
        }
    }
}
