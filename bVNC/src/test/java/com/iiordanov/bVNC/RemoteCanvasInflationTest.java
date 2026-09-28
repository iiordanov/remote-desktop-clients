package com.iiordanov.bVNC;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests waiting on a background thread for RemoteCanvas to be laid out with a size.
 */
@RunWith(RobolectricTestRunner.class)
public class RemoteCanvasInflationTest {
    private static final long STILL_WAITING_CHECK_MS = 300;
    private static final long WAIT_TIMEOUT_MS = 2000;

    private RemoteCanvas canvas;

    @Before
    public void setUp() {
        Activity host = Robolectric.buildActivity(Activity.class).setup().get();
        canvas = new RemoteCanvas(host, null);
    }

    @Test
    public void waitUntilInflated_afterLayout_returns() throws InterruptedException {
        canvas.layout(0, 0, 1000, 800);
        Thread waiter = startWaiter();
        waiter.join(WAIT_TIMEOUT_MS);
        assertFalse("waitUntilInflated did not return", waiter.isAlive());
    }

    @Test
    public void waitUntilInflated_beforeLayout_blocksUntilLaidOut() throws InterruptedException {
        Thread waiter = startWaiter();
        waiter.join(STILL_WAITING_CHECK_MS);
        assertTrue("waitUntilInflated returned before the view had a size", waiter.isAlive());
        canvas.layout(0, 0, 1000, 800);
        waiter.join(WAIT_TIMEOUT_MS);
        assertFalse("waitUntilInflated did not return after layout", waiter.isAlive());
    }

    private Thread startWaiter() {
        Thread waiter = new Thread(canvas::waitUntilInflated);
        waiter.setDaemon(true);
        waiter.start();
        return waiter;
    }
}
