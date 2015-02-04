/* usbredirfilter.h usb redirection filter header

   Copyright 2012 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __USBREDIRFILTER_H
#define __USBREDIRFILTER_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usbredirfilter_rule {
    int device_class;       /* 0-255, -1 to match any class */
    int vendor_id;          /* 0-65535, -1 to match any id */
    int product_id;         /* 0-65535, -1 to match any id */
    int device_version_bcd; /* 0-255, -1 to match any version */
    int allow;              /* 0: deny redir for this device, non 0: allow */
};

/* Read a filter string and parse it into an array of usbredirfilter_rule-s.

   Where each rule has the form of:
   <class>,<vendor>,<product>,<version>,<allow>
   Assuming "," as the specified token_sep character.

   And the rules are themselves are separated by the rule_sep character, ie:
   <rule1>|<rule2>|<rule3>

   Assuming "|" as the rule_sep character. Note that with the seperator used
   in this example the format matches the format as written by the RHEV-M USB
   filter editor tool.

   Note that the seperators must be single character strings!

   On success the rules get returned in rules_ret and rules_count_ret, the
   returned rules array should be freed with free() when the caller is done
   with it.

   Return value: 0 on success, -ENOMEM when allocating the rules array fails,
       or -EINVAL when there is on parsing error.
*/
int usbredirfilter_string_to_rules(
    const char *filter_str, const char *token_sep, const char *rule_sep,
    struct usbredirfilter_rule **rules_ret, int *rules_count_ret);

/* Convert a set of rules back to a string suitable for passing to
   usbredirfilter_string_to_rules(); The returned string must be free()-ed
   by the caller when it is done with it.

   Return value: The string on sucess, or NULL if the rules fail verification,
      or when allocating the string fails.
*/
char *usbredirfilter_rules_to_string(const struct usbredirfilter_rule *rules,
    int rules_count, const char *token_sep, const char *rule_sep);

/* Check if redirection of a device with the passed in device info is allowed
   by the passed set of filter rules.

   Since a device has class info at both the device level and the interface
   level, this function does multiple passes.

   First the rules are checked one by one against the given device info using
   the device class info, if a matching rule is found, the result of the check
   is that of that rule.

   Then the same is done for each interface the device has, substituting the
   device class info with the class info from the interfaces.

   Note that under certain circumstances some passes are skipped:
   - For devices with a device class of 0x00 or 0xef, the pass which checks the
     device class is skipped.
   - If the usbredirfilter_fl_dont_skip_non_boot_hid flag is not passed then
     for devices with more then 1 interface and an interface with an interface
     class of 0x03, an interface subclass of 0x00 and an interface protocol
     of 0x00. the check is skipped for that interface. This allows to skip ie
     checking the interface for volume buttons one some usbaudio class devices.

   If the result of all (not skipped) passes is allow, then 0 will be returned,
   which indicates that redirection should be allowed.

   If the result of a matching rule is deny, then processing stops and
   -EPERM will be returned.

   If a given pass does not match any rules, then processing stops and
   -ENOENT will be returned. This behavior can be changed with the
   usbredirfilter_fl_default_allow flag, if this flag is set the result of a
   pass with no matching rules will be allow.

   Return value:
    0        Redirection is allowed
    -EINVAL  Invalid parameters
    -EPERM   Redirection is blocked by the filter rules
    -ENOENT  None of the rules matched the device (during one of the passes)
*/
enum {
    usbredirfilter_fl_default_allow = 0x01,
    usbredirfilter_fl_dont_skip_non_boot_hid = 0x02,
};
int usbredirfilter_check(
    const struct usbredirfilter_rule *rules, int rules_count,
    uint8_t device_class, uint8_t device_subclass, uint8_t device_protocol,
    uint8_t *interface_class, uint8_t *interface_subclass,
    uint8_t *interface_protocol, int interface_count,
    uint16_t vendor_id, uint16_t product_id, uint16_t device_version_bcd,
    int flags);

/* Sanity check the passed in rules

   Return value: 0 on success, -EINVAL when some values are out of bound. */
int usbredirfilter_verify(
    const struct usbredirfilter_rule *rules, int rules_count);

/* Print the passed in rules to FILE out in human readable format */
void usbredirfilter_print(
    const struct usbredirfilter_rule *rules, int rules_count, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
