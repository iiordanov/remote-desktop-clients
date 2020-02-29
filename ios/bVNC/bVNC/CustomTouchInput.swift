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
    var lastTime: Double = 0.0

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
    
    func sendPointerEvent(touch: UITouch, forceSend: Bool, firstDown: Bool, secondDown: Bool, thirdDown: Bool) {
        if (touch.view != nil) {
            self.width = touch.view!.frame.width
            self.height = touch.view!.frame.height
            let point = touch.location(in: touch.view)
            let newX = point.x
            let newY = point.y
            let currentTime = CACurrentMediaTime()
            print ("Current time: " + String(currentTime) + " last time: " + String(lastTime))
            if (forceSend || abs(self.lastTime - currentTime) > 0.1 && abs(lastX - newX) > 1.0 && abs(lastX - newX) > 1.0) {
                lastX = newX
                lastY = newY
                Background {
                    if(forceSend) {
                        Thread.sleep(forTimeInterval: 0.2)
                    }
                    self.lastTime = currentTime
                    sendPointerEventToServer(Int32(self.width), Int32(self.height), Int32(newX), Int32(newY), firstDown, secondDown, thirdDown)
                }
            }
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        for touch in touches {
            let point = touch.location(in: touch.view)
            for (index, finger)  in fingers.enumerated() {
                if finger == nil {
                    fingers[index] = touch
                    print("finger down: \(index+1): x=\(point.x) , y=\(point.y)")
                    sendPointerEvent(touch: touch, forceSend: false, firstDown: true, secondDown: false, thirdDown: false)
                    break
                }
            }
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesMoved(touches, with: event)
        for touch in touches {
            let point = touch.location(in: touch.view)
            for (index,finger) in fingers.enumerated() {
                if let finger = finger, finger == touch {
                    print("finger moved: \(index+1): x=\(point.x) , y=\(point.y)")
                    sendPointerEvent(touch: touch, forceSend: false, firstDown: true, secondDown: false, thirdDown: false)
                    break
                }
            }
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(touches, with: event)
        for touch in touches {
            let point = touch.location(in: touch.view)
            for (index,finger) in fingers.enumerated() {
                if let finger = finger, finger == touch {
                    fingers[index] = nil
                    print("finger lifted: \(index+1): x=\(point.x) , y=\(point.y)")
                    sendPointerEvent(touch: touch, forceSend: true, firstDown: false, secondDown: false, thirdDown: false)
                    break
                }
            }
        }
    }

    override func touchesCancelled(_ touches: Set<UITouch>?, with event: UIEvent?) {
        super.touchesCancelled(touches!, with: event)
        guard let touches = touches else {
            return
        }
        touchesEnded(touches, with: event)
    }
}
