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

  func toPointer() -> UnsafePointer<UInt8>? {
    guard let data = self.data(using: String.Encoding.utf8) else { return nil }

    let buffer = UnsafeMutablePointer<UInt8>.allocate(capacity: data.count)
    let stream = OutputStream(toBuffer: buffer, capacity: data.count)

    stream.open()
    data.withUnsafeBytes({ (p: UnsafePointer<UInt8>) -> Void in
      stream.write(p, maxLength: data.count)
    })
    stream.close()

    return UnsafePointer<UInt8>(buffer)
  }
}

class CustomTextInput: UIButton, UIKeyInput{
    public var hasText: Bool { return false }
    let lock = NSLock()

    public func insertText(_ text: String){
        //print("Sending: " + text + ", number of characters: " + String(text.count))
        for char in text.unicodeScalars {
            Background {
                if !sendKeyEventInt(Int32(String(char.value))!) {
                    sendKeyEvent(String(char))
                }
            }
        }
    }
    
    public func deleteBackward(){
        Background {
            self.lock.lock()
            sendKeyEventWithKeySym(0xff08);
            self.lock.unlock()
        }
    }
    
    @objc func toggleFirstResponder() -> Bool {
        if (self.isFirstResponder) {
            print("Keyboard should be showing already, hiding it.")
            becomeFirstResponder()
            return resignFirstResponder()
        } else {
            print("Showing keyboard.")
            return becomeFirstResponder()
        }
    }
    
    override var canBecomeFirstResponder: Bool {
        return true
    }
}
