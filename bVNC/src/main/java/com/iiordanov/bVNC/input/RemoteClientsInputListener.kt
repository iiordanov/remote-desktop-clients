package com.iiordanov.bVNC.input

import android.app.Activity
import android.os.Build
import android.os.SystemClock
import android.util.Log
import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import com.iiordanov.bVNC.Constants
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors


class RemoteClientsInputListener(
    val activity: Activity,
    private val keyInputHandler: KeyInputHandler?,
    private val touchInputHandler: TouchInputHandler?,
    val resetOnScreenKeys: (input: Int) -> Int,
    val useDpadAsArrows: Boolean,
) : View.OnKeyListener {
    private val tag: String = "OnKeyListener"
    val workerPool: ExecutorService = Executors.newFixedThreadPool(1)

    override fun onKey(v: View?, keyCode: Int, evt: KeyEvent): Boolean {
        var consumed = false
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            return if (evt.action == KeyEvent.ACTION_DOWN) activity.onKeyDown(
                keyCode,
                evt
            ) else activity.onKeyUp(keyCode, evt)
        }
        try {
            if (evt.action == KeyEvent.ACTION_DOWN || evt.action == KeyEvent.ACTION_MULTIPLE) {
                consumed =
                    workerPool.run { keyInputHandler?.onKeyDownEvent(keyCode, evt) ?: false }
            } else if (evt.action == KeyEvent.ACTION_UP) {
                consumed = workerPool.run { keyInputHandler?.onKeyUpEvent(keyCode, evt) ?: false }
            }
            resetOnScreenKeys(keyCode)
        } catch (e: NullPointerException) {
            Log.e(tag, "NullPointerException ignored")
        }
        return consumed
    }

    fun sendText(s: String) {
        workerPool.submit { _sendText(s) }
    }

    fun _sendText(s: String) {
        for (i in 0 until s.length) {
            var event: KeyEvent? = null
            val c = s[i]
            if (Character.isISOControl(c)) {
                if (c == '\n') {
                    val keyCode = KeyEvent.KEYCODE_ENTER
                    keyInputHandler?.onKeyDownEvent(
                        keyCode,
                        KeyEvent(KeyEvent.ACTION_DOWN, keyCode)
                    )
                    throttleInputByWaiting()
                    keyInputHandler?.onKeyDownEvent(keyCode, KeyEvent(KeyEvent.ACTION_UP, keyCode))
                }
            } else {
                event = KeyEvent(
                    SystemClock.uptimeMillis(),
                    s.substring(i, i + 1),
                    KeyCharacterMap.FULL,
                    0
                )
                keyInputHandler?.onKeyDownEvent(event.keyCode, event)
                throttleInputByWaiting()
            }
        }
    }

    private fun throttleInputByWaiting() {
        try {
            Thread.sleep(5)
        } catch (e: InterruptedException) {
            Log.e(tag, "InterruptedException ignored")
        }
    }

    fun onTrackballEvent(event: MotionEvent?): Boolean {
        try {
            // If we are using the Dpad as arrow keys, don't send the event to the inputHandler.
            return if (useDpadAsArrows) false else workerPool.run {
                touchInputHandler!!.onTouchEvent(event)
            }
        } catch (e: NullPointerException) {
            Log.e(tag, "NullPointerException ignored")
        }
        return false
    }

    // Send touch events or mouse events like button clicks to be handled.
    fun onTouchEvent(event: MotionEvent?): Boolean {
        try {
            return workerPool.run { touchInputHandler!!.onTouchEvent(event) }
        } catch (e: NullPointerException) {
            Log.e(tag, "NullPointerException ignored")
        }
        return false
    }

    // Send e.g. mouse events like hover and scroll to be handled.
    fun onGenericMotionEvent(event: MotionEvent): Boolean {
        // Ignore TOOL_TYPE_FINGER events that come from the touchscreen with HOVER type action
        // which cause pointer jumping trouble in simulated touchpad for some devices.
        var toolTypeFinger = false
        if (Constants.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            toolTypeFinger = event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER
        }
        val a = event.action
        val isHoverEnter = a == MotionEvent.ACTION_HOVER_ENTER
        val isHoverExit = a == MotionEvent.ACTION_HOVER_EXIT
        val isHoverEventFromFingerOnTouchscreen = (
                a == MotionEvent.ACTION_HOVER_MOVE && event.source == InputDevice.SOURCE_TOUCHSCREEN && toolTypeFinger
                )
        if (!(isHoverEnter || isHoverExit || isHoverEventFromFingerOnTouchscreen)) {
            try {
                return workerPool.run { touchInputHandler!!.onTouchEvent(event) }
            } catch (e: NullPointerException) {
                Log.e(tag, "NullPointerException ignored")
            }
        }
        return false
    }
}