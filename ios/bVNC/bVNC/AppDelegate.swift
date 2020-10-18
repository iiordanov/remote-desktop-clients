//
//  AppDelegate.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import UIKit

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Override point for customization after application launch.
        StoreReviewHelper.incrementAppOpenedCount()
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
            
            let connectionsMenu = UIMenu(title: "Connections", image: nil, identifier: UIMenu.Identifier("connections"), children: [disconnectCommand])
            builder.replace(menu: .application, with: connectionsMenu)
        }
    }
    
    @objc func disconnect() {
        globalStateKeeper?.scheduleDisconnectTimer(
            wasDrawing: globalStateKeeper?.isDrawing ?? false)
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
