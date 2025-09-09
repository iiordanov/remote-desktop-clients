package com.undatech.opaque;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Handler;

public interface Viewable {
    /**
     * Waits until the view has been inflated with non-zero dimensions.
     * Blocks the calling thread until getWidth() and getHeight() return non-zero values.
     */
    void waitUntilInflated();

    /**
     * Gets the desired width for the remote desktop connection.
     * Takes into account custom resolution settings and connection preferences.
     * @return the desired width in pixels
     */
    int getDesiredWidth();

    /**
     * Gets the desired height for the remote desktop connection.
     * Takes into account custom resolution settings and connection preferences.
     * @return the desired height in pixels
     */
    int getDesiredHeight();

    /**
     * Reallocates the drawable with the specified dimensions.
     * Creates a new bitmap data structure to hold the remote desktop image.
     * @param width the new width in pixels
     * @param height the new height in pixels
     */
    void reallocateDrawable(int width, int height);

    /**
     * Gets the bitmap containing the remote desktop image data.
     * @return the bitmap, or null if not yet initialized
     */
    Bitmap getBitmap();

    /**
     * Triggers a redraw of the specified rectangular region.
     * @param x the X coordinate of the region to redraw
     * @param y the Y coordinate of the region to redraw
     * @param width the width of the region to redraw
     * @param height the height of the region to redraw
     */
    void reDraw(int x, int y, int width, int height);

    /**
     * Triggers a redraw of the specified rectangular region (float version).
     * @param x the X coordinate of the region to redraw
     * @param y the Y coordinate of the region to redraw
     * @param width the width of the region to redraw
     * @param height the height of the region to redraw
     */
    void reDraw(float x, float y, float width, float height);

    /**
     * Sets the position of the mouse pointer on the remote desktop.
     * @param x the X coordinate of the mouse pointer
     * @param y the Y coordinate of the mouse pointer
     */
    void setMousePointerPosition(int x, int y);

    /**
     * Sets the mouse mode for the remote connection.
     * @param relative true for relative mouse mode, false for absolute
     */
    void mouseMode(boolean relative);

    /**
     * Checks if the canvas is able to pan (scroll around the desktop).
     * @return true if panning is enabled and possible
     */
    boolean isAbleToPan();

    /**
     * Gets the Android context associated with this view.
     * @return the context
     */
    Context getContext();

    /**
     * Invalidates the current mouse cursor position, triggering a redraw of the cursor.
     */
    void invalidateMousePosition();

    /**
     * Checks if mouse following pan mode is enabled.
     * When enabled, the view pans to keep the mouse pointer visible.
     * @return true if mouse follow pan is enabled
     */
    boolean getMouseFollowPan();

    /**
     * Gets the absolute X position of the current pan/scroll offset.
     * @return the absolute X position in remote desktop coordinates
     */
    int getAbsX();

    /**
     * Gets the absolute Y position of the current pan/scroll offset.
     * @return the absolute Y position in remote desktop coordinates
     */
    int getAbsY();

    /**
     * Gets the visible width of the desktop taking zoom into account.
     * @return the visible desktop width in remote coordinates
     */
    int getVisibleDesktopWidth();

    /**
     * Gets the visible height of the desktop taking zoom into account.
     * @return the visible desktop height in remote coordinates
     */
    int getVisibleDesktopHeight();

    /**
     * Gets the total width of the remote desktop image.
     * @return the image width in pixels
     */
    int getImageWidth();

    /**
     * Gets the total height of the remote desktop image.
     * @return the image height in pixels
     */
    int getImageHeight();

    /**
     * Gets the width of the view widget itself.
     * @return the view width in pixels
     */
    int getWidth();

    /**
     * Gets the height of the view widget itself.
     * @return the view height in pixels
     */
    int getHeight();

    /**
     * Checks if the full screen mode is being used.
     * @return true if using full screen mode
     */
    boolean isUseFull();

    /**
     * Synchronizes the scroll position between the view and the underlying drawable.
     * Used primarily for large bitmap data implementations.
     */
    void syncScroll();

    /**
     * Notifies the drawable that it can stop waiting for updates.
     * Called after remote server update processing is complete.
     */
    void doneWaiting();

    /**
     * Moves the soft cursor to the specified position and triggers a redraw.
     * @param updateRectX the X coordinate for the cursor
     * @param updateRectY the Y coordinate for the cursor
     */
    void softCursorMove(int updateRectX, int updateRectY);

    /**
     * Displays a short toast message on screen.
     * @param message the message to display
     */
    void displayShortToastMessage(final CharSequence message);

    /**
     * Posts a drawable setter operation to the UI thread.
     * Used to update the drawable on the main thread.
     */
    void postDrawableSetter();

    /**
     * Prepares for a full framebuffer update request.
     * @param incremental true for incremental update, false for full update
     */
    void prepareFullUpdateRequest(boolean incremental);

    /**
     * Gets the X offset of the drawable's visible region.
     * @return the X offset in pixels
     */
    int getXoffset();

    /**
     * Gets the Y offset of the drawable's visible region.
     * @return the Y offset in pixels
     */
    int getYoffset();

    /**
     * Gets the bitmap width of the underlying drawable.
     * @return the bitmap width in pixels
     */
    int bmWidth();

    /**
     * Gets the bitmap height of the underlying drawable.
     * @return the bitmap height in pixels
     */
    int bmHeight();

    /**
     * Calculates the appropriate remote width based on view dimensions and connection settings.
     * @param width the view width
     * @param height the view height
     * @return the calculated remote width
     */
    int getRemoteWidth(int width, int height);

    /**
     * Calculates the appropriate remote height based on view dimensions and connection settings.
     * @param width the view width
     * @param height the view height
     * @return the calculated remote height
     */
    int getRemoteHeight(int width, int height);

    /**
     * Writes a screenshot of the current desktop to a file.
     * @param filePath the path where the screenshot should be saved
     * @param dstWidth the width of the output image (used for scaling)
     */
    void writeScreenshotToFile(String filePath, int dstWidth);

    /**
     * Disposes of the drawable and releases associated resources.
     * Should be called when the view is no longer needed.
     */
    void disposeDrawable();

    /**
     * Get the current zoom factor
     * @return the zoom factor
     */
    float getZoomFactor();

    /**
     * Get the display density
     * @return the display density
     */
    float getDisplayDensity();

    /**
     * Get the top offset of the canvas view
     * @return the top offset
     */
    int getTop();

    /**
     * Move the pan to make the pointer visible
     */
    void movePanToMakePointerVisible();

    /**
     * Pan the view relatively by the given offsets (float version)
     * @param deltaX the X offset
     * @param deltaY the Y offset
     */
    boolean relativePan(float deltaX, float deltaY);

    /**
     * Pan the view relatively by the given offsets (int version)
     * @param deltaX the X offset
     * @param deltaY the Y offset
     * @return true if the pan was successful
     */
    boolean relativePan(int deltaX, int deltaY);

    /**
     * Pan the view to the absolute coordinates
     * @param x the absolute X coordinate
     * @param y the absolute Y coordinate
     */
    void absolutePan(int x, int y);

    /**
     * Invalidate the canvas view to trigger a redraw
     */
    void invalidate();

    /**
     * Get/set whether the cursor is being moved
     * @return true if cursor is being moved
     */
    boolean isCursorBeingMoved();
    void setCursorBeingMoved(boolean cursorBeingMoved);

    /**
     * Change the zoom with the given parameters
     * @param scaleFactor the scale factor
     * @param fx the focal X coordinate
     * @param fy the focal Y coordinate
     */
    void changeZoom(float scaleFactor, float fx, float fy);


    /**
     * Check if the canvas zoomer is able to pan
     * @return true if able to pan
     */
    boolean isZoomerAbleToPan();

    /**
     * Get the handler for posting events
     * @return the handler
     */
    Handler getHandler();

    /**
     * Get resources for accessing strings and other resources
     * @return the resources
     */
    Resources getResources();

    /**
     * Display a short toast message
     * @param messageResId the resource ID of the message
     */
    void displayShortToastMessage(int messageResId);
}
