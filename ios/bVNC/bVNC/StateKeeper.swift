//
//  StateKeeper.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-22.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import Combine
import SwiftUI

class StateKeeper: ObservableObject {
    let objectWillChange = PassthroughSubject<StateKeeper, Never>()
    var currentConnection: [String: String]
    var settings = UserDefaults.standard
    var scene: UIScene?
    var window: UIWindow?
    var imageView: TouchEnabledUIImageView?
    var vncSession: VncSession?
    
    init() {
        // Load settings for current connection
        currentConnection = self.settings.dictionary(forKey: "selectedConnection") as? [String:String] ?? [:]
    }
    
    var currentPage: String = "page1" {
        didSet {
            self.objectWillChange.send(self)
        }
    }

    func setScene(scene: UIScene) {
        self.scene = scene
    }

    func setWindow(window: UIWindow) {
        self.window = window
    }

    func setImageView(imageView: TouchEnabledUIImageView) {
        self.imageView = imageView
    }

    func connect() {
        // Save settings for current connection
        self.settings.set(self.currentConnection, forKey: "selectedConnection")
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!)
        self.vncSession!.connect(currentConnection: self.currentConnection)
        self.currentPage = "page4"
    }

    @objc func disconnect() {
        imageView?.disableTouch()
        self.vncSession?.disconnect()
        // TODO: Show a spinner instead of going to the connection screen and allow the backend to indicate when the disconnection is completed
        //let contentView = ContentView(stateKeeper: self)
        //window!.rootViewController = UIHostingController(rootView: contentView)
        self.currentPage = "page3"
    }
}
