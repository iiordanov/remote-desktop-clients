package com.iiordanov.bVNC;

import android.util.Log;

import androidx.annotation.NonNull;

import java.net.InetAddress;
import java.net.URI;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Bounded host name resolution and the SPICE proxy selection built on top of it. Lookups are
 * raced against a deadline so a stalled DNS response can never block the caller: a proxy host
 * that resolves to a non-loopback address is kept, anything else falls back to the API host.
 */
public final class HostResolution {

    /** Maximum time in milliseconds to wait for a name lookup before giving up. */
    static final long DNS_TIMEOUT_MS = 3000L;

    private static final String TAG = "HostResolution";

    static final Lookup SYSTEM_LOOKUP = InetAddress::getByName;

    private static final ExecutorService EXECUTOR = Executors.newCachedThreadPool(runnable -> {
        Thread thread = new Thread(runnable, "HostResolution");
        thread.setDaemon(true);
        return thread;
    });

    /** Blocking name lookup, isolated so resolution can be exercised deterministically. */
    interface Lookup {
        /** @return the resolved address; never null — a host that cannot be resolved must throw. */
        InetAddress resolve(String host) throws Exception;
    }

    private HostResolution() {
    }

    /**
     * Chooses the proxy string to write into a SPICE connection file.
     *
     * @param proxy   proxy URL from the display data
     * @param apiHost API host to substitute when the proxy host is unusable
     * @return the proxy string to write
     */
    public static String resolveProxy(String proxy, String apiHost) {
        return resolveProxy(proxy, apiHost, SYSTEM_LOOKUP, DNS_TIMEOUT_MS);
    }

    static String resolveProxy(String proxy, String apiHost, Lookup lookup, long timeoutMs) {
        if (proxy == null || proxy.isEmpty()) {
            Log.i(TAG, "SPICE proxy: none in display data, keeping as-is");
            return "";
        }
        if (apiHost == null || apiHost.isEmpty()) {
            Log.w(TAG, "SPICE proxy: no API host available to substitute, keeping proxy as-is");
            return proxy;
        }
        String proxyHost = extractProxyHost(proxy);
        if (proxyHost == null) {
            Log.i(TAG, "SPICE proxy: no host parseable from '" + redactProxy(proxy) + "', keeping as-is");
            return proxy;
        }
        if (proxyHost.equalsIgnoreCase(apiHost)) {
            Log.i(TAG, "SPICE proxy: proxy host '" + proxyHost + "' equals API host, keeping as-is");
            return proxy;
        }
        Resolution resolution = boundedLookup(lookup, proxyHost, timeoutMs);
        String logPrefix = "SPICE proxy: proxy host '" + proxyHost + "' " + resolution + ", ";
        if (resolution.keep()) {
            Log.i(TAG, logPrefix + "keeping");
            return proxy;
        }
        String substituted = substituteProxyHost(proxy, proxyHost, apiHost);
        if (substituted.equals(proxy)) {
            Log.w(TAG, logPrefix + "but host not replaceable in proxy string, keeping as-is");
            return proxy;
        }
        Log.w(TAG, logPrefix + "substituting proxy host with API host '" + apiHost + "'");
        return substituted;
    }

    /** Outcome of a bounded lookup; kept when the host mapped to a non-loopback address. */
    private record Resolution(InetAddress address, boolean timedOut, long elapsedMs) {

        /** Captures the elapsed time at creation, measured from {@code startNanos}. */
        private static Resolution of(InetAddress address, boolean timedOut, long startNanos) {
            long elapsedMs = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - startNanos);
            return new Resolution(address, timedOut, elapsedMs);
        }

        private boolean keep() {
            return address != null && !address.isLoopbackAddress();
        }

        @NonNull
        @Override
        public String toString() {
            String outcome = timedOut ? "timed out"
                    : (address == null ? "could not be resolved" : "resolved to " + address.getHostAddress());
            return outcome + " in " + elapsedMs + "ms";
        }
    }

    /** Runs the lookup on a pool thread and gives up after the deadline, discarding late answers. */
    private static Resolution boundedLookup(Lookup lookup, String host, long timeoutMs) {
        long startNanos = System.nanoTime();
        Future<InetAddress> future = EXECUTOR.submit(() -> lookup.resolve(host));
        try {
            InetAddress address = future.get(timeoutMs, TimeUnit.MILLISECONDS);
            return Resolution.of(address, false, startNanos);
        } catch (TimeoutException e) {
            future.cancel(true);
            return Resolution.of(null, true, startNanos);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return Resolution.of(null, false, startNanos);
        } catch (ExecutionException e) {
            return Resolution.of(null, false, startNanos);
        }
    }

    private static String extractProxyHost(String proxy) {
        try {
            String host = new URI(proxy).getHost();
            return (host == null || host.isEmpty()) ? null : host;
        } catch (Exception e) {
            return null;
        }
    }

    private static String substituteProxyHost(String proxy, String proxyHost, String apiHost) {
        return Pattern.compile("//" + Pattern.quote(proxyHost)).matcher(proxy)
                .replaceFirst(Matcher.quoteReplacement("//" + apiHost));
    }

    /** Masks userinfo before '@' so proxy credentials never land in logcat exports. */
    static String redactProxy(String proxy) {
        int authority = proxy.indexOf("//");
        if (authority < 0) {
            return proxy;
        }
        authority += 2;
        int at = proxy.indexOf('@', authority);
        int path = proxy.indexOf('/', authority);
        if (at < 0 || (path >= 0 && path < at)) {
            return proxy;
        }
        return proxy.substring(0, authority) + "***" + proxy.substring(at);
    }
}
