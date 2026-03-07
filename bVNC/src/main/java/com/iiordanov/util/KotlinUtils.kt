package com.iiordanov.util

import android.content.Context
import android.view.View
import com.google.android.material.snackbar.Snackbar

object KotlinUtils {
    fun makeSnackbarLong(context: Context, view: View, resId: Int) {
        Snackbar.make(
            context, view, context.getString(resId), Snackbar.LENGTH_LONG
        ).show()
    }

}