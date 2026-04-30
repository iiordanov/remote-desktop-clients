/**
 * Copyright (C) 2026- Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * <p>
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * <p>
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC.dialogs;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;

import com.iiordanov.bVNC.Constants;
import com.iiordanov.bVNC.Utils;

import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Discovers remote desktop servers on the local network using Android's NSD (mDNS/DNS-SD) API.
 * Queues resolve requests since NsdManager only handles one resolution at a time.
 */
public class NetworkDiscovery {
    public static final String SERVICE_TYPE_VNC = "_rfb._tcp.";
    public static final String SERVICE_TYPE_RDP = "_rdp._tcp.";
    public static final String SERVICE_TYPE_SPICE = "_spice._tcp.";
    public static final String SERVICE_TYPE_WEB = "_http._tcp.";
    private static final String TAG = "NetworkDiscovery";
    private final NsdManager nsdManager;
    private final String serviceType;
    private final Listener listener;
    private final Queue<NsdServiceInfo> resolveQueue = new ArrayDeque<>();
    private boolean resolving = false;
    private boolean started = false;
    private NsdManager.DiscoveryListener discoveryListener;

    public NetworkDiscovery(Context context, String serviceType, Listener listener) {
        this.nsdManager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
        this.serviceType = serviceType;
        this.listener = listener;
    }

    /**
     * Returns the mDNS service type for the current app, or null if mDNS discovery is not used.
     * Only VNC uses mDNS; other protocols rely on port scanning.
     */
    public static String serviceTypeForApp(Context context) {
        if (Utils.isVnc(context)) return SERVICE_TYPE_VNC;
        if (Utils.isRdp(context)) return SERVICE_TYPE_RDP;
        if (Utils.isSpice(context)) return SERVICE_TYPE_SPICE;
        if (Utils.isOpaque(context)) return SERVICE_TYPE_WEB;
        return null;
    }

    /**
     * Returns the TCP ports to scan for servers on the local network for the current app.
     */
    public static int[] portsForApp(Context context) {
        if (Utils.isVnc(context)) return Constants.PORT_SCAN_VNC;
        if (Utils.isRdp(context)) return Constants.PORT_SCAN_RDP;
        if (Utils.isSpice(context)) return Constants.PORT_SCAN_SPICE;
        if (Utils.isOpaque(context)) return Constants.PORT_SCAN_OPAQUE;
        return Constants.PORT_SCAN_VNC;
    }

    public void start() {
        if (started) {
            return;
        }
        started = true;

        discoveryListener = new NsdManager.DiscoveryListener() {
            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                Log.e(TAG, "Discovery start failed: " + errorCode);
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                Log.e(TAG, "Discovery stop failed: " + errorCode);
            }

            @Override
            public void onDiscoveryStarted(String serviceType) {
                Log.d(TAG, "Discovery started for " + serviceType);
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                Log.d(TAG, "Discovery stopped for " + serviceType);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service found: " + serviceInfo.getServiceName());
                enqueueResolve(serviceInfo);
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service lost: " + serviceInfo.getServiceName());
            }
        };

        nsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, discoveryListener);
    }

    public void stop() {
        if (!started) return;
        started = false;
        try {
            nsdManager.stopServiceDiscovery(discoveryListener);
        } catch (Exception e) {
            Log.w(TAG, "Error stopping discovery: " + e.getMessage());
        }
    }

    private synchronized void enqueueResolve(NsdServiceInfo serviceInfo) {
        if (resolving) {
            resolveQueue.add(serviceInfo);
        } else {
            resolveNext(serviceInfo);
        }
    }

    private synchronized void resolveNext(NsdServiceInfo serviceInfo) {
        resolving = true;
        nsdManager.resolveService(serviceInfo, new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
                Log.w(TAG, "Resolve failed for " + serviceInfo.getServiceName() + ": " + errorCode);
                resolveFromQueue();
            }

            @Override
            public void onServiceResolved(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Resolved: " + serviceInfo.getServiceName()
                        + " @ " + serviceInfo.getHost().getHostAddress()
                        + ":" + serviceInfo.getPort());
                listener.onServerFound(new DiscoveredServer(
                        serviceInfo.getServiceName(),
                        serviceInfo.getHost().getHostAddress(),
                        serviceInfo.getPort()));
                resolveFromQueue();
            }
        });
    }

    private synchronized void resolveFromQueue() {
        NsdServiceInfo next = resolveQueue.poll();
        if (next != null) {
            resolveNext(next);
        } else {
            resolving = false;
        }
    }

    public interface Listener {
        void onServerFound(DiscoveredServer server);
    }

    public record DiscoveredServer(String name, String host, int port) {
    }
}
