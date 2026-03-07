package com.iiordanov.permissions;

public enum AudioPermissionGroups {
    RECORD_AUDIO("record_audio"),
    RECORD_AND_MODIFY_AUDIO("record_and_modify_audio");

    private final String text;

    AudioPermissionGroups(final String text) {
        this.text = text;
    }

    @Override
    public String toString() {
        return text;
    }
}
