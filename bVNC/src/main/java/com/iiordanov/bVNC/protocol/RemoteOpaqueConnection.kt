package com.iiordanov.bVNC.protocol

import android.content.Context
import android.util.Log
import com.iiordanov.bVNC.App
import com.iiordanov.bVNC.COLORMODEL
import com.iiordanov.bVNC.Utils
import com.iiordanov.bVNC.input.RemoteSpiceKeyboard
import com.iiordanov.bVNC.input.RemoteSpicePointer
import com.undatech.opaque.Connection
import com.undatech.opaque.MessageDialogs
import com.undatech.opaque.RemoteClientLibConstants
import com.undatech.opaque.SpiceCommunicator
import com.undatech.opaque.Viewable
import com.undatech.remoteClientUi.R
import java.io.File

open class RemoteOpaqueConnection(
    context: Context,
    connection: Connection?,
    canvas: Viewable,
    hideKeyboardAndExtraKeys: Runnable,
) : RemoteConnection(context, connection, canvas, hideKeyboardAndExtraKeys) {
    private val tag: String = "RemoteOpaqueConnection"
    protected var spiceComm: SpiceCommunicator? = null

    override fun initializeConnection() {
        super.initializeConnection()
        checkNetworkConnectivity()
        spiceComm = SpiceCommunicator(
            context, handler, canvas,
            connection.isRequestingNewDisplayResolution || connection.rdpResType == RemoteClientLibConstants.RDP_GEOM_SELECT_CUSTOM,
            !Utils.isFree(context) && connection.isUsbEnabled, App.debugLog
        )
        rfbConn = spiceComm
        pointer = RemoteSpicePointer(spiceComm, context, this, canvas, handler, !connection.useDpadAsArrows, App.debugLog)
        try {
            keyboard = RemoteSpiceKeyboard(
                context.resources, spiceComm, canvas, this,
                handler, connection.layoutMap, App.debugLog
            )
        } catch (e: Throwable) {
            handleUncaughtException(e, R.string.error_spice_unable_to_connect)
        }
    }

    protected fun startConnectionDirectlyOrFromFile() {
        val connectionConfigFile = connection.connectionConfigFile
        if (connectionConfigFile == null) {
            startConnection()
        } else {
            checkConfigFileAndStart(connectionConfigFile)
        }
    }

    private fun checkConfigFileAndStart(connectionConfigFile: String?) {
        Log.d(tag, "Initializing session from vv file: $connectionConfigFile")
        val f = File(connectionConfigFile)
        if (!f.exists()) {
            // Quit with an error if the file does not exist.
            MessageDialogs.displayMessageAndFinish(context, R.string.vv_file_not_found, R.string.error_dialog_title)
        }
        startFromVvFile(connectionConfigFile)
    }

    open fun startConnection() {

    }

    open fun startFromVvFile(configFileName: String?) {
        val cThread: Thread = object : Thread() {
            override fun run() {
                try {
                    spiceComm?.startSessionFromVvFile(configFileName, connection.isAudioPlaybackEnabled)
                } catch (e: Throwable) {
                    handleUncaughtException(e, R.string.error_spice_unable_to_connect)
                }
            }
        }
        cThread.start()
    }

    override fun getSshTunnelTargetAddress(): String = ""

    override fun isColorModel(cm: COLORMODEL) = false
    override fun setColorModel(cm: COLORMODEL?) {}

    @Throws(java.lang.Exception::class)
    override fun correctAfterRotation() {
        if (connection.isRequestingNewDisplayResolution) {
            spiceComm?.requestResolution(canvas.width, canvas.height)
        }
    }

    protected fun <T> castAsObject(someObject: T) = (someObject as Object)
}