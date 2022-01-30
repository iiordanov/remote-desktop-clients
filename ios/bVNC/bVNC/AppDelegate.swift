/**
 * Copyright (C) 2020- Morpheusly Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

import UIKit
import SwiftUI

var globalStateKeeper: StateKeeper?
var globalTextInput: CustomTextInput?

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {
    var stateKeeper: StateKeeper = StateKeeper()
    var textInput: CustomTextInput?
    var commands: [UIKeyCommand]?
    var physicalKeyboardHandler = PhysicalKeyboardHandler()
    
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
        guard let currentInstance = self.stateKeeper.getCurrentInstance() else {
            log_callback_str(message: "No currently connected instance, ignoring \(#function)")
            return
        }
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
                } else if shiftDown {
                    if self.physicalKeyboardHandler.keyCodeWithShiftModifierToString[key.keyCode.rawValue] != nil {
                        text = self.physicalKeyboardHandler.keyCodeWithShiftModifierToString[key.keyCode.rawValue]!
                    } else {
                        // TODO: This means we can't send Ctrl/Alt+Shift+[:non-alpha:] that are not in the
                        // keyCodeWithShiftModifierToString map.
                        // Try implementing .control and .alternate UIKeyCommand keyCommands to avoid this limitation.
                        text = key.charactersIgnoringModifiers.uppercased()
                    }
                } else {
                    text = key.charactersIgnoringModifiers
                }
                if self.physicalKeyboardHandler.specialKeyToXKeySymMap[text] != nil {
                    let xKeySym = self.physicalKeyboardHandler.specialKeyToXKeySymMap[text]
                    sendKeyEventWithKeySym(currentInstance, xKeySym!)
                } else {
                    textInput?.insertText(text)
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
    
    func isiOSAppOnMac() -> Bool {
      if #available(iOS 14.0, *) {
        return ProcessInfo.processInfo.isiOSAppOnMac
      }
      return false
    }

    override var keyCommands: [UIKeyCommand]? {
        // Do not do all the additional work of trying to capture modifiers on iOS devices because it causes soft keyboard lag
        if !self.isiOSAppOnMac() || self.commands != nil {
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
        guard let currentInstance = self.stateKeeper.getCurrentInstance() else {
            log_callback_str(message: "No currently connected instance, ignoring \(#function)")
            return
        }
        
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
                sendUniDirectionalKeyEventWithKeySym(currentInstance, XK_Super_L, true)
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
                sendUniDirectionalKeyEventWithKeySym(currentInstance, XK_Super_L, false)
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
