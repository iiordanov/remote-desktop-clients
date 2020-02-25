//
//  VncSession.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-02-22.
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
var globalStateKeeper: StateKeeper?
var globalDisconnectButton: Button<Text>?


func callback(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Void {
    UserInterface {
        if (!getMaintainConnection()) {
            return
        }
        globalWindow!.rootViewController = UIHostingController(rootView: globalContentView)
        globalWindow!.makeKeyAndVisible()

        //print("On UI Thread: ", fbW, fbH, x, y, w, h)
        globalImageView?.removeFromSuperview()
        globalImageView = nil
        globalImageView = UIImageView(frame: CGRect(x: 0, y: 0, width: Int(fbW), height: Int(fbH)))
        globalImageView?.image = imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))
        globalImageView!.frame = globalWindow!.bounds
        globalWindow!.addSubview(globalImageView!)
        
        let button = UIButton(frame: CGRect(x: 100, y: 100, width: 100, height: 50))
        button.setTitle("Disconnect", for: [])
        button.addTarget(globalStateKeeper, action: #selector(globalStateKeeper?.disconnect), for: .touchUpInside)
        globalWindow!.addSubview(button)
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

class VncSession {
    let scene: UIScene, stateKeeper: StateKeeper, window: UIWindow
    
    init(scene: UIScene, stateKeeper: StateKeeper, window: UIWindow) {
        self.scene = scene
        self.stateKeeper = stateKeeper
        self.window = window
        globalStateKeeper = stateKeeper
    }
    
    func captureScreen(window: UIWindow) -> UIImage {
        UIGraphicsBeginImageContextWithOptions(window.bounds.size, false, UIScreen.main.scale)
        window.layer.render(in: UIGraphicsGetCurrentContext()!)
        let image = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()
        return image!
    }
    
    func loadTextFile(path: String) -> String {
        var contents = "Not Loaded"
        do { contents = try String(contentsOfFile: path, encoding: String.Encoding.utf8) }
            catch { print("Error loading file") }
        return contents
    }
    
    func connect(connectionSettings: ConnectionSettings) {
        if let windowScene = scene as? UIWindowScene {
            globalScene = windowScene
            globalWindow = window
        }
        
        // Print out contents of CA file added for testing.
        let ca_path = Bundle.main.path(forResource: "ca", ofType: "pem")
        print("Contents of ca.pem file built into the package:")
        print(loadTextFile(path: ca_path!))

        let addressAndPort = connectionSettings.address! + ":" + connectionSettings.port!
        let user = connectionSettings.username!
        let pass = connectionSettings.password!
        let cert = connectionSettings.cert!
        // TODO: Write out CA to a file.

        Background {
            print("Connecting in the background...")
            connectVnc(callback,
                       UnsafeMutablePointer<Int8>(mutating: (addressAndPort as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (user as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (pass as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (ca_path as! NSString).utf8String))
        }
    }
    
    func disconnect() {
        disconnectVnc()
    }
}
