/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */


package com.undatech.opaque;

import com.undatech.opaque.input.InputHandlerDirectSwipePan;

public class Constants {
    
    public static final int SDK_INT = android.os.Build.VERSION.SDK_INT;
    
    public static final int DIALOG_DISPLAY_VMS     = 0;
    public static final int DIALOG_X509_CERT       = 1;
    public static final int LAUNCH_VNC_VIEWER      = 2;
    public static final int VM_LAUNCHED            = 3;
    public static final int SPICE_CONNECT_SUCCESS  = 4;
    public static final int SPICE_CONNECT_FAILURE  = 5;
    public static final int OVIRT_AUTH_FAILURE     = 6;
    public static final int OVIRT_SSL_HANDSHAKE_FAILURE = 7;
    public static final int VM_LOOKUP_FAILED       = 8;
    public static final int NO_VM_FOUND_FOR_USER   = 9;
    public static final int GET_PASSWORD           = 10;
    public static final int VV_FILE_ERROR          = 11;
    public static final int OVIRT_TIMEOUT          = 12;
    public static final int VV_OVER_HTTP_FAILURE   = 13;
    public static final int VV_OVER_HTTPS_FAILURE  = 14;
    public static final int PVE_FAILED_TO_AUTHENTICATE = 15;
    public static final int PVE_FAILED_TO_CONNECT      = 16;
    public static final int PVE_FAILED_TO_PARSE_JSON   = 17;
    public static final int PVE_VMID_NOT_NUMERIC       = 18;
    public static final int PVE_API_UNEXPECTED_CODE    = 19;
    public static final int PVE_API_IO_ERROR           = 20;
    public static final int PVE_TIMEOUT_COMMUNICATING  = 21;
    public static final int PVE_NULL_DATA              = 22;
    public static final int VV_DOWNLOAD_TIMEOUT        = 23;
    public static final int GET_OTP_CODE               = 24;
    
    public static final int EXTRA_KEYS_OFF         = 0;
    public static final int EXTRA_KEYS_ON          = 1;
    public static final int EXTRA_KEYS_TIMEOUT     = 2;
    
    public static final int ADVANCED_SETTINGS      = 1;
    public static final int DEFAULT_SETTINGS       = 2;
    
    public static final String DEFAULT_INPUT_METHOD_ID = InputHandlerDirectSwipePan.ID;
    
    public static final int SHORT_VIBRATION        = 50;
    
    public static final String DEFAULT_LAYOUT_MAP = "English (US)";
    
    public static final String ACTION_USB_PERMISSION = "com.undatech.opaque.USB_PERMISSION";
    public static final int usbDeviceTimeout = 5000;
    public static final int usbDevicePermissionTimeout = 15000;
    
    public static final String DEFAULT_SETTINGS_FILE = "defaultSettings";
    public static final String EXPORT_SETTINGS_FILE = "OpaqueExportSettings.json";

    
    public static final int URL_BUFFER_SIZE        = 3000;
    public static final int VV_GET_FILE_TIMEOUT    = 17000;
    
    public static final String PVE_DEFAULT_REALM = "pam";
    public static final String PVE_DEFAULT_NODE = "pve";
    public static final String PVE_DEFAULT_VIRTUALIZATION = "qemu";
    
    public static final String GET_OTP_CODE_ID = "getOtpCode";
    public static final String GET_PASSWORD_ID = "getPassword";
}
