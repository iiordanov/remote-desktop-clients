/**
 * Copyright (C) 2023 Iordan Iordanov
 * <p>
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

package com.iiordanov.util


object NetworkUtils {
    /**
     * Returns true if the address string is an IPv6 address (literal or bracket-enclosed).
     * IPv6 addresses always contain more than one colon; an IPV4 host:port string has exactly one.
     */
    fun isValidIpv6Address(address: String?): Boolean {
        if (address == null) return false
        return address.count { it == ':' } > 1
    }
}
