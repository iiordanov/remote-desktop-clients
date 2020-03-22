//
//  CustomTouchInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

let insetDimension: CGFloat = 10

extension UIImage {
    func imageWithInsets(insets: UIEdgeInsets) -> UIImage? {
        UIGraphicsBeginImageContextWithOptions(
            CGSize(width: self.size.width + insets.left + insets.right,
                   height: self.size.height + insets.top + insets.bottom), false, self.scale)
        let _ = UIGraphicsGetCurrentContext()
        let origin = CGPoint(x: insets.left, y: insets.top)
        self.draw(at: origin)
        let imageWithInsets = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()
        return imageWithInsets
    }
}

extension UIImage {
    func image(byDrawingImage image: UIImage, inRect rect: CGRect) -> UIImage! {
        UIGraphicsBeginImageContext(size)
        draw(in: CGRect(x: 0, y: 0, width: size.width, height: size.height))
        image.draw(in: rect)
        let result = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()
        return result
    }
}

class TouchEnabledUIImageView: UIImageView {
    var fingers = [UITouch?](repeating: nil, count:5)
    var width: CGFloat = 0.0
    var height: CGFloat = 0.0
    var lastX: CGFloat = 0.0
    var lastY: CGFloat = 0.0
    var newX: CGFloat = 0.0
    var newY: CGFloat = 0.0
    var viewTransform: CGAffineTransform = CGAffineTransform()
    var timeLast: Double = 0.0
    let timeThreshhold: Double = 0.06
    var touchEnabled: Bool = false
    var firstDown: Bool = false
    var secondDown: Bool = false
    var thirdDown: Bool = false
    var point: CGPoint = CGPoint(x: 0, y: 0)
    let lock = NSLock()
    let fingerLock = NSLock()
    var panGesture: UIPanGestureRecognizer?
    var pinchGesture: UIPinchGestureRecognizer?
    var moveEventsSinceFingerDown = 0
    var inScrolling = false
    var inPanning = false
    var panningToleranceEvents = 0

    func initialize() {
        isMultipleTouchEnabled = true
        self.width = self.frame.width
        self.height = self.frame.height
        panGesture = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        pinchGesture = UIPinchGestureRecognizer(target: self, action: #selector(handleZooming(_:)))
        panGesture?.minimumNumberOfTouches = 2
        panGesture?.maximumNumberOfTouches = 2

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
    
    func enableTouch() {
        touchEnabled = true
    }
    
    func disableTouch() {
        touchEnabled = false
    }
    
    func isOutsideImageBoundaries(touch: UITouch, touchView: UIView) -> Bool {
        if (!touch.view!.isKind(of: UIImageView.self)) {
            return false
        }
        return true
    }
    
    func setViewParameters(touch: UITouch, touchView: UIView) {
        self.width = touchView.frame.width
        self.height = touchView.frame.height
        self.point = touch.location(in: touchView)
        self.viewTransform = touchView.transform
        let sDx = (touchView.center.x - self.point.x)/self.width
        let sDy = (touchView.center.y - self.point.y)/self.height
        self.newX = (self.point.x)*viewTransform.a + insetDimension/viewTransform.a
        self.newY = (self.point.y)*viewTransform.d + insetDimension/viewTransform.d
    }
    
    func sendDownThenUpEvent(moving: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool, fourthDown: Bool, fifthDown: Bool) {
        if (self.touchEnabled) {
            Background {
                self.lock.lock()
                self.sendPointerEvent(moving: moving, firstDown: firstDown, secondDown: secondDown, thirdDown: thirdDown, fourthDown: fourthDown, fifthDown: fifthDown)
                if (!moving) {
                    Thread.sleep(forTimeInterval: 0.02)
                    self.sendPointerEvent(moving: moving, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: false)
                }
                self.lock.unlock()
            }
        }
    }
    
    func sendPointerEvent(moving: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool, fourthDown: Bool, fifthDown: Bool) {
        let timeNow = CACurrentMediaTime();
        let timeDiff = timeNow - self.timeLast
        if !moving || (abs(self.lastX - self.newX) > 1.0 || abs(self.lastY - self.newY) > 1.0) && timeDiff > timeThreshhold {
            sendPointerEventToServer(Int32(self.width), Int32(self.height), Int32(self.newX), Int32(self.newY), firstDown, secondDown, thirdDown, fourthDown, fifthDown)
            self.lastX = self.newX
            self.lastY = self.newY
            self.timeLast = timeNow
        }
    }
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !isOutsideImageBoundaries(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }
            
            for (index, finger)  in self.fingers.enumerated() {
                if finger == nil {
                    self.fingerLock.lock()
                    self.fingers[index] = touch
                    self.fingerLock.unlock()
                    if index == 0 {
                        self.inScrolling = false
                        self.inPanning = false
                        self.moveEventsSinceFingerDown = 0
                        print("ONE FINGER Detected, marking this a left-click")
                        self.firstDown = true
                        self.secondDown = false
                        self.thirdDown = false
                        // Record location only for first index
                        if let touchView = touch.view {
                            self.setViewParameters(touch: touch, touchView: touchView)
                        }
                    }
                    if index == 1 {
                        print("TWO FINGERS Detected, marking this a right-click")
                        self.firstDown = false
                        self.secondDown = false
                        self.thirdDown = true
                    }
                    if index == 2 {
                        print("THREE FINGERS Detected, marking this a middle-click")
                        self.firstDown = false
                        self.secondDown = true
                        self.thirdDown = false
                    }
                    break
                }
            }
        }
    }
    
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesMoved(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !isOutsideImageBoundaries(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }
            
            for (index, finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    if index == 0 {
                        // Record location only for first index
                        if let touchView = touch.view {
                            self.setViewParameters(touch: touch, touchView: touchView)
                        }
                        if moveEventsSinceFingerDown > 4 {
                            self.sendDownThenUpEvent(moving: true, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                        } else {
                            print("Discarding some touch events")
                            moveEventsSinceFingerDown += 1
                        }
                    }
                    break
                }
            }
        }
    }
    
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !isOutsideImageBoundaries(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }
            
            for (index, finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    self.fingerLock.lock()
                    self.fingers[index] = nil
                    self.fingerLock.unlock()
                    if (index == 0) {
                        if (self.panGesture?.state == .began || self.pinchGesture?.state == .began) {
                            print("Currently panning or zooming and first finger lifted, not sending mouse events.")
                        } else {
                            print("Not panning or zooming and first finger lifted, sending mouse events.")
                            self.sendDownThenUpEvent(moving: false, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                            self.firstDown = false
                            self.secondDown = false
                            self.thirdDown = false
                        }
                    } else {
                        print("Fingers other than first lifted, not sending mouse events.")
                    }
                    break
                }
            }
        }
    }
    
    override func touchesCancelled(_ touches: Set<UITouch>?, with event: UIEvent?) {
        super.touchesCancelled(touches!, with: event)
        guard let touches = touches else { return }
        self.touchesEnded(touches, with: event)
    }
    
    func enableGestures() {
        isUserInteractionEnabled = true
        if let pinchGesture = pinchGesture { addGestureRecognizer(pinchGesture) }
        if let panGesture = panGesture { addGestureRecognizer(panGesture) }
    }
    
    @objc private func handlePan(_ sender: UIPanGestureRecognizer) {
        let translation = sender.translation(in: sender.view)

        if let view = sender.view {
            let scaleX = sender.view!.transform.a
            let scaleY = sender.view!.transform.d

            //print ("abs(scaleX*translation.x): \(abs(scaleX*translation.x)), abs(scaleY*translation.y): \(abs(scaleY*translation.y))")
            // If scrolling or tolerance for scrolling is exceeded
            if (!self.inPanning && (self.inScrolling || abs(scaleX*translation.x) < 0.2 && abs(scaleY*translation.y) > 1.0)) {
                // If tolerance for scrolling was just exceeded, begin scroll event
                if (!self.inScrolling) {
                    self.inScrolling = true
                    self.point = sender.location(in: view)
                    self.viewTransform = view.transform
                    self.newX = self.point.x*viewTransform.a
                    self.newY = self.point.y*viewTransform.d
                }
                let timeNow = CACurrentMediaTime();
                let timeDiff = timeNow - self.timeLast
                if translation.y > 0 && timeDiff > timeThreshhold {
                    sendDownThenUpEvent(moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: true, fifthDown: false)
                    self.timeLast = timeNow
                } else if translation.y < 0 && timeDiff > timeThreshhold {
                    sendDownThenUpEvent(moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: true)
                    self.timeLast = timeNow
                }
                return
            }
            self.inPanning = true
            var newCenterX = view.center.x + scaleX*translation.x
            var newCenterY = view.center.y + scaleY*translation.y
            let scaledWidth = sender.view!.frame.width/scaleX
            let scaledHeight = sender.view!.frame.height/scaleY
            if sender.view!.frame.minX/scaleX >= 50/scaleX { newCenterX = view.center.x - 5 }
            if sender.view!.frame.minY/scaleY >= 50/scaleY { newCenterY = view.center.y - 5 }
            if sender.view!.frame.minX/scaleX <= -50/scaleX - (scaleX-1.0)*scaledWidth/scaleX { newCenterX = view.center.x + 5 }
            if sender.view!.frame.minY/scaleY <= -50/scaleY - globalStateKeeper!.keyboardHeight/scaleY - (scaleY-1.0)*scaledHeight/scaleY { newCenterY = view.center.y + 5 }
            view.center = CGPoint(x: newCenterX, y: newCenterY)
            sender.setTranslation(CGPoint.zero, in: view)
        }
    }
    
    @objc private func handleZooming(_ sender: UIPinchGestureRecognizer) {
        let scale = sender.scale
        let transformResult = sender.view?.transform.scaledBy(x: sender.scale, y: sender.scale)
        guard let newTransform = transformResult, newTransform.a > 1, newTransform.d > 1 else { return }

        if let view = sender.view {
            let scaledWidth = sender.view!.frame.width/scale
            let scaledHeight = sender.view!.frame.height/scale
            if view.center.x/scale < -20 { view.center.x = -20*scale }
            if view.center.y/scale < -20 { view.center.y = -20*scale }
            if view.center.x/scale > scaledWidth/2 + 20 { view.center.x = (scaledWidth/2 + 20)*scale }
            if view.center.y/scale > scaledHeight/2 + 20 { view.center.y = (scaledHeight/2 + 20)*scale }
        }
        sender.view?.transform = newTransform
        sender.scale = 1
        //print("Frame: \(sender.view!.frame)")
    }
    
}
