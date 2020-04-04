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
    
    static let bH = CGFloat(30.0)
    static let bW = CGFloat(40.0)
    static let tbW = CGFloat(30.0)
    static let bSp = CGFloat(5.0)
    static let tbSp = CGFloat(3.0)
    static let z = CGFloat(0.0)
    static let bBg = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.2)
    
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
    var clientLog: [String] = []
    var sshForwardingLock: NSLock = NSLock()
    var sshForwardingStatus: Bool = false
    var globalWriteTlsLock: NSLock = NSLock()
    var frames = 0
    var reDrawTimer: Timer = Timer()
    var orientationTimer: Timer = Timer()
    var disconnectTimer: Timer = Timer()
    var screenUpdateTimer: Timer = Timer()
    var fbW: Int32 = 0
    var fbH: Int32 = 0
    var data: UnsafeMutablePointer<UInt8>?
    var minScale: CGFloat = 0
    
    var topSpacing: CGFloat = bH
    var topButtonSpacing: CGFloat = 0.0
    var leftSpacing: CGFloat = 0.0
    
    var orientation: Int = -1 /* -1 == Uninitialized, 0 == Portrait, 1 == Landscape */
    
    var disconnectedDueToBackgrounding: Bool = false
    var currInst: Int = 0
    
    var cl: UnsafeMutableRawPointer?
    
    var isDrawing: Bool = false;
    var isKeptFresh: Bool = false;
    
    var currentTransition: String = "";
    
    // Dictionaries desctibing onscreen ToggleButton type buttons
    let topButtonData: [ String: [ String: Any ] ] = [
        "escButton": [ "title": "Esc", "lx": 0*bW+0*tbSp, "ly": z, "bg": bBg, "send": XK_Escape, "tgl": false, "top": true, "right": false ],
        "f1Button": [ "title": "F1", "lx": 1*bW+1*tbSp, "ly": z, "bg": bBg, "send": XK_F1, "tgl": false, "top": true, "right": false ],
        "f2Button": [ "title": "F2", "lx": 2*bW+2*tbSp, "ly": z, "bg": bBg, "send": XK_F2, "tgl": false, "top": true, "right": false ],
        "f3Button": [ "title": "F3", "lx": 3*bW+3*tbSp, "ly": z, "bg": bBg, "send": XK_F3, "tgl": false, "top": true, "right": false ],
        "f4Button": [ "title": "F4", "lx": 4*bW+4*tbSp, "ly": z, "bg": bBg, "send": XK_F4, "tgl": false, "top": true, "right": false ],
        "f5Button": [ "title": "F5", "lx": 5*bW+5*tbSp, "ly": z, "bg": bBg, "send": XK_F5, "tgl": false, "top": true, "right": false ],
        "f6Button": [ "title": "F6", "lx": 6*bW+6*tbSp, "ly": z, "bg": bBg, "send": XK_F6, "tgl": false, "top": true, "right": false ],
        "f7Button": [ "title": "F7", "lx": 7*bW+7*tbSp, "ly": z, "bg": bBg, "send": XK_F7, "tgl": false, "top": true, "right": false ],
        "f8Button": [ "title": "F8", "lx": 8*bW+8*tbSp, "ly": z, "bg": bBg, "send": XK_F8, "tgl": false, "top": true, "right": false ],
        "f9Button": [ "title": "F9", "lx": 9*bW+9*tbSp, "ly": z, "bg": bBg, "send": XK_F9, "tgl": false, "top": true, "right": false ],
        "f10Button": [ "title": "F10", "lx": 10*bW+10*tbSp, "ly": z, "bg": bBg, "send": XK_F10, "tgl": false, "top": true, "right": false ],
        "f11Button": [ "title": "F11", "lx": 11*bW+11*tbSp, "ly": z, "bg": bBg, "send": XK_F11, "tgl": false, "top": true, "right": false ],
        "f12Button": [ "title": "F12", "lx": 12*bW+12*tbSp, "ly": z, "bg": bBg, "send": XK_F12, "tgl": false, "top": true, "right": false ],
    ]

    let modifierButtonData: [ String: [ String: Any ] ] = [
        "ctrlButton": [ "title": "Ctr", "lx": 0*bW+0*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Control_L, "tgl": true, "top": false, "right": false ],
        "superButton": [ "title": "Sup", "lx": 1*bW+1*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Super_L, "tgl": true, "top": false, "right": false ],
        "altButton": [ "title": "Alt", "lx": 2*bW+2*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Alt_L, "tgl": true, "top": false, "right": false ],
    ]
    
    let keyboardButtonData: [ String: [ String: Any ] ] = [
        "tabButton": [ "title": "Tab", "lx": 0*bW+0*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Tab, "tgl": false, "top": false, "right": false ],
        "leftButton": [ "title": "←", "lx": 4*bW+3*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Left, "tgl": false, "top": false, "right": true ],
        "downButton": [ "title": "↓", "lx": 3*bW+2*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Down, "tgl": false, "top": false, "right": true ],
        "rightButton": [ "title": "→", "lx": 2*bW+1*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Right, "tgl": false, "top": false, "right": true ],
        "upButton": [ "title": "↑", "lx": 3*bW+2*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Up, "tgl": false, "top": false, "right": true ],
    ]

    let interfaceButtonData: [ String: [ String: Any ] ] = [
        "disconnectButton": [ "title": "Dsc", "lx": 1*bW+0*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": Int32(-1), "tgl": false, "top": false, "right": true ],
        "keyboardButton": [ "title": "Kbd", "lx": 1*bW+0*bSp, "ly": 1*bH+0*bSp, "bg": bBg, "send": Int32(-1), "tgl": false, "top": false, "right": true ],
    ]

    @objc func reDraw() {
        UserInterface {
            if (self.isDrawing) {
                self.imageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: self.data, withWidth: Int(self.fbW), withHeight: Int(self.fbH))!)
            }
        }
    }
    
    func rescheduleReDrawTimer(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32) {
        self.data = data
        self.fbW = fbW
        self.fbH = fbH
        self.reDrawTimer.invalidate()
        self.reDrawTimer = Timer.scheduledTimer(timeInterval: 0.1, target: self, selector: #selector(reDraw), userInfo: nil, repeats: false)
    }
    
    @objc func requestScreenUpdate() {
        print("Firing off a whole screen update request.")
        sendWholeScreenUpdateRequest(cl)
    }
    
    func rescheduleScreenUpdateRequest() {
        self.screenUpdateTimer.invalidate()
        self.screenUpdateTimer = Timer.scheduledTimer(timeInterval: 1, target: self, selector: #selector(requestScreenUpdate), userInfo: nil, repeats: false)
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
        yesNoDialogResponse = 0
        self.isKeptFresh = false
        self.clientLog = []
        self.clientLog.append("Client Log:\n\n")
        self.registerForNotifications()
        // Needed in case we need to save a certificate during connection or change settings.
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        let contentView = ContentView(stateKeeper: self)
        self.window!.rootViewController = MyUIHostingController(rootView: contentView)
        self.window!.makeKeyAndVisible()
        currInst = currInst + 1
        isDrawing = true;
        self.vncSession = VncSession(scene: self.scene!, window: self.window!, instance: currInst, stateKeeper: self)
        self.vncSession!.connect(currentConnection: selectedConnection)
        showConnectionInProgress()
        createAndRepositionButtons()
    }
    
    func showConnectedSession() {
        UserInterface {
            self.currentPage = "connectedSession"
        }
    }

    func showConnectionInProgress() {
        UserInterface {
            self.currentPage = "connectionInProgress"
        }
    }
    
    func reconnectIfDisconnectedDueToBackgrounding() {
        if disconnectedDueToBackgrounding {
            disconnectedDueToBackgrounding = false
            connect(index: self.connectionIndex)
        } else if !self.isDrawing {
            self.showConnections()
        }
    }
    
    func disconnectDueToBackgrounding() {
        if (self.isDrawing) {
            disconnectedDueToBackgrounding = true
            disconnect()
        }
    }
    
    @objc func lazyDisconnect() {
        print("Disconnecting and navigating to the disconnection screen")
        self.currInst += 1
        self.isDrawing = false
        self.deregisterFromNotifications()
    }

    
    @objc func disconnect() {
        let wasDrawing = self.isDrawing
        lazyDisconnect()
        UserInterface {
            self.toggleModifiersIfDown()
        }
        print("wasDrawing(): \(wasDrawing)")
        if (wasDrawing) {
            self.vncSession?.disconnect(cl: cl)
            UserInterface {
                self.removeButtons()
                self.hideKeyboard()
                self.imageView?.disableTouch()
                self.imageView?.removeFromSuperview()
                self.showConnections()
            }
        } else {
            print("\(#function) called but maintainConnection was already false")
            self.showConnections()
        }
    }
    
    func hideKeyboard() {
        (self.interfaceButtons["keyboardButton"] as? CustomTextInput)?.hideKeyboard()
    }
    
    func addNewConnection() {
        print("Adding new connection and navigating to connection setup screen")
        self.connectionIndex = -1
        self.selectedConnection = [:]
        UserInterface {
            self.currentPage = "addOrEditConnection"
        }
    }

    func editConnection(index: Int) {
        print("Editing connection at index \(index) and navigating to setup screen")
        self.connectionIndex = index
        self.selectedConnection = connections[index]
        UserInterface {
            self.currentPage = "addOrEditConnection"
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
            connection.forEach() { setting in // Iterate through new settings to avoid losing e.g. ssh and x509 fingerprints
                self.selectedConnection[setting.key] = setting.value
            }
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
        if isDrawing {
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
        if isDrawing {
            self.createAndRepositionButtons()
            self.setButtonsVisibility(buttons: keyboardButtons, isHidden: true)
            self.setButtonsVisibility(buttons: modifierButtons, isHidden: true)
            self.setButtonsVisibility(buttons: topButtons, isHidden: true)
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
        rescheduleOrientationTimer()
    }
    
    func rescheduleOrientationTimer() {
        self.reDrawTimer.invalidate()
        self.orientationTimer = Timer.scheduledTimer(timeInterval: 0.5, target: self, selector: #selector(correctTopSpacingForOrientation), userInfo: nil, repeats: false)
    }
    
    @objc func correctTopSpacingForOrientation() {
        minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))

        let windowX = window?.frame.maxX ?? 0
        let windowY = window?.frame.maxY ?? 0
        //print ("windowX: \(windowX), windowY: \(windowY)")
        var newOrientation = 0
        if (windowX > windowY) {
            newOrientation = 1
        }
        
        if newOrientation == 0 {
            //print("New orientation is portrait")
            leftSpacing = 0
            topSpacing = max(StateKeeper.bH, (globalWindow!.bounds.maxY - CGFloat(fbH)*minScale)/4)
            topButtonSpacing = 0
        } else if newOrientation == 1 {
            //print("New orientation is landscape")
            leftSpacing = (globalWindow!.bounds.maxX - CGFloat(fbW)*minScale)/2
            topSpacing = StateKeeper.bH
            topButtonSpacing = 0
        }
        
        if (newOrientation != orientation) {
            orientation = newOrientation
            imageView?.frame = CGRect(x: leftSpacing, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale)
            createAndRepositionButtons()
            print("Corrected spacing and minScale for new orientation, left: \(leftSpacing), top: \(topSpacing), minScale: \(minScale)")
        } else {
            //print("Actual orientation appears not to have changed, not disturbing the displayed image or buttons.")
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
        interfaceButtons = createButtonsFromData(populateDict: interfaceButtons, buttonData: interfaceButtonData, width: StateKeeper.bW, height: StateKeeper.bH, spacing: StateKeeper.bSp)
        interfaceButtons["disconnectButton"]?.addTarget(self, action: #selector(self.disconnect), for: .touchDown)

        topButtons = createButtonsFromData(populateDict: topButtons, buttonData: topButtonData, width: StateKeeper.tbW, height: StateKeeper.bH, spacing: StateKeeper.tbSp)
        modifierButtons = createButtonsFromData(populateDict: modifierButtons, buttonData: modifierButtonData, width: StateKeeper.bW, height: StateKeeper.bH, spacing: StateKeeper.bSp)
        keyboardButtons = createButtonsFromData(populateDict: keyboardButtons, buttonData: keyboardButtonData, width: StateKeeper.bW, height: StateKeeper.bH, spacing: StateKeeper.bSp)

    }
    
    func createButtonsFromData(populateDict: [String: UIButton], buttonData: [ String: [ String: Any ] ], width: CGFloat, height: CGFloat, spacing: CGFloat ) -> [String: UIButton] {
        var newButtonDict: [ String: UIButton ] = [:]
        buttonData.forEach() { button in
            let b = button.value
            let title = b["title"] as! String
            let topButton = b["top"] as! Bool
            let rightButton = b["right"] as! Bool
            if populateDict[button.key] == nil {
                // Create the button only if not already in the dictionary
                let background = b["bg"] as! UIColor
                let toSend = b["send"] as! Int32
                let toggle = b["tgl"] as! Bool
                let nb = ToggleButton(frame: CGRect(), title: title, background: background, stateKeeper: self, toSend: toSend, toggle: toggle)
                newButtonDict[button.key] = nb
            } else {
                // Otherwise, reuse the existing button.
                newButtonDict[button.key] = populateDict[button.key]
            }
            
            // In either case, adjust the location of the button
            // Left and right buttons have different logic for calculating x position
            var locX = b["lx"] as! CGFloat
            if rightButton {
                locX = window!.frame.width - (b["lx"] as! CGFloat)
            }
            // Top and bottom buttons have different logic for when they go up and down.
            var locY = b["ly"] as! CGFloat + topButtonSpacing
            if !topButton {
                locY = (window?.frame.height ?? 0) - (b["ly"] as! CGFloat) - self.keyboardHeight
            }
            // Top buttons can wrap around and go a row down if they are out of horizontal space.
            let windowWidth = window?.frame.maxX ?? 0
            if topButton && locX + width > window?.frame.maxX ?? 0 {
                //print ("Need to wrap button: \(title) to left and a row down")
                locY = locY + height + spacing
                locX = locX - windowWidth + width
            }
            newButtonDict[button.key]?.frame = CGRect(x: locX, y: locY, width: width, height: height)
        }
        return newButtonDict
    }

    func addButtons(buttons: [String: UIButton]) {
        //print("Adding buttons to superview")
        buttons.forEach(){ button in
            globalWindow!.addSubview(button.value)
        }
    }

    func removeButtons() {
        //print("Removing buttons from superview")
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
    
    func keepSessionRefreshed() {
        print(#function)
        BackgroundLowPrio {
            if !self.isKeptFresh {
                self.isKeptFresh = true
                self.rescheduleScreenUpdateRequest()
                while (self.isDrawing) {
                    keepSessionFresh(self.cl)
                    sleep(1)
                }
            }
        }
    }
}
