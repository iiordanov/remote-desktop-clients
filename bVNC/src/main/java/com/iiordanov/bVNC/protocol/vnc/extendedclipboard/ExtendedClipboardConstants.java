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

/**
 * Constants for the Extended Clipboard protocol.
 */
public final class ExtendedClipboardConstants {
    public static final int FORMAT_UTF8 = 1;
    public static final int ACTION_MASK = 0xff000000;
    public static final int ACTION_CAPS = 1 << 24;
    public static final int ACTION_REQUEST = 1 << 25;
    public static final int ACTION_NOTIFY = 1 << 27;
    public static final int ACTION_PROVIDE = 1 << 28;
    public static final int MAX_MESSAGE_SIZE = 65536;
    public static final int MAX_DECOMPRESSED_SIZE = 131072;
    private ExtendedClipboardConstants() {
    }
}
