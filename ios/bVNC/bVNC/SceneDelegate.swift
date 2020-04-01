//
//  SceneDelegate.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

import UIKit
import SwiftUI

class MyUIHostingController<Content> : UIHostingController<Content> where Content : View {
    override var prefersStatusBarHidden: Bool {
        return true
    }
}

class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    
    var window: UIWindow?
    @ObservedObject var stateKeeper: StateKeeper = StateKeeper()

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession,
               options connectionOptions: UIScene.ConnectionOptions) {
        // Use this method to optionally configure and attach the UIWindow `window` to the provided UIWindowScene `scene`.
        // If using a storyboard, the `window` property will automatically be initialized and attached to the scene.
        // This delegate does not imply the connecting scene or session are new (see `application:configurationForConnectingSceneSession` instead).
        
        globalStateKeeper = stateKeeper

        // Create the SwiftUI view that provides the window contents.
        let contentView = ContentView(stateKeeper: self.stateKeeper)
        
        // Use a UIHostingController as window root view controller.
        
        if let windowScene = scene as? UIWindowScene {
            window = UIWindow(windowScene: windowScene)
            window!.rootViewController = MyUIHostingController(rootView: contentView)
            //window!.rootViewController?.modalPresentationCapturesStatusBarAppearance = true

            window!.makeKeyAndVisible()
            self.stateKeeper.setScene(scene: scene)
            self.stateKeeper.setWindow(window: window!)
        }
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        // Called as the scene is being released by the system.
        // This occurs shortly after the scene enters the background, or when its session is discarded.
        // Release any resources associated with this scene that can be re-created the next time the scene connects.
        // The scene may re-connect later, as its session was not neccessarily discarded (see `application:didDiscardSceneSessions` instead).
        print("\(#function) called.")
    }
    
    func sceneDidBecomeActive(_ scene: UIScene) {
        // Called when the scene has moved from an inactive state to an active state.
        // Use this method to restart any tasks that were paused (or not yet started) when the scene was inactive.
        print("\(#function) called.")
        stateKeeper.reconnectIfDisconnectedDueToBackgrounding()
    }
    
    func sceneWillResignActive(_ scene: UIScene) {
        // Called when the scene will move from an active state to an inactive state.
        // This may occur due to temporary interruptions (ex. an incoming phone call).
        print("\(#function) called.")
    }
    
    func sceneWillEnterForeground(_ scene: UIScene) {
        // Called as the scene transitions from the background to the foreground.
        // Use this method to undo the changes made on entering the background.
        print("\(#function) called.")
    }
    
    func sceneDidEnterBackground(_ scene: UIScene) {
        // Called as the scene transitions from the foreground to the background.
        // Use this method to save data, release shared resources, and store enough scene-specific state information
        // to restore the scene back to its current state.
        print("\(#function) called, disconnecting.")
        stateKeeper.disconnectDueToBackgrounding()
    }
}
