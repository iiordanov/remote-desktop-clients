//
//  KeyboardObserving.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-03-26.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//
// From: https://www.scaledrone.com/blog/fixing-common-issues-with-the-ios-keyboard/

import Foundation

public protocol KeyboardObserving: class {
    
    func keyboardWillShow(withSize size: CGSize)
    func keyboardWillHide()
}

extension KeyboardObserving {
    
    public func addKeyboardObservers(to notificationCenter: NotificationCenter) {
        notificationCenter.addObserver(
            forName: UIResponder.keyboardWillShowNotification,
            object: nil,
            queue: nil,
            using: { [weak self] notification in
                let key = UIResponder.keyboardFrameEndUserInfoKey
                guard let keyboardSizeValue = notification.userInfo?[key] as? NSValue else {
                    return;
                }
                
                let keyboardSize = keyboardSizeValue.cgRectValue
                self?.keyboardWillShow(withSize: keyboardSize.size)
        })
        notificationCenter.addObserver(
            forName: UIResponder.keyboardWillHideNotification,
            object: nil,
            queue: nil,
            using: { [weak self] _ in
                self?.keyboardWillHide()
        })
    }
    
    public func removeKeyboardObservers(from notificationCenter: NotificationCenter) {
        notificationCenter.removeObserver(
            self,
            name: UIResponder.keyboardWillHideNotification,
            object: nil)
        notificationCenter.removeObserver(
            self,
            name: UIResponder.keyboardWillShowNotification,
            object: nil)
    }
}

