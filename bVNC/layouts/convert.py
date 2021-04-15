# Copyright (C) 2013- Iordan Iordanov
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
# USA.
#

import sys
import os
import logging
import collections
import re

root = logging.getLogger()
root.setLevel(logging.DEBUG)

handler = logging.StreamHandler(sys.stdout)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
handler.setFormatter(formatter)
root.addHandler(handler)

layoutLocation = "/usr/share/qemu/keymaps/"
targetLocation = "../src/main/assets/layouts/"
layouts = {"en-us" : "English (US)", "en-gb" : "English (UK)", "de" : "German (Germany)", "fr" : "French (France)",
           "es" : "Spanish (Spain, Traditional Sort)", "sv" : "Swedish (Sweden)", "pl" : "Polish (Programmers)",
           "it" : "Italian (Italy)", "hu" : "Hungarian (Hungary)", "da" : "Danish", "pt" : "Portuguese (Portugal)",
           "pt-br" : "Portuguese (Brazil)", "de-ch" : "German (Switzerland)", "fr-ch" : "French (Switzerland)",
           "sl": "Slovenian" }
commonKeyCodeFile = "common_keycodes.in"
nameToUnicodeFile = "name_to_unicode.in"

unicodeMask = 0x100000
shiftMask = 0x10000
altGrMask = 0x20000

def loadNameToUnicode (nameToUnicodeFile):
    f = open(nameToUnicodeFile)
    nameToUnicode = {}
    for l in f.readlines():
        l = l.split()
        if l != None:
            nameToUnicode[l[0]] = l[1]

    return nameToUnicode

def loadCommonKeyCodes (commonFile, keyMap):
    f = open(commonFile)
    for l in f.readlines():
        l = l.split()
        if l != None:
            keyMap[int(l[1])] = [int(l[2])]

    return keyMap

def loadKeyMap (nameToUnicode, keyMapFile, keyMap):
    try:
        f = open(keyMapFile)
    except:
        logging.warning(f"Unable to open {keyMapFile} file, skipping")
        return
    lines = f.readlines()
    deadKeys = {}
    for l in lines:
        l = l.split()
        if len(l) > 1 and l[0] != "#" and l[0] != "include":
            name = str(l[0])

            scanCodes = list(l[1:])

            # Convert first scancode to an integer (this is the actual scancode)
            scanCodes[0] = int(scanCodes[0], 16)

            # Go through the scan codes and apply shift, altGr mask and skip if numlock is found
            # We iterate on a copy of the list of scanCodes in order to be able to safely modify
            # the original list.
            skip = False
            addUpper = False
            for s in list(scanCodes):
                if s == "shift":
                    scanCodes.remove(s)
                    scanCodes[0] |= shiftMask
                elif s == "altgr":
                    scanCodes.remove(s)
                    scanCodes[0] |= altGrMask
                elif s == "localstate":
                    scanCodes.remove(s)
                elif s == "numlock":
                    skip = True
                elif s == "addupper":
                    scanCodes.remove(s)
                    addUpper = True
                elif s == "inhibit":
                    scanCodes.remove(s)

            # If we're supposed to skip this entry, continue to next iteration.
            if skip:
                continue

            if addUpper:
                upperCaseScanCodes = list(scanCodes)
                upperCaseScanCodes[0] |= shiftMask
                keyMap[(int(nameToUnicode[name], 16) - 0x20) | unicodeMask] = upperCaseScanCodes
            try:
                keyMap[int(nameToUnicode[name], 16) | unicodeMask] = scanCodes
            except KeyError as e:
                if re.match('U[0-9A-Fa-f]{4}', name):
                    logging.warning(f"Name {e} in qemu keymap looks like a unicode character, using its value to add to keymap")
                    print(int(name[1:], 16) | unicodeMask, scanCodes)
                    keyMap[int(name[1:], 16) | unicodeMask] = scanCodes
                else:
                    logging.error(f"Could not find unicode value for: {e}")
                pass

            # Detect and store dead keys
            if name.startswith("dead_"):
                deadKeys[name[5:]] = scanCodes

    # Get a list of all name-unicode pairs that are made with a dead key
    # present in this layout.
    for name, unicode in nameToUnicode.items():
        add = False
        scodes = []
        for key, scanCode in deadKeys.items():
            if str(name).__contains__(str(key)):
                scodes += scanCode
                name = str(name).replace(str(key), "")
                if name.endswith("_"):
                    name = name[:name.__len__()-1]
                    add = True
                elif name.__len__() == 1:
                    add = True

        # If we've discovered a unicode key with an accent (dead-key) for which this layout has
        # all the necessary dead keys, make a new entry in keyMap with the list of scanCodes
        # containing each dead-key's scan code and ending on the scan code of the plain character.
        if add:
            unicode = int(unicode, 16) | unicodeMask
            plainLetterUnicode = int(nameToUnicode[name], 16) | unicodeMask
            try:
                scodes += keyMap[plainLetterUnicode]
                # Do not replace existing mappings with deadkey sequences as that would override
                # key mappings that are directly accessible from a button on the keyboard. Examples
                # are a, o, u umlaut in German
                if not keyMap.__contains__(unicode):
                    keyMap[unicode] = scodes
            except KeyError as e:
                #print ("Could not find: " + name)
                pass

def generateLayoutMap():
    map = {}
    for layout in os.listdir(layoutLocation):
        f = open(os.path.join(layoutLocation, layout))
        name = "NONE"
        lines = f.readlines()
        for l in lines:
            if l.startswith("# name:"):
                try:
                    name = l.split()[2].replace('"', '')
                    map[layout] = name
                except:
                    logging.error(f"Error parsing the name of layout {l}. Manually add it to layouts map.")
                    pass
                break
    logging.info(f"Automatically generated map: {map}")
    return map

if __name__ == "__main__":
    nameToUnicode = loadNameToUnicode(nameToUnicodeFile)
    # Generate keymaps automatically from qemu keymaps directory
    auto_layouts = generateLayoutMap()
    # Update with manually crafted names.
    auto_layouts.update(layouts)

    for k, v in auto_layouts.items():
        keyMap = {}
        loadCommonKeyCodes (commonKeyCodeFile, keyMap)
        loadKeyMap(nameToUnicode, layoutLocation + "common", keyMap)
        loadKeyMap(nameToUnicode, layoutLocation + str(k), keyMap)

        f = open(targetLocation + str(v), "w")
        for u, s in sorted(keyMap.items()):
            #print(u, s)
            f.write(str(u) + " " + " ".join(map(str, s)) + "\n")
        f.close()
