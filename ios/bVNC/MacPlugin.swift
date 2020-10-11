//
//  MacPlugin.swift
//  AppKitBundle
//
//  Created by iordan iordanov on 2020-10-11.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import AppKit

class MacPlugin: NSView, Plugin {

    /*
    required override init(coder: NSCoder) {
        super.init(coder: coder)!
    }
    
    required override init() {
    }
     */
    
    override func viewDidUnhide() {
        print("SOMETHING5")
    }

    func sayHello() {        
        print("SOMETHING1")

        let alert = NSAlert()
        alert.alertStyle = .informational
        alert.messageText = "Hello from AppKit!"
        alert.informativeText = "It Works!"
        alert.addButton(withTitle: "OK")
        //alert.runModal()
    }
    
    override func keyDown(with: NSEvent) {
        print("SOMETHING5")
    }
    
    override func becomeFirstResponder() -> Bool {
        print("SOMETHING4")
        return super.becomeFirstResponder()
    }
    
    override var acceptsFirstResponder: Bool { return true }
}
