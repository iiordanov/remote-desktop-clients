package com.iiordanov.bVNC.input

import android.app.Activity
import android.util.Log
import android.view.KeyEvent
import android.view.View


class RemoteClientsOnKeyListener(
    val activity: Activity,
    private val inputHandler: InputHandler?,
    val resetOnScreenKeys: (input: Int) -> Int
) : View.OnKeyListener {
    private val tag: String = "OnKeyListener"

    override fun onKey(v: View?, keyCode: Int, evt: KeyEvent): Boolean {
        var consumed = false
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            return if (evt.action == KeyEvent.ACTION_DOWN) activity.onKeyDown(
                keyCode, evt
            ) else activity.onKeyUp(keyCode, evt)
        }
        try {
            if (evt.action == KeyEvent.ACTION_DOWN || evt.action == KeyEvent.ACTION_MULTIPLE) {
                consumed = inputHandler?.onKeyDown(keyCode, evt) ?: false
            } else if (evt.action == KeyEvent.ACTION_UP) {
                consumed = inputHandler?.onKeyUp(keyCode, evt) ?: false
            }
            resetOnScreenKeys(keyCode)
        } catch (e: NullPointerException) {
            Log.e(tag, "NullPointerException ignored")
        }
        return consumed
    }

}