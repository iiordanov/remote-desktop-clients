package com.undatech.opaque;

import android.content.Context;
import android.hardware.usb.UsbManager;
import android.os.Handler;
import android.widget.Toast;

import org.freedesktop.gstreamer.GStreamer;

public class NativeLib {

    static {
        System.loadLibrary("ndkdemo");
    }

    /**
     * Adds two integers, returning their sum
     */
    public native int add( int v1, int v2 );

    /**
     * Returns Hello World string
     */
    public native String hello();
}
