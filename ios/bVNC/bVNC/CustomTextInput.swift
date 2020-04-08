//
//  CustomTextInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

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
}

class CustomTextInput: UIButton, UIKeyInput{
    public var hasText: Bool { return false }
    var stateKeeper: StateKeeper?
    
    init(stateKeeper: StateKeeper) {
        super.init(frame: CGRect())
        self.stateKeeper = stateKeeper;
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    public func insertText(_ text: String){
        //print("Sending: " + text + ", number of characters: " + String(text.count))
        for char in text.unicodeScalars {
            Background {
                if !sendKeyEventInt(self.stateKeeper!.cl!, Int32(String(char.value))!) {
                    sendKeyEvent(self.stateKeeper!.cl!, String(char))
                }
                self.stateKeeper?.toggleModifiersIfDown()
            }
        }
    }
    
    public func deleteBackward(){
        Background {
            sendKeyEventWithKeySym(self.stateKeeper!.cl!, 0xff08);
            self.stateKeeper?.toggleModifiersIfDown()
        }
    }
    
    @objc func toggleFirstResponder() -> Bool {
        if (self.isFirstResponder) {
            print("Keyboard should be showing already, hiding it.")
            return hideKeyboard()
        } else {
            return showKeyboard()
        }
    }

    @objc func hideKeyboard() -> Bool {
        print("Hiding keyboard.")
        becomeFirstResponder()
        return resignFirstResponder()
    }

    @objc func showKeyboard() -> Bool {
        print("Showing keyboard.")
        return becomeFirstResponder()
    }
    
    override var canBecomeFirstResponder: Bool {
        return true
    }
}
