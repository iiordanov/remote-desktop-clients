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

import com.tigervnc.rdr.InStream;

import java.io.IOException;

/**
 * Handles Extended Clipboard protocol operations.
 */
public interface ExtendedClipboardHandler {

    /**
     * Reads and processes an Extended Clipboard message from the input stream.
     *
     * @param is            The input stream to read from
     * @param messageLength The length of the clipboard message
     * @throws IOException if I/O errors occur
     */
    void readExtendedClipboardMessage(InStream is, int messageLength) throws IOException;

    /**
     * Announces clipboard change to remote server (sends NOTIFY).
     * Server may respond with REQUEST to get the actual data.
     *
     * @param clipboardText The new clipboard text
     * @throws IOException if I/O errors occur
     */
    void announceClipboardChange(String clipboardText) throws IOException;

    /**
     * Sends clipboard capabilities to the server.
     * Called during initialization or when server requests capabilities.
     *
     * @throws IOException if I/O errors occur
     */
    void sendCapabilities() throws IOException;

    /**
     * Checks if Extended Clipboard protocol is enabled.
     *
     * @return true if Extended Clipboard is enabled
     */
    boolean isEnabled();

    /**
     * Enables or disables Extended Clipboard protocol
     * depending on server capabilities.
     *
     * @param enabled true to enable, false to disable
     */
    void setEnabled(boolean enabled);

    /**
     * Callback interface for clipboard events.
     */
    interface ClipboardEventListener {
        /**
         * Called when clipboard text is received from remote server.
         *
         * @param text The clipboard text received
         */
        void onClipboardReceived(String text);

        /**
         * Called when an error occurs during clipboard operations.
         *
         * @param message   Error description
         * @param exception The exception that occurred (may be null)
         */
        void onClipboardError(String message, Exception exception);
    }
}
