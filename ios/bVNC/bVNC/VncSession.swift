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
var globalDisconnectButton: UIButton?
var globalKeyboardButton: CustomTextInput?
var globalCtrlButton: ToggleButton?
var globalAltButton: ToggleButton?
var globalSuperButton: ToggleButton?
var globalTabButton: ToggleButton?

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
        print("Connection failure, going back to connection setup screen.")
        let contentView = ContentView(stateKeeper: globalStateKeeper!)
        globalWindow?.rootViewController = UIHostingController(rootView: contentView)
        globalStateKeeper!.currentPage = "page1"
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
        
        globalImageView = TouchEnabledUIImageView(frame: CGRect(x: 0, y: 0, width: Int(fbW), height: Int(fbH)))
        globalImageView!.frame = CGRect(x: 0, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale)
        globalImageView!.backgroundColor = UIColor.gray
        globalImageView!.enableGestures()
        globalImageView!.enableTouch()
        globalStateKeeper?.setImageView(imageView: globalImageView!)
        globalWindow!.addSubview(globalImageView!)
        addButtons()
    }
}

func update_callback(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Void {
    UserInterface {
        //print("Graphics update: ", fbW, fbH, x, y, w, h)
        autoreleasepool {
            globalImageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))!).imageWithInsets(insets: UIEdgeInsets(top: insetDimension, left: insetDimension, bottom: insetDimension, right: insetDimension))
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

func createButtons() {
    let disconnectButton = UIButton(frame: CGRect(
        x: globalWindow!.frame.width-buttonWidth,
        y: topSpacing,
        width: buttonWidth, height: buttonHeight))
    
    disconnectButton.setTitle("D", for: [])
    disconnectButton.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5)
    disconnectButton.addTarget(globalStateKeeper, action: #selector(globalStateKeeper?.disconnect), for: .touchDown)
    globalDisconnectButton = disconnectButton
    
    let keyboardButton = CustomTextInput(frame: CGRect(
        x: globalWindow!.frame.width-buttonWidth,
        y: topSpacing+buttonSpacing+buttonHeight,
        width: buttonWidth, height: buttonHeight))
    
    keyboardButton.setTitle("K", for: [])
    keyboardButton.backgroundColor = UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5)
    keyboardButton.addTarget(globalKeyboardButton, action: #selector(globalKeyboardButton?.toggleFirstResponder), for: .touchDown)
    globalKeyboardButton = keyboardButton
    
    let ctrlButton = ToggleButton(frame: CGRect(x: 0, y: topSpacing, width: buttonWidth, height: buttonHeight), title: "C", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5), stateKeeper: globalStateKeeper!, toSend: XK_Control_L, toggle: true)
    globalCtrlButton = ctrlButton
    
    let altButton = ToggleButton(frame: CGRect(x: 0, y: topSpacing+buttonSpacing+buttonHeight, width: buttonWidth, height: buttonHeight), title: "A", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5), stateKeeper: globalStateKeeper!, toSend: XK_Alt_L, toggle: true)
    globalAltButton = altButton

    let superButton = ToggleButton(frame: CGRect(x: 0, y: topSpacing+2*(buttonSpacing+buttonHeight), width: buttonWidth, height: buttonHeight), title: "S", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5), stateKeeper: globalStateKeeper!, toSend: XK_Super_L, toggle: true)
    globalSuperButton = superButton

    let tabButton = ToggleButton(frame: CGRect(x: 0, y: topSpacing+3*(buttonSpacing+buttonHeight), width: buttonWidth, height: buttonHeight), title: "T", background: UIColor(red: 0.5, green: 0.5, blue: 0.5, alpha: 0.5), stateKeeper: globalStateKeeper!, toSend: XK_Tab, toggle: false)
    globalTabButton = tabButton
}

func addButtons() {
    globalWindow!.addSubview(globalDisconnectButton!)
    globalWindow!.addSubview(globalKeyboardButton!)
    globalWindow!.addSubview(globalCtrlButton!)
    globalWindow!.addSubview(globalAltButton!)
    globalWindow!.addSubview(globalSuperButton!)
    globalWindow!.addSubview(globalTabButton!)
}

func removeButtons() {
    globalDisconnectButton!.removeFromSuperview()
    globalKeyboardButton!.removeFromSuperview()
    globalCtrlButton!.removeFromSuperview()
    globalAltButton!.removeFromSuperview()
    globalSuperButton!.removeFromSuperview()
    globalTabButton!.removeFromSuperview()
}

class VncSession {
    let scene: UIScene, stateKeeper: StateKeeper, window: UIWindow
    
    init(scene: UIScene, stateKeeper: StateKeeper, window: UIWindow) {
        self.scene = scene
        self.stateKeeper = stateKeeper
        self.window = window
        globalStateKeeper = stateKeeper
        correctTopSpacingForOrientation()
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
            createButtons()
        }

        // Print out contents of CA file added for testing.
        let ca_path = Bundle.main.path(forResource: "ca", ofType: "pem")
        print("Contents of ca.pem file built into the package:")
        print(loadTextFile(path: ca_path!))

        let sshAddress = connectionSettings.sshAddress!
        let sshPort = connectionSettings.sshPort!
        let sshUser = connectionSettings.sshUser!
        let sshPass = connectionSettings.sshPass!
        let vncPort = connectionSettings.port!
        let vncAddress = connectionSettings.address!
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
                    UnsafeMutablePointer<Int8>(mutating: (sshUser as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPass as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: ("127.0.0.1" as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshForwardPort as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncAddress as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncPort as NSString).utf8String))
            }
            addressAndPort = "127.0.0.1" + ":" + sshForwardPort
        }
        
        let user = connectionSettings.username!
        let pass = connectionSettings.password!
        // TODO: Write out CA to a file.
        let cert = connectionSettings.cert!

        Background {
            print("Waiting for SSH forwarding to complete successfully")
            var continueConnecting = true
            if sshAddress != "" {
                // Wait for SSH Tunnel to be established for 15 seconds
                continueConnecting = sshForwardingLock.lock(before: Date(timeIntervalSinceNow: 15))
                if !continueConnecting {
                    print("Timeout establishing SSH Tunnel")
                } else if (sshForwardingStatus != true) {
                    print("Failed to establish SSH Tunnel")
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
                print("Something went wrong, not connecting to VNC server.")
                failure_callback()
                // TODO: Show error to user.
            }
        }
        
        NotificationCenter.default.addObserver(self, selector: #selector(self.orientationChanged),
            name: UIDevice.orientationDidChangeNotification, object: nil
        );
    }
    
    func correctTopSpacingForOrientation() {
        if UIDevice.current.orientation.isLandscape {
            print("Landscape")
            topSpacing = 0
        }

        if UIDevice.current.orientation.isPortrait {
            print("Portrait")
            topSpacing = 20
        }
    }
    
    @objc func orientationChanged(_ notification: NSNotification) {
        print("Device rotated, correcting button layout.")
        if getMaintainConnection() {
            correctTopSpacingForOrientation()
            removeButtons()
            createButtons()
            addButtons()
        }
    }
    
    func disconnect() {
        Background {
            disconnectVnc()
        }
    }
}
