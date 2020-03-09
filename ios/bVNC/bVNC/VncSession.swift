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
var globalImageView: TouchEnabledUIImageView?
var globalStateKeeper: StateKeeper?
var globalDisconnectButton: UIButton?
var globalKeyboardButton: CustomTextInput?
var globalHideKeyboardButton: UIButton?

extension UIImage {
    func image(byDrawingImage image: UIImage, inRect rect: CGRect) -> UIImage! {
        UIGraphicsBeginImageContext(size)
        draw(in: CGRect(x: 0, y: 0, width: size.width, height: size.height))
        image.draw(in: rect)
        let result = UIGraphicsGetImageFromCurrentImageContext()
        UIGraphicsEndImageContext()
        return result
    }
}

func failure_callback() -> Void {
    UserInterface {
        globalImageView?.disableTouch()
        print("We were told to quit by the native library.")
        let contentView = ContentView(stateKeeper: globalStateKeeper!)
        globalWindow?.rootViewController = UIHostingController(rootView: contentView)
        globalStateKeeper!.currentPage = "page1"
    }
}

func resize_callback(fbW: Int32, fbH: Int32) -> Void {
    UserInterface {
        globalWindow!.rootViewController = UIHostingController(rootView: globalContentView)
        globalWindow!.makeKeyAndVisible()
        globalImageView = TouchEnabledUIImageView(frame: CGRect(x: 0, y: 0, width: Int(fbW), height: Int(fbH)))
        globalImageView!.frame = globalWindow!.bounds
        globalImageView!.enableGestures()
        globalImageView!.enableTouch()
        globalStateKeeper?.setImageView(imageView: globalImageView!)
        globalWindow!.addSubview(globalImageView!)
        globalWindow!.addSubview(globalDisconnectButton!)
        globalWindow!.addSubview(globalKeyboardButton!)
        globalWindow!.addSubview(globalHideKeyboardButton!)
    }
}

func update_callback(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Void {
    UserInterface {
        //print("Graphics update: ", fbW, fbH, x, y, w, h)
        autoreleasepool {
            globalImageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))!)
        }
    }
}
// This is awesome!!!
/*
£¥
 */
func imageFromARGB32Bitmap(pixels: UnsafeMutablePointer<UInt8>?, withWidth: Int, withHeight: Int) -> CGImage? {
    guard withWidth > 0 && withHeight > 0 else { return nil }
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue).union(.byteOrder32Big)
    let bitsPerComponent = 8
    /*
    guard let context: CGContext = CGContext(data: pixels, width: withWidth, height: withHeight, bitsPerComponent: bitsPerComponent, bytesPerRow: 4*withWidth, space: colorSpace, bitmapInfo: bitmapInfo.rawValue) else {
        print("Could not create CGContext")
        return nil
    }
    return context.makeImage()
    */
    let bitsPerPixel = 32
    return CGImage(width: withWidth,
                             height: withHeight,
                             bitsPerComponent: bitsPerComponent,
                             bitsPerPixel: bitsPerPixel,
                             bytesPerRow: 4*withWidth,
                             space: colorSpace,
                             bitmapInfo: bitmapInfo,
                             provider: CGDataProvider(data: NSData(bytes: pixels, length: withWidth*withHeight*4))!,
                             decode: nil,
                             shouldInterpolate: true,
                             intent: .defaultIntent)
}

class VncSession {
    let scene: UIScene, stateKeeper: StateKeeper, window: UIWindow
    
    init(scene: UIScene, stateKeeper: StateKeeper, window: UIWindow) {
        self.scene = scene
        self.stateKeeper = stateKeeper
        self.window = window
        globalStateKeeper = stateKeeper
        let disconnectButton = UIButton(frame: CGRect(x: 100, y: 20, width: 20, height: 20))
        disconnectButton.setTitle("D", for: [])
        disconnectButton.backgroundColor = UIColor(red: 100/255, green: 100/255, blue: 100/255, alpha: 0.3)
        disconnectButton.addTarget(globalStateKeeper, action: #selector(globalStateKeeper?.disconnect), for: .touchUpInside)
        globalDisconnectButton = disconnectButton
        let keyboardButton = CustomTextInput(frame: CGRect(x: 130, y: 20, width: 20, height: 20))
        keyboardButton.setTitle("K", for: [])
        keyboardButton.backgroundColor = UIColor(red: 100/255, green: 100/255, blue: 100/255, alpha: 0.3)
        keyboardButton.addTarget(globalKeyboardButton, action: #selector(globalKeyboardButton?.becomeFirstResponder), for: .touchUpInside)
        globalKeyboardButton = keyboardButton

        let hideKeyboardButton = UIButton(frame: CGRect(x: 160, y: 20, width: 20, height: 20))
        hideKeyboardButton.setTitle("H", for: [])
        hideKeyboardButton.backgroundColor = UIColor(red: 100/255, green: 100/255, blue: 100/255, alpha: 0.3)
        hideKeyboardButton.addTarget(globalKeyboardButton, action: #selector(globalKeyboardButton?.resignFirstResponder), for: .touchUpInside)
        globalHideKeyboardButton = hideKeyboardButton
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
            connectVnc(update_callback, resize_callback, failure_callback,
                       UnsafeMutablePointer<Int8>(mutating: (addressAndPort as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (user as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (pass as NSString).utf8String),
                       UnsafeMutablePointer<Int8>(mutating: (ca_path as! NSString).utf8String))
        }
    }
    
    func disconnect() {
        Background {
            disconnectVnc()
        }
    }
}
