//
//  ToggleButton.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-03-19.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

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
        down = !down
        print("Toggled my xksysym: \(toSend!), down: \(down)")
        sendUniDirectionalKeyEventWithKeySym(toSend!, down)
        UserInterface {
            if self.down {
                self.backgroundColor = self.originalBackground?.withAlphaComponent(1)
            } else {
                self.backgroundColor = self.originalBackground
            }
            self.setNeedsDisplay()
        }
    }

    @objc func sendText() {
        print("Sending my xksysym: \(toSend!), up and then down.")
        sendKeyEventWithKeySym(toSend!)
        self.stateKeeper?.toggleModifiersIfDown()
        //AudioServicesPlayAlertSound(kSystemSoundID_Vibrate);
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
