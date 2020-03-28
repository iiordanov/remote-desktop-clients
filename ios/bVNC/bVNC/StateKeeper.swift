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

let bH = CGFloat(30.0)
let bW = CGFloat(40.0)
let tbW = CGFloat(30.0)
let bSp = CGFloat(5.0)
let tbSp = CGFloat(3.0)
let z = CGFloat(0.0)
let bBg = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)

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
    var topButtons: [String: UIButton]
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
    
    var topSpacing: CGFloat = 20.0
    var topButtonSpacing: CGFloat = 20.0
    var leftSpacing: CGFloat = 0.0
    
    var orientation: Int = -1 /* -1 == Uninitialized, 0 == Portrait, 1 == Landscape */
    
    // Dictionary desctibing onscreen ToggleButton type buttons
    let topButtonData: [ String : [ String: [ String: Any ] ] ] = [
    "topButtons": [
        "escButton": [ "title": "Esc", "lx": 0*bW+0*tbSp, "ly": z, "bg": bBg, "send": XK_Escape, "tgl": false ],
        "f1Button": [ "title": "F1", "lx": 1*bW+1*tbSp, "ly": z, "bg": bBg, "send": XK_F1, "tgl": false ],
        "f2Button": [ "title": "F2", "lx": 2*bW+2*tbSp, "ly": z, "bg": bBg, "send": XK_F2, "tgl": false ],
        "f3Button": [ "title": "F3", "lx": 3*bW+3*tbSp, "ly": z, "bg": bBg, "send": XK_F3, "tgl": false ],
        "f4Button": [ "title": "F4", "lx": 4*bW+4*tbSp, "ly": z, "bg": bBg, "send": XK_F4, "tgl": false ],
        "f5Button": [ "title": "F5", "lx": 5*bW+5*tbSp, "ly": z, "bg": bBg, "send": XK_F5, "tgl": false ],
        "f6Button": [ "title": "F6", "lx": 6*bW+6*tbSp, "ly": z, "bg": bBg, "send": XK_F6, "tgl": false ],
        "f7Button": [ "title": "F7", "lx": 7*bW+7*tbSp, "ly": z, "bg": bBg, "send": XK_F7, "tgl": false ],
        "f8Button": [ "title": "F8", "lx": 8*bW+8*tbSp, "ly": z, "bg": bBg, "send": XK_F8, "tgl": false ],
        "f9Button": [ "title": "F9", "lx": 9*bW+9*tbSp, "ly": z, "bg": bBg, "send": XK_F9, "tgl": false ],
        "f10Button": [ "title": "F10", "lx": 10*bW+10*tbSp, "ly": z, "bg": bBg, "send": XK_F10, "tgl": false ],
        "f11Button": [ "title": "F11", "lx": 11*bW+11*tbSp, "ly": z, "bg": bBg, "send": XK_F11, "tgl": false ],
        "f12Button": [ "title": "F12", "lx": 12*bW+12*tbSp, "ly": z, "bg": bBg, "send": XK_F12, "tgl": false ],
        ]
    ]
    
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
        connectionIndex = -1
        selectedConnection = [:]
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
        interfaceButtons = [:]
        keyboardButtons = [:]
        modifierButtons = [:]
        topButtons = [:]
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
        // Needed in case we need to save a certificate during connection or change settings.
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        
        self.vncSession = VncSession(scene: self.scene!, stateKeeper: self, window: self.window!, instance: 0)
        self.vncSession!.connect(currentConnection: selectedConnection)
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
    
    func addNewConnection() {
        print("Adding new connection and navigating to connection setup screen")
        self.connectionIndex = -1
        self.selectedConnection = [:]
        UserInterface {
            self.currentPage = "page1"
        }
    }

    func editConnection(index: Int) {
        print("Editing connection at index \(index) and navigating to setup screen")
        self.connectionIndex = index
        self.selectedConnection = connections[index]
        UserInterface {
            self.currentPage = "page1"
        }
    }

    func deleteCurrentConnection() {
        print("Deleting connection at index \(self.connectionIndex) and navigating to list of connections screen")
        // Do something only if we were not adding a new connection.
        if connectionIndex >= 0 {
            print("Deleting connection with index \(connectionIndex)")
            self.connections.remove(at: self.connectionIndex)
            self.selectedConnection = [:]
            self.connectionIndex = -1
            saveSettings()
        } else {
            print("We were adding a new connection, so not deleting anything")
        }
        UserInterface {
            self.currentPage = "connectionsList"
        }
    }
    
    func saveSettings() {
        print("Saving settings")
        if connectionIndex >= 0 {
            self.connections[connectionIndex] = selectedConnection
        }
        self.settings.set(self.connections, forKey: "connections")
    }
    
    func saveConnection(connection: [String: String]) {
        // Negative index indicates we are adding a connection, otherwise we are editing one.
        if (connectionIndex < 0) {
            print("Saving a new connection and navigating to list of connections")
            self.connections.append(connection)
        } else {
            print("Saving a connection at index \(self.connectionIndex) and navigating to list of connections")
            self.selectedConnection = connection
        }
        self.saveSettings()
        self.showConnections()
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
            self.addButtons(buttons: self.topButtons)
            self.setButtonsVisibility(buttons: self.topButtons, isHidden: false)
        }
    }
    
    func keyboardWillHide() {
        print("Keyboard will be hidden, height: \(self.keyboardHeight)")
        self.keyboardHeight = 0
        if getMaintainConnection() {
            self.createAndRepositionButtons()
            self.setButtonsVisibility(buttons: keyboardButtons, isHidden: true)
            self.setButtonsVisibility(buttons: modifierButtons, isHidden: true)
            self.setButtonsVisibility(buttons: topButtons, isHidden: true)
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
            correctTopSpacingForOrientation()
        }
    }
    
    func correctTopSpacingForOrientation() {
        minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))

        let windowX = window?.frame.maxX ?? 0
        let windowY = window?.frame.maxY ?? 0
        var newOrientation = 0
        if (windowX > windowY) {
            newOrientation = 1
        }
        
        if newOrientation == 0 {
            print("New orientation is portrait")
            leftSpacing = 0
            topSpacing = (globalWindow!.bounds.maxY - CGFloat(fbH)*minScale)/4
            topButtonSpacing = 20
        } else if newOrientation == 1 {
            print("New orientation is landscape")
            leftSpacing = (globalWindow!.bounds.maxX - CGFloat(fbW)*minScale)/2
            topSpacing = 0
            topButtonSpacing = 0
        }
        
        if (newOrientation != orientation) {
            orientation = newOrientation
            imageView?.frame = CGRect(x: leftSpacing, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale)
            removeButtons()
            createAndRepositionButtons()
            addButtons(buttons: keyboardButtons)
            addButtons(buttons: interfaceButtons)
            setButtonsVisibility(buttons: keyboardButtons, isHidden: true)
            setButtonsVisibility(buttons: modifierButtons, isHidden: true)
            print("Corrected spacing and minScale for orientation, left: \(leftSpacing), top: \(topSpacing), minScale: \(minScale)")
        } else {
            print("Actual orientation appears not to have changed, not disturbing the displayed image or buttons.")
        }
    }
    
    func createAndRepositionButtons() {
        print("Ensuring buttons are initialized, and positioning them where they should be")
        if (self.interfaceButtons["keyboardButton"] == nil) {
            let b = CustomTextInput(stateKeeper: self)
            b.setTitle("Kbd", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(b, action: #selector(b.toggleFirstResponder), for: .touchDown)
            self.interfaceButtons["keyboardButton"] = b
        }
        self.interfaceButtons["keyboardButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-bW,
            y: globalWindow!.frame.height-1*bH-0*bSp-self.keyboardHeight,
            width: bW, height: bH)

        if (self.interfaceButtons["disconnectButton"] == nil) {
            let b = ToggleButton()
            b.setTitle("Dsc", for: [])
            b.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
            b.addTarget(self, action: #selector(self.disconnect), for: .touchDown)
            self.interfaceButtons["disconnectButton"] = b
        }
        self.interfaceButtons["disconnectButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-bW,
            y: globalWindow!.frame.height-2*bH-1*bSp-self.keyboardHeight,
            width: bW, height: bH)
        
        if (self.modifierButtons["ctrlButton"] == nil) {
            let ctrlButton = ToggleButton(frame: CGRect(), title: "Ctr", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Control_L, toggle: true)
            self.modifierButtons["ctrlButton"] = ctrlButton
        }
        self.modifierButtons["ctrlButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-1*bH-self.keyboardHeight, width: bW, height: bH)

        if (self.modifierButtons["superButton"] == nil) {
            let superButton = ToggleButton(frame: CGRect(), title: "Sup", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Super_L, toggle: true)
            self.modifierButtons["superButton"] = superButton
        }
        self.modifierButtons["superButton"]!.frame = CGRect(x: 1*bW+1*bSp, y: globalWindow!.frame.height-1*bH-self.keyboardHeight, width: bW, height: bH)

        if (self.modifierButtons["altButton"] == nil) {
            let altButton = ToggleButton(frame: CGRect(), title: "Alt", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Alt_L, toggle: true)
            self.modifierButtons["altButton"] = altButton
        }
        self.modifierButtons["altButton"]!.frame = CGRect(x: 2*bW+2*bSp, y: globalWindow!.frame.height-1*bH-self.keyboardHeight, width: bW, height: bH)

        if (self.keyboardButtons["tabButton"] == nil) {
            let tabButton = ToggleButton(frame: CGRect(), title: "Tab", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Tab, toggle: false)
            self.keyboardButtons["tabButton"] = tabButton
        }
        self.keyboardButtons["tabButton"]!.frame = CGRect(x: 0, y: globalWindow!.frame.height-2*bH-1*bSp-self.keyboardHeight, width: bW, height: bH)
        
        if (self.keyboardButtons["leftButton"] == nil) {
            let leftButton = ToggleButton(frame: CGRect(), title: "←", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Left, toggle: false)
            self.keyboardButtons["leftButton"] = leftButton
        }
        self.keyboardButtons["leftButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-4*bW-3*bSp,
            y: globalWindow!.frame.height-1*bH-self.keyboardHeight,
            width: bW, height: bH)

        if (self.keyboardButtons["rightButton"] == nil) {
            let rightButton = ToggleButton(frame: CGRect(), title: "→", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Right, toggle: false)
            self.keyboardButtons["rightButton"] = rightButton
        }
        self.keyboardButtons["rightButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-2*bW-1*bSp,
            y: globalWindow!.frame.height-1*bH-self.keyboardHeight,
            width: bW, height: bH)

        if (self.keyboardButtons["upButton"] == nil) {
            let upButton = ToggleButton(frame: CGRect(), title: "↑", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Up, toggle: false)
            self.keyboardButtons["upButton"] = upButton
        }
        self.keyboardButtons["upButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-3*bW-2*bSp,
            y: globalWindow!.frame.height-2*bH-1*bSp-self.keyboardHeight,
            width: bW, height: bH)

        if (self.keyboardButtons["downButton"] == nil) {
            let downButton = ToggleButton(frame: CGRect(), title: "↓", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2), stateKeeper: self, toSend: XK_Down, toggle: false)
            self.keyboardButtons["downButton"] = downButton
        }
        self.keyboardButtons["downButton"]!.frame = CGRect(
            x: globalWindow!.frame.width-3*bW-2*bSp,
            y: globalWindow!.frame.height-1*bH-self.keyboardHeight,
            width: bW, height: bH)
        
        createButtonsFromData(buttonData: topButtonData)
    }
    
    func createButtonsFromData(buttonData: [ String : [ String: [ String: Any ] ] ]) {
        buttonData.forEach() { buttonSet in
            buttonSet.value.forEach() { button in
                if topButtons[button.key] == nil {
                    let b = button.value
                    let title = b["title"] as! String
                    let background = b["bg"] as! UIColor
                    let toSend = b["send"] as! Int32
                    let toggle = b["tgl"] as! Bool
                    var locX = b["lx"] as! CGFloat
                    var locY = b["ly"] as! CGFloat + topButtonSpacing
                    let windowWidth = window?.frame.maxX ?? 0
                    if locX + bW > window?.frame.maxX ?? 0 {
                        print ("I should wrap this button around: \(title)")
                        locY = locY + bH + tbSp
                        locX = locX - windowWidth + tbW - 2*tbSp
                    }
                    let nb = ToggleButton(frame: CGRect(), title: title, background: background, stateKeeper: self, toSend: toSend, toggle: toggle)
                    nb.frame = CGRect(x: locX, y: locY, width: bW, height: bH)
                    topButtons[button.key] = nb
                }
            }
        }

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
        self.topButtons.forEach(){ button in
            button.value.removeFromSuperview()
        }
        self.topButtons = [:]
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
