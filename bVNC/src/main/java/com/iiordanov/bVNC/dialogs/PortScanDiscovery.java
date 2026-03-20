/**
 * Copyright (C) 2013- Iordan Iordanov
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

package com.iiordanov.bVNC.dialogs;

import android.util.Log;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.util.Enumeration;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Scans the local /24 subnet for open TCP ports and reports discovered servers as they are found.
 * One thread is spawned per host; all ports for that host are checked sequentially so that the
 * total number of simultaneous connections stays bounded.
 */
public class PortScanDiscovery {
    private static final String TAG = "PortScanDiscovery";

    /**
     * Thread pool capped at 100 to avoid triggering SYN-flood protection on home routers,
     * which silently drops packets when too many half-open connections arrive from one source.
     */
    private static final int THREAD_POOL_SIZE = 100;
    /**
     * Milliseconds to wait for a TCP connection before giving up on a port.
     * 500ms gives headroom for two beacon intervals of 802.11 PSM buffering (~200ms each)
     * and for router ARP resolution on the first contact with a host.
     */
    private static final int SOCKET_TIMEOUT_MS = 500;

    public interface Listener {
        void onServerFound(NetworkDiscovery.DiscoveredServer server);
        void onProgressUpdate(int scanned, int total);
        void onScanComplete();
    }

    private ExecutorService executor;
    private final AtomicBoolean stopped = new AtomicBoolean(false);

    /**
     * Starts scanning the local subnet for the given ports. Results are delivered to
     * {@code listener} from background threads as soon as each open port is found.
     * Call {@link #stop()} to cancel an in-progress scan.
     */
    public void start(int[] ports, Listener listener) {
        stopped.set(false);

        String subnet = getLocalSubnet();
        if (subnet == null) {
            Log.w(TAG, "Cannot determine local subnet — skipping port scan");
            listener.onScanComplete();
            return;
        }

        executor = Executors.newFixedThreadPool(THREAD_POOL_SIZE);
        final int totalHosts = 254;
        final AtomicInteger pending = new AtomicInteger(totalHosts);

        for (int h = 1; h <= totalHosts; h++) {
            final String ip = subnet + "." + h;
            executor.submit(() -> {
                if (!stopped.get()) {
                    for (int port : ports) {
                        if (stopped.get()) break;
                        if (isPortOpen(ip, port)) {
                            listener.onServerFound(
                                    new NetworkDiscovery.DiscoveredServer(ip, ip, port));
                        }
                    }
                }
                int remaining = pending.decrementAndGet();
                if (!stopped.get()) {
                    listener.onProgressUpdate(totalHosts - remaining, totalHosts);
                    if (remaining == 0) {
                        listener.onScanComplete();
                    }
                }
            });
        }

        // No more tasks will be submitted; threads will terminate after finishing.
        executor.shutdown();
    }

    /** Cancels the scan. Already-reported servers remain in the adapter. */
    public void stop() {
        stopped.set(true);
        if (executor != null) {
            executor.shutdownNow();
        }
    }

    private boolean isPortOpen(String host, int port) {
        try (Socket socket = new Socket()) {
            socket.connect(new InetSocketAddress(host, port), SOCKET_TIMEOUT_MS);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Returns the /24 subnet prefix (e.g. {@code "192.168.1"}) of the first non-loopback
     * IPv4 address found on an active network interface, or {@code null} if none is found.
     */
    private String getLocalSubnet() {
        try {
            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            if (interfaces == null) return null;
            while (interfaces.hasMoreElements()) {
                NetworkInterface ni = interfaces.nextElement();
                if (!ni.isUp() || ni.isLoopback() || ni.isVirtual()) continue;
                Enumeration<InetAddress> addrs = ni.getInetAddresses();
                while (addrs.hasMoreElements()) {
                    InetAddress addr = addrs.nextElement();
                    if (addr instanceof Inet4Address && !addr.isLoopbackAddress()) {
                        String ip = addr.getHostAddress();
                        if (ip == null) continue;
                        int lastDot = ip.lastIndexOf('.');
                        if (lastDot > 0) return ip.substring(0, lastDot);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error detecting local subnet: " + e.getMessage());
        }
        return null;
    }
}
