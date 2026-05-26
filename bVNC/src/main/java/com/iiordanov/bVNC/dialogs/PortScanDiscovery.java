/**
 * Copyright (C) 2013- Iordan Iordanov
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

import android.util.Log;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Scans every up, non-loopback IPv4 interface's /24 subnet plus the loopback
 * address for open TCP ports and reports discovered servers as they are found.
 * Host addresses are built from raw 4-byte values via the {@link InetAddress}
 * APIs (no IPv4-text concatenation). One thread is spawned per host; all ports
 * for that host are checked sequentially so the total number of simultaneous
 * connections stays bounded. Reverse-DNS is not attempted — only mDNS-discovered
 * servers carry a real name; port-scan results are reported with their IP.
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
    /**
     * Lower bound on the network prefix length we will scan. An iface advertising
     * a /16 (65k hosts) or wider would blow up scan time; cap at /24 so we only
     * sweep the 254 hosts around the device's own address.
     */
    private static final int MIN_PREFIX_LENGTH = 24;

    public interface Listener {
        void onServerFound(NetworkDiscovery.DiscoveredServer server);

        void onProgressUpdate(int scanned, int total);

        void onScanComplete();
    }

    private ExecutorService executor;
    private final AtomicBoolean stopped = new AtomicBoolean(false);

    /**
     * Starts scanning the local subnet (if detectable) plus the loopback address
     * for the given ports. Results are delivered to {@code listener} from
     * background threads as soon as each open port is found. Call {@link #stop()}
     * to cancel an in-progress scan.
     */
    public void start(int[] ports, Listener listener) {
        stopped.set(false);

        List<InetAddress> hosts = collectScanTargets();

        executor = Executors.newFixedThreadPool(THREAD_POOL_SIZE);
        final int totalHosts = hosts.size();
        final AtomicInteger pending = new AtomicInteger(totalHosts);

        for (InetAddress addr : hosts) {
            executor.submit(() -> {
                if (!stopped.get()) {
                    for (int port : ports) {
                        if (stopped.get()) break;
                        if (isPortOpen(addr, port)) {
                            String ip = addr.getHostAddress();
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

    private boolean isPortOpen(InetAddress host, int port) {
        try (Socket socket = new Socket()) {
            socket.connect(new InetSocketAddress(host, port), SOCKET_TIMEOUT_MS);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Builds the list of IPv4 addresses to scan: every host in the /24 surrounding
     * each up, non-loopback, non-virtual interface's IPv4 address, plus the loopback
     * address. Deduplicated so multi-iface devices whose ifaces share a /24 (e.g.
     * Wi-Fi + USB tether to the same router) do not get scanned twice.
     */
    private List<InetAddress> collectScanTargets() {
        Set<InetAddress> targets = new LinkedHashSet<>();
        try {
            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            while (interfaces != null && interfaces.hasMoreElements()) {
                NetworkInterface ni = interfaces.nextElement();
                if (!ni.isUp() || ni.isLoopback() || ni.isVirtual()) continue;
                for (InterfaceAddress ia : ni.getInterfaceAddresses()) {
                    InetAddress addr = ia.getAddress();
                    if (!(addr instanceof Inet4Address) || addr.isLoopbackAddress()) continue;
                    appendHostsInSubnet(targets, (Inet4Address) addr, ia.getNetworkPrefixLength());
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Error enumerating interfaces: " + e.getMessage());
        }
        if (targets.isEmpty()) {
            Log.w(TAG, "No usable IPv4 interface — scanning loopback only");
        }
        // Picks up servers running locally on the device itself (Termux, KVM
        // port-forwards, dev work) regardless of network state.
        targets.add(InetAddress.getLoopbackAddress());
        return new ArrayList<>(targets);
    }

    /**
     * Appends every assignable host address in the /max(prefixLen, 24) subnet
     * around {@code ifaceAddr} to {@code out}, skipping the network and broadcast
     * addresses. Operates on raw 4-byte address values via
     * {@link InetAddress#getByAddress(byte[])} so no IPv4-text parsing is involved.
     */
    private void appendHostsInSubnet(Set<InetAddress> targets, Inet4Address ifaceAddr, int prefixLen) {
        int hostBits = 32 - Math.max(prefixLen, MIN_PREFIX_LENGTH);
        if (hostBits <= 0) return; // /32 — nothing to scan around a single host.
        int subnetMask = 0xFFFFFFFF << hostBits;
        int networkAddress = ByteBuffer.wrap(ifaceAddr.getAddress()).getInt() & subnetMask;
        int assignableHostCount = (1 << hostBits) - 2; // exclude network + broadcast
        for (int hostIndex = 1; hostIndex <= assignableHostCount; hostIndex++) {
            byte[] addrBytes = ByteBuffer.allocate(4).putInt(networkAddress + hostIndex).array();
            try {
                targets.add(InetAddress.getByAddress(addrBytes));
            } catch (UnknownHostException ignored) { /* 4-byte arrays never throw */ }
        }
    }
}
