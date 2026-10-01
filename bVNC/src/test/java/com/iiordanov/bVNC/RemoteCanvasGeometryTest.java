package com.iiordanov.bVNC;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Matrix;
import android.view.MotionEvent;

import com.iiordanov.bVNC.input.TouchInputDelegate;
import com.iiordanov.bVNC.input.TouchInputHandler;
import com.iiordanov.bVNC.input.TouchInputHandlerDirectSwipePan;
import com.iiordanov.bVNC.input.TouchInputMapping;
import com.undatech.opaque.Connection;
import com.undatech.opaque.InputCarriable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.function.Supplier;

/**
 * Characterizes how RemoteCanvas and the scaling modes place the remote desktop in the view, how far it
 * can be panned, and how touches are mapped back to remote desktop coordinates.
 */
@RunWith(RobolectricTestRunner.class)
public class RemoteCanvasGeometryTest {
    private static final int[][] VIEW_AND_FRAMEBUFFER_SIZES = {
            {1000, 800, 1920, 1080},
            {1001, 801, 1920, 1080},
            {2000, 1200, 800, 600},
            {2000, 1600, 800, 600},
            {1000, 2000, 1280, 800},
            {1000, 800, 1000, 800},
    };

    private RemoteCanvas canvas;
    private RemoteCanvasActivity activity;
    private TouchInputHandlerDirectSwipePan touchHandler;

    @Test
    public void fitToScreen_framebufferLargerThanView_isCentered() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        new FitToScreenScaling().setScaleTypeForActivity(activity);
        assertCentered();
    }

    @Test
    public void fitToScreen_framebufferSmallerThanView_isCentered() {
        createLaidOutCanvas(2000, 1200, 800, 600);
        new FitToScreenScaling().setScaleTypeForActivity(activity);
        assertCentered();
    }

    @Test
    public void oneToOne_framebufferLargerThanView_showsTopLeftCorner() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(0, 0);
        assertFramebufferOriginAt(0, 0);
    }

    @Test
    public void oneToOne_framebufferSmallerThanView_isCentered() {
        createLaidOutCanvas(2000, 1600, 800, 600);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(0, 0);
        assertCentered();
    }

    @Test
    public void zoom_framebufferSmallerThanViewAtMinimumScale_isCentered() {
        createLaidOutCanvas(2000, 1200, 800, 600);
        new ZoomScaling().setScaleTypeForActivity(activity);
        assertEquals(2.0f, canvas.getZoomFactor(), 0.0001f);
        assertCentered();
    }

    @Test
    public void relativePan_framebufferSmallerThanView_staysCentered() {
        createLaidOutCanvas(2000, 1600, 800, 600);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(300, 200);
        assertCentered();
    }

    @Test
    public void relativePan_framebufferNarrowerButTallerThanView_centersHorizontallyAndPansVertically() {
        createLaidOutCanvas(2000, 800, 800, 1080);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(0, 10000);
        assertFramebufferOriginAt((2000 - 800) / 2f, -(1080 - 800 + Constants.BOTTOM_MARGIN));
        assertTouchLandsOnRenderedPixel("centered horizontally and panned vertically");
    }

    @Test
    public void relativePan_framebufferOnePixelShorterThanView_keepsPanMargins() {
        createLaidOutCanvas(800, 601, 800, 600);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(0, 10000);
        assertEquals(600 - 601 + Constants.BOTTOM_MARGIN, canvas.getAbsY());
    }

    @Test
    public void relativePan_framebufferSmallerThanAreaAboveSoftKeyboard_isCenteredInThatArea() {
        createLaidOutCanvas(2000, 1600, 800, 600);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.setVisibleDesktopHeight(1000);
        canvas.relativePan(0, 0);
        assertFramebufferOriginAt((2000 - 800) / 2f, (1000 - 600) / 2f);
        assertTouchLandsOnRenderedPixel("centered above the soft keyboard");
    }

    @Test
    public void absolutePan_framebufferSmallerThanView_isCentered() {
        createLaidOutCanvas(2000, 1600, 800, 600);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.absolutePan(100, 100);
        assertCentered();
        assertTouchLandsOnRenderedPixel("centered by an absolute pan");
    }

    @Test
    public void fitToScreen_touchLandsOnRenderedPixel() {
        forEachViewAndFramebufferSize(FitToScreenScaling::new);
    }

    @Test
    public void oneToOne_touchLandsOnRenderedPixel() {
        forEachViewAndFramebufferSize(OneToOneScaling::new);
    }

    @Test
    public void zoom_touchLandsOnRenderedPixel() {
        forEachViewAndFramebufferSize(ZoomScaling::new);
    }

    @Test
    public void zoom_touchLandsOnRenderedPixelAfterPinchAndPan() {
        for (int[] sizes : VIEW_AND_FRAMEBUFFER_SIZES) {
            createLaidOutCanvas(sizes[0], sizes[1], sizes[2], sizes[3]);
            ZoomScaling zoom = new ZoomScaling();
            zoom.setScaleTypeForActivity(activity);
            zoom.changeZoom(activity, 1.7f, sizes[0] / 3f, sizes[1] / 4f);
            assertTouchLandsOnRenderedPixel(describe(sizes) + " after pinch");
            canvas.relativePan(123, 77);
            assertTouchLandsOnRenderedPixel(describe(sizes) + " after pinch and pan");
        }
    }

    @Test
    public void fitToScreen_viewResizedWithoutReapplyingScaling_touchStillLandsOnRenderedPixel() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        new FitToScreenScaling().setScaleTypeForActivity(activity);
        canvas.layout(0, 0, 700, 900);
        assertTouchLandsOnRenderedPixel("resized to 700x900");
    }

    @Test
    public void fitToScreen_scalingReappliedAfterResize_recentersFramebuffer() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        FitToScreenScaling fitToScreen = new FitToScreenScaling();
        fitToScreen.setScaleTypeForActivity(activity);
        canvas.layout(0, 0, 1400, 800);
        fitToScreen.setScaleTypeForActivity(activity);
        assertCentered();
        assertTouchLandsOnRenderedPixel("resized to 1400x800 and rescaled");
    }

    @Test
    public void relativePan_allowsPanningPastBottomAndTopByMargins() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        new OneToOneScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(10000, 10000);
        assertEquals(1920 - 1000, canvas.getAbsX());
        assertEquals(1080 - 800 + Constants.BOTTOM_MARGIN, canvas.getAbsY());
        canvas.relativePan(-10000, -10000);
        assertEquals(0, canvas.getAbsX());
        assertEquals(-Constants.TOP_MARGIN, canvas.getAbsY());
    }

    @Test
    public void relativePan_atScaleTwo_bottomMarginStaysTheSameOnScreen() {
        createLaidOutCanvas(2000, 1200, 800, 600);
        new ZoomScaling().setScaleTypeForActivity(activity);
        canvas.relativePan(0, 10000);
        assertEquals(Constants.BOTTOM_MARGIN / 2, canvas.getAbsY());
    }

    @Test
    public void relativePan_isIgnoredInFitToScreen() {
        createLaidOutCanvas(1000, 800, 1920, 1080);
        new FitToScreenScaling().setScaleTypeForActivity(activity);
        int absX = canvas.getAbsX();
        int absY = canvas.getAbsY();
        assertFalse(canvas.relativePan(100, 100));
        assertEquals(absX, canvas.getAbsX());
        assertEquals(absY, canvas.getAbsY());
    }

    private void forEachViewAndFramebufferSize(Supplier<AbstractScaling> scalingFactory) {
        for (int[] sizes : VIEW_AND_FRAMEBUFFER_SIZES) {
            createLaidOutCanvas(sizes[0], sizes[1], sizes[2], sizes[3]);
            scalingFactory.get().setScaleTypeForActivity(activity);
            canvas.relativePan(0, 0);
            assertTouchLandsOnRenderedPixel(describe(sizes));
        }
    }

    private void createLaidOutCanvas(int viewWidth, int viewHeight, int fbWidth, int fbHeight) {
        Activity host = Robolectric.buildActivity(Activity.class).setup().get();
        canvas = new RemoteCanvas(host, null);
        canvas.myDrawable = new FullBufferBitmapData(fbWidth, fbHeight, canvas);
        canvas.myDrawable.setImageDrawable(canvas);

        activity = mock(RemoteCanvasActivity.class);
        when(activity.getCanvas()).thenReturn(canvas);
        when(activity.getConnection()).thenReturn(mock(Connection.class));
        activity.touchInputHandler = mock(TouchInputHandler.class);

        touchHandler = new TouchInputHandlerDirectSwipePan(
                mock(TouchInputDelegate.class), canvas, mock(InputCarriable.class), false, 1f);
        canvas.layout(0, 0, viewWidth, viewHeight);
    }

    private void assertCentered() {
        float[] topLeft = viewPointOf(0, 0);
        float[] bottomRight = viewPointOf(canvas.getImageWidth(), canvas.getImageHeight());
        float left = topLeft[0];
        float right = canvas.getWidth() - bottomRight[0];
        float top = topLeft[1];
        float bottom = canvas.getHeight() - bottomRight[1];
        // Pan, scroll and centering offsets are whole pixels, so a margin can be off by one and a half desktop
        // pixels plus a screen pixel.
        float tolerance = 2f * (1.5f * canvas.getZoomFactor() + 1f);
        assertEquals("left and right margins", left, right, tolerance);
        assertEquals("top and bottom margins", top, bottom, tolerance);
    }

    private void assertFramebufferOriginAt(float expectedViewX, float expectedViewY) {
        float[] origin = viewPointOf(0, 0);
        assertEquals("framebuffer origin X in view", expectedViewX, origin[0], 1f);
        assertEquals("framebuffer origin Y in view", expectedViewY, origin[1], 1f);
    }

    private void assertTouchLandsOnRenderedPixel(String scenario) {
        float scale = canvas.getZoomFactor();
        // Scroll is rounded to a screen pixel, the centering offset to half a desktop pixel and the touch point
        // to a desktop pixel.
        float toleranceInFramebufferPixels = 1f / scale + 1.5f;
        for (float fractionX : new float[]{0.02f, 0.5f, 0.98f}) {
            for (float fractionY : new float[]{0.02f, 0.5f, 0.98f}) {
                float viewX = fractionX * canvas.getWidth();
                float viewY = fractionY * canvas.getHeight();
                float[] rendered = framebufferPointRenderedAt(viewX, viewY);
                MotionEvent e = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, viewX, viewY, 0);
                int touchX = TouchInputMapping.remoteX(touchHandler, e);
                int touchY = TouchInputMapping.remoteY(touchHandler, e);
                e.recycle();
                String where = scenario + ", scale " + scale + ", view point (" + viewX + ", " + viewY + ")";
                assertEquals(where + ": X", rendered[0], touchX, toleranceInFramebufferPixels);
                assertEquals(where + ": Y", rendered[1], touchY, toleranceInFramebufferPixels);
            }
        }
    }

    private float[] framebufferPointRenderedAt(float viewX, float viewY) {
        Matrix inverse = new Matrix();
        assertTrue("image matrix is invertible", canvas.getImageMatrix().invert(inverse));
        float[] point = {viewX + canvas.getScrollX(), viewY + canvas.getScrollY()};
        inverse.mapPoints(point);
        return point;
    }

    private float[] viewPointOf(float framebufferX, float framebufferY) {
        float[] point = {framebufferX, framebufferY};
        canvas.getImageMatrix().mapPoints(point);
        point[0] -= canvas.getScrollX();
        point[1] -= canvas.getScrollY();
        return point;
    }

    private static String describe(int[] sizes) {
        return "view " + sizes[0] + "x" + sizes[1] + ", framebuffer " + sizes[2] + "x" + sizes[3];
    }
}
