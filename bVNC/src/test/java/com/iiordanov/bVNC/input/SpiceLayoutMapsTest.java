package com.iiordanov.bVNC.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.KeyEvent;

import com.undatech.opaque.InputCarriable;
import com.undatech.opaque.RfbConnectable;
import com.undatech.opaque.Viewable;
import com.undatech.opaque.input.RemoteKeyboardState;
import com.undatech.opaque.input.RemotePointer;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Characterizes the scan codes aSPICE sends for keys that every SPICE layout map shares.
 */
@RunWith(RobolectricTestRunner.class)
public class SpiceLayoutMapsTest {
    private static final String LAYOUTS = "layouts";
    // Generator input the layouts are made from, relative to the module directory tests run in.
    private static final String COMMON_KEY_CODES = "layouts/common_keycodes.in";
    // Android key codes and the scan codes sent for them. Extended (E0) scan codes are 0x100 + code.
    private static final int[][] KEY_CODES_AND_SCAN_CODES = {
            {KeyEvent.KEYCODE_DPAD_UP, 0x148},
            {KeyEvent.KEYCODE_DPAD_DOWN, 0x150},
            {KeyEvent.KEYCODE_DPAD_LEFT, 0x14b},
            {KeyEvent.KEYCODE_DPAD_RIGHT, 0x14d},
            {KeyEvent.KEYCODE_PAGE_UP, 0x149},
            {KeyEvent.KEYCODE_PAGE_DOWN, 0x151},
            {KeyEvent.KEYCODE_FORWARD_DEL, 0x153},
            {KeyEvent.KEYCODE_MOVE_HOME, 0x147},
            {KeyEvent.KEYCODE_MOVE_END, 0x14f},
            {KeyEvent.KEYCODE_INSERT, 0x152},
            {KeyEvent.KEYCODE_FORWARD, 0x169},
            {KeyEvent.KEYCODE_BOOKMARK, 0x166},
            {KeyEvent.KEYCODE_SEARCH, 0x165},
            {KeyEvent.KEYCODE_ENVELOPE, 0x16c},
            {KeyEvent.KEYCODE_CALCULATOR, 0x121},
            {KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE, 0x122},
            {KeyEvent.KEYCODE_MEDIA_STOP, 0x124},
    };
    // Keys that have no PC keyboard scan code.
    private static final int[] KEY_CODES_WITHOUT_SCAN_CODES = {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_CHANNEL_UP,
    };

    @Test
    public void everyLayout_sendsNothingForKeysWithoutScanCodes() throws IOException {
        Context context = RuntimeEnvironment.getApplication();
        for (String layout : context.getAssets().list(LAYOUTS)) {
            RfbConnectable spice = connectedSpice();
            RemoteSpiceKeyboard keyboard = new RemoteSpiceKeyboard(context.getResources(), spice, viewable(context),
                    remoteInput(), null, layout, false);
            for (int keyCode : KEY_CODES_WITHOUT_SCAN_CODES) {
                keyboard.processLocalKeyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode), 0);
            }
            verify(spice, never().description(layout)).writeKeyEvent(anyInt(), anyInt(), anyBoolean());
        }
    }

    @Test
    public void everyLayout_sendsExtendedKeyScanCodes() throws IOException {
        Context context = RuntimeEnvironment.getApplication();
        String[] layouts = context.getAssets().list(LAYOUTS);
        assertNotEquals(0, layouts.length);
        for (String layout : layouts) {
            RfbConnectable spice = connectedSpice();
            RemoteSpiceKeyboard keyboard = new RemoteSpiceKeyboard(context.getResources(), spice, viewable(context),
                    remoteInput(), null, layout, false);
            for (int[] keyCodeAndScanCode : KEY_CODES_AND_SCAN_CODES) {
                int keyCode = keyCodeAndScanCode[0];
                String keyDescription = layout + ", key code " + keyCode;
                clearInvocations(spice);
                keyboard.processLocalKeyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode), 0);
                verify(spice, description(keyDescription)).writeKeyEvent(keyCodeAndScanCode[1], 0, true);
                verify(spice, description(keyDescription)).writeKeyEvent(anyInt(), anyInt(), anyBoolean());
            }
        }
    }

    @Test
    public void everyLayout_hasTheSameScanCodesForExactlyTheCommonKeyCodes() throws IOException {
        Context context = RuntimeEnvironment.getApplication();
        Set<String> commonKeyCodes = new HashSet<>();
        for (String line : Files.readAllLines(Paths.get(COMMON_KEY_CODES), StandardCharsets.UTF_8)) {
            commonKeyCodes.add(line.split(" ")[1]);
        }
        assertNotEquals(0, commonKeyCodes.size());
        Map<String, String> firstLayoutScanCodes = null;
        for (String layout : context.getAssets().list(LAYOUTS)) {
            Map<String, String> scanCodes = new HashMap<>();
            for (String line : readLayout(layout).split("\n")) {
                String[] keyAndScanCodes = line.split(" ", 2);
                boolean isKeyCode = Integer.parseInt(keyAndScanCodes[0]) < RemoteSpiceKeyboard.UNICODE_MASK;
                if (isKeyCode || commonKeyCodes.contains(keyAndScanCodes[0])) {
                    scanCodes.put(keyAndScanCodes[0], keyAndScanCodes[1]);
                }
            }
            assertEquals(layout, commonKeyCodes, scanCodes.keySet());
            if (firstLayoutScanCodes == null) {
                firstLayoutScanCodes = scanCodes;
            } else {
                assertEquals(layout, firstLayoutScanCodes, scanCodes);
            }
        }
    }

    @Test
    public void frenchLayout_isFrenchFranceLayout() throws IOException {
        assertEquals(readLayout("French (France)"), readLayout("French"));
    }

    private static String readLayout(String layout) throws IOException {
        Context context = RuntimeEnvironment.getApplication();
        try (InputStream in = context.getAssets().open(LAYOUTS + "/" + layout)) {
            return new String(in.readAllBytes(), StandardCharsets.UTF_8);
        }
    }

    private static RfbConnectable connectedSpice() {
        RfbConnectable spice = mock(RfbConnectable.class);
        spice.remoteKeyboardState = new RemoteKeyboardState(false);
        when(spice.isInNormalProtocol()).thenReturn(true);
        return spice;
    }

    private static Viewable viewable(Context context) {
        Viewable viewable = mock(Viewable.class);
        when(viewable.getContext()).thenReturn(context);
        return viewable;
    }

    private static InputCarriable remoteInput() {
        InputCarriable remoteInput = mock(InputCarriable.class);
        when(remoteInput.getPointer()).thenReturn(mock(RemotePointer.class));
        return remoteInput;
    }
}
