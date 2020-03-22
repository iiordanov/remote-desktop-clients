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
    var down: Bool = true
    
    
    init (frame: CGRect, title: String, background: UIColor, stateKeeper: StateKeeper, toSend: Int32, toggle: Bool) {
        super.init(frame: frame)
        self.setTitle(title, for: [])
        self.originalBackground = background
        self.backgroundColor = background
        if toggle {
            self.addTarget(self, action: #selector(self.sendToggleText), for: .touchDown)
        } else {
            self.addTarget(self, action: #selector(self.sendText), for: .touchDown)
        }
        self.toSend = toSend
    }
    
    @objc func sendToggleText() {
        print("Toggling my xksysym: \(toSend!), down: \(down)")
        sendUniDirectionalKeyEventWithKeySym(toSend!, down)
        if down {
            self.backgroundColor = originalBackground?.withAlphaComponent(1)
        } else {
            self.backgroundColor = originalBackground
        }
        down = !down
    }

    @objc func sendText() {
        print("Sending my xksysym: \(toSend!), up and then down.")
        sendKeyEventWithKeySym(toSend!)
        //AudioServicesPlayAlertSound(kSystemSoundID_Vibrate);
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
    }
}
