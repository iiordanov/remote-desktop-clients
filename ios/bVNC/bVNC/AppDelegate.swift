//
//  AppDelegate.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import UIKit

@available(macCatalyst 13.4, *)
private extension AppDelegate {
    func keyPressed(_ key: UIKey) {
        switch key.keyCode {
        case .keyboardLeftControl:
            print("keyboardLeftControl down")
            sendUniDirectionalKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Control_L, true)
        case .keyboardRightControl:
            print("keyboardRightControl down")
            sendUniDirectionalKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Control_R, true)
        case .keyboardLeftAlt:
            print("keyboardLeftAlt down")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Alt_L, true)
        case .keyboardRightAlt:
            print("keyboardRightAlt down")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Alt_R, true)
        case .keyboardLeftShift:
            print("keyboardLeftShift down")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Shift_L, true)
        case .keyboardRightShift:
            print("keyboardRightShift down")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Shift_R, true)
        default:
            if key.modifierFlags.contains(.command) {
                print("command down")
                sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Super_L, true)
            }
            break
        }

        for text in key.charactersIgnoringModifiers {
            for char in text.unicodeScalars {
                Background {
                    if !sendKeyEventInt(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, Int32(String(char.value))!, true) {
                        sendKeyEvent(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, String(char), true)
                    }
                }
                globalStateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
            }
        }
    }
    
    func keyReleased(_ key: UIKey) {
        for text in key.charactersIgnoringModifiers {
            for char in text.unicodeScalars {
                Background {
                    if !sendKeyEventInt(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, Int32(String(char.value))!, false) {
                        sendKeyEvent(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, String(char), false)
                    }
                }
                globalStateKeeper?.rescheduleScreenUpdateRequest(timeInterval: 0.2, fullScreenUpdate: false, recurring: false)
            }
        }
        
        switch key.keyCode {
        case .keyboardLeftControl:
            print("keyboardLeftControl up")
            sendUniDirectionalKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Control_L, false)
        case .keyboardRightControl:
            print("keyboardRightControl up")
            sendUniDirectionalKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Control_R, false)
        case .keyboardLeftAlt:
            print("keyboardLeftAlt up")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Alt_L, false)
        case .keyboardRightAlt:
            print("keyboardRightAlt up")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Alt_R, false)
        case .keyboardLeftShift:
            print("keyboardLeftShift up")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Shift_L, false)
        case .keyboardRightShift:
            print("keyboardRightShift up")
            sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Shift_R, false)
        default:
            if key.modifierFlags.contains(.command) {
                print("command up")
                sendKeyEventWithKeySym(globalStateKeeper!.cl[globalStateKeeper!.currInst]!, XK_Super_L, false)
            }
            break
        }
    }
}


@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {
    var plugin: Plugin?
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Override point for customization after application launch.
        StoreReviewHelper.incrementAppOpenedCount()
        //loadPlugin()
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
    
    /*
    override func pressesBegan(_ presses: Set<UIPress>,
                               with event: UIPressesEvent?) {
        super.pressesBegan(presses, with: event)
        if #available(macCatalyst 13.4, *) {
            presses.first?.key.map(keyPressed)
        }
    }

    override func pressesEnded(_ presses: Set<UIPress>,
                               with event: UIPressesEvent?) {
        super.pressesEnded(presses, with: event)
        if #available(macCatalyst 13.4, *) {
            presses.first?.key.map(keyReleased)
        }
    }

    override func pressesCancelled(_ presses: Set<UIPress>,
                                   with event: UIPressesEvent?) {
        super.pressesCancelled(presses, with: event)
        if #available(macCatalyst 13.4, *) {
            presses.first?.key.map(keyReleased)
        }
    }
    */
    
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
            
            let connectionsMenu = UIMenu(title: "Connections", image: nil, identifier: UIMenu.Identifier("connections"), children: [disconnectCommand])
            builder.replace(menu: .application, with: connectionsMenu)
        }
    }
    
    @objc func disconnect() {
        globalStateKeeper?.scheduleDisconnectTimer(
            wasDrawing: globalStateKeeper?.isDrawing ?? false)
    }
    
    private func loadPlugin() {
        /// 1. Form the plugin's bundle URL
        let bundleFileName = "AppKitBundle.bundle"
        guard let bundleURL = Bundle.main.builtInPlugInsURL?
                                    .appendingPathComponent(bundleFileName) else { return }

        /// 2. Create a bundle instance with the plugin URL
        guard let bundle = Bundle(url: bundleURL) else { return }

        /// 3. Load the bundle and our plugin class
        let className = "AppKitBundle.MacPlugin"
        guard let pluginClass = bundle.classNamed(className) as? Plugin.Type else { return }

        /// 4. Create an instance of the plugin class
        /*
        let data = Data()
        do {
            let plugin = try? pluginClass.init(coder: NSKeyedUnarchiver(forReadingFrom: data))
            plugin?.sayHello()
        }*/
        
        plugin = pluginClass.init()
        plugin?.sayHello()
        print("SOMETHING3", plugin?.becomeFirstResponder())
    }
}
