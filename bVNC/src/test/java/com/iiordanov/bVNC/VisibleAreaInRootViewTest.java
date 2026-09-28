package com.iiordanov.bVNC;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests conversion of the window's visible display frame from screen coordinates to the coordinates
 * of the root view, for full-screen and windowed (split-screen, freeform) layouts.
 */
@RunWith(RobolectricTestRunner.class)
public class VisibleAreaInRootViewTest {

    @Test
    public void fullScreenWindow_isUnchanged() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(0, 0, 1080, 2340), new int[]{0, 0});
        assertEquals(new Rect(0, 0, 1080, 2340), area);
    }

    @Test
    public void windowBelowDisplayCutout_isShiftedUpByCutout() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(0, 80, 1080, 2340), new int[]{0, 80});
        assertEquals(new Rect(0, 0, 1080, 2260), area);
    }

    @Test
    public void windowUnderDisplayCutout_excludesCutout() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(0, 80, 1080, 2340), new int[]{0, 0});
        assertEquals(new Rect(0, 80, 1080, 2340), area);
    }

    @Test
    public void bottomSplitScreenWindow_isRelativeToRootView() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(0, 1210, 1080, 2340), new int[]{0, 1210});
        assertEquals(new Rect(0, 0, 1080, 1130), area);
    }

    @Test
    public void freeformWindowWithCaption_isRelativeToRootViewAndExcludesCaption() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(300, 200, 1500, 1100), new int[]{300, 240});
        assertEquals(new Rect(0, 0, 1200, 860), area);
    }

    @Test
    public void freeformWindowPartlyCoveredBySoftKeyboard_endsAtKeyboardTop() {
        Rect area = RemoteCanvasActivity.getVisibleAreaInRootView(
                new Rect(300, 200, 1500, 700), new int[]{300, 240});
        assertEquals(new Rect(0, 0, 1200, 460), area);
    }
}
