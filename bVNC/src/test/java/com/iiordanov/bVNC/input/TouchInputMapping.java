package com.iiordanov.bVNC.input;

import android.view.MotionEvent;

/**
 * Gives tests outside this package access to the touch-to-remote coordinate mapping of the direct input modes.
 */
public final class TouchInputMapping {
    private TouchInputMapping() {
    }

    /**
     * @param handler input handler that maps the event
     * @param e       touch event in view coordinates
     * @return remote desktop X coordinate the handler sends for the event
     */
    public static int remoteX(TouchInputHandlerDirectSwipePan handler, MotionEvent e) {
        return handler.getX(e);
    }

    /**
     * @param handler input handler that maps the event
     * @param e       touch event in view coordinates
     * @return remote desktop Y coordinate the handler sends for the event
     */
    public static int remoteY(TouchInputHandlerDirectSwipePan handler, MotionEvent e) {
        return handler.getY(e);
    }
}
