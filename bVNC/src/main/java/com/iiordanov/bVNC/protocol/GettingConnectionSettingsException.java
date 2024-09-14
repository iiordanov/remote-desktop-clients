package com.iiordanov.bVNC.protocol;

public class GettingConnectionSettingsException extends Exception {
    int errorStringId;

    public GettingConnectionSettingsException(int errorStringId) {
        this.errorStringId = errorStringId;
    }

    public int getErrorStringId() {
        return errorStringId;
    }
}
