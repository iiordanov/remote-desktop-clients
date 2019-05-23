package com.undatech.opaque;

import android.graphics.Bitmap;

public interface Viewable {
    void waitUntilInflated();
    int getDesiredWidth();
    int getDesiredHeight();
    void reallocateDrawable(int width, int height);
    Bitmap getBitmap();
    void reDraw(int x, int y, int width, int height);
    void setMousePointerPosition(int x, int y);
    void mouseMode(boolean relative);
}
