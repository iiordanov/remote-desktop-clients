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

