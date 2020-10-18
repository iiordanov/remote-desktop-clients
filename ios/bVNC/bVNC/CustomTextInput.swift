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

class CustomTextInput: UIButton, UIKeyInput{
    public var hasText: Bool { return false }
    var stateKeeper: StateKeeper?
    
    init(stateKeeper: StateKeeper) {
        super.init(frame: CGRect())
        self.stateKeeper = stateKeeper;
        if stateKeeper.macOs {
            self.becomeFirstResponder()
        }
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    public func insertText(_ text: String){
        //log_callback_str(message: "Sending: " + text + ", number of characters: " + String(text.count))
        for char in text.unicodeScalars {
            Background {
                if !sendKeyEventInt(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, Int32(String(char.value))!) {
                    sendKeyEvent(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, String(char))
                }
                self.stateKeeper?.toggleModifiersIfDown()
            }
            self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
        }
    }
    
    public func deleteBackward(){
        Background {
            sendKeyEventWithKeySym(self.stateKeeper!.cl[self.stateKeeper!.currInst]!, 0xff08);
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
        if stateKeeper?.macOs == true {
            return true
        }
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
    }
    
    override var keyCommands: [UIKeyCommand]? {
        return [
            UIKeyCommand(input: "", modifierFlags: .command, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: .alternate, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: .shift, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: .alphaShift, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: .numericPad, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: .control, action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: [.control, .alternate], action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: [.control, .command], action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: [.control, .alternate], action: #selector(captureModifiers)),
            UIKeyCommand(input: "", modifierFlags: [.control, .shift], action: #selector(captureModifiers)),
        ]
    }

    @objc func captureModifiers(sender: UIKeyCommand) {
        print(sender.modifierFlags)
    }
}
