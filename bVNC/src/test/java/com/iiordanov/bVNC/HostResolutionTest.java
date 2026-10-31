package com.iiordanov.bVNC;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

import java.net.InetAddress;
import java.net.UnknownHostException;

public class HostResolutionTest {

    @Test
    public void resolvesIpLiteralsWithoutNetwork() {
        String kept = HostResolution.resolveProxy("http://93.184.216.34:3128", "api.example.com",
                HostResolution.SYSTEM_LOOKUP, HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://93.184.216.34:3128", kept);
        String substituted = HostResolution.resolveProxy("http://127.0.0.1:3128", "api.example.com",
                HostResolution.SYSTEM_LOOKUP, HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://api.example.com:3128", substituted);
    }

    @Test
    public void timesOutWhenLookupOutlivesTheDeadline() {
        HostResolution.Lookup hangingLookup = host -> {
            Thread.sleep(5000);
            throw new UnknownHostException(host);
        };
        String resolved = HostResolution.resolveProxy("http://proxy.invalid:3128", "api.example.com",
                hangingLookup, 20);
        assertEquals("http://api.example.com:3128", resolved);
    }

    @Test
    public void resolveProxyKeepsMissingProxyAsEmpty() {
        String kept = HostResolution.resolveProxy(null, "api.example.com", host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals("", kept);
        kept = HostResolution.resolveProxy("", "api.example.com", host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals("", kept);
    }

    @Test
    public void resolveProxyKeepsProxyWithoutApiHost() {
        String proxy = "http://proxy.invalid:3128";
        String kept = HostResolution.resolveProxy(proxy, null, host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
        kept = HostResolution.resolveProxy(proxy, "", host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
    }

    @Test
    public void resolveProxyKeepsProxyWhenProxyHostEqualsApiHost() {
        String proxy = "http://API.Example.COM:3128";
        String kept = HostResolution.resolveProxy(proxy, "api.example.com", host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
    }

    @Test
    public void resolveProxyKeepsProxyWhenProxyHostUnparseable() {
        String proxy = "not a uri";
        String kept = HostResolution.resolveProxy(proxy, "api.example.com", host -> null, HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
    }

    @Test
    public void resolveProxyKeepsProxyWhenResolvedAddressIsNonLocalhost() {
        String proxy = "http://proxy.example.com:3128";
        String kept = HostResolution.resolveProxy(proxy, "api.example.com",
                host -> InetAddress.getByName("93.184.216.34"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
    }

    @Test
    public void resolveProxySubstitutesProxyHostWhenResolvedAddressIsLoopback() {
        String resolved = HostResolution.resolveProxy("http://proxy.invalid:3128", "api.example.com",
                host -> InetAddress.getByName("127.0.0.1"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://api.example.com:3128", resolved);
    }

    @Test
    public void resolveProxySubstitutesProxyHostWhenResolvedAddressIsIpv6Loopback() {
        String resolved = HostResolution.resolveProxy("http://proxy.invalid:3128", "api.example.com",
                host -> InetAddress.getByName("::1"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://api.example.com:3128", resolved);
    }

    @Test
    public void resolveProxySubstitutesProxyHostWhenLookupFails() {
        HostResolution.Lookup failingLookup = host -> {
            throw new UnknownHostException(host);
        };
        String resolved = HostResolution.resolveProxy("http://proxy.invalid:3128", "api.example.com",
                failingLookup, HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://api.example.com:3128", resolved);
    }

    @Test
    public void resolveProxySubstitutionKeepsPortAndPath() {
        String resolved = HostResolution.resolveProxy("http://proxy.invalid:3128/tunnel:9090", "api.example.com",
                host -> InetAddress.getByName("127.0.0.1"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals("http://api.example.com:3128/tunnel:9090", resolved);
    }

    @Test
    public void resolveProxySubstitutionWorksWithoutPort() {
        String resolved = HostResolution.resolveProxy("socks5://proxy.invalid", "api.example.com",
                host -> InetAddress.getByName("127.0.0.1"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals("socks5://api.example.com", resolved);
    }

    @Test
    public void resolveProxyKeepsProxyWhenHostNotDirectlyAfterSlashes() {
        String proxy = "http://user@proxy.invalid:3128";
        String kept = HostResolution.resolveProxy(proxy, "api.example.com",
                host -> InetAddress.getByName("127.0.0.1"), HostResolution.DNS_TIMEOUT_MS);
        assertEquals(proxy, kept);
    }

    @Test
    public void redactProxyMasksCredentialsOnly() {
        assertEquals("http://***@proxy.invalid:3128",
                HostResolution.redactProxy("http://user:pw@proxy.invalid:3128"));
        assertEquals("http://proxy.invalid:3128/a@b",
                HostResolution.redactProxy("http://proxy.invalid:3128/a@b"));
    }
}
