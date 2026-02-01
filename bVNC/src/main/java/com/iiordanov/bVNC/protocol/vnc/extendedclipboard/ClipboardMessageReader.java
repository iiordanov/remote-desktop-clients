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

import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_CAPS;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_MASK;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_NOTIFY;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_PROVIDE;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_REQUEST;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.FORMAT_UTF8;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.MAX_MESSAGE_SIZE;

import android.util.Log;

import com.tigervnc.rdr.InStream;

import java.io.IOException;

/**
 * Reads and parses Extended Clipboard protocol messages.
 */
public class ClipboardMessageReader {
    private static final String TAG = "ClipboardMessageReader";

    /**
     * Reads and parses an Extended Clipboard message from the input stream.
     *
     * @param is            The input stream to read from
     * @param messageLength The length of the clipboard message (should be negative)
     * @return Parsed clipboard message
     * @throws IOException if I/O errors occur or message is malformed
     */
    public ClipboardMessage readMessage(InStream is, int messageLength) throws IOException {
        // messageLength is negative for Extended Clipboard, convert to positive
        int len = -messageLength;

        if (len < 4) {
            throw new IOException("Malformed Extended Clipboard message: length too short");
        }

        // Check size limit (bug fix from TurboVNC commit 28e27192)
        if (len > MAX_MESSAGE_SIZE) {
            Log.e(TAG, "Ignoring " + len + "-byte Extended Clipboard message (limit = " +
                    MAX_MESSAGE_SIZE + " bytes)");
            skipBytes(is, len);
            return new ClipboardMessage(ClipboardMessage.Type.UNKNOWN, 0, null);
        }

        // Read flags
        int flags;
        try {
            flags = is.readInt();
        } catch (Exception e) {
            throw new IOException("Failed to read clipboard flags", e);
        }

        int action = flags & ACTION_MASK;
        int remainingLen = len - 4; // Subtract flags size

        Log.d(TAG, "Extended Clipboard: len=" + len + ", flags=0x" + Integer.toHexString(flags) +
                ", action=0x" + Integer.toHexString(action) + ", remainingLen=" + remainingLen);

        // Determine message type and read data if needed
        if ((action & ACTION_CAPS) != 0) {
            byte[] data = readRemainingData(is, remainingLen);
            return new ClipboardMessage(ClipboardMessage.Type.CAPS, flags, data);
        } else if (action == ACTION_PROVIDE) {
            byte[] data = readRemainingData(is, remainingLen);
            return new ClipboardMessage(ClipboardMessage.Type.PROVIDE, flags, data);
        } else if (action == ACTION_NOTIFY) {
            byte[] data = readRemainingData(is, remainingLen);
            return new ClipboardMessage(ClipboardMessage.Type.NOTIFY, flags, data);
        } else if (action == ACTION_REQUEST) {
            byte[] data = readRemainingData(is, remainingLen);
            return new ClipboardMessage(ClipboardMessage.Type.REQUEST, flags, data);
        } else {
            // Unknown action, skip remaining data
            skipBytes(is, remainingLen);
            return new ClipboardMessage(ClipboardMessage.Type.UNKNOWN, flags, null);
        }
    }

    /**
     * Reads remaining message data from the stream.
     *
     * @param is     The input stream
     * @param length Number of bytes to read
     * @return The data read, or null if length is 0
     * @throws IOException if I/O error occurs
     */
    private byte[] readRemainingData(InStream is, int length) throws IOException {
        if (length == 0) {
            return null;
        }

        byte[] data = new byte[length];
        try {
            is.readBytes(data, 0, length);
        } catch (Exception e) {
            throw new IOException("Failed to read clipboard data", e);
        }
        return data;
    }

    /**
     * Skips the specified number of bytes in the input stream.
     *
     * @param is       The input stream
     * @param numBytes Number of bytes to skip
     * @throws IOException if I/O error occurs
     */
    private void skipBytes(InStream is, int numBytes) throws IOException {
        if (numBytes <= 0) {
            return;
        }

        byte[] skip = new byte[numBytes];
        try {
            is.readBytes(skip, 0, numBytes);
        } catch (Exception e) {
            throw new IOException("Failed to skip bytes", e);
        }
    }

    /**
     * Result of parsing a clipboard message.
     */
    public record ClipboardMessage(Type type, int flags, byte[] data) {
        public boolean hasUTF8Format() {
            return (flags & FORMAT_UTF8) != 0;
        }

        public enum Type {
            CAPS,
            REQUEST,
            NOTIFY,
            PROVIDE,
            UNKNOWN
        }
    }
}
