package com.iiordanov.bVNC.input;

import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.mock;
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

/**
 * Characterizes the scan codes aSPICE sends for keys that every SPICE layout map shares.
 */
@RunWith(RobolectricTestRunner.class)
public class SpiceLayoutMapsTest {
    private static final String LAYOUTS = "layouts";
    // Android key codes and the scan codes sent for them. Extended (E0) scan codes are 0x100 + code.
    private static final int[][] KEY_CODES_AND_SCAN_CODES = {
            {KeyEvent.KEYCODE_DPAD_UP, 72},
            {KeyEvent.KEYCODE_DPAD_DOWN, 80},
            {KeyEvent.KEYCODE_DPAD_LEFT, 75},
            {KeyEvent.KEYCODE_DPAD_RIGHT, 77},
            {KeyEvent.KEYCODE_PAGE_UP, 73},
            {KeyEvent.KEYCODE_PAGE_DOWN, 81},
            {KeyEvent.KEYCODE_FORWARD_DEL, 83},
            {KeyEvent.KEYCODE_MOVE_HOME, 0x147},
            {KeyEvent.KEYCODE_MOVE_END, 0x14f},
            {KeyEvent.KEYCODE_INSERT, 0x152},
    };

    @Test
    public void everyLayout_sendsNavigationKeyScanCodes() throws IOException {
        Context context = RuntimeEnvironment.getApplication();
        String[] layouts = context.getAssets().list(LAYOUTS);
        assertNotEquals(0, layouts.length);
        for (String layout : layouts) {
            RfbConnectable spice = connectedSpice();
            RemoteSpiceKeyboard keyboard = new RemoteSpiceKeyboard(context.getResources(), spice, viewable(context),
                    remoteInput(), null, layout, false);
            for (int[] keyCodeAndScanCode : KEY_CODES_AND_SCAN_CODES) {
                int keyCode = keyCodeAndScanCode[0];
                keyboard.processLocalKeyEvent(keyCode, new KeyEvent(KeyEvent.ACTION_DOWN, keyCode), 0);
                verify(spice, description(layout + ", key code " + keyCode))
                        .writeKeyEvent(keyCodeAndScanCode[1], 0, true);
            }
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
