//
//  SceneDelegate.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

import UIKit
import SwiftUI

func Background(_ block: @escaping ()->Void) {
    DispatchQueue.global(qos: .default).async(execute: block)
}

func UserInterface(_ block: @escaping ()->Void) {
    DispatchQueue.main.async(execute: block)
}

var globalContentView: Image?
var globalScene: UIWindowScene?
var globalWindow: UIWindow?
var globalImageView: UIImageView?

func callback(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Void {
    UserInterface {
        //print("On UI Thread: ", fbW, fbH, x, y, w, h)
        globalImageView?.removeFromSuperview()
        globalImageView = nil
        globalImageView = UIImageView(frame: CGRect(x: 0, y: 0, width: Int(fbW), height: Int(fbH)))
        globalImageView?.image = imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))
        globalImageView!.frame = globalWindow!.bounds
        globalWindow!.addSubview(globalImageView!)
    }
}

func imageFromARGB32Bitmap(pixels: UnsafeMutablePointer<UInt8>?, withWidth: Int, withHeight: Int) -> UIImage? {
    guard withWidth > 0 && withHeight > 0 else { return nil }
    let rgbColorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue).union(.byteOrder32Big)
    let bitsPerComponent = 8
    let bitsPerPixel = 32
    guard let providerRef = CGDataProvider(data: NSData(bytes: pixels, length: withWidth*withHeight*4)) else { return nil }
    guard let cgim = CGImage(width: withWidth,
                             height: withHeight,
                             bitsPerComponent: bitsPerComponent,
                             bitsPerPixel: bitsPerPixel,
                             bytesPerRow: 4*withWidth,
                             space: rgbColorSpace,
                             bitmapInfo: bitmapInfo,
                             provider: providerRef,
                             decode: nil,
                             shouldInterpolate: true,
                             intent: .defaultIntent) else { return nil }
    return UIImage(cgImage: cgim)
}

class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    
    var window: UIWindow?
    
    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        // Use this method to optionally configure and attach the UIWindow `window` to the provided UIWindowScene `scene`.
        // If using a storyboard, the `window` property will automatically be initialized and attached to the scene.
        // This delegate does not imply the connecting scene or session are new (see `application:configurationForConnectingSceneSession` instead).
        
        // Create the SwiftUI view that provides the window contents.
        //let contentView = ContentView()
        
        // Use a UIHostingController as window root view controller.
        
        if let windowScene = scene as? UIWindowScene {
            globalScene = windowScene
            globalWindow = UIWindow(windowScene: globalScene!)
            globalWindow!.rootViewController = UIHostingController(rootView: globalContentView)
            globalWindow!.makeKeyAndVisible()
        }
        
        let addr = "ADDRESS:PORT"
        let user = "USER"
        let password = "PASSWORD"
        Background {
            print("Connecting in the background...")
            connectVnc(callback,
                       UnsafeMutablePointer<Int8>(mutating: (addr as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (user as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (password as NSString).utf8String))
        }
        
    }
    
    func sceneDidDisconnect(_ scene: UIScene) {
        // Called as the scene is being released by the system.
        // This occurs shortly after the scene enters the background, or when its session is discarded.
        // Release any resources associated with this scene that can be re-created the next time the scene connects.
        // The scene may re-connect later, as its session was not neccessarily discarded (see `application:didDiscardSceneSessions` instead).
    }
    
    func sceneDidBecomeActive(_ scene: UIScene) {
        // Called when the scene has moved from an inactive state to an active state.
        // Use this method to restart any tasks that were paused (or not yet started) when the scene was inactive.
    }
    
    func sceneWillResignActive(_ scene: UIScene) {
        // Called when the scene will move from an active state to an inactive state.
        // This may occur due to temporary interruptions (ex. an incoming phone call).
    }
    
    func sceneWillEnterForeground(_ scene: UIScene) {
        // Called as the scene transitions from the background to the foreground.
        // Use this method to undo the changes made on entering the background.
    }
    
    func sceneDidEnterBackground(_ scene: UIScene) {
        // Called as the scene transitions from the foreground to the background.
        // Use this method to save data, release shared resources, and store enough scene-specific state information
        // to restore the scene back to its current state.
    }
}
