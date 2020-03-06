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
        print("Sending: " + text + ", number of characters: " + String(text.count))
        for char in text {
            Background {
                self.lock.lock()
                sendKeyEvent(String(char).toPointer());
                self.lock.unlock()
            }
        }
    }
    public func deleteBackward(){
        print(#function)

        Background {
            self.lock.lock()
            sendKeyEventWithKeySym(0xff08);
            self.lock.unlock()
        }
    }
    
    override func becomeFirstResponder() -> Bool {
        if (!self.isFirstResponder) {
            print("Trying to show keyboard.")
            return super.becomeFirstResponder()
        } else {
            print("Keyboard should be showing already, not attempting to show it.")
        }
        return true;
    }
    
    override func resignFirstResponder() -> Bool {
        print("Trying to hide keyboard.")
        super.becomeFirstResponder()
        return super.resignFirstResponder()
    }

    override var canBecomeFirstResponder: Bool {return true}
}
