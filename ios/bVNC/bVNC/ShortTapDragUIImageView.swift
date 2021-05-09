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

import Foundation
import UIKit

class ShortTapDragUIImageView: TouchEnabledUIImageView {
    
    override func initialize() {
        super.initialize()
        panGesture = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        panGesture?.minimumNumberOfTouches = 2
        panGesture?.maximumNumberOfTouches = 2
    }
    
    @objc private func handlePan(_ sender: UIPanGestureRecognizer) {
        if self.secondDown {
            return
        }
        let translation = sender.translation(in: sender.view)

        if let view = sender.view {
            let scaleX = sender.view!.transform.a
            let scaleY = sender.view!.transform.d
            
            //print ("abs(scaleX*translation.x): \(abs(scaleX*translation.x)), abs(scaleY*translation.y): \(abs(scaleY*translation.y))")
            if self.stateKeeper?.macOs == true {
                self.inPanning = false
                self.inScrolling = true
            }
            if (!self.inPanning && (self.inScrolling || abs(scaleY*translation.y)/abs(scaleX*translation.x) > 1.7)) {
                // If tolerance for scrolling was just exceeded, begin scroll event
                if (!self.inScrolling) {
                    self.inScrolling = true
                    let point = sender.location(in: view)
                    self.viewTransform = view.transform
                    self.newX = point.x*viewTransform.a
                    self.newY = point.y*viewTransform.d
                }
                
                super.handleScroll(sender)
                return
            }
            panView(sender: sender)
        }
    }
}
