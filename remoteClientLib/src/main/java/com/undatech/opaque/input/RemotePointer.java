package com.undatech.opaque.input;

import static com.undatech.opaque.util.InputUtils.isNoQwertyKbd;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;

import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.RemoteClientLibConstants;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.util.GeneralUtils;

public abstract class RemotePointer {
    private static final String TAG = "RemotePointer";

    public static final int POINTER_DOWN_MASK = 0x8000; // 32768
    public static float DEFAULT_SENSITIVITY = 2.0f;
    public static boolean DEFAULT_ACCELERATED = true;
    /**
     * Current state of "mouse" buttons
     */
    protected int pointerMask = 0;
    protected int prevPointerMask = 0;
    protected InputCarriable remoteInput;
    protected Viewable canvas;
    protected Context context;
    protected Handler handler;
    protected RfbConnectable protocomm;
    /**
     * Indicates where the mouse pointer is located.
     */
    protected int pointerX, pointerY;
    protected boolean relativeEvents = false;
    protected float sensitivity = DEFAULT_SENSITIVITY;
    protected boolean accelerated = DEFAULT_ACCELERATED;
    protected boolean debugLogging;
    private final boolean useDpadAsPointer;
    MouseScroller scroller;
    MouseMover mover;

    public RemotePointer(
            RfbConnectable protocomm,
            Context context,
            InputCarriable remoteInput,
            Viewable canvas,
            Handler handler,
            boolean useDpadAsPointer,
            boolean debugLogging
    ) {
        this.protocomm = protocomm;
        this.context = context;
        this.remoteInput = remoteInput;
        this.canvas = canvas;
        this.handler = handler;
        //pointerX  = canvas.getImageWidth()/2;
        //pointerY  = canvas.getImageHeight()/2;
        scroller = new MouseScroller();
        mover = new MouseMover();
        this.debugLogging = debugLogging;
        this.useDpadAsPointer = useDpadAsPointer;
    }

    protected boolean shouldBeRightClick(KeyEvent e) {
        boolean result = false;
        int keyCode = e.getKeyCode();

        // If the camera button is pressed
        if (keyCode == KeyEvent.KEYCODE_CAMERA) {
            result = true;
            // Or the back button is pressed
        } else if (keyCode == KeyEvent.KEYCODE_BACK) {
            // Determine SDK
            boolean preGingerBread = android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD;
            // Whether the source is a mouse (getSource() is not available pre-Gingerbread)
            boolean mouseSource = (!preGingerBread && e.getSource() == InputDevice.SOURCE_MOUSE);
            // Whether the device has a qwerty keyboard
            boolean noQwertyKbd = isNoQwertyKbd(context);
            // Whether the device is pre-Gingerbread or the event came from the "hard buttons"
            boolean fromVirtualHardKey = preGingerBread || (e.getFlags() & KeyEvent.FLAG_VIRTUAL_HARD_KEY) != 0;
            if (mouseSource || noQwertyKbd || fromVirtualHardKey) {
                result = true;
            }
        }

        return result;
    }

    public int getX() {
        return pointerX;
    }

    public void setX(int newX) {
        pointerX = newX;
    }

    public int getY() {
        return pointerY;
    }

    public void setY(int newY) {
        pointerY = newY;
    }

    /**
     * Move mouse pointer to specified coordinates.
     */
    public void movePointer(int x, int y) {
        //android.util.Log.d("RemotePointer", "movePointer");
        canvas.invalidateMousePosition();
        pointerX = x;
        pointerY = y;
        canvas.invalidateMousePosition();
        moveMouseButtonUp(x, y, 0);
    }

    /**
     * If necessary move the pointer to be visible.
     */
    public void movePointerToMakeVisible() {
        //android.util.Log.d("RemotePointer", "movePointerToMakeVisible");
        if (canvas.getMouseFollowPan()) {
            int absX = canvas.getAbsX();
            int absY = canvas.getAbsY();
            int vW = canvas.getVisibleDesktopWidth();
            int vH = canvas.getVisibleDesktopHeight();
            if (pointerX < absX || pointerX >= absX + vW ||
                    pointerY < absY || pointerY >= absY + vH) {
                movePointer(absX + vW / 2, absY + vH / 2);
            }
        }
    }

    /**
     * Handles any hardware buttons designated to perform mouse events.
     */
    public boolean hardwareButtonsAsMouseEvents(int keyCode, KeyEvent e, int combinedMetastate) {
        boolean used = false;
        boolean down = (e.getAction() == KeyEvent.ACTION_DOWN) ||
                (e.getAction() == KeyEvent.ACTION_MULTIPLE);
        if (down)
            pointerMask = POINTER_DOWN_MASK;
        else
            pointerMask = 0;

        if (shouldBeRightClick(e)) {
            rightClickMouse(combinedMetastate);
            used = true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            scrollMouse(keyCode, e);
            used = true;
        } else if (useDpadAsPointer && keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
            sendMouseClickUpOrDown(down);
            used = true;
        } else if (useDpadAsPointer && InputConstants.INSTANCE.getDpadKeyCodes().contains(keyCode)) {
            moveMouse(keyCode, e);
            used = true;
        }

        return used;
    }

    private void rightClickMouse(int combinedMetastate) {
        rightButtonDown(getX(), getY(), combinedMetastate);
        SystemClock.sleep(5);
        moveMouseButtonUp();
    }

    private void scrollMouse(int keyCode, KeyEvent e) {
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            scroller.direction = 0;
        } else {
            scroller.direction = 1;
        }

        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            handler.post(scroller);
        } else {
            handler.removeCallbacks(scroller);
        }
        releaseButton(pointerX, pointerY, 0);
    }

    private void moveMouse(int keyCode, KeyEvent e) {
        mover.direction = keyCode;
        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            handler.post(mover);
        } else {
            handler.removeCallbacks(mover);
        }
    }

    private void sendMouseClickUpOrDown(boolean down) {
        if (down) {
            handler.sendEmptyMessage(RemoteClientLibConstants.SHOW_KEYBOARD_ICON);
            this.leftButtonDown(pointerX, pointerY, 0);
        } else {
            this.releaseButton(pointerX, pointerY, 0);
        }
    }

    abstract public void leftButtonDown(int x, int y, int metaState);

    abstract public void middleButtonDown(int x, int y, int metaState);

    abstract public void rightButtonDown(int x, int y, int metaState);

    abstract public void scrollUp(int x, int y, int metaState);

    abstract public void scrollDown(int x, int y, int metaState);

    abstract public void scrollLeft(int x, int y, int metaState);

    abstract public void scrollRight(int x, int y, int metaState);

    abstract public void releaseButton(int x, int y, int metaState);

    abstract public void moveMouse(int x, int y, int metaState);

    abstract public void moveMouseButtonDown(int x, int y, int metaState);

    abstract public void moveMouseButtonUp(int x, int y, int metaState);

    /**
     * Convenience method to scroll up at the current pointer position.
     */
    public void scrollUp() {
        scrollUp(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to scroll down at the current pointer position.
     */
    public void scrollDown() {
        scrollDown(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to scroll left at the current pointer position.
     */
    public void scrollLeft() {
        scrollLeft(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to scroll right at the current pointer position.
     */
    public void scrollRight() {
        scrollRight(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to perform a middle mouse button click at the current pointer position.
     */
    public void middleClick() {
        middleButtonDown(pointerX, pointerY, 0);
        releaseButton(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to release button at the current pointer position.
     */
    public void releaseButton() {
        releaseButton(pointerX, pointerY, 0);
    }

    /**
     * Convenience method to perform moveMouseButtonUp at the current pointer position.
     */
    public void moveMouseButtonUp() {
        moveMouseButtonUp(pointerX, pointerY, 0);
    }

    public boolean isRelativeEvents() {
        return relativeEvents;
    }

    public void setRelativeEvents(boolean relativeEvents) {
        this.relativeEvents = relativeEvents;
        if (relativeEvents) {
            setSensitivity(1.0f);
            setAccelerated(false);
        } else {
            setSensitivity(DEFAULT_SENSITIVITY);
            setAccelerated(DEFAULT_ACCELERATED);
        }
    }

    public float getSensitivity() {
        return sensitivity;
    }

    public void setSensitivity(float sensitivity) {
        this.sensitivity = sensitivity;
    }

    public boolean isAccelerated() {
        return accelerated;
    }

    public void setAccelerated(boolean accelerated) {
        this.accelerated = accelerated;
    }

    public class MouseScroller implements Runnable {
        public int direction = 0;
        int delay = 100;

        @Override
        public void run() {
            if (direction == 0) {
                RemotePointer.this.scrollUp(pointerX, pointerY, 0);
            } else {
                RemotePointer.this.scrollDown(pointerX, pointerY, 0);
            }
            handler.postDelayed(this, delay);
        }
    }

    public class MouseMover implements Runnable {
        public int direction = 0;
        int delay = 50;
        int pixelsToMove = 5;

        @Override
        public void run() {
            int x = pointerX;
            int y = pointerY;
            if (direction == KeyEvent.KEYCODE_DPAD_LEFT) {
                x -= pixelsToMove;
            } else if (direction == KeyEvent.KEYCODE_DPAD_RIGHT) {
                x += pixelsToMove;
            } else if (direction == KeyEvent.KEYCODE_DPAD_UP) {
                y -= pixelsToMove;
            } else if (direction == KeyEvent.KEYCODE_DPAD_DOWN) {
                y += pixelsToMove;
            }
            if (x < 0) {
                RemotePointer.this.releaseButton(x, y, 0);
                handler.sendEmptyMessage(RemoteClientLibConstants.SHOW_KEYBOARD);
            } else {
                RemotePointer.this.moveMouse(x, y, 0);
                handler.postDelayed(this, delay);
            }
        }
    }

    protected void setNewPointerPosition(int x, int y) {
        int imageWidth = canvas.getImageWidth();
        int imageHeight = canvas.getImageHeight();
        pointerX = x;
        pointerY = y;
        // Do not let mouse pointer leave the bounds of the desktop.
        if (pointerX < 0) {
            pointerX = 0;
        } else if (pointerX >= imageWidth) {
            pointerX = imageWidth - 1;
        }
        if (pointerY < 0) {
            pointerY = 0;
        } else if (pointerY >= imageHeight) {
            pointerY = imageHeight - 1;
        }
        GeneralUtils.debugLog(this.debugLogging, TAG, "Sending absolute mouse event at: " + pointerX +
                ", " + pointerY + ", pointerMask: " + pointerMask);
    }
}
