package com.iiordanov.permissions

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import android.util.Log
import android.view.View
import androidx.core.net.toUri
import com.iiordanov.bVNC.Utils
import com.iiordanov.util.KotlinUtils
import com.undatech.remoteClientUi.R

class BatteryOptimizationDisabler(val context: Context, val view: View) {
    val tag = "BatteryOptimizationD..."

    fun requestBatteryOptimizationExemptionAutomaticallyOnce() {
        val requested = Utils.querySharedPreferenceBoolean(context, "batteryOptimizationExemptionRequested", false)
        if (!requested) {
            requestBatteryOptimizationExemption(false)
            Utils.setSharedPreferenceBoolean(context, "batteryOptimizationExemptionRequested", true)
        }
    }

    fun requestBatteryOptimizationExemption(fromUserAction: Boolean) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val intent = Intent()
            val packageName: String? = Utils.pName(context)
            val pm = context.getSystemService(Context.POWER_SERVICE) as PowerManager?

            if (pm != null && !pm.isIgnoringBatteryOptimizations(packageName)) {
                try {
                    intent.setAction(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
                    intent.setData(("package:$packageName").toUri())
                    context.startActivity(intent)
                } catch (e: ActivityNotFoundException) {
                    KotlinUtils.makeSnackbarLong(context, view, R.string.disable_battery_optimizations_failed)
                    Log.e(tag, "ActivityNotFoundException:\n" + Log.getStackTraceString(e))
                }
            } else {
                if (fromUserAction) {
                    KotlinUtils.makeSnackbarLong(context, view, R.string.disable_battery_optimizations_already_done)
                }
            }
        }
    }
}