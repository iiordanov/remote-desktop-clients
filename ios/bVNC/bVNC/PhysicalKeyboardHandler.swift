/**
 * Copyright (C) 2021- Morpheusly Inc. All rights reserved.
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

import Foundation
import UIKit

class PhysicalKeyboardHandler {
    var specialKeyToXKeySymMap: [String: Int32]
    var keyCodeWithShiftModifierToString: [Int: String]
    
    init() {
        if #available(iOS 13.4, *) {
            self.specialKeyToXKeySymMap = [
                UIKeyCommand.f1: XK_F1,
                UIKeyCommand.f2: XK_F2,
                UIKeyCommand.f3: XK_F3,
                UIKeyCommand.f4: XK_F4,
                UIKeyCommand.f5: XK_F5,
                UIKeyCommand.f6: XK_F6,
                UIKeyCommand.f7: XK_F7,
                UIKeyCommand.f8: XK_F8,
                UIKeyCommand.f9: XK_F9,
                UIKeyCommand.f10: XK_F10,
                UIKeyCommand.f11: XK_F11,
                UIKeyCommand.f12: XK_F12,
                UIKeyCommand.inputEscape: XK_Escape,
                UIKeyCommand.inputHome: XK_Home,
                UIKeyCommand.inputEnd: XK_End,
                UIKeyCommand.inputPageUp: XK_Page_Up,
                UIKeyCommand.inputPageDown: XK_Page_Down,
                UIKeyCommand.inputUpArrow: XK_Up,
                UIKeyCommand.inputDownArrow: XK_Down,
                UIKeyCommand.inputLeftArrow: XK_Left,
                UIKeyCommand.inputRightArrow: XK_Right,
            ]

            self.keyCodeWithShiftModifierToString = [
                UIKeyboardHIDUsage.keyboardEqualSign.rawValue: "+",
                UIKeyboardHIDUsage.keyboardHyphen.rawValue: "_",
                UIKeyboardHIDUsage.keyboard0.rawValue: ")",
                UIKeyboardHIDUsage.keyboard9.rawValue: "(",
                UIKeyboardHIDUsage.keyboard8.rawValue: "*",
                UIKeyboardHIDUsage.keyboard7.rawValue: "&",
                UIKeyboardHIDUsage.keyboard6.rawValue: "^",
                UIKeyboardHIDUsage.keyboard5.rawValue: "%",
                UIKeyboardHIDUsage.keyboard4.rawValue: "$",
                UIKeyboardHIDUsage.keyboard3.rawValue: "#",
                UIKeyboardHIDUsage.keyboard2.rawValue: "@",
                UIKeyboardHIDUsage.keyboard1.rawValue: "!",
                UIKeyboardHIDUsage.keyboardGraveAccentAndTilde.rawValue: "~",
                UIKeyboardHIDUsage.keyboardCloseBracket.rawValue: "}",
                UIKeyboardHIDUsage.keyboardOpenBracket.rawValue: "{",
                UIKeyboardHIDUsage.keyboardQuote.rawValue: "\"",
                UIKeyboardHIDUsage.keyboardSemicolon.rawValue: ":",
                UIKeyboardHIDUsage.keyboardSlash.rawValue: "?",
                UIKeyboardHIDUsage.keyboardPeriod.rawValue: ">",
                UIKeyboardHIDUsage.keyboardComma.rawValue: "<",
                UIKeyboardHIDUsage.keyboardBackslash.rawValue: "|"
            ]
            
        } else {
            self.specialKeyToXKeySymMap = [:]
            self.keyCodeWithShiftModifierToString = [:]
        }
    }
}
