package com.iiordanov.bVNC.protocol

import android.content.Context
import com.iiordanov.bVNC.Utils
import com.undatech.opaque.Connection
import com.undatech.opaque.Viewable
import com.undatech.remoteClientUi.R

class RemoteConnectionFactory(
    val context: Context,
    val connection: Connection?,
    val viewable: Viewable,
    val vvFileName: String?,
) {
    // This flag indicates whether this is the VNC client
    private var isVnc = Utils.isVnc(context)

    // This flag indicates whether this is the RDP client
    private var isRdp = Utils.isRdp(context)

    // This flag indicates whether this is the SPICE client
    private var isSpice = Utils.isSpice(context)

    // This flag indicates whether this is the Opaque client
    private var isOpaque = Utils.isOpaque(context)

    // This flag indicates whether Opaque is connecting to oVirt
    private var isOvirt = connection?.connectionTypeString == context.resources.getString(R.string.connection_type_ovirt)

    // This flag indicates whether Opaque is connecting to Proxmox
    private var isProxmox = connection?.connectionTypeString == context.resources.getString(R.string.connection_type_pve)

    private var isOpaqueHandlingVvFile = isOpaque && vvFileName != null

    fun build(): RemoteConnection {
        val remoteConnection: RemoteConnection
        if (isSpice) {
            remoteConnection = RemoteSpiceConnection(context, viewable)
        } else if (isRdp) {
            remoteConnection = RemoteRdpConnection(context, viewable)
        } else if (isVnc) {
            remoteConnection = RemoteVncConnection(context, viewable)
        } else if (isOpaque) {
            remoteConnection = if (isOvirt) {
                RemoteOvirtConnection(context, viewable)
            } else if (isProxmox) {
                RemoteProxmoxConnection(context, viewable)
            } else if (isOpaqueHandlingVvFile) {
                // For handling vv files
                RemoteOpaqueConnection(context, viewable)
            } else {
                throw IllegalStateException("Connection must be one of oVirt or Proxmox if app type is Opaque")
            }
        } else {
            throw IllegalStateException("App type must be one of VNC, RDP, SPICE or Opaque")
        }
        return remoteConnection
    }
}