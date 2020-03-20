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
    var selectedConnection: [String: String]
    var connections: [Dictionary<String, String>]
    var connectionIndex: Int
    var settings = UserDefaults.standard
    var scene: UIScene?
    var window: UIWindow?
    var imageView: TouchEnabledUIImageView?
    var vncSession: VncSession?
    
    init() {
        // Load settings for current connection
        selectedConnection = self.settings.dictionary(forKey: "selectedConnection") as? [String:String] ?? [:]
        connectionIndex = self.settings.integer(forKey: "connectionIndex")
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
    }
    
    var currentPage: String = "page0" {
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

    func connect(index: Int) {
        print("Connecting and navigating to the connection screen")
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!)
        self.vncSession!.connect(currentConnection: self.selectedConnection)
        self.currentPage = "page4"
    }

    @objc func disconnect() {
        print("Disconnecting and navigating to the disconnection screen")
        imageView?.disableTouch()
        self.vncSession?.disconnect()
        self.currentPage = "page3"
    }
    
    func addNew() {
        print("Adding new connection and navigating to connection setup screen")
        self.connectionIndex = -1
        self.selectedConnection = [:]
        self.currentPage = "page1"
    }

    func edit(index: Int) {
        print("Editing connection at index \(index)")
        self.connectionIndex = index
        self.selectedConnection = connections[index]
        print("Navigating to setup screen for connection at index \(self.connectionIndex)")
        self.currentPage = "page1"
    }

    func deleteCurrent() {
        print("Deleting connection at index \(self.connectionIndex) and navigating to list of connections screen")
        // Do something only if we were not adding a new connection.
        if connectionIndex >= 0 {
            print("Deleting connection with index \(connectionIndex)")
            self.connections.remove(at: self.connectionIndex)
            self.connectionIndex = 0
            if self.connections.count > 0 {
                self.selectedConnection = self.connections[connectionIndex]
            } else {
                self.selectedConnection = [:]
            }
            self.settings.set(self.selectedConnection, forKey: "selectedConnection")
            self.settings.set(self.connectionIndex, forKey: "connectionIndex")
            self.settings.set(self.connections, forKey: "connections")
        } else {
            print("We were adding a new connection, so not deleting anything")
        }
        self.currentPage = "page0"
    }
    
    func saveSettings() {
        print("Saving settings")
        self.settings.set(self.selectedConnection, forKey: "selectedConnection")
        self.settings.set(self.connectionIndex, forKey: "connectionIndex")
        self.settings.set(self.connections, forKey: "connections")
    }
    
    func saveNewConnection(connection: [String: String]) {
        self.selectedConnection = connection
        // Negative index indicates we are adding a connection, otherwise we are editing one.
        if (connectionIndex < 0) {
            print("Saving a new connection and navigating to list of connections")
            self.connectionIndex = self.connections.count - 1
            self.connections.append(connection)
        } else {
            print("Saving a connection at index \(self.connectionIndex) and navigating to list of connections")
            self.connections[connectionIndex] = connection
        }
        self.saveSettings()
        self.currentPage = "page0"
    }
    
    func showConnections() {
        self.currentPage = "page0"
    }
}
