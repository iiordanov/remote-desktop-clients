//
//  Plugin.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-10-10.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation

@objc(Plugin)
protocol Plugin: NSObjectProtocol {
    /*
     init(coder: NSCoder)*/
    init()
    func sayHello()
    func becomeFirstResponder() -> Bool
}
