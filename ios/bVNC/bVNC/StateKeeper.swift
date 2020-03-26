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

class StateKeeper: ObservableObject, KeyboardObserving {
    let objectWillChange = PassthroughSubject<StateKeeper, Never>()
    var selectedConnection: [String: String]
    var connections: [Dictionary<String, String>]
    var connectionIndex: Int
    var settings = UserDefaults.standard
    var scene: UIScene?
    var window: UIWindow?
    var title: String?
    var message: String?
    var yesNoDialogLock: NSLock = NSLock()
    var yesNoDialogResponse: Int32 = 0
    var imageView: TouchEnabledUIImageView?
    var vncSession: VncSession?
    var modifierButtons: [String: UIButton]
    var keyboardButtons: [String: UIButton]
    var interfaceButtons: [String: UIButton]
    var keyboardHeight: CGFloat = 0.0
    var clientLog: String = ""
    var sshForwardingLock: NSLock = NSLock()
    var sshForwardingStatus: Bool = false
    var globalWriteTlsLock: NSLock = NSLock()
    var frames = 0
    var reDrawTimer: Timer = Timer()
    var fbW: Int32 = 0
    var fbH: Int32 = 0
    var data: UnsafeMutablePointer<UInt8>?
    var minScale: CGFloat = 0
    
    let buttonHeight: CGFloat = 30.0
    let buttonWidth: CGFloat = 40.0
    var topSpacing: CGFloat = 20.0
    var leftSpacing: CGFloat = 0.0
    let buttonSpacing: CGFloat = 5.0

    func redraw() {
        self.imageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: self.data, withWidth: Int(self.fbW), withHeight: Int(self.fbH))!)
    }
    
    @objc func fireTimer() {
        redraw()
    }
    
    func rescheduleTimer(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32) {
        self.data = data
        self.fbW = fbW
        self.fbH = fbH
        self.reDrawTimer.invalidate()
        self.reDrawTimer = Timer.scheduledTimer(timeInterval: 0.2, target: self, selector: #selector(fireTimer), userInfo: nil, repeats: false)
    }
    
    init() {
        // Load settings for current connection
        selectedConnection = self.settings.dictionary(forKey: "selectedConnection") as? [String:String] ?? [:]
        connectionIndex = self.settings.integer(forKey: "connectionIndex")
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
        interfaceButtons = [:]
        keyboardButtons = [:]
        modifierButtons = [:]
    }
    
    var currentPage: String = "connectionsList" {
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
        goToBlankPage()
        yesNoDialogResponse = 0
        self.clientLog = "Client Log:\n\n"
        self.registerForNotifications()
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!, instance: 0)
        self.vncSession!.connect(currentConnection: self.selectedConnection)
    }
    
    func goToConnectedSession() {
        UserInterface {
            self.currentPage = "connectedSession"
        }
    }

    func goToBlankPage() {
        UserInterface {
            self.currentPage = "blankPage"
        }
    }
    
    @objc func disconnect() {
        print("Disconnecting and navigating to the disconnection screen")
        self.deregisterFromNotifications()
        UserInterface {
            self.toggleModifiersIfDown()
        }
        self.vncSession?.disconnect()
        UserInterface {
            self.removeButtons()
            (self.interfaceButtons["keyboardButton"] as! CustomTextInput).hideKeyboard()
            self.imageView?.disableTouch()
            self.currentPage = "page3"
        }
    }
    
    func addNew() {
        print("Adding new connection and navigating to connection setup screen")
        self.connectionIndex = -1
        self.selectedConnection = [:]
        UserInterface {
            self.currentPage = "page1"
        }
    }

    func edit(index: Int) {
        print("Editing connection at index \(index)")
        self.connectionIndex = index
        self.selectedConnection = connections[index]
        print("Navigating to setup screen for connection at index \(self.connectionIndex)")
        UserInterface {
            self.currentPage = "page1"
        }
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
        UserInterface {
            self.currentPage = "connectionsList"
        }
    }
    
    func saveSettings() {
        print("Saving settings")
        self.settings.set(self.selectedConnection, forKey: "selectedConnection")
        self.settings.set(self.connectionIndex, forKey: "connectionIndex")
        self.connections[connectionIndex] = self.selectedConnection
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
        UserInterface {
            self.currentPage = "connectionsList"
        }
    }
    
    func showConnections() {
        UserInterface {
            self.currentPage = "connectionsList"
        }
    }
    
    func showError(title: String) {
        self.title = title
        UserInterface {
            self.currentPage = "dismissableErrorMessage"
        }
    }
    
    func keyboardWillShow(withSize keyboardSize: CGSize) {
        print("Keyboard will be shown, height: \(self.keyboardHeight)")
        self.keyboardHeight = keyboardSize.height
        if getMaintainConnection() {
            self.createAndRepositionButtons()
            self.addButtons(buttons: self.keyboardButtons)
            self.setButtonsVisibility(buttons: self.keyboardButtons, isHidden: false)
            self.addButtons(buttons: self.modifierButtons)
            self.setButtonsVisibility(buttons: self.modifierButtons, isHidden: false)
        }
    }
    
    func keyboardWillHide() {
      print("Keyboard will be hidden, height: \(self.keyboardHeight)")
      self.keyboardHeight = 0
      if getMaintainConnection() {
          self.createAndRepositionButtons()
          self.setButtonsVisibility(buttons: keyboardButtons, isHidden: true)
          self.setButtonsVisibility(buttons: modifierButtons, isHidden: true)
      }
    }
    
    func moveAllButtons(yDistance: CGFloat) {
        self.interfaceButtons.forEach() { button in
            button.value.center = CGPoint(x: button.value.center.x, y: button.value.center.y + yDistance)
        }
        self.keyboardButtons.forEach() { button in
            button.value.center = CGPoint(x: button.value.center.x, y: button.value.center.y + yDistance)
        }
        self.modifierButtons.forEach() { button in
            button.value.center = CGPoint(x: button.value.center.x, y: button.value.center.y + yDistance)
        }
    }

    func setButtonsVisibility(buttons: [String: UIButton], isHidden: Bool) {
        buttons.forEach() { button in
            button.value.isHidden = isHidden
        }
    }
    
    func registerForNotifications() {
        addKeyboardObservers(to: .default)
        NotificationCenter.default.addObserver(self, selector: #selector(self.orientationChanged),
            name: UIDevice.orientationDidChangeNotification, object: nil)
    }

    func deregisterFromNotifications(){
        removeKeyboardObservers(from: .default)
        NotificationCenter.default.removeObserver(self)
    }
    
    @objc func orientationChanged(_ notification: NSNotification) {
        print("Orientation changed")
        if getMaintainConnection() && currentPage == "connectedSession" {
            //print("Device rotated, correcting button layout.")
            correctTopSpacingForOrientation()
            removeButtons()
            createAndRepositionButtons()
            addButtons(buttons: keyboardButtons)
            addButtons(buttons: interfaceButtons)
            setButtonsVisibility(buttons: keyboardButtons, isHidden: true)
            setButtonsVisibility(buttons: modifierButtons, isHidden: true)
        }
    }
    
    func correctTopSpacingForOrientation() {
        minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))
        print("Orientation isValidInterfaceOrientation \(UIDevice.current.orientation.isValidInterfaceOrientation)")

        if UIDevice.current.orientation.isFlat {
            print("Orientation is flat")
            leftSpacing = (globalWindow!.bounds.maxX - CGFloat(fbW)*minScale)/2
            topSpacing = 0
        }

        if UIDevice.current.orientation.isLandscape {
            print("Orientation is landscape")
            leftSpacing = (globalWindow!.bounds.maxX - CGFloat(fbW)*minScale)/2
            topSpacing = 0
        }
        
        if UIDevice.current.orientation.isPortrait {
            print("Orientation is portrait")
            leftSpacing = 0
            topSpacing = (globalWindow!.bounds.maxY - CGFloat(fbH)*minScale)/4
        }
        imageView?.frame = CGRect(x: leftSpacing, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale)
        print("Corrected spacing and minScale for orientation, left: \(leftSpacing), top: \(topSpacing), minScale: \(minScale)")
    }
    
    func createAndRepositionButtons() {
        //print("Creating buttons")
        if (self.interfaceButtons["keyboardButton"] == nil) {
            let b = CustomTextInput(stateKeeper: self)
            b.setTitle("K", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(b, action: #selector(b.toggleFirstResponder), for: .touchDown)
            self.interfaceButtons["keyboardButton"] = b
        }
        self.interfaceButtons["keyboardButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-buttonWidth,
            y: globalWindow!.frame.height-1*buttonHeight-0*buttonSpacing-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.interfaceButtons["disconnectButton"] == nil) {
            let b = UIButton()
            b.setTitle("D", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(self, action: #selector(self.disconnect), for: .touchDown)
            self.interfaceButtons["disconnectButton"] = b
        }
        self.interfaceButtons["disconnectButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-buttonWidth,
            y: globalWindow!.frame.height-2*buttonHeight-1*buttonSpacing-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)
        
        if (self.modifierButtons["ctrlButton"] == nil) {
            let ctrlButton = ToggleButton(frame: CGRect(), title: "C", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Control_L, toggle: true)
            self.modifierButtons["ctrlButton"] = ctrlButton
        }
        self.modifierButtons["ctrlButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.modifierButtons["superButton"] == nil) {
            let superButton = ToggleButton(frame: CGRect(), title: "S", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Super_L, toggle: true)
            self.modifierButtons["superButton"] = superButton
        }
        self.modifierButtons["superButton"]!.frame = CGRect(x: 1*buttonWidth+1*buttonSpacing, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.modifierButtons["altButton"] == nil) {
            let altButton = ToggleButton(frame: CGRect(), title: "A", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Alt_L, toggle: true)
            self.modifierButtons["altButton"] = altButton
        }
        self.modifierButtons["altButton"]!.frame = CGRect(x: 2*buttonWidth+2*buttonSpacing, y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight, width: buttonWidth, height: buttonHeight)

        if (self.keyboardButtons["tabButton"] == nil) {
            let tabButton = ToggleButton(frame: CGRect(), title: "T", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Tab, toggle: false)
            self.keyboardButtons["tabButton"] = tabButton
        }
        self.keyboardButtons["tabButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-2*buttonHeight-1*buttonSpacing-self.keyboardHeight, width: buttonWidth, height: buttonHeight)
        
        if (self.keyboardButtons["leftButton"] == nil) {
            let leftButton = ToggleButton(frame: CGRect(), title: "←", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Left, toggle: false)
            self.keyboardButtons["leftButton"] = leftButton
        }
        self.keyboardButtons["leftButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-4*buttonWidth-3*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.keyboardButtons["rightButton"] == nil) {
            let rightButton = ToggleButton(frame: CGRect(), title: "→", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Right, toggle: false)
            self.keyboardButtons["rightButton"] = rightButton
        }
        self.keyboardButtons["rightButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-2*buttonWidth-1*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.keyboardButtons["upButton"] == nil) {
            let upButton = ToggleButton(frame: CGRect(), title: "↑", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Up, toggle: false)
            self.keyboardButtons["upButton"] = upButton
        }
        self.keyboardButtons["upButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-3*buttonWidth-2*buttonSpacing,
            y: globalWindow!.frame.height-2*buttonHeight-1*buttonSpacing-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)

        if (self.keyboardButtons["downButton"] == nil) {
            let downButton = ToggleButton(frame: CGRect(), title: "↓", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Down, toggle: false)
            self.keyboardButtons["downButton"] = downButton
        }
        self.keyboardButtons["downButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-3*buttonWidth-2*buttonSpacing,
            y: globalWindow!.frame.height-1*buttonHeight-self.keyboardHeight,
            width: buttonWidth, height: buttonHeight)
    }

    func addButtons(buttons: [String: UIButton]) {
        //print("Adding buttons")
        buttons.forEach(){ button in
            globalWindow!.addSubview(button.value)
        }
    }

    func removeButtons() {
        //print("Removing buttons")
        self.interfaceButtons.forEach(){ button in
            button.value.removeFromSuperview()
        }
        self.modifierButtons.forEach(){ button in
            button.value.removeFromSuperview()
        }
        self.keyboardButtons.forEach(){ button in
            button.value.removeFromSuperview()
        }
    }
    
    /*
     Indicates the user will need to answer yes / no at a dialog.
     */
    func yesNoResponseRequired(title: String, message: String) -> Int32 {
        self.title = title
        self.message = message
        
        // Make sure current thread does not hold the lock
        self.yesNoDialogLock.unlock()

        UserInterface {
            // Acquire the lock on the UI thread
            self.yesNoDialogLock.unlock()
            self.yesNoDialogLock.lock()

            self.yesNoDialogResponse = 0
            self.currentPage = "yesNoMessage"
        }
        
        // Allow some time for the UI thread to aquire the lock
        Thread.sleep(forTimeInterval: 1)
        // Wait for approval on a lock held by the UI thread.
        self.yesNoDialogLock.lock()
        // Release the lock
        self.yesNoDialogLock.unlock()
        
        return self.yesNoDialogResponse
    }
    
    /*
     Sets the user's response to to a yes / no dialog.
     */
    func setYesNoReponse(response: Bool, pageYes: String, pageNo: String) {
        var responseInt: Int32 = 0
        if response {
            responseInt = 1
        }
        UserInterface {
            self.yesNoDialogResponse = responseInt
            self.yesNoDialogLock.unlock()
            if response {
                self.currentPage = pageYes
            } else {
                self.currentPage = pageNo
            }
        }
    }
    
    func toggleModifiersIfDown() {
        self.modifierButtons.forEach() { button in
            print ("Toggling \(button.key) if down")
            (button.value as! ToggleButton).sendUpIfToggled()
        }
    }
}
