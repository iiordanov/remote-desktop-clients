package com.iiordanov.bVNC.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.KeyEvent;
import android.view.View;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RemoteInputConnectionTest {
    private RecordingConnection connection;

    @Before
    public void setUp() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() ->
                connection = new RecordingConnection(new View(
                        InstrumentationRegistry.getInstrumentation().getTargetContext())));
    }

    @Test
    public void backspaceReachesRemoteWithEmptyLocalBuffer() {
        assertTrue(connection.deleteSurroundingText(1, 0));
        assertKeyPairs(KeyEvent.KEYCODE_DEL);
    }

    @Test
    public void backspaceWorksImmediatelyAfterTyping() {
        assertTrue(connection.commitText("hello", 1));
        connection.events.clear();

        assertTrue(connection.deleteSurroundingText(1, 0));
        assertTrue(connection.deleteSurroundingText(1, 0));
        assertKeyPairs(KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_DEL);
    }

    @Test
    public void forwardsAllRequestedDeletionsInBothDirections() {
        assertTrue(connection.deleteSurroundingText(3, 2));
        assertKeyPairs(KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_DEL,
                KeyEvent.KEYCODE_FORWARD_DEL, KeyEvent.KEYCODE_FORWARD_DEL);
    }

    @Test
    public void codePointDeletionsReachRemote() {
        assertTrue(connection.deleteSurroundingTextInCodePoints(2, 1));
        assertKeyPairs(KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_FORWARD_DEL);
    }

    @Test
    public void zeroLengthDeletionDoesNotSendKeys() {
        assertTrue(connection.deleteSurroundingText(0, 0));
        assertTrue(connection.deleteSurroundingTextInCodePoints(0, 0));
        assertKeyPairs();
    }

    @Test
    public void invalidLengthsDoNotPartiallyDelete() {
        assertFalse(connection.deleteSurroundingText(-1, 1));
        assertFalse(connection.deleteSurroundingText(1, -1));
        assertFalse(connection.deleteSurroundingTextInCodePoints(-1, 1));
        assertFalse(connection.deleteSurroundingTextInCodePoints(1, -1));
        assertKeyPairs();
    }

    @Test
    public void committedTextStillUsesExistingKeyEventPath() {
        assertTrue(connection.commitText("hello", 1));
        assertEquals(1, connection.events.size());
        assertEquals(KeyEvent.ACTION_MULTIPLE, connection.events.get(0).getAction());
        assertEquals("hello", connection.events.get(0).getCharacters());
        assertEquals(0, connection.getEditable().length());
    }

    @Test
    public void compositionStillWaitsForCommit() {
        assertTrue(connection.setComposingText("hello", 1));
        assertTrue(connection.setComposingText("help", 1));
        assertEquals(0, connection.events.size());

        assertTrue(connection.commitText("help", 1));
        assertEquals(1, connection.events.size());
        assertEquals("help", connection.events.get(0).getCharacters());
    }

    private void assertKeyPairs(int... keyCodes) {
        assertEquals(keyCodes.length * 2, connection.events.size());
        for (int i = 0; i < keyCodes.length; i++) {
            KeyEvent down = connection.events.get(i * 2);
            KeyEvent up = connection.events.get(i * 2 + 1);
            assertEquals(KeyEvent.ACTION_DOWN, down.getAction());
            assertEquals(KeyEvent.ACTION_UP, up.getAction());
            assertEquals(keyCodes[i], down.getKeyCode());
            assertEquals(keyCodes[i], up.getKeyCode());
        }
    }

    private static class RecordingConnection extends RemoteInputConnection {
        final List<KeyEvent> events = new ArrayList<>();

        RecordingConnection(View view) {
            super(view);
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            events.add(event);
            return true;
        }
    }
}
