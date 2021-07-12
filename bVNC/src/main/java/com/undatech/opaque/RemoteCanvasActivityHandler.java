package com.undatech.opaque;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

import com.iiordanov.bVNC.Utils;
import com.undatech.opaque.Connection;

public class RemoteCanvasActivityHandler extends Handler {
    private static String TAG = "RemoteCanvasActivityHandler";
    private Connection connection;
    private Context context;

    public RemoteCanvasActivityHandler(Context context) {
        super();
        context = context;
    }

    public Connection getConnection() {
        return connection;
    }

    public void setConnection(Connection connection) {
        this.connection = connection;
    }

    @Override
    public void handleMessage(final Message msg) {
        Bundle s;
        android.util.Log.d(TAG, "Handling message, msg.what: " + msg.what);
        if (msg.what == RemoteClientLibConstants.REPORT_TOOLBAR_POSITION) {
            android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION");
            if (connection.getUseLastPositionToolbar()) {
                int useLastPositionToolbarX = Utils.getIntFromMessage(msg, "useLastPositionToolbarX");
                int useLastPositionToolbarY = Utils.getIntFromMessage(msg, "useLastPositionToolbarY");
                android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, X Coordinate" + useLastPositionToolbarX);
                android.util.Log.d(TAG, "Handling message, REPORT_TOOLBAR_POSITION, Y Coordinate" + useLastPositionToolbarY);
                connection.setUseLastPositionToolbarX(useLastPositionToolbarX);
                connection.setUseLastPositionToolbarY(useLastPositionToolbarY);
            }
        } else {
            super.handleMessage(msg);
        }
    }
}
