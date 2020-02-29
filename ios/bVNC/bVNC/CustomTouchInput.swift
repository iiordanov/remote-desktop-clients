//
//  CustomTouchInput.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-27.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import UIKit

class CustomTouchInput: UIWindow {
    var fingers = [UITouch?](repeating: nil, count:5)
    var width: CGFloat = 0.0
    var height: CGFloat = 0.0
    var lastX: CGFloat = 0.0
    var lastY: CGFloat = 0.0
    var newX: CGFloat = 0.0
    var newY: CGFloat = 0.0
    var lastTime: Double = 0.0
    var touchEnabled: Bool = false
    var firstDown: Bool = false
    var secondDown: Bool = false
    var thirdDown: Bool = false
    var point: CGPoint = CGPoint(x: 0, y: 0)

    override init(windowScene: UIWindowScene) {
        super.init(windowScene: windowScene)
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
    
    func sendPointerEvent(action: String, index: Int, touch: UITouch, forceSend: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool) {
        Background {
            print(#function)
            if (!firstDown && !secondDown && !thirdDown &&
                !self.firstDown && !self.secondDown && !self.thirdDown) {
                print("No mouse button is down but we were told to send button up event, returning.")
                return
            }
            self.firstDown = firstDown
            self.secondDown = secondDown
            self.thirdDown = thirdDown
            let currentTime = CACurrentMediaTime()
            print(action + ": \(index+1): x=\(self.point.x) , y=\(self.point.y)")
            print("currentTime - lastTime: " + String(abs(self.lastTime - currentTime)) + " last time: " + String(self.lastTime), " forceSend: " + String(forceSend) + " touchEnabled: " + String(self.touchEnabled))
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
                self.width = touchView.frame.width
                self.height = touchView.frame.height
                self.point = touch.location(in: touchView)
                self.newX = self.point.x
                self.newY = self.point.y
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
                self.width = touchView.frame.width
                self.height = touchView.frame.height
                self.point = touch.location(in: touchView)
                self.newX = self.point.x
                self.newY = self.point.y
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
                self.width = touchView.frame.width
                self.height = touchView.frame.height
                self.point = touch.location(in: touchView)
                self.newX = self.point.x
                self.newY = self.point.y
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
