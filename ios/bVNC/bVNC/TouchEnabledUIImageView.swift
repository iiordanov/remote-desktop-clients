//
//  CustomTouchInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

extension UIImageView {
    private static var _x = [String:CGFloat]()
    var x:CGFloat {
        get {
            let tmpAddress = String(format: "%p", unsafeBitCast(self, to: Int.self))
            return UIImageView._x[tmpAddress] ?? 0.0
        }
        set(newValue) {
            let tmpAddress = String(format: "%p", unsafeBitCast(self, to: Int.self))
            UIImageView._x[tmpAddress] = newValue
        }
    }
    
    func enableGestures() {
        let panGesture = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        panGesture.minimumNumberOfTouches = 2
        panGesture.maximumNumberOfTouches = 2
        let pinchGesture = UIPinchGestureRecognizer(target: self, action: #selector(startZooming(_:)))
        isUserInteractionEnabled = true
        addGestureRecognizer(pinchGesture)
        addGestureRecognizer(panGesture)
    }
    
    @objc private func handlePan(_ sender: UIPanGestureRecognizer) {
        let scaleX = sender.view!.transform.a
        let scaleY = sender.view!.transform.d
        let translation = sender.translation(in: sender.view)
        if let view = sender.view {
            view.center = CGPoint(x: (view.center.x + scaleX*translation.x), y: (view.center.y + scaleY*translation.y))
        }
        sender.setTranslation(CGPoint.zero, in: sender.view)
    }
    
    @objc private func startZooming(_ sender: UIPinchGestureRecognizer) {
        let transformResult = sender.view?.transform.scaledBy(x: sender.scale, y: sender.scale)
        guard let newTransform = transformResult, newTransform.a > 1, newTransform.d > 1 else { return }
        sender.view?.transform = newTransform
        sender.scale = 1
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
    var lastTime: Double = 0.0
    var touchEnabled: Bool = false
    var firstDown: Bool = false
    var secondDown: Bool = false
    var thirdDown: Bool = false
    var point: CGPoint = CGPoint(x: 0, y: 0)

    override init(image: UIImage?) {
        super.init(image: image)
        isMultipleTouchEnabled = true
        self.width = self.frame.width
        self.height = self.frame.height
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        self.width = self.frame.width
        self.height = self.frame.height
    }

    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        isMultipleTouchEnabled = true
    }
    
    func enableTouch() {
        touchEnabled = true
    }

    func disableTouch() {
        touchEnabled = false
    }
    
    func setViewParameters(touch: UITouch, touchView: UIView) -> Bool {
        if (!touch.view!.isKind(of: UIImageView.self)) {
            return false
        }
        self.width = touchView.frame.width
        self.height = touchView.frame.height
        self.point = touch.location(in: touchView)
        self.viewTransform = touchView.transform
        self.newX = self.point.x*viewTransform.a
        self.newY = self.point.y*viewTransform.d
        return true
    }
    
    func sendPointerEvent(action: String, index: Int, touch: UITouch, forceSend: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool) {
        Background {
            //print(#function)
            if (!firstDown && !secondDown && !thirdDown &&
                !self.firstDown && !self.secondDown && !self.thirdDown) {
                print("No mouse button is down but we were told to send button up event, returning.")
                return
            }
            self.firstDown = firstDown
            self.secondDown = secondDown
            self.thirdDown = thirdDown
            let currentTime = CACurrentMediaTime()
            print(action + ": \(index+1): x=\(self.point.x), y=\(self.point.y), tx=\(self.viewTransform.tx), ty=\(self.viewTransform.ty), a=\(self.viewTransform.a), b=\(self.viewTransform.b), c=\(self.viewTransform.c), d=\(self.viewTransform.d)")
            //print("currentTime - lastTime: " + String(abs(self.lastTime - currentTime)) + " last time: " + String(self.lastTime), " forceSend: " + String(forceSend) + " touchEnabled: " + String(self.touchEnabled))
            if (self.touchEnabled && (forceSend || abs(self.lastTime - currentTime) > 0.1 && abs(self.lastX - self.newX) > 1.0 && abs(self.lastX - self.newX) > 1.0)) {
                self.lastX = self.newX
                self.lastY = self.newY
                if(forceSend) {
                    Thread.sleep(forTimeInterval: 0.2)
                }
                self.lastTime = currentTime
                sendPointerEventToServer(Int32(self.width), Int32(self.height), Int32(self.newX), Int32(self.newY), firstDown, secondDown, thirdDown)
            }
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !setViewParameters(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }

            for (index, finger)  in self.fingers.enumerated() {
                if finger == nil {
                    self.fingers[index] = touch
                    self.sendPointerEvent(action: "finger down", index: index, touch: touch, forceSend: false, firstDown: true, secondDown: false, thirdDown: false)
                    break
                }
            }
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesMoved(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !setViewParameters(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }

            for (index,finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    self.sendPointerEvent(action: "finger moved", index: index, touch: touch, forceSend: false, firstDown: true, secondDown: false, thirdDown: false)
                    break
                }
            }
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(touches, with: event)
        for touch in touches {
            if let touchView = touch.view {
                if !setViewParameters(touch: touch, touchView: touchView) {
                    print("Touch is outside image, ignoring.")
                    continue
                }
            } else {
                print("Could not unwrap touch.view, sending event at last coordinates.")
            }

            for (index,finger) in self.fingers.enumerated() {
                if let finger = finger, finger == touch {
                    self.fingers[index] = nil
                    self.sendPointerEvent(action: "finger lifted", index: index, touch: touch, forceSend: true, firstDown: false, secondDown: false, thirdDown: false)
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
}
