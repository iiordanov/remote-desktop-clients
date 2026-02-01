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

import static com.iiordanov.bVNC.protocol.vnc.extendedclipboard.ExtendedClipboardConstants.MAX_DECOMPRESSED_SIZE;

import android.util.Log;

import com.iiordanov.bVNC.Utils;
import com.tigervnc.rdr.MemOutStream;
import com.tigervnc.rdr.ZlibOutStream;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

/**
 * Handles compression and decompression of clipboard data.
 */
public class ClipboardDataProcessor {
    private static final String TAG = "ClipboardDataProcessor";

    /**
     * Compresses clipboard text using zlib compression with UTF-8 encoding.
     * Applies CRLF line ending conversion before compression.
     *
     * @param text The text to compress
     * @return Compressed data ready for transmission
     * @throws IOException if compression fails
     */
    public byte[] compressClipboardText(String text) throws IOException {
        if (text == null) {
            throw new IllegalArgumentException("Clipboard text cannot be null");
        }

        String textWithCRLF = Utils.convertCRLF(text);
        byte[] textBytes = encodeToUTF8(textWithCRLF);

        return compressWithZlib(textBytes);
    }

    /**
     * Decompresses and parses clipboard data received from server.
     * Handles Z_STREAM_END for QEMU server compatibility.
     *
     * @param compressedData The compressed data to decompress
     * @return Decompressed clipboard text with LF line endings, or null if parsing failed
     * @throws DataFormatException if decompression fails
     */
    public String decompressClipboardText(byte[] compressedData) throws DataFormatException {
        if (compressedData == null || compressedData.length == 0) {
            Log.w(TAG, "Empty compressed data");
            return null;
        }

        byte[] decompressed = decompressWithZlib(compressedData);
        return parseDecompressedData(decompressed);
    }

    /**
     * Compresses data using zlib with proper framing for Extended Clipboard protocol.
     * Format: 4-byte length + UTF-8 text + null terminator
     */
    private byte[] compressWithZlib(byte[] textBytes) throws IOException {
        MemOutStream mos = new MemOutStream();
        ZlibOutStream zos = new ZlibOutStream();

        try {
            zos.setUnderlying(mos);

            zos.writeU32(textBytes.length + 1);
            zos.writeBytes(textBytes, 0, textBytes.length);
            zos.writeU8(0); // null terminator

            zos.flush();

            byte[] result = new byte[mos.length()];
            System.arraycopy(mos.data(), 0, result, 0, mos.length());
            return result;
        } finally {
            zos.close();
        }
    }

    /**
     * Decompresses zlib-compressed data.
     * Handles Z_STREAM_END for servers that close stream after each transfer (QEMU).
     */
    private byte[] decompressWithZlib(byte[] compressedData) throws DataFormatException {
        Inflater inflater = new Inflater();
        try {
            inflater.setInput(compressedData);

            byte[] decompressed = new byte[MAX_DECOMPRESSED_SIZE];
            int decompressedLen = inflater.inflate(decompressed);

            // Z_STREAM_END handling
            if (decompressedLen == 0 && !inflater.finished()) {
                throw new DataFormatException("Decompression produced no data");
            }

            byte[] result = new byte[decompressedLen];
            System.arraycopy(decompressed, 0, result, 0, decompressedLen);
            return result;
        } finally {
            inflater.end();
        }
    }

    /**
     * Parses decompressed clipboard data.
     * Format: 4-byte length (big-endian) + UTF-8 text + null terminator
     */
    private String parseDecompressedData(byte[] data) {
        if (data.length < 4) {
            Log.w(TAG, "Decompressed data too short");
            return null;
        }

        int textLen = readBigEndianInt32(data, 0);

        // Validate text length
        if (textLen <= 0 || textLen > data.length - 4) {
            Log.w(TAG, "Invalid clipboard text length: " + textLen);
            return null;
        }

        // textLen includes null terminator, so subtract 1 for actual text
        int actualTextLen = Math.min(textLen - 1, data.length - 4);
        String text = decodeFromUTF8(data, 4, actualTextLen);

        return Utils.convertLF(text);
    }

    /**
     * Encodes string to UTF-8 bytes.
     */
    private byte[] encodeToUTF8(String text) {
        return text.getBytes(StandardCharsets.UTF_8);
    }

    /**
     * Decodes UTF-8 bytes to string.
     */
    private String decodeFromUTF8(byte[] data, int offset, int length) {
        return new String(data, offset, length, StandardCharsets.UTF_8);
    }

    /**
     * Reads a 32-bit big-endian integer from byte array.
     */
    private int readBigEndianInt32(byte[] data, int offset) {
        return ((data[offset] & 0xff) << 24) |
                ((data[offset + 1] & 0xff) << 16) |
                ((data[offset + 2] & 0xff) << 8) |
                (data[offset + 3] & 0xff);
    }
}
