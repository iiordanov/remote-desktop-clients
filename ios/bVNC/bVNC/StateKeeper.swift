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
    var connectionSettings: ConnectionSettings = ConnectionSettings()
    var settings = UserDefaults.standard
    var scene: UIScene?
    var window: CustomTouchInput?
    var vncSession: VncSession?
    
    var currentPage: String = "page1" {
        didSet {
            self.objectWillChange.send(self)
        }
    }

    func setScene(scene: UIScene) {
        self.scene = scene
    }

    func setWindow(window: CustomTouchInput) {
        self.window = window
    }

    func connect() {
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!)
        self.vncSession!.connect(connectionSettings: self.connectionSettings)
    }

    @objc func disconnect() {
        window?.disableTouch()
        self.vncSession?.disconnect()
        // TODO: Show a spinner instead of going to the connection screen and allow the backend to indicate when the disconnection is completed
        let contentView = ContentView(stateKeeper: self)
        window!.rootViewController = UIHostingController(rootView: contentView)
        self.currentPage = "page3"
    }

}
