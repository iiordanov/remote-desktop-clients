/**
 * Copyright (C) 2017- Iordan Iordanov
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


package com.undatech.opaque.input;

import java.lang.reflect.Field;

import android.content.Context;
import android.os.Handler;
import android.view.ScaleGestureDetector;

public class MyScaleGestureDetector extends ScaleGestureDetector {
    private static final String TAG = "MyScaleGestureDetector";

    public MyScaleGestureDetector(Context context, OnScaleGestureListener listener) {
        super(context, listener);
        adjustDefaults();
    }

    public MyScaleGestureDetector(Context context, OnScaleGestureListener listener, Handler handler) {
        super(context, listener, handler);
        adjustDefaults();
    }

    public void adjustDefaults() {
        try {
            Field field = ScaleGestureDetector.class.getDeclaredField("mMinSpan");
            field.setAccessible(true);
            field.set(this, new Integer(0));
            field.setAccessible(false);
            
            field = ScaleGestureDetector.class.getDeclaredField("mSpanSlop");
            field.setAccessible(true);
            field.set(this, new Integer(0));
            field.setAccessible(false);
        } catch (NoSuchFieldException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        }
    }
}