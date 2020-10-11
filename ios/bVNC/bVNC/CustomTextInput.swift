//
//  CustomTextInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

/*
extension String {

  func toPointer() -> UnsafeMutablePointer<UInt8>? {
    guard let data = self.data(using: String.Encoding.utf8) else { return nil }

    let buffer = UnsafeMutablePointer<UInt8>.allocate(capacity: data.count)
    let stream = OutputStream(toBuffer: buffer, capacity: data.count)

    stream.open()
    
    data.withUnsafeBytes({ (p: UnsafePointer<UInt8>) -> Void in
      stream.write(p, maxLength: data.count)
    })
    stream.close()

    return UnsafeMutablePointer<UInt8>(buffer)
  }
}*/

class CustomTextInput: UIButton, UIKeyInput {
    var plugin: Plugin?

    public var hasText: Bool { return false }
    var stateKeeper: StateKeeper?
    
    init(stateKeeper: StateKeeper) {
        super.init(frame: CGRect())
        self.stateKeeper = stateKeeper;
        if stateKeeper.macOs {
            //self.resignFirstResponder()
            loadPlugin()
        }
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    public func insertText(_ text: String){
        //log_callback_str(message: "Sending: " + text + ", number of characters: " + String(text.count))
        for char in text.unicodeScalars {
            Background {
                if !sendKeyEventInt(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, Int32(String(char.value))!, true) {
                    sendKeyEvent(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, String(char), true)
                }
                if !sendKeyEventInt(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, Int32(String(char.value))!, false) {
                    sendKeyEvent(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, String(char), false)
                }
                self.stateKeeper?.toggleModifiersIfDown()
            }
            self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
        }
    }
    
    public func deleteBackward(){
        Background {
            sendKeyEventWithKeySym(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, 0xff08, true);
            sendKeyEventWithKeySym(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, 0xff08, false);
            self.stateKeeper?.toggleModifiersIfDown()
        }
        self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
    }
    
    @objc func toggleFirstResponder() -> Bool {
        if (self.isFirstResponder) {
            log_callback_str(message: "Keyboard should be showing already, hiding it.")
            return hideKeyboard()
        } else {
            return showKeyboard()
        }
    }

    @objc func hideKeyboard() -> Bool {
        log_callback_str(message: "Hiding keyboard.")
        becomeFirstResponder()
        return resignFirstResponder()
    }

    @objc func showKeyboardFunction() -> Bool {
        log_callback_str(message: "showKeyboardFunction called")
        resignFirstResponder()
        return becomeFirstResponder()
    }

    
    @objc func showKeyboard() -> Bool {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            let _ = self.showKeyboardFunction()
        }
        log_callback_str(message: "Showing keyboard with delay")
        return true

    }
    
    override var canBecomeFirstResponder: Bool {
        return true
        //return stateKeeper?.macOs != true
    }
    
    
    override func becomeFirstResponder() -> Bool {
        if stateKeeper?.macOs == true {
            return plugin?.becomeFirstResponder() ?? false
        } else {
            return super.becomeFirstResponder()
        }
    }
    
    private func loadPlugin() {
        print("SOMETHING0")
        /// 1. Form the plugin's bundle URL
        let bundleFileName = "AppKitBundle.bundle"
        guard let bundleURL = Bundle.main.builtInPlugInsURL?
                                    .appendingPathComponent(bundleFileName) else { return }

        /// 2. Create a bundle instance with the plugin URL
        guard let bundle = Bundle(url: bundleURL) else { return }

        /// 3. Load the bundle and our plugin class
        let className = "AppKitBundle.MacPlugin"
        guard let pluginClass = bundle.classNamed(className) as? Plugin.Type else { return }

        /// 4. Create an instance of the plugin class
        /*
        let data = Data()
        do {
            let plugin = try? pluginClass.init(coder: NSKeyedUnarchiver(forReadingFrom: data))
            plugin?.sayHello()
        }*/
        
        plugin = pluginClass.init()
        //plugin?.sayHello()
        print("SOMETHING3", plugin!.becomeFirstResponder())
    }
}
