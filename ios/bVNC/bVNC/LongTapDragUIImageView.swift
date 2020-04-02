//
//  CustomTouchInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

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
    
    override init(image: UIImage?) {
        super.init(image: image)
        initialize()
    }
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        initialize()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        initialize()
    }
        
    @objc private func handlePan(_ sender: UIPanGestureRecognizer) {
        if sender.state == .ended {
            self.inPanDragging = false
        }
        let translation = sender.translation(in: sender.view)

        if let view = sender.view {
            let scaleX = sender.view!.transform.a
            let scaleY = sender.view!.transform.d
            
            //print ("abs(scaleX*translation.x): \(abs(scaleX*translation.x)), abs(scaleY*translation.y): \(abs(scaleY*translation.y))")
            if (!self.inPanDragging && !self.inPanning && (self.inScrolling || abs(scaleY*translation.y)/abs(scaleX*translation.x) > 1.7)) {

                // If tolerance for scrolling was just exceeded, begin scroll event
                if (!self.inScrolling) {
                    self.inScrolling = true
                    let point = sender.location(in: view)
                    self.viewTransform = view.transform
                    self.newX = point.x*viewTransform.a
                    self.newY = point.y*viewTransform.d
                }
                
                if translation.y > 0 {
                    //print("\(#function), up")
                    sendDownThenUpEvent(scrolling: true, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: true, fifthDown: false)
                } else if translation.y < 0 {
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
            } else {
                //print("\(#function), panning")
                self.inPanning = true
                var newCenterX = view.center.x + scaleX*translation.x
                var newCenterY = view.center.y + scaleY*translation.y
                let scaledWidth = sender.view!.frame.width/scaleX
                let scaledHeight = sender.view!.frame.height/scaleY
                if sender.view!.frame.minX/scaleX >= 50/scaleX { newCenterX = view.center.x - 5 }
                if sender.view!.frame.minY/scaleY >= 50/scaleY + globalStateKeeper!.topSpacing/scaleY { newCenterY = view.center.y - 5 }
                if sender.view!.frame.minX/scaleX <= -50/scaleX - (scaleX-1.0)*scaledWidth/scaleX { newCenterX = view.center.x + 5 }
                if sender.view!.frame.minY/scaleY <= -50/scaleY - globalStateKeeper!.keyboardHeight/scaleY - (scaleY-1.0)*scaledHeight/scaleY { newCenterY = view.center.y + 5 }
                view.center = CGPoint(x: newCenterX, y: newCenterY)
                sender.setTranslation(CGPoint.zero, in: view)
            }
        }
    }
    
    @objc private func handleLongTap(_ sender: UILongPressGestureRecognizer) {
        if let touchView = sender.view {
            if sender.state == .began {
                AudioServicesPlaySystemSound(1100);
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
