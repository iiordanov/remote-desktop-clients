//
//  CustomTouchInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

let insetDimension: CGFloat = 0

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

class TouchEnabledUIImageView: UIImageView, UIContextMenuInteractionDelegate {
    var fingers = [UITouch?](repeating: nil, count:5)
    var width: CGFloat = 0.0
    var height: CGFloat = 0.0
    var lastX: CGFloat = 0.0
    var lastY: CGFloat = 0.0
    var newX: CGFloat = 0.0
    var newY: CGFloat = 0.0
    var newDoubleTapX: CGFloat = 0.0
    var newDoubleTapY: CGFloat = 0.0
    var pendingDoubleTap: Bool = false
    var viewTransform: CGAffineTransform = CGAffineTransform()
    var timeLast: Double = 0.0
    let timeThreshold: Double = 0.02
    var tapLast: Double = 0
    let doubleTapTimeThreshold: Double = 0.5
    let doubleTapDistanceThreshold: CGFloat = 20.0
    var touchEnabled: Bool = false
    var firstDown: Bool = false
    var secondDown: Bool = false
    var thirdDown: Bool = false
    let lock = NSLock()
    let fingerLock = NSLock()
    var panGesture: UIPanGestureRecognizer?
    var pinchGesture: UIPinchGestureRecognizer?
    var tapGesture: UITapGestureRecognizer?
    var primaryClickGesture: UITapGestureRecognizer?
    var secondaryClickGesture: UITapGestureRecognizer?
    var longTapGesture: UILongPressGestureRecognizer?
    var doubleTapDragGesture: UILongPressGestureRecognizer?
    var hoverGesture: UIHoverGestureRecognizer?
    var scrollWheelGesture: UIPanGestureRecognizer?
    var inLeftDragging = false
    var moveEventsSinceFingerDown = 0
    var inScrolling = false
    var inPanning = false
    var inPanDragging = false
    var panningToleranceEvents = 0
    
    var tapGestureDetected = false
    
    var stateKeeper: StateKeeper?

    func initialize() {
        isMultipleTouchEnabled = true
        self.width = self.frame.width
        self.height = self.frame.height
        tapGesture = UITapGestureRecognizer(target: self, action: #selector(handleTap(_:)))
        tapGesture?.numberOfTapsRequired = 1
        pinchGesture = UIPinchGestureRecognizer(target: self, action: #selector(handleZooming(_:)))
        hoverGesture = UIHoverGestureRecognizer(target: self, action: #selector(handleHovering(_:)))
        
        if #available(iOS 13.4, *) {
            /*
            // Primary and secondary click gesture
            primaryClickGesture = UITapGestureRecognizer(target: self, action: #selector(handlePrimaryClick(_:)))
            primaryClickGesture?.buttonMaskRequired = UIEvent.ButtonMask.primary
            secondaryClickGesture = UITapGestureRecognizer(target: self, action: #selector(handleSecondaryClick(_:)))
            secondaryClickGesture?.buttonMaskRequired = UIEvent.ButtonMask.secondary
            */
            // Pan gesture to recognize mouse-wheel scrolling
            scrollWheelGesture = UIPanGestureRecognizer(target: self, action: #selector(handleScroll(_:)))
            scrollWheelGesture?.allowedScrollTypesMask = UIScrollTypeMask.discrete
            scrollWheelGesture?.maximumNumberOfTouches = 0;
        }
        
        doubleTapDragGesture = UILongPressGestureRecognizer(target: self, action: #selector(handleDrag(_:)))
        doubleTapDragGesture?.minimumPressDuration = 0.05
        doubleTapDragGesture?.numberOfTapsRequired = 1
        
        // Method of detecting two-finger tap/click on trackpad. Not adding unless this is running on a Mac
        // because it also captures long-taps on a touch screen
        if self.stateKeeper?.macOs == true {
            let interaction = UIContextMenuInteraction(delegate: self)
            self.addInteraction(interaction)
        }
    }
    
    @objc func handleScroll(_ sender: UIPanGestureRecognizer) {
        let translation = sender.translation(in: sender.view)
        if translation.y > 0 {
            sendDownThenUpEvent(scrolling: true, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: true, fifthDown: false)
        } else if translation.y < 0 {
            sendDownThenUpEvent(scrolling: true, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: true)
        }
    }

    override init(image: UIImage?) {
        super.init(image: image)
        initialize()
    }
    
    init(frame: CGRect, stateKeeper: StateKeeper?) {
        super.init(frame: frame)
        self.stateKeeper = stateKeeper
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
    
    func setViewParameters(point: CGPoint, touchView: UIView, setDoubleTapCoordinates: Bool=false) {
        //print(#function)
        self.width = touchView.frame.width
        self.height = touchView.frame.height
        self.viewTransform = touchView.transform
        //let sDx = (touchView.center.x - self.point.x)/self.width
        //let sDy = (touchView.center.y - self.point.y)/self.height
        self.newX = (point.x)*viewTransform.a + insetDimension/viewTransform.a
        self.newY = (point.y)*viewTransform.d + insetDimension/viewTransform.d
        if setDoubleTapCoordinates {
            self.pendingDoubleTap = true
            newDoubleTapX = newX
            newDoubleTapY = newY
        }
    }
    
    func sendDownThenUpEvent(scrolling: Bool, moving: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool, fourthDown: Bool, fifthDown: Bool) {
        if (self.touchEnabled) {
            Background {
                let timeNow = CACurrentMediaTime()
                let timeDiff = timeNow - self.timeLast
                if ((!moving && !scrolling) || (moving || scrolling) && timeDiff >= self.timeThreshold) {
                    self.sendPointerEvent(scrolling: scrolling, moving: moving, firstDown: firstDown, secondDown: secondDown, thirdDown: thirdDown, fourthDown: fourthDown, fifthDown: fifthDown)
                    if (!moving) {
                        //print ("Sleeping \(self.timeThreshhold)s before sending up event.")
                        Thread.sleep(forTimeInterval: self.timeThreshold)
                        self.sendPointerEvent(scrolling: scrolling, moving: moving, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: false)
                    }
                    self.timeLast = CACurrentMediaTime()
                }
            }
        }
    }
    
    func sendPointerEvent(scrolling: Bool, moving: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool, fourthDown: Bool, fifthDown: Bool) {
        //let timeNow = CACurrentMediaTime();
        //let timeDiff = timeNow - self.timeLast
        if !moving || (abs(self.lastX - self.newX) > 1.0 || abs(self.lastY - self.newY) > 1.0) {
            let cl = self.stateKeeper!.cl[self.stateKeeper!.currInst]!
            sendPointerEventToServer(cl, Float32(self.width), Float32(self.height), Float32(self.newX), Float32(self.newY), firstDown, secondDown, thirdDown, fourthDown, fifthDown)
            self.lastX = self.newX
            self.lastY = self.newY
            //self.timeLast = timeNow
            self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
        }
    }
        
    func enableGestures() {
        isUserInteractionEnabled = true
        if let pinchGesture = pinchGesture { addGestureRecognizer(pinchGesture) }
        if let panGesture = panGesture {
            addGestureRecognizer(panGesture)
            if #available(iOS 13.4, *) {
                panGesture.allowedScrollTypesMask = UIScrollTypeMask.continuous
            }
        }
        if let tapGesture = tapGesture { addGestureRecognizer(tapGesture) }
        if let longTapGesture = longTapGesture { addGestureRecognizer(longTapGesture) }
        if let hoverGesture = hoverGesture { addGestureRecognizer(hoverGesture) }
        if let scrollWheelGesture = scrollWheelGesture { addGestureRecognizer(scrollWheelGesture) }
        if let doubleTapDragGesture = doubleTapDragGesture { addGestureRecognizer(doubleTapDragGesture) }

        /*
        if let primaryClickGesture = primaryClickGesture { addGestureRecognizer(primaryClickGesture) }
        if let secondaryClickGesture = secondaryClickGesture { addGestureRecognizer(secondaryClickGesture) }
        */
    }
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !isOutsideImageBoundaries(touch: touch, touchView: touchView) {
                    log_callback_str(message: "Touch is outside image, ignoring.")
                    continue
                }
            } else {
                log_callback_str(message: "Could not unwrap touch.view, sending event at last coordinates.")
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
                        log_callback_str(message: "Single index detected, marking this a left-click")
                        self.firstDown = true
                        self.secondDown = false
                        self.thirdDown = false
                        // Record location only for first index
                        if let touchView = touch.view {
                            self.setViewParameters(point: touch.location(in: touchView), touchView: touchView)
                        }
                    }
                    if index == 1 {
                        log_callback_str(message: "Two indexes detected, marking this a right-click")
                        self.firstDown = false
                        self.secondDown = false
                        self.thirdDown = true
                    }
                    if index == 2 {
                        log_callback_str(message: "Three indexes detected, marking this a middle-click")
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
                    log_callback_str(message: "Touch is outside image, ignoring.")
                    continue
                }
            } else {
                log_callback_str(message: "Could not unwrap touch.view, sending event at last coordinates.")
            }
            
            for (index, finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    if index == 0 {
                        if stateKeeper!.macOs || moveEventsSinceFingerDown > 8 {
                            //log_callback_str(message: "\(#function) +\(self.firstDown) + \(self.secondDown) + \(self.thirdDown)")
                            self.inPanDragging = true
                            self.sendDownThenUpEvent(scrolling: false, moving: true, firstDown: self.firstDown, secondDown:     self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                        } else {
                            //print("Discarding some touch events")
                            moveEventsSinceFingerDown += 1
                        }
                        // Record location only for first index
                        if let touchView = touch.view {
                            self.setViewParameters(point: touch.location(in: touchView), touchView: touchView)
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
                    log_callback_str(message: "Touch is outside image, ignoring.")
                    continue
                }
            } else {
                log_callback_str(message: "Could not unwrap touch.view, sending event at last coordinates.")
            }
            
            for (index, finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    self.fingerLock.lock()
                    self.fingers[index] = nil
                    self.fingerLock.unlock()
                    if (index == 0) {
                        if (self.inLeftDragging || self.tapGestureDetected || self.panGesture?.state == .began || self.pinchGesture?.state == .began) {
                            log_callback_str(message: "Currently left-dragging, single or double-tapping, panning or zooming and first finger lifted, not sending mouse events.")
                        } else {
                            log_callback_str(message: "Not panning or zooming and first finger lifted, sending mouse events.")
                            self.sendDownThenUpEvent(scrolling: false, moving: false, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                            self.firstDown = false
                            self.secondDown = false
                            self.thirdDown = false
                        }
                        self.tapGestureDetected = false
                    } else {
                        log_callback_str(message: "Fingers other than first lifted, not sending mouse events.")
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
    
    @objc func handleHovering(_ sender: UIHoverGestureRecognizer) {
        //print(#function)
        sendPointerEvent(scrolling: false, moving: true, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
        if let touchView = sender.view {
            self.setViewParameters(point: sender.location(in: touchView), touchView: touchView)
        } else {
            return
        }
    }

    @objc func handleZooming(_ sender: UIPinchGestureRecognizer) {
        if (self.stateKeeper?.allowZooming != true || self.secondDown || self.inScrolling || self.inPanning) {
            return
        }
        let scale = sender.scale
        if sender.scale < 0.95 || sender.scale > 1.05 {
            log_callback_str(message: "Preventing large skips in scale.")
        }
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
        self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
    }
    
    @objc private func handleTap(_ sender: UITapGestureRecognizer) {
        if !self.secondDown && !self.thirdDown {
            self.tapGestureDetected = true
            self.firstDown = true
            self.secondDown = false
            self.thirdDown = false
            if let touchView = sender.view {
                let timeNow = CACurrentMediaTime()
                if timeNow - tapLast > doubleTapTimeThreshold {
                    log_callback_str(message: "Single tap detected.")
                    self.setViewParameters(point: sender.location(in: touchView), touchView: touchView, setDoubleTapCoordinates: true)
                } else if self.pendingDoubleTap {
                    log_callback_str(message: "Potential double tap detected.")
                    self.pendingDoubleTap = false
                    self.setViewParameters(point: sender.location(in: touchView), touchView: touchView)
                    let distance = abs(lastX - newX) + abs(lastY - newY)
                    if distance < doubleTapDistanceThreshold {
                        log_callback_str(message: "Second tap was \(distance) away from first, sending click at previous coordinates.")
                        newX = newDoubleTapX
                        newY = newDoubleTapY
                    } else {
                        log_callback_str(message: "Second tap was \(distance) away from first, threshhold: \(doubleTapDistanceThreshold).")
                    }
                }
                self.tapLast = timeNow
                self.sendDownThenUpEvent(scrolling: false, moving: false, firstDown: self.firstDown, secondDown: self.secondDown, thirdDown: self.thirdDown, fourthDown: false, fifthDown: false)
                self.firstDown = false
                self.secondDown = false
                self.thirdDown = false
            }
        } else {
            log_callback_str(message: "Other fingers were down, not acting on single tap")
        }
    }

    func panView(sender: UIPanGestureRecognizer) -> Void {
        var tempVerticalOnlyPan = false
        if !self.stateKeeper!.allowPanning && !(self.stateKeeper!.keyboardHeight > 0) {
            // Panning is disallowed and keyboard is not up, not doing anything
            return
        } else if !self.stateKeeper!.allowPanning && self.stateKeeper!.keyboardHeight > 0 {
            // Panning is disallowed but keyboard is up so we allow vertical panning temporarily
            tempVerticalOnlyPan = true
        }
        
        if let view = sender.view {
            let scaleX = sender.view!.transform.a
            let scaleY = sender.view!.transform.d
            let translation = sender.translation(in: sender.view)

            //print("\(#function), panning")
            self.inPanning = true
            var newCenterX = view.center.x + scaleX*translation.x
            var newCenterY = view.center.y + scaleY*translation.y
            let scaledWidth = sender.view!.frame.width/scaleX
            let scaledHeight = sender.view!.frame.height/scaleY
            
            if sender.view!.frame.minX/scaleX >= 50/scaleX && view.center.x - newCenterX < 0 { newCenterX = view.center.x }
            if sender.view!.frame.minY/scaleY >= 50/scaleY + globalStateKeeper!.topSpacing/scaleY && view.center.y - newCenterY < 0 { newCenterY = view.center.y }
            if sender.view!.frame.minX/scaleX <= -50/scaleX - (scaleX-1.0)*scaledWidth/scaleX && newCenterX - view.center.x < 0 { newCenterX = view.center.x }
            if sender.view!.frame.minY/scaleY <= -50/scaleY - globalStateKeeper!.keyboardHeight/scaleY - (scaleY-1.0)*scaledHeight/scaleY && newCenterY - view.center.y < 0 { newCenterY = view.center.y }
            
            if tempVerticalOnlyPan {
                // Do not allow panning sideways if this is a temporary vertical pan
                newCenterX = view.center.x
            }
            view.center = CGPoint(x: newCenterX, y: newCenterY)
            sender.setTranslation(CGPoint.zero, in: view)
            self.stateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
        }
    }
    
    func contextMenuInteraction(_ interaction: UIContextMenuInteraction, configurationForMenuAtLocation location: CGPoint) -> UIContextMenuConfiguration? {
        //print(#function, interaction)
        if let view = interaction.view {
            self.setViewParameters(point: interaction.location(in: view), touchView: view, setDoubleTapCoordinates: true)
            self.sendDownThenUpEvent(scrolling: false, moving: false, firstDown: false, secondDown: false, thirdDown: true, fourthDown: false, fifthDown: false)
        } else {
            log_callback_str(message: "Could not unwrap interaction.view, sending event at last coordinates.")
        }
        return nil
    }

    @objc func handleDrag(_ sender: UILongPressGestureRecognizer) {
        //print(#function, sender)
        if let view = sender.view {
            self.setViewParameters(point: sender.location(in: view), touchView: view, setDoubleTapCoordinates: false)
            switch sender.state {
            case .began, .changed:
                self.sendPointerEvent(scrolling: false, moving: false, firstDown: true, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: false)
                break
            case .ended, .cancelled, .failed:
                self.setViewParameters(point: sender.location(in: view), touchView: view, setDoubleTapCoordinates: false)
                self.sendPointerEvent(scrolling: false, moving: false, firstDown: false, secondDown: false, thirdDown: false, fourthDown: false, fifthDown: false)
                break
            @unknown default:
                log_callback_str(message: "Unknown state in UILongPressGestureRecognizer. Ignoring.")
            }
        } else {
            log_callback_str(message: "Could not unwrap interaction.view, sending event at last coordinates.")
        }
    }

    @objc private func handlePrimaryClick(_ sender: UITapGestureRecognizer) {
        print(#function, sender)
    }

    @objc private func handleSecondaryClick(_ sender: UITapGestureRecognizer) {
        print(#function, sender)
    }
}
