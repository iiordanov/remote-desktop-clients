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
    var errorMessage: String?
    var imageView: TouchEnabledUIImageView?
    var vncSession: VncSession?
    var bottomButtons: [String: UIButton]
    var keyboardHeight: CGFloat = 0.0
    
    init() {
        // Load settings for current connection
        selectedConnection = self.settings.dictionary(forKey: "selectedConnection") as? [String:String] ?? [:]
        connectionIndex = self.settings.integer(forKey: "connectionIndex")
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
        bottomButtons = [:]
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
        registerForNotifications()
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!)
        self.vncSession!.connect(currentConnection: self.selectedConnection)
        self.currentPage = "page4"
    }

    @objc func disconnect() {
        print("Disconnecting and navigating to the disconnection screen")
        imageView?.disableTouch()
        deregisterFromNotifications()
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
    
    func showError(message: String) {
        self.errorMessage = message
        self.currentPage = "dismissableErrorMessage"
    }
    
    @objc func keyboardWasShown(notification: NSNotification){
        let info = notification.userInfo!
        let keyboardSize = (info[UIResponder.keyboardFrameBeginUserInfoKey] as? NSValue)?.cgRectValue.size
        keyboardHeight = keyboardSize?.height ?? 0
        print("Keyboard will be shown, height: \(keyboardHeight)")
        if getMaintainConnection() {
            createAndRepositionButtons()
        }
    }

    @objc func keyboardWillBeHidden(notification: NSNotification){
        let info = notification.userInfo!
        let keyboardSize = (info[UIResponder.keyboardFrameBeginUserInfoKey] as? NSValue)?.cgRectValue.size
        keyboardHeight = 0
        if getMaintainConnection() {
            createAndRepositionButtons()
        }
        print("Keyboard will be hidden, height: \(keyboardHeight)")
    }
    
    func moveAllButtons(yDistance: CGFloat) {
        self.bottomButtons.forEach(){ button in
            button.value.center = CGPoint(x: button.value.center.x, y: button.value.center.y + yDistance)
        }
    }

    func registerForNotifications(){
        NotificationCenter.default.addObserver(self, selector: #selector(self.keyboardWillBeHidden(notification:)), name: UIResponder.keyboardWillHideNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(self.keyboardWasShown(notification:)), name: UIResponder.keyboardWillShowNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(self.orientationChanged),
            name: UIDevice.orientationDidChangeNotification, object: nil)
    }

    func deregisterFromNotifications(){
        NotificationCenter.default.removeObserver(self)
    }
    
    @objc func orientationChanged(_ notification: NSNotification) {
        print("Device rotated, correcting button layout.")
        if getMaintainConnection() {
            correctTopSpacingForOrientation()
            removeButtons()
            createAndRepositionButtons()
            addButtons()
        }
    }
    
    func correctTopSpacingForOrientation() {
        if UIDevice.current.orientation.isLandscape {
            print("Landscape")
            topSpacing = 0
        }

        if UIDevice.current.orientation.isPortrait {
            print("Portrait")
            topSpacing = 20
        }
    }
    
    func createAndRepositionButtons() {
        print("Creating buttons")
        if (self.bottomButtons["keyboardButton"] == nil) {
            let b = CustomTextInput()
            b.setTitle("K", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(b, action: #selector(b.toggleFirstResponder), for: .touchDown)
            self.bottomButtons["keyboardButton"] = b
        }
        self.bottomButtons["keyboardButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-buttonWidth,
            y: globalWindow!.frame.height-3*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["disconnectButton"] == nil) {
            let b = UIButton()
            b.setTitle("D", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(globalStateKeeper, action: #selector(globalStateKeeper?.disconnect), for: .touchDown)
            self.bottomButtons["disconnectButton"] = b
        }
        self.bottomButtons["disconnectButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-buttonWidth,
            y: globalWindow!.frame.height-4*buttonHeight-buttonSpacing-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)
        
        if (self.bottomButtons["ctrlButton"] == nil) {
            let ctrlButton = ToggleButton(frame: CGRect(), title: "C", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Control_L, toggle: true)
            self.bottomButtons["ctrlButton"] = ctrlButton
        }
        self.bottomButtons["ctrlButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["superButton"] == nil) {
            let superButton = ToggleButton(frame: CGRect(), title: "S", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Super_L, toggle: true)
            self.bottomButtons["superButton"] = superButton
        }
        self.bottomButtons["superButton"]!.frame = CGRect(x: 1*buttonWidth+1*buttonSpacing, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["altButton"] == nil) {
            let altButton = ToggleButton(frame: CGRect(), title: "A", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Alt_L, toggle: true)
            self.bottomButtons["altButton"] = altButton
        }
        self.bottomButtons["altButton"]!.frame = CGRect(x: 2*buttonWidth+2*buttonSpacing, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["tabButton"] == nil) {
            let tabButton = ToggleButton(frame: CGRect(), title: "T", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Tab, toggle: false)
            self.bottomButtons["tabButton"] = tabButton
        }
        self.bottomButtons["tabButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-2*buttonHeight-1*buttonSpacing-self.keyboardHeight, width: buttonWidth, height: buttonHeight)
        
        if (self.bottomButtons["leftButton"] == nil) {
            let leftButton = ToggleButton(frame: CGRect(), title: "←", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Left, toggle: false)
            self.bottomButtons["leftButton"] = leftButton
        }
        self.bottomButtons["leftButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-3*buttonWidth-2*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["rightButton"] == nil) {
            let rightButton = ToggleButton(frame: CGRect(), title: "→", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Right, toggle: false)
            self.bottomButtons["rightButton"] = rightButton
        }
        self.bottomButtons["rightButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-1*buttonWidth-0*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["upButton"] == nil) {
            let upButton = ToggleButton(frame: CGRect(), title: "↑", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Up, toggle: false)
            self.bottomButtons["upButton"] = upButton
        }
        self.bottomButtons["upButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-2*buttonWidth-1*buttonSpacing,
            y: globalWindow!.frame.height-2*buttonHeight-1*buttonSpacing-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.bottomButtons["downButton"] == nil) {
            let downButton = ToggleButton(frame: CGRect(), title: "↓", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Down, toggle: false)
            self.bottomButtons["downButton"] = downButton
        }
        self.bottomButtons["downButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-2*buttonWidth-1*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)
    }

    func addButtons() {
        print("Adding buttons")
        self.bottomButtons.forEach(){ button in
            globalWindow!.addSubview(button.value)
        }
    }

    func removeButtons() {
        print("Removing buttons")
        self.bottomButtons.forEach(){ button in
            button.value.removeFromSuperview()
        }
    }

}
