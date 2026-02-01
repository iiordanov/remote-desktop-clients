/**
 * Copyright (C) 2026 Iordan Iordanov
 * Copyright (C) 2022 D. R. Commander (TurboVNC)
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.bVNC.protocol.vnc.extendedclipboard;

import android.util.Log;

import com.tigervnc.rdr.InStream;
import com.tigervnc.rdr.OutStream;

import java.io.IOException;
import java.util.zip.DataFormatException;

/**
 * Implementation of Extended Clipboard protocol.
 * Coordinates between reader, writer, and data processor components.
 */
public class ExtendedClipboardProtocol implements ExtendedClipboardHandler {
    private static final String TAG = "ExtendedClipboardProto";

    private final ClipboardMessageReader messageReader;
    private final ClipboardMessageWriter messageWriter;
    private final ClipboardDataProcessor dataProcessor;
    private final ClipboardEventListener callback;

    private boolean enabled;
    private String pendingClipboardText;

    /**
     * Creates an Extended Clipboard protocol handler.
     *
     * @param outputStream The stream to write messages to
     * @param writeLock    Lock object for synchronizing writes to outputStream
     * @param callback     Callback for clipboard events
     */
    public ExtendedClipboardProtocol(OutStream outputStream, Object writeLock, ClipboardEventListener callback) {
        if (outputStream == null) {
            throw new IllegalArgumentException("OutStream cannot be null");
        }
        if (writeLock == null) {
            throw new IllegalArgumentException("writeLock cannot be null");
        }
        if (callback == null) {
            throw new IllegalArgumentException("ClipboardCallback cannot be null");
        }

        this.messageReader = new ClipboardMessageReader();
        this.messageWriter = new ClipboardMessageWriter(outputStream, writeLock);
        this.dataProcessor = new ClipboardDataProcessor();
        this.callback = callback;
        this.enabled = false;
        this.pendingClipboardText = null;
    }

    @Override
    public void readExtendedClipboardMessage(InStream is, int messageLength) throws IOException {
        ClipboardMessageReader.ClipboardMessage message;
        try {
            message = messageReader.readMessage(is, messageLength);
        } catch (IOException e) {
            callback.onClipboardError("Failed to read clipboard message", e);
            throw e;
        }

        // Always process CAPS messages to enable Extended Clipboard
        if (message.type() == ClipboardMessageReader.ClipboardMessage.Type.CAPS) {
            handleCapabilities(message);
            return;
        }

        if (!enabled) {
            Log.d(TAG, "Extended Clipboard not enabled, ignoring message");
            return;
        }

        switch (message.type()) {
            case NOTIFY:
                handleNotify(message);
                break;

            case REQUEST:
                handleRequest(message);
                break;

            case PROVIDE:
                handleProvide(message);
                break;

            case UNKNOWN:
                Log.w(TAG, "Unknown clipboard message type, flags: 0x" +
                        Integer.toHexString(message.flags()));
                break;
        }
    }

    @Override
    public void announceClipboardChange(String clipboardText) throws IOException {
        if (!enabled) {
            Log.d(TAG, "Extended Clipboard not negotiated with server, not announcing change");
            return;
        }

        if (clipboardText == null || clipboardText.isEmpty()) {
            Log.d(TAG, "Empty clipboard text, not announcing");
            return;
        }

        pendingClipboardText = clipboardText;
        messageWriter.writeNotifyMessage();
        Log.d(TAG, "Announced clipboard change to server");
    }

    @Override
    public void sendCapabilities() throws IOException {
        messageWriter.writeCapabilitiesMessage();
        Log.d(TAG, "Sent capabilities to server");
    }

    @Override
    public boolean isEnabled() {
        return enabled;
    }

    @Override
    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        Log.d(TAG, "Extended Clipboard " + (enabled ? "enabled" : "disabled"));
    }

    /**
     * Handles server capabilities message.
     * Responds with client capabilities and enables Extended Clipboard.
     */
    private void handleCapabilities(ClipboardMessageReader.ClipboardMessage message) {
        Log.d(TAG, "Server advertised Extended Clipboard capabilities: 0x" +
                Integer.toHexString(message.flags()));

        try {
            sendCapabilities();
            enabled = true;
            Log.d(TAG, "Extended Clipboard capabilities negotiated successfully");
        } catch (IOException e) {
            callback.onClipboardError("Failed to send capabilities", e);
        }
    }

    /**
     * Handles server notify message.
     * Server announces clipboard change, client should request data.
     */
    private void handleNotify(ClipboardMessageReader.ClipboardMessage message) {
        if (!message.hasUTF8Format()) {
            Log.d(TAG, "Server notified clipboard change, but no UTF-8 format. " +
                    "flags=0x" + Integer.toHexString(message.flags()) +
                    ", dataLen=" + (message.data() != null ? message.data().length : 0));
            return;
        }

        Log.d(TAG, "Server notified clipboard change, requesting data. " +
                "flags=0x" + Integer.toHexString(message.flags()));

        try {
            messageWriter.writeRequestMessage();
        } catch (IOException e) {
            callback.onClipboardError("Failed to send clipboard request", e);
        }
    }

    /**
     * Handles server request message.
     * Server requests clipboard data that client announced with NOTIFY.
     */
    private void handleRequest(ClipboardMessageReader.ClipboardMessage message) {
        if (!message.hasUTF8Format()) {
            Log.d(TAG, "Server requested clipboard, but not UTF-8 format");
            return;
        }

        if (pendingClipboardText == null) {
            Log.w(TAG, "Server requested clipboard, but no pending text");
            return;
        }

        Log.d(TAG, "Server requested clipboard, providing data");

        try {
            byte[] compressedData = dataProcessor.compressClipboardText(pendingClipboardText);
            messageWriter.writeProvideMessage(compressedData);
            pendingClipboardText = null;
        } catch (IOException e) {
            callback.onClipboardError("Failed to provide clipboard data", e);
        }
    }

    /**
     * Handles server provide message.
     * Server provides actual clipboard data in response to client REQUEST.
     */
    private void handleProvide(ClipboardMessageReader.ClipboardMessage message) {
        if (!message.hasUTF8Format()) {
            Log.d(TAG, "Server provided clipboard, but not UTF-8 format");
            return;
        }

        if (message.data() == null || message.data().length == 0) {
            Log.w(TAG, "Server provided empty clipboard data");
            return;
        }

        Log.d(TAG, "Server provided clipboard data, decompressing");

        try {
            String clipboardText = dataProcessor.decompressClipboardText(message.data());

            if (clipboardText != null && !clipboardText.isEmpty()) {
                callback.onClipboardReceived(clipboardText);
            } else {
                Log.w(TAG, "Decompressed clipboard text is empty");
            }
        } catch (DataFormatException e) {
            callback.onClipboardError("Failed to decompress clipboard data", e);
        }
    }
}
