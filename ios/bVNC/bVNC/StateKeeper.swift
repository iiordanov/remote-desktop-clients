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

class StateKeeper: NSObject, ObservableObject, KeyboardObserving, NSCoding {
    static let bH = CGFloat(30.0)
    static let bW = CGFloat(40.0)
    static let tbW = CGFloat(30.0)
    static let bSp = CGFloat(5.0)
    static let tbSp = CGFloat(3.0)
    static let z = CGFloat(0.0)
    static let bBg = UIColor(red: 0.9, green: 0.9, blue: 0.9, alpha: 0.5)
    
    let objectWillChange = PassthroughSubject<StateKeeper, Never>()
    var selectedConnection: [String: String]
    var connections: [Dictionary<String, String>]
    var connectionIndex: Int
    var settings = UserDefaults.standard
    var title: String?
    var localizedTitle: LocalizedStringKey?
    var message: String = ""
    var localizedMessages: [ LocalizedStringKey ] = []
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
    var sshTunnelingStarted: Bool = false
    var globalWriteTlsLock: NSLock = NSLock()
    var frames = 0
    var reDrawTimer: Timer = Timer()
    var superUpKeyTimer: Timer = Timer()
    var orientationTimer: Timer = Timer()
    var screenUpdateTimer: Timer = Timer()
    var disconnectTimer: Timer = Timer()
    var fbW: Int32 = 0
    var fbH: Int32 = 0
    var data: UnsafeMutablePointer<UInt8>?
    var minScale: CGFloat = 0
    var macOs: Bool = false;
    
    var topSpacing: CGFloat = bH
    var topButtonSpacing: CGFloat = 0.0
    var leftSpacing: CGFloat = 0.0
    
    var orientation: Int = -1 /* -1 == Uninitialized, 0 == Portrait, 1 == Landscape */
    
    var disconnectedDueToBackgrounding: Bool = false
    var currInst: Int = -1
    
    var cl: [UnsafeMutableRawPointer?]
    var maxClCapacity = 1000
    
    var isDrawing: Bool = false;
    var isKeptFresh: Bool = false;
    
    var currentTransition: String = "";
    var logLock: NSLock = NSLock()
    var spinner = UIActivityIndicatorView(style: .large)

    var allowZooming = true
    var allowPanning = true
    
    var originalImageRect: CGRect = CGRect()
    
    var modifiers = [
        XK_Control_L: false,
        XK_Control_R: false,
        XK_Alt_L: false,
        XK_Alt_R: false,
        XK_Shift_L: false,
        XK_Shift_R: false,
        XK_Super_L: false,
        XK_Super_R: false,
    ]

    // Dictionaries desctibing onscreen ToggleButton type buttons
    let topButtonData: [ String: [ String: Any ] ] = [
        "f1Button": [ "title": "F1", "lx": 1*tbW+1*tbSp, "ly": z, "bg": bBg, "send": XK_F1, "tgl": false, "top": true, "right": false ],
        "f2Button": [ "title": "F2", "lx": 2*tbW+2*tbSp, "ly": z, "bg": bBg, "send": XK_F2, "tgl": false, "top": true, "right": false ],
        "f3Button": [ "title": "F3", "lx": 3*tbW+3*tbSp, "ly": z, "bg": bBg, "send": XK_F3, "tgl": false, "top": true, "right": false ],
        "f4Button": [ "title": "F4", "lx": 4*tbW+4*tbSp, "ly": z, "bg": bBg, "send": XK_F4, "tgl": false, "top": true, "right": false ],
        "f5Button": [ "title": "F5", "lx": 5*tbW+5*tbSp, "ly": z, "bg": bBg, "send": XK_F5, "tgl": false, "top": true, "right": false ],
        "f6Button": [ "title": "F6", "lx": 6*tbW+6*tbSp, "ly": z, "bg": bBg, "send": XK_F6, "tgl": false, "top": true, "right": false ],
        "f7Button": [ "title": "F7", "lx": 7*tbW+7*tbSp, "ly": z, "bg": bBg, "send": XK_F7, "tgl": false, "top": true, "right": false ],
        "f8Button": [ "title": "F8", "lx": 8*tbW+8*tbSp, "ly": z, "bg": bBg, "send": XK_F8, "tgl": false, "top": true, "right": false ],
        "f9Button": [ "title": "F9", "lx": 9*tbW+9*tbSp, "ly": z, "bg": bBg, "send": XK_F9, "tgl": false, "top": true, "right": false ],
        "f10Button": [ "title": "F10", "lx": 10*tbW+10*tbSp, "ly": z, "bg": bBg, "send": XK_F10, "tgl": false, "top": true, "right": false ],
        "f11Button": [ "title": "F11", "lx": 11*tbW+11*tbSp, "ly": z, "bg": bBg, "send": XK_F11, "tgl": false, "top": true, "right": false ],
        "f12Button": [ "title": "F12", "lx": 12*tbW+12*tbSp, "ly": z, "bg": bBg, "send": XK_F12, "tgl": false, "top": true, "right": false ],
        "pageUp": [ "title": "⇞", "lx": 13*tbW+13*tbSp, "ly": z, "bg": bBg, "send": XK_Page_Up, "tgl": false, "top": true, "right": false ],
        "pageDown": [ "title": "⇟", "lx": 14*tbW+14*tbSp, "ly": z, "bg": bBg, "send": XK_Page_Down, "tgl": false, "top": true, "right": false ],
        "home": [ "title": "⇤", "lx": 15*tbW+15*tbSp, "ly": z, "bg": bBg, "send": XK_Home, "tgl": false, "top": true, "right": false ],
        "end": [ "title": "⇥", "lx": 16*tbW+16*tbSp, "ly": z, "bg": bBg, "send": XK_End, "tgl": false, "top": true, "right": false ],

    ]

    let modifierButtonData: [ String: [ String: Any ] ] = [
        "ctrlButton": [ "title": "⌃", "lx": 0*bW+0*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Control_L, "tgl": true, "top": false, "right": false ],
        "superButton": [ "title": "❖", "lx": 1*bW+1*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Super_L, "tgl": true, "top": false, "right": false ],
        "altButton": [ "title": "⎇", "lx": 2*bW+2*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Alt_L, "tgl": true, "top": false, "right": false ],
        "shiftButton": [ "title": "⇧", "lx": 2*bW+2*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Shift_L, "tgl": true, "top": false, "right": false ],
    ]
    
    let keyboardButtonData: [ String: [ String: Any ] ] = [
        "escButton": [ "title": "⎋", "lx": 0*bW+0*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Escape, "tgl": false, "top": false, "right": false ],
        "tabButton": [ "title": "↹", "lx": 1*bW+1*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Tab, "tgl": false, "top": false, "right": false ],
        "leftButton": [ "title": "←", "lx": 4*bW+3*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Left, "tgl": false, "top": false, "right": true ],
        "downButton": [ "title": "↓", "lx": 3*bW+2*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Down, "tgl": false, "top": false, "right": true ],
        "rightButton": [ "title": "→", "lx": 2*bW+1*bSp, "ly": 1*bH, "bg": bBg, "send": XK_Right, "tgl": false, "top": false, "right": true ],
        "upButton": [ "title": "↑", "lx": 3*bW+2*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": XK_Up, "tgl": false, "top": false, "right": true ],
    ]

    let interfaceButtonData: [ String: [ String: Any ] ] = [
        "disconnectButton": [ "title": "", "lx": 1*bW+0*bSp, "ly": 2*bH+1*bSp, "bg": bBg, "send": Int32(-1), "tgl": false, "top": false, "right": true, "image": "arrowshape.turn.up.left" ],
        "keyboardButton": [ "title": "", "lx": 1*bW+0*bSp, "ly": 1*bH+0*bSp, "bg": bBg, "send": Int32(-1), "tgl": false, "top": false, "right": true, "image": "keyboard" ],
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
        if (self.isDrawing) {
            self.reDrawTimer = Timer.scheduledTimer(timeInterval: 0.1, target: self, selector: #selector(reDraw), userInfo: nil, repeats: false)
        }
    }
    
    func rescheduleSuperKeyUpTimer() {
        self.superUpKeyTimer.invalidate()
        self.superUpKeyTimer = Timer.scheduledTimer(timeInterval: 0.2, target: self, selector: #selector(sendSuperKeyUp), userInfo: nil, repeats: false)
    }
    
    @objc func sendSuperKeyUp() {
        sendUniDirectionalKeyEventWithKeySym(self.cl[self.currInst], XK_Super_L, false)
        self.modifiers[XK_Super_L] = false
        self.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
    }
    
    @objc func requestFullScreenUpdate(sender: Timer) {
        if self.isDrawing && (sender.userInfo as! Int) == self.currInst {
            //print("Firing off a whole screen update request.")
            sendWholeScreenUpdateRequest(cl[currInst], true)
        }
    }

    @objc func requestPartialScreenUpdate(sender: Timer) {
        if self.isDrawing && (sender.userInfo as! Int) == self.currInst {
            //print("Firing off a partial screen update request.")
            sendWholeScreenUpdateRequest(cl[currInst], true)
        }
    }

    @objc func requestRecurringPartialScreenUpdate(sender: Timer) {
        if self.isDrawing && (sender.userInfo as! Int) == self.currInst {
            //print("Firing off a recurring partial screen update request.")
            sendWholeScreenUpdateRequest(cl[currInst], false)
            UserInterface {
                self.rescheduleScreenUpdateRequest(timeInterval: 30, fullScreenUpdate: false, recurring: true)
            }
        }
    }
    
    func rescheduleScreenUpdateRequest(timeInterval: TimeInterval, fullScreenUpdate: Bool, recurring: Bool) {
        UserInterface {
            self.screenUpdateTimer.invalidate()
            if (self.isDrawing) {
                if (fullScreenUpdate) {
                    //print("Scheduling full screen update")
                    self.screenUpdateTimer = Timer.scheduledTimer(timeInterval: timeInterval, target: self, selector: #selector(self.requestFullScreenUpdate(sender:)), userInfo: self.currInst, repeats: false)
                } else if !recurring {
                    //print("Scheduling non-recurring partial screen update")
                    self.screenUpdateTimer = Timer.scheduledTimer(timeInterval: timeInterval, target: self, selector: #selector(self.requestRecurringPartialScreenUpdate), userInfo: self.currInst, repeats: false)
                } else {
                    //print("Scheduling recurring partial screen update")
                    self.screenUpdateTimer = Timer.scheduledTimer(timeInterval: timeInterval, target: self, selector: #selector(self.requestRecurringPartialScreenUpdate), userInfo: self.currInst, repeats: false)
                }
            }
        }
    }
    
    override init() {
        #if targetEnvironment(macCatalyst)
            self.macOs = true
        #endif
        // Load settings for current connection
        connectionIndex = -1
        selectedConnection = [:]
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
        interfaceButtons = [:]
        keyboardButtons = [:]
        modifierButtons = [:]
        topButtons = [:]
        cl = Array<UnsafeMutableRawPointer?>(repeating:UnsafeMutableRawPointer.allocate(byteCount: 0, alignment: MemoryLayout<UInt8>.alignment), count: maxClCapacity);
    }

    required init?(coder: NSCoder) {
        // Load settings for current connection
        connectionIndex = -1
        selectedConnection = [:]
        connections = self.settings.array(forKey: "connections") as? [Dictionary<String, String>] ?? []
        interfaceButtons = [:]
        keyboardButtons = [:]
        modifierButtons = [:]
        topButtons = [:]
        cl = Array<UnsafeMutableRawPointer?>(repeating:UnsafeMutableRawPointer.allocate(byteCount: 0, alignment: MemoryLayout<UInt8>.alignment), count: maxClCapacity);
    }

    func encode(with coder: NSCoder) {
    }
    
    var currentPage: String = "connectionsList" {
        didSet {
            self.objectWillChange.send(self)
        }
    }

    func setImageView(imageView: TouchEnabledUIImageView) {
        self.imageView = imageView
    }

    func connect(index: Int) {
        log_callback_str(message: "Connecting and navigating to the connection screen")
        yesNoDialogResponse = 0
        self.isKeptFresh = false
        self.clientLog = []
        self.clientLog.append("\n\n")
        self.registerForNotifications()
        // Needed in case we need to save a certificate during connection or change settings.
        self.connectionIndex = index
        self.selectedConnection = self.connections[index]
        self.allowZooming = Bool(selectedConnection["allowZooming"] ?? "true") ?? true && !macOs
        self.allowPanning = Bool(selectedConnection["allowPanning"] ?? "true") ?? true && !macOs
        let contentView = ContentView(stateKeeper: self)
        globalWindow!.rootViewController = MyUIHostingController(rootView: contentView)
        globalWindow!.makeKeyAndVisible()
        currInst = (currInst + 1) % maxClCapacity
        isDrawing = true;
        self.toggleModifiersIfDown()
        self.vncSession = VncSession(instance: currInst, stateKeeper: self)
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
            let wasDrawing = self.isDrawing
            self.imageView?.disableTouch()
            self.isDrawing = false
            scheduleDisconnectTimer(interval: 0, wasDrawing: wasDrawing)
        }
    }
    
    @objc func lazyDisconnect() {
        log_callback_str(message: "Lazy disconnecting")
        self.imageView?.disableTouch()
        self.isDrawing = false
        self.deregisterFromNotifications()
        self.orientationTimer.invalidate()
        self.reDrawTimer.invalidate()
        self.screenUpdateTimer.invalidate()
    }

    @objc func disconnect(sender: Timer) {
        self.currInst = (currInst + 1) % maxClCapacity
        let wasDrawing = (sender.userInfo as! Bool)
        if !disconnectedDueToBackgrounding {
            _ = self.saveImage(image: self.captureScreen(imageView: self.imageView ?? UIImageView()))
        }
        UserInterface {
            self.toggleModifiersIfDown()
        }
        log_callback_str(message: "wasDrawing(): \(wasDrawing)")
        if (wasDrawing) {
            self.vncSession?.disconnect()
            UserInterface {
                self.removeButtons()
                self.hideKeyboard()
                self.imageView?.disableTouch()
                self.imageView?.removeFromSuperview()
            }
        } else {
            log_callback_str(message: "\(#function) called but wasDrawing was already false")
        }
        spinner.removeFromSuperview()
        if !wasDrawing {
            showConnections()
        }
        StoreReviewHelper.checkAndAskForReview()
    }
    
    @objc func scheduleDisconnectTimer(interval: Double = 1, wasDrawing: Bool) {
        UserInterface {
            log_callback_str(message: "Scheduling disconnect")
            self.spinner.frame.origin.x = (globalWindow?.frame.size.width ?? 0) / 2 - self.spinner.frame.size.width / 2
            self.spinner.frame.origin.y = (globalWindow?.frame.size.height ?? 0) / 2 - self.spinner.frame.size.height / 2
            self.spinner.translatesAutoresizingMaskIntoConstraints = false
            self.spinner.startAnimating()
            globalWindow?.addSubview(self.spinner)
            self.lazyDisconnect()
            self.disconnectTimer.invalidate()
            self.disconnectTimer = Timer.scheduledTimer(timeInterval: interval, target: self, selector: #selector(self.disconnect(sender:)), userInfo: wasDrawing, repeats: false)
        }
    }
    
    func hideKeyboard() {
        _ = (self.interfaceButtons["keyboardButton"] as? CustomTextInput)?.hideKeyboard()
    }
    
    func addNewConnection() {
        log_callback_str(message: "Adding new connection and navigating to connection setup screen")
        self.connectionIndex = -1
        self.selectedConnection = [:]
        UserInterface {
            self.currentPage = "addOrEditConnection"
        }
    }

    func showHelp(messages: [ LocalizedStringKey ]) {
        log_callback_str(message: "Showing help screen")
        self.localizedMessages = messages
        UserInterface {
            self.currentPage = "helpDialog"
        }
    }
    
    func editConnection(index: Int) {
        log_callback_str(message: "Editing connection at index \(index) and navigating to setup screen")
        self.connectionIndex = index
        self.selectedConnection = connections[index]
        UserInterface {
            self.currentPage = "addOrEditConnection"
        }
    }

    func deleteCurrentConnection() {
        log_callback_str(message: "Deleting connection at index \(self.connectionIndex) and navigating to list of connections screen")
        // Do something only if we were not adding a new connection.
        if connectionIndex >= 0 {
            log_callback_str(message: "Deleting connection with index \(connectionIndex)")
            let deleteScreenshotResult = deleteFile(name: connections[self.connectionIndex]["screenShotFile"]!)
            log_callback_str(message: "Deleting connection screenshot \(deleteScreenshotResult)")
            self.connections.remove(at: self.connectionIndex)
            self.selectedConnection = [:]
            self.connectionIndex = -1
            saveSettings()
        } else {
            log_callback_str(message: "We were adding a new connection, so not deleting anything")
        }
        UserInterface {
            self.currentPage = "connectionsList"
        }
    }
    
    func saveSettings() {
        log_callback_str(message: "Saving settings")
        if connectionIndex >= 0 {
            self.connections[connectionIndex] = selectedConnection
        }
        self.settings.set(self.connections, forKey: "connections")
    }
    
    func saveConnection(connection: [String: String]) {
        // Negative index indicates we are adding a connection, otherwise we are editing one.
        if (connectionIndex < 0) {
            log_callback_str(message: "Saving a new connection and navigating to list of connections")
            self.connections.append(connection)
        } else {
            log_callback_str(message: "Saving a connection at index \(self.connectionIndex) and navigating to list of connections")
            connection.forEach() { setting in // Iterate through new settings to avoid losing e.g. ssh and x509 fingerprints
                self.selectedConnection[setting.key] = setting.value
            }
        }
        self.saveSettings()
        self.showConnections()
    }
    
    func showConnections() {
        UserInterface {
            let contentView = ContentView(stateKeeper: self)
            globalWindow!.rootViewController = MyUIHostingController(rootView: contentView)
            globalWindow!.makeKeyAndVisible()
            self.spinner.removeFromSuperview()
            self.currentPage = "connectionsList"
        }
    }
    
    func showError(title: LocalizedStringKey) {
        self.localizedTitle = title
        UserInterface {
            self.currentPage = "dismissableErrorMessage"
        }
    }

    func showLog(title: LocalizedStringKey, text: String) {
        self.localizedTitle = title
        self.message = text
        UserInterface {
            self.currentPage = "dismissableMessage"
        }
    }

    func keyboardWillShow(withSize keyboardSize: CGSize) {
        log_callback_str(message: "Keyboard will be shown, height: \(self.keyboardHeight)")
        if !self.allowPanning {
            self.saveImageRect()
        }
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
        log_callback_str(message: "Keyboard will be hidden, height: \(self.keyboardHeight)")
        if !self.allowPanning {
            self.setImageRect(newRect: self.originalImageRect)
        }
        
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
        //removeKeyboardObservers(from: .default)
        NotificationCenter.default.removeObserver(self)
    }
    
    @objc func orientationChanged(_ notification: NSNotification) {
        rescheduleOrientationTimer()
    }
    
    func rescheduleOrientationTimer() {
        self.reDrawTimer.invalidate()
        if (self.isDrawing) {
            self.orientationTimer = Timer.scheduledTimer(timeInterval: 0.5, target: self, selector: #selector(correctTopSpacingForOrientation), userInfo: nil, repeats: false)
        }
    }
    
    @objc func correctTopSpacingForOrientation() {
        minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))

        let windowX = globalWindow?.frame.maxX ?? 0
        let windowY = globalWindow?.frame.maxY ?? 0
        //print ("windowX: \(windowX), windowY: \(windowY)")
        var newOrientation = 0
        if (windowX > windowY) {
            newOrientation = 1
        }
        
        if newOrientation == 0 {
            //log_callback_str(message: "New orientation is portrait")
            leftSpacing = 0
            topSpacing = max(StateKeeper.bH, (globalWindow!.bounds.maxY - CGFloat(fbH)*minScale)/4)
            topButtonSpacing = 0
        } else if newOrientation == 1 {
            //log_callback_str(message: "New orientation is landscape")
            leftSpacing = (globalWindow!.bounds.maxX - CGFloat(fbW)*minScale)/2
            topSpacing = min(StateKeeper.bH, (globalWindow!.bounds.maxY - CGFloat(fbH)*minScale)/2)
            topButtonSpacing = 0
        }
        
        if (newOrientation != orientation) {
            orientation = newOrientation
            setImageRect(newRect: CGRect(x: leftSpacing, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale))
            createAndRepositionButtons()
        } else {
            //log_callback_str(message: "Actual orientation appears not to have changed, not disturbing the displayed image or buttons.")
        }
    }
    
    func saveImageRect() {
        self.originalImageRect = imageView?.frame ?? CGRect()
    }
    
    func setImageRect(newRect: CGRect) {
        imageView?.frame = newRect
        log_callback_str(message: "Set image rect to: \(newRect)")
    }
    
    func createAndRepositionButtons() {
        log_callback_str(message: "Ensuring buttons are initialized, and positioning them where they should be")
        if (self.interfaceButtons["keyboardButton"] == nil) {
            guard let b = globalTextInput else {
                return
            }
            b.addTarget(b, action: #selector(b.toggleFirstResponder), for: .touchDown)
            if let imageName = interfaceButtonData["keyboardButton"]!["image"] {
                if let image = UIImage(systemName: imageName as! String) {
                    b.setImage(image, for: .normal)
                    b.tintColor = .white
                    b.backgroundColor = StateKeeper.bBg
                }
            }
            self.interfaceButtons["keyboardButton"] = b
        }
        interfaceButtons = createButtonsFromData(populateDict: interfaceButtons, buttonData: interfaceButtonData, width: StateKeeper.bW, height: StateKeeper.bH, spacing: StateKeeper.bSp)
        interfaceButtons["disconnectButton"]?.addTarget(self, action: #selector(self.scheduleDisconnectTimer), for: .touchDown)

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
                if let imageName = b["image"] {
                    if let image = UIImage(systemName: imageName as! String) {
                        nb.setTitle(nil, for: .normal)
                        nb.setImage(image, for: .normal)
                        nb.tintColor = .white
                    }
                }
                newButtonDict[button.key] = nb
            } else {
                // Otherwise, reuse the existing button.
                newButtonDict[button.key] = populateDict[button.key]
            }
            
            // In either case, adjust the location of the button
            // Left and right buttons have different logic for calculating x position
            var locX = b["lx"] as! CGFloat
            if rightButton {
                locX = globalWindow!.frame.width - (b["lx"] as! CGFloat)
            }
            // Top and bottom buttons have different logic for when they go up and down.
            var locY = b["ly"] as! CGFloat + topButtonSpacing
            if !topButton {
                locY = (globalWindow?.frame.height ?? 0) - (b["ly"] as! CGFloat) - self.keyboardHeight
            }
            // Top buttons can wrap around and go a row down if they are out of horizontal space.
            let windowWidth = globalWindow?.frame.maxX ?? 0
            if topButton && locX + width > globalWindow?.frame.maxX ?? 0 {
                //print ("Need to wrap button: \(title) to left and a row down")
                locY = locY + height + spacing
                locX = locX - windowWidth + width
            }
            newButtonDict[button.key]?.frame = CGRect(x: locX, y: locY, width: width, height: height)
        }
        return newButtonDict
    }

    func addButtons(buttons: [String: UIButton]) {
        //log_callback_str(message: "Adding buttons to superview")
        buttons.forEach(){ button in
            globalWindow!.addSubview(button.value)
        }
    }

    func removeButtons() {
        //log_callback_str(message: "Removing buttons from superview")
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
    func yesNoResponseRequired(title: LocalizedStringKey, messages: [ LocalizedStringKey ], nonLocalizedMessage: String) -> Int32 {
        self.localizedTitle = title
        self.localizedMessages = messages
        self.message = nonLocalizedMessage
        
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
    
    @objc func sendModifierIfNotDown(modifier: Int32) {
        if !modifiers[modifier]! {
            modifiers[modifier] = true
            //print("Sending modifier", modifier)
            sendUniDirectionalKeyEventWithKeySym(self.cl[self.currInst]!, modifier, true)
        }
    }

    @objc func releaseModifierIfDown(modifier: Int32) {
        if modifiers[modifier]! {
            modifiers[modifier] = false
            //print("Releasing modifier", modifier)
            sendUniDirectionalKeyEventWithKeySym(self.cl[self.currInst]!, modifier, false)
        }
    }
    
    func toggleModifiersIfDown() {
        modifiers.forEach {
            releaseModifierIfDown(modifier: $0.key)
        }

        self.modifierButtons.forEach() { button in
            //print ("Toggling \(button.key) if down")
            (button.value as! ToggleButton).sendUpIfToggled()
        }
    }
    
    func keepSessionRefreshed() {
        BackgroundLowPrio {
            if !self.isKeptFresh {
                log_callback_str(message: "Will keep session fresh")
                self.isKeptFresh = true
                self.rescheduleScreenUpdateRequest(timeInterval: 1, fullScreenUpdate: true, recurring: false)
                self.rescheduleScreenUpdateRequest(timeInterval: 2, fullScreenUpdate: false, recurring: true)
            }
        }
    }
    
    func captureScreen(imageView: UIImageView) -> UIImage {
        UIGraphicsBeginImageContextWithOptions(imageView.bounds.size, false, UIScreen.main.scale)
        guard let currentContext = UIGraphicsGetCurrentContext() else { return UIImage() }
        imageView.layer.render(in: currentContext)
        let image = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()
        return image!
    }
    
    func saveImage(image: UIImage) -> Bool {
        guard let data = image.jpegData(compressionQuality: 1) ?? image.pngData() else {
            return false
        }
        guard let directory = try? FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: false) as NSURL else {
            return false
        }
        do {
            try data.write(to: directory.appendingPathComponent(String(self.selectedConnection["screenShotFile"]!))!)
            return true
        } catch {
            log_callback_str(message: error.localizedDescription)
            return false
        }
    }
    
    func deleteFile(name: String) -> Bool {
        guard let directory = try? FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: false) as NSURL else {
            return false
        }
        do {
            try FileManager.default.removeItem(at: directory.appendingPathComponent(name)!)
            return true
        } catch {
            log_callback_str(message: error.localizedDescription)
            return false
        }
    }
    
	/*
    // Used to simulate failure with signal_handler
    @objc func fail() {
        Background {
            signal_handler(13, nil, nil);
        }
    }
    var failureTimer: Timer?
    func rescheduleFailureTimer() {
        UserInterface {
            log_callback_str(message: "Scheduling failure timer.")
            self.failureTimer?.invalidate()
            self.failureTimer = Timer.scheduledTimer(timeInterval: 2, target: self, selector: #selector(self.fail), userInfo: nil, repeats: false)
        }
    }
    */

}
