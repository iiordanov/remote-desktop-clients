//
//  AppDelegate.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import UIKit
import SwiftUI

var globalStateKeeper: StateKeeper?
var globalTextInput: CustomTextInput?

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {
    var stateKeeper: StateKeeper = StateKeeper()
    var textInput: CustomTextInput?
    var commands: [UIKeyCommand]?
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Override point for customization after application launch.
        StoreReviewHelper.incrementAppOpenedCount()
        globalStateKeeper = stateKeeper
        textInput = CustomTextInput(stateKeeper: stateKeeper)
        globalTextInput = textInput
        return true
    }

    // MARK: UISceneSession Lifecycle

    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        // Called when a new scene session is being created.
        // Use this method to select a configuration to create the new scene with.
        return UISceneConfiguration(name: "Default Configuration", sessionRole: connectingSceneSession.role)
    }

    func application(_ application: UIApplication, didDiscardSceneSessions sceneSessions: Set<UISceneSession>) {
        // Called when the user discards a scene session.
        // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
        // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
    }
    
    override func buildMenu(with builder: UIMenuBuilder) {
        if builder.system == .main {
            builder.remove(menu: .edit)
            builder.remove(menu: .format)
            builder.remove(menu: .help)
            builder.remove(menu: .file)
            builder.remove(menu: .window)
            builder.remove(menu: .view)
            
            let disconnectCommand = UICommand(title: "Disconnect",
                      action: #selector(disconnect),
                      discoverabilityTitle: "disconnect")
            let quitCommand = UICommand(title: "Quit",
                      action: #selector(quit),
                      discoverabilityTitle: "quit")

            let actionsMenu = UIMenu(title: "Actions", image: nil, identifier: UIMenu.Identifier("actions"), children: [disconnectCommand, quitCommand])
            builder.replace(menu: .application, with: actionsMenu)
        }
    }
    
    @objc func disconnect() {
        globalStateKeeper?.scheduleDisconnectTimer(interval: 1,
            wasDrawing: globalStateKeeper?.isDrawing ?? false)
        globalStateKeeper?.scheduleDisconnectTimer(interval: 2, wasDrawing: false)
    }

    @objc func quit() {
        disconnect()
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
            UIApplication.shared.perform(#selector(NSXPCConnection.suspend))
             DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) {
              exit(0)
             }
        }
    }

    override func pressesBegan(_ presses: Set<UIPress>,
                               with event: UIPressesEvent?) {
        for p in presses {
            guard let key = p.key else {
                continue
            }
            var shiftDown = false
            var altOrCtrlDown = false
            if key.modifierFlags.contains(.control) {
                altOrCtrlDown = true
                self.stateKeeper.sendModifierIfNotDown(modifier: XK_Control_L)
            }
            if key.modifierFlags.contains(.alternate) {
                altOrCtrlDown = true
                self.stateKeeper.sendModifierIfNotDown(modifier: XK_Alt_L)
            }
            if key.modifierFlags.contains(.shift) {
                shiftDown = true
                self.stateKeeper.sendModifierIfNotDown(modifier: XK_Shift_L)
            }
            if key.modifierFlags.contains(.alphaShift) {
                shiftDown = true
            }

            if key.characters != "" {
                var text = ""
                if shiftDown && !altOrCtrlDown {
                    text = key.characters
                } else {
                    // TODO: This means we can't send Ctrl/Alt+Shift+[:non-alpha:] keys like Ctrl+Shift+=.
                    // Try implementing .control and .alternate UIKeyCommand keyCommands to avoid this limitation.
                    if shiftDown {
                        text = key.charactersIgnoringModifiers.uppercased()
                    } else {
                        text = key.charactersIgnoringModifiers
                    }
                }
                if #available(iOS 13.4, *) {
                    let specialKeyMap = [
                        UIKeyCommand.f1: XK_F1,
                        UIKeyCommand.f2: XK_F2,
                        UIKeyCommand.f3: XK_F3,
                        UIKeyCommand.f4: XK_F4,
                        UIKeyCommand.f5: XK_F5,
                        UIKeyCommand.f6: XK_F6,
                        UIKeyCommand.f7: XK_F7,
                        UIKeyCommand.f8: XK_F8,
                        UIKeyCommand.f9: XK_F9,
                        UIKeyCommand.f10: XK_F10,
                        UIKeyCommand.f11: XK_F11,
                        UIKeyCommand.f12: XK_F12,
                        UIKeyCommand.inputEscape: XK_Escape,
                        UIKeyCommand.inputHome: XK_Home,
                        UIKeyCommand.inputEnd: XK_End,
                        UIKeyCommand.inputPageUp: XK_Page_Up,
                        UIKeyCommand.inputPageDown: XK_Page_Down,
                        UIKeyCommand.inputUpArrow: XK_Up,
                        UIKeyCommand.inputDownArrow: XK_Down,
                        UIKeyCommand.inputLeftArrow: XK_Left,
                        UIKeyCommand.inputRightArrow: XK_Right,
                    ]
                    if specialKeyMap[text] != nil {
                        sendKeyEventWithKeySym(self.stateKeeper.cl[self.stateKeeper.currInst]!, specialKeyMap[text]!)
                    } else {
                        textInput?.insertText(text)
                    }
                }
            }
        }
    }

    override func pressesEnded(_ presses: Set<UIPress>,
                               with event: UIPressesEvent?) {
        for p in presses {
            guard let key = p.key else {
                continue
            }
            if key.modifierFlags.contains(.control) {
                self.stateKeeper.releaseModifierIfDown(modifier: XK_Control_L)
            }
            if key.modifierFlags.contains(.alternate) {
                self.stateKeeper.releaseModifierIfDown(modifier: XK_Alt_L)
            }
            if key.modifierFlags.contains(.shift) {
                self.stateKeeper.releaseModifierIfDown(modifier: XK_Shift_L)
            }
        }
    }


    override func pressesCancelled(_ presses: Set<UIPress>,
                                   with event: UIPressesEvent?) {
        pressesEnded(presses, with: event)
    }
    
    override var keyCommands: [UIKeyCommand]? {
        if self.commands != nil {
            return self.commands
        }
        self.commands = (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .shift], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .alternate], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .control], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .shift, .alternate], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .shift, .control], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .control, .alternate], action: #selector(captureCmd))})
        commands! += (0...255).map({UIKeyCommand(input: String(UnicodeScalar($0)), modifierFlags: [.command, .control, .alternate, .shift], action: #selector(captureCmd))})
        commands! += [
            //UIKeyCommand(input: "", modifierFlags: [.command], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .shift], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .alternate], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .control], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .control, .shift], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .control, .alternate], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .alternate, .shift], action: #selector(captureCmd)),
            UIKeyCommand(input: "", modifierFlags: [.command, .control, .alternate, .shift], action: #selector(captureCmd))
        ]
        return self.commands
    }
    
    @objc func captureCmd(sender: UIKeyCommand) {
        var anotherModifier = false
        if sender.modifierFlags.contains(.control) {
            self.stateKeeper.sendModifierIfNotDown(modifier: XK_Control_L)
            anotherModifier = true
        }
        if sender.modifierFlags.contains(.alternate) {
            self.stateKeeper.sendModifierIfNotDown(modifier: XK_Alt_L)
            anotherModifier = true
        }
        if sender.modifierFlags.contains(.shift) {
            self.stateKeeper.modifiers[XK_Shift_L] = true
            anotherModifier = true
        }

        /*
         // This implementation is able to send a single Start/Super key command, but
         // causes stray Start/Super key to be sent when Command-Tabbing away from the app.
         // UIKeyCommand(input: "", modifierFlags: [.command], action: #selector(captureCmd))
         // needs to be added above
        if sender.modifierFlags.contains(.command) {
            self.stateKeeper.sendModifierIfNotDown(modifier: XK_Super_L)
            self.stateKeeper.rescheduleSuperKeyUpTimer()
        }
        
        if sender.input != "" {
            if self.stateKeeper.modifiers[XK_Shift_L]! {
                textInput?.insertText(sender.input!.uppercased())
            } else {
                textInput?.insertText(sender.input!.lowercased())
            }
        } else if sender.input == "" && !anotherModifier {
            self.stateKeeper.releaseModifierIfDown(modifier: XK_Control_L)
            self.stateKeeper.releaseModifierIfDown(modifier: XK_Alt_L)
            self.stateKeeper.modifiers[XK_Shift_L] = false
        }
        */
        
        if sender.input != "" || anotherModifier {
            if !self.stateKeeper.modifiers[XK_Super_L]! {
                self.stateKeeper.modifiers[XK_Super_L] = true
                print("Command key not down and sent with a different modifier or key, sending Super down")
                sendUniDirectionalKeyEventWithKeySym(self.stateKeeper.cl[self.stateKeeper.currInst]!, XK_Super_L, true)
            }
            if sender.input != "" {
                if self.stateKeeper.modifiers[XK_Shift_L]! {
                    textInput?.insertText(sender.input!.uppercased())
                } else {
                    textInput?.insertText(sender.input!.lowercased())
                }
            }
        }

        if sender.input == "" && !anotherModifier {
            if self.stateKeeper.modifiers[XK_Super_L]! {
                self.stateKeeper.modifiers[XK_Super_L] = false
                print("Command key was previously marked as down, sending Super up")
                sendUniDirectionalKeyEventWithKeySym(self.stateKeeper.cl[self.stateKeeper.currInst]!, XK_Super_L, false)
                self.stateKeeper.releaseModifierIfDown(modifier: XK_Control_L)
                self.stateKeeper.releaseModifierIfDown(modifier: XK_Alt_L)
                self.stateKeeper.modifiers[XK_Shift_L] = false
                
            }
        }
    }
}

extension UIApplication {

    static var appVersion: String? {
        return Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String
    }

    static var appId: String? {
        return Bundle.main.bundleIdentifier
    }
}
