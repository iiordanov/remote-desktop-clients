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
import AudioToolbox

class LongTapDragUIImageView: TouchEnabledUIImageView {
    
    override func initialize() {
        super.initialize()
        panGesture = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        panGesture?.minimumNumberOfTouches = 1
        panGesture?.maximumNumberOfTouches = 2
        longTapGesture = UILongPressGestureRecognizer(target: self, action: #selector(handleLongTap(_:)))
        pinchGesture = UIPinchGestureRecognizer(target: self, action: #selector(handleZooming(_:)))
    }
            
    @objc private func handlePan(_ sender: UIPanGestureRecognizer) {
        if sender.state == .ended {
            self.inPanDragging = false
            if !inPanning {
                // If there was actual pointer interaction to the server, request a refresh
                self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.5, fullScreenUpdate: false, recurring: false)
            }
        }
        
        let translation = sender.translation(in: sender.view)

        if let view = sender.view {
            let scaleX = sender.view!.transform.a
            let scaleY = sender.view!.transform.d
            
            //print ("abs(scaleX*translation.x): \(abs(scaleX*translation.x)), abs(scaleY*translation.y): \(abs(scaleY*translation.y))")
            // self.thirdDown (which marks a right click) helps ensure this mode does not scroll with one finger
            if (!self.inPanDragging && !self.inPanning && self.thirdDown &&
                (self.inScrolling || abs(scaleY*translation.y)/abs(scaleX*translation.x) > 1.4 )) {

                // If tolerance for scrolling was just exceeded, begin scroll event
                if (!self.inScrolling) {
                    self.inScrolling = true
                    let point = sender.location(in: view)
                    self.viewTransform = view.transform
                    self.newX = point.x*viewTransform.a
                    self.newY = point.y*viewTransform.d
                }
                
                if translation.y > 20 {
                    //print("\(#function), up")
                    sendDownThenUpEvent(scrolling: true, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: true, fifthDown: false)
                } else if translation.y < -20 {
                    //print("\(#function), down")
                    sendDownThenUpEvent(scrolling: true, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: true)
                }
                return
            } else if self.secondDown || self.thirdDown {
                //print("\(#function), second or third dragging")
                self.inPanDragging = true
                if let touchView = sender.view {
                    self.setViewParameters(point: sender.location(in: touchView), touchView: touchView)
                    let moving = !(sender.state == .ended)
                    self.sendDownThenUpEvent(scrolling: false, moving: moving, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                }
                return
            } else if abs(scaleY*translation.y) > 0.25 || abs(scaleX*translation.x) > 0.25 {
                panView(sender: sender)
            }
        }
    }
    
    @objc private func handleLongTap(_ sender: UILongPressGestureRecognizer) {
        if let touchView = sender.view {
            if sender.state == .began {
                AudioServicesPlaySystemSound(1100);
                self.inLeftDragging = true
            } else if sender.state == .ended {
                self.inLeftDragging = false
            }
            self.setViewParameters(point: sender.location(in: touchView), touchView: touchView)
            self.firstDown = !(sender.state == .ended)
            let moving = self.firstDown
            self.secondDown = false
            self.thirdDown = false
            self.sendDownThenUpEvent(scrolling: false, moving: moving, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
        }
    }

}
