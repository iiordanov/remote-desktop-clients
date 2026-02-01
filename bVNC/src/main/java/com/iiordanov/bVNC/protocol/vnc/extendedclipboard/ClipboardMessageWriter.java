/**
 * Copyright (C) 2025 Iordan Iordanov
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
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_NOTIFY;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_PROVIDE;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.ACTION_REQUEST;
import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.FORMAT_UTF8;

import com.tigervnc.rdr.OutStream;

import java.io.IOException;

/**
 * Builds and writes Extended Clipboard protocol messages.
 */
@SuppressWarnings("ClassCanBeRecord")
public class ClipboardMessageWriter {
    private static final int CLIENT_CUT_TEXT = 6;
    private static final int HEADER_SIZE = 8;
    private static final int FLAGS_SIZE = 4;

    private final OutStream outputStream;
    private final Object writeLock;

    /**
     * Creates a message writer for the given output stream.
     *
     * @param outputStream The stream to write messages to
     * @param writeLock    The lock object to synchronize writes (should be same as RfbProto uses)
     */
    public ClipboardMessageWriter(OutStream outputStream, Object writeLock) {
        if (outputStream == null) {
            throw new IllegalArgumentException("OutStream cannot be null");
        }
        if (writeLock == null) {
            throw new IllegalArgumentException("writeLock cannot be null");
        }
        this.outputStream = outputStream;
        this.writeLock = writeLock;
    }

    /**
     * Writes capabilities message to server.
     * Advertises support for UTF-8 format and all clipboard actions.
     *
     * @throws IOException if I/O error occurs
     */
    public void writeCapabilitiesMessage() throws IOException {
        int flags = FORMAT_UTF8 | ACTION_CAPS |
                ACTION_REQUEST |
                ACTION_NOTIFY |
                ACTION_PROVIDE;

        // Length: 4 bytes for flags + 4 bytes per format (1 format = UTF8)
        int messageLen = -(FLAGS_SIZE + FLAGS_SIZE);

        byte[] header = buildMessageHeader(messageLen);
        byte[] flagsBytes = encodeBigEndianInt32(flags);
        byte[] sizeBytes = encodeBigEndianInt32(0); // 0 = no size limit

        writeMessage(header, flagsBytes, sizeBytes);
    }

    /**
     * Writes notify message to server.
     * Announces that clipboard has changed, server may respond with REQUEST.
     *
     * @throws IOException if I/O error occurs
     */
    public void writeNotifyMessage() throws IOException {
        buildAndWriteMessage(-FLAGS_SIZE, ACTION_NOTIFY, null);
    }

    /**
     * Writes request message to server.
     * Requests clipboard data after receiving NOTIFY.
     *
     * @throws IOException if I/O error occurs
     */
    public void writeRequestMessage() throws IOException {
        buildAndWriteMessage(-FLAGS_SIZE, ACTION_REQUEST, null);
    }

    /**
     * Writes provide message with compressed clipboard data.
     * Sends actual clipboard content in response to REQUEST.
     *
     * @param compressedData The compressed clipboard data
     * @throws IOException if I/O error occurs
     */
    public void writeProvideMessage(byte[] compressedData) throws IOException {
        if (compressedData == null || compressedData.length == 0) {
            throw new IllegalArgumentException("Compressed data cannot be null or empty");
        }

        int totalPayloadLen = FLAGS_SIZE + compressedData.length;
        int negativeLen = -totalPayloadLen;

        buildAndWriteMessage(negativeLen, ACTION_PROVIDE, compressedData);
    }

    private void buildAndWriteMessage(int negativeLen, int action, byte[] compressedData) throws IOException {
        byte[] header = buildMessageHeader(negativeLen);
        int flags = action | FORMAT_UTF8;
        byte[] flagsBytes = encodeBigEndianInt32(flags);
        writeMessage(header, flagsBytes, compressedData);
    }

    private void writeMessage(byte[] header, byte[] flagsBytes, byte[] compressedData) throws IOException {
        synchronized (writeLock) {
            outputStream.write(header);
            outputStream.write(flagsBytes);
            if (compressedData != null) {
                outputStream.write(compressedData);
            }
        }
    }

    /**
     * Builds the 8-byte Extended Clipboard message header.
     * Format: ClientCutText message type + 3 padding bytes + length (negative for Extended)
     *
     * @param length The message length (negative indicates Extended Clipboard)
     * @return 8-byte header array
     */
    private byte[] buildMessageHeader(int length) {
        byte[] header = new byte[HEADER_SIZE];
        header[0] = (byte) CLIENT_CUT_TEXT;
        header[1] = header[2] = header[3] = 0; // Padding

        // Encode length as big-endian 32-bit integer
        byte[] lengthBytes = encodeBigEndianInt32(length);
        System.arraycopy(lengthBytes, 0, header, 4, 4);

        return header;
    }

    /**
     * Encodes a 32-bit integer as a big-endian byte array.
     *
     * @param value The integer to encode
     * @return 4-byte array containing the big-endian representation
     */
    private byte[] encodeBigEndianInt32(int value) {
        byte[] bytes = new byte[4];
        bytes[0] = (byte) ((value >> 24) & 0xff);
        bytes[1] = (byte) ((value >> 16) & 0xff);
        bytes[2] = (byte) ((value >> 8) & 0xff);
        bytes[3] = (byte) (value & 0xff);
        return bytes;
    }
}
