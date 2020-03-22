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

let buttonHeight: CGFloat = 30.0
let buttonWidth: CGFloat = 40.0
var topSpacing: CGFloat = 20.0
let buttonSpacing: CGFloat = 5.0

var globalContentView: Image?
var globalScene: UIWindowScene?
var globalWindow: UIWindow?
var globalImageView: TouchEnabledUIImageView?
var globalStateKeeper: StateKeeper?

var sshForwardingLock: NSLock = NSLock()
var sshForwardingStatus: Bool = false

func ssh_forward_success() -> Void {
    print("SSH library is telling us we can proceed with the VNC connection")
    sshForwardingStatus = true
    sshForwardingLock.unlock()
}

func ssh_forward_failure() -> Void {
    print("SSH library is telling us it failed to set up SSH forwarding")
    sshForwardingStatus = false
    sshForwardingLock.unlock()
}

func failure_callback_str(message: String) {
    failure_callback(message: UnsafeMutablePointer<Int8>(mutating: (message as NSString).utf8String!))
}

func failure_callback(message: UnsafeMutablePointer<Int8>?) -> Void {
    UserInterface {
        globalImageView?.disableTouch()
        let contentView = ContentView(stateKeeper: globalStateKeeper!)
        globalWindow?.rootViewController = UIHostingController(rootView: contentView)
        if message != nil {
            print("Connection failure, going back to connection setup screen.")
            globalStateKeeper?.showError(message: String(cString: message!))
        } else {
            print("Successful exit, no error was reported.")
            globalStateKeeper?.showConnections()
        }
    }
}

/**
 *
 * @return The smallest scale supported by the implementation; the scale at which
 * the bitmap would be smaller than the screen
 */
func getMinimumScale(fbW: CGFloat, fbH: CGFloat) -> CGFloat {
    return min(globalWindow!.bounds.maxX / fbW, globalWindow!.bounds.maxY / fbH);
}

func widthRatioLessThanHeightRatio(fbW: CGFloat, fbH: CGFloat) -> Bool {
    return globalWindow!.bounds.maxX / fbW < globalWindow!.bounds.maxY / fbH;
}

func resize_callback(fbW: Int32, fbH: Int32) -> Void {
    UserInterface {
        let minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))
        
        globalStateKeeper?.imageView = TouchEnabledUIImageView(frame: CGRect(x: 0, y: 0, width: Int(fbW), height: Int(fbH)))
        globalStateKeeper?.imageView?.frame = CGRect(x: 0, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale)
        globalStateKeeper?.imageView?.backgroundColor = UIColor.gray
        globalStateKeeper?.imageView?.enableGestures()
        globalStateKeeper?.imageView?.enableTouch()
        globalWindow!.addSubview(globalStateKeeper!.imageView!)
        globalStateKeeper?.createAndRepositionButtons()
        globalStateKeeper?.addButtons()
    }
}

func update_callback(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Void {
    UserInterface {
        //print("Graphics update: ", fbW, fbH, x, y, w, h)
        autoreleasepool {
            globalStateKeeper?.imageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))!)
        }
    }
}

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
        stateKeeper.correctTopSpacingForOrientation()
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
    
    func connect(currentConnection: [String:String]) {
        if let windowScene = scene as? UIWindowScene {
            globalScene = windowScene
            globalWindow = window
            self.stateKeeper.createAndRepositionButtons()
        }

        // Print out contents of CA file added for testing.
        let ca_path = Bundle.main.path(forResource: "ca", ofType: "pem")
        print("Contents of ca.pem file built into the package:")
        print(loadTextFile(path: ca_path!))

        let sshAddress = currentConnection["sshAddress"] ?? ""
        let sshPort = currentConnection["sshPort"] ?? ""
        let sshUser = currentConnection["sshUser"] ?? ""
        let sshPass = currentConnection["sshPass"] ?? ""
        let vncPort = currentConnection["port"] ?? ""
        let vncAddress = currentConnection["address"] ?? ""
        let sshForwardPort = String(arc4random_uniform(30000) + 30000)
        
        var addressAndPort = vncAddress + ":" + vncPort

        if sshAddress != "" {
            Background {
                sshForwardingLock.unlock()
                sshForwardingLock.lock()
                print("Setting up SSH forwarding")
                setupSshPortForward(
                    ssh_forward_success,
                    ssh_forward_failure,
                    UnsafeMutablePointer<Int8>(mutating: (sshAddress as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPort as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshUser as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPass as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: ("127.0.0.1" as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshForwardPort as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncAddress as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncPort as NSString).utf8String))
            }
            addressAndPort = "127.0.0.1" + ":" + sshForwardPort
        }
        
        let user = currentConnection["username"] ?? ""
        let pass = currentConnection["password"] ?? ""
        // TODO: Write out CA to a file.
        let cert = currentConnection["cert"] ?? ""

        Background {
            var message = ""
            var continueConnecting = true
            if sshAddress != "" {
                print("Waiting for SSH forwarding to complete successfully")
                // Wait for SSH Tunnel to be established for 15 seconds
                continueConnecting = sshForwardingLock.lock(before: Date(timeIntervalSinceNow: 15))
                if !continueConnecting {
                    message = "Timeout establishing SSH Tunnel"
                    print(message)
                } else if (sshForwardingStatus != true) {
                    message = "Failed to establish SSH Tunnel"
                    print(message)
                    continueConnecting = false
                } else {
                    print("SSH Tunnel indicated to be successful")
                    sshForwardingLock.unlock()
                }
            }
            if continueConnecting {
                print("Connecting VNC Session in the background...")
                connectVnc(update_callback, resize_callback, failure_callback,
                           UnsafeMutablePointer<Int8>(mutating: (addressAndPort as NSString).utf8String),
                           UnsafeMutablePointer<Int8>(mutating: (user as NSString).utf8String),
                           UnsafeMutablePointer<Int8>(mutating: (pass as NSString).utf8String),
                           UnsafeMutablePointer<Int8>(mutating: (ca_path as! NSString).utf8String))
            } else {
                message = "Something went wrong, not connecting to VNC server.\n\n" + message
                print(message)
                failure_callback_str(message: message)
            }
        }        
    }
        
    func disconnect() {
        Background {
            disconnectVnc()
        }
    }
}
