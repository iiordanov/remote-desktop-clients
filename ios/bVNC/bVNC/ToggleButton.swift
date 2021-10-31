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
import AudioToolbox

class ToggleButton: UIButton {
    var originalBackground: UIColor?
    var toSend: Int32?
    var down: Bool = false
    var stateKeeper: StateKeeper?
    
    init (frame: CGRect, title: String, background: UIColor, stateKeeper: StateKeeper, toSend: Int32, toggle: Bool) {
        super.init(frame: frame)
        self.stateKeeper = stateKeeper
        self.setTitle(title, for: [])
        self.originalBackground = background
        self.backgroundColor = background
        if toggle && toSend >= 0 {
            self.addTarget(self, action: #selector(self.sendToggleText), for: .touchDown)
        } else if (toSend >= 0) {
            self.addTarget(self, action: #selector(self.sendText), for: .touchDown)
        }
        self.toSend = toSend
    }
    
    @objc func sendToggleText() {
        guard let currentInstance = self.stateKeeper?.getCurrentInstance() else {
            log_callback_str(message: "No currently connected instance, ignoring \(#function)")
            return
        }
        AudioServicesPlaySystemSound(1100);
        down = !down
        log_callback_str(message: "ToggleButton: Toggled my xksysym: \(toSend!), down: \(down)")
        sendUniDirectionalKeyEventWithKeySym(currentInstance, toSend!, down)
        self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
        UserInterface {
            if self.down {
                self.backgroundColor = self.originalBackground?.withAlphaComponent(0.75)
            } else {
                self.backgroundColor = self.originalBackground?.withAlphaComponent(0.5)
            }
            self.setNeedsDisplay()
        }
    }

    @objc func sendText() {
        guard let currentInstance = self.stateKeeper?.getCurrentInstance() else {
            log_callback_str(message: "No currently connected instance, ignoring \(#function)")
            return
        }
        
        AudioServicesPlaySystemSound(1100);
        log_callback_str(message: "ToggleButton: Sending my xksysym: \(toSend!), up and then down.")
        sendKeyEventWithKeySym(currentInstance, toSend!)
        self.stateKeeper?.toggleModifiersIfDown()
        self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
    }

    @objc func sendUpIfToggled() {
        if down {
            sendToggleText()
        }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
    }
}
