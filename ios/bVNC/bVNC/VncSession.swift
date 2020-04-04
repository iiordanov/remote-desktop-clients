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
    DispatchQueue.global(qos: .userInteractive).async(execute: block)
}

func BackgroundLowPrio(_ block: @escaping ()->Void) {
    DispatchQueue.global(qos: .background).async(execute: block)
}

func UserInterface(_ block: @escaping ()->Void) {
    DispatchQueue.main.async(execute: block)
}

var globalContentView: Image?
var globalScene: UIWindowScene?
var globalWindow: UIWindow?
var globalImageView: TouchEnabledUIImageView?
var globalStateKeeper: StateKeeper?


func lock_write_tls_callback_swift(instance: Int32) -> Void {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return }

    globalStateKeeper?.globalWriteTlsLock.lock();
}

func unlock_write_tls_callback_swift(instance: Int32) -> Void {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return }

    globalStateKeeper?.globalWriteTlsLock.unlock();
}

func ssh_forward_success() -> Void {
    print("SSH library is telling us we can proceed with the VNC connection")
    globalStateKeeper?.sshForwardingStatus = true
    globalStateKeeper?.sshForwardingLock.unlock()
}

func ssh_forward_failure() -> Void {
    print("SSH library is telling us it failed to set up SSH forwarding")
    globalStateKeeper?.sshForwardingStatus = false
    globalStateKeeper?.sshForwardingLock.unlock()
}

func failure_callback_str(instance: Int, message: String?) {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return }

    UserInterface {
        globalStateKeeper?.hideKeyboard()
        globalImageView?.disableTouch()
        let contentView = ContentView(stateKeeper: globalStateKeeper!)
        globalWindow?.rootViewController = MyUIHostingController(rootView: contentView)
        globalWindow?.makeKeyAndVisible()

        if message != nil {
            print("Connection failure, showing error with title \(message!).")
            globalStateKeeper?.showError(title: message!)
        } else {
            print("Successful exit, no error was reported.")
            globalStateKeeper?.showConnections()
        }
    }
}

func failure_callback_swift(instance: Int32, message: UnsafeMutablePointer<UInt8>?) -> Void {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return }

    if (instance != globalStateKeeper!.currInst) { return }

    if message != nil {
        print("Will show error with title: \(String(cString: message!))")
        failure_callback_str(instance: Int(instance), message: String(cString: message!))
    } else {
        failure_callback_str(instance: Int(instance), message: nil)
    }
}

func log_callback(message: UnsafeMutablePointer<Int8>?) -> Void {
    // Prevent the log from growing too large
    if globalStateKeeper?.clientLog.count ?? 0 > 500 {
        globalStateKeeper?.clientLog.remove(at: 0)
    }
    // TODO: Bad Access here
    globalStateKeeper?.clientLog.append(String(cString: message!))
}

func yes_no_dialog_callback(instance: Int32, title: UnsafeMutablePointer<Int8>?, message: UnsafeMutablePointer<Int8>?, fingerPrint1: UnsafeMutablePointer<Int8>?, fingerPrint2: UnsafeMutablePointer<Int8>?, type: UnsafeMutablePointer<Int8>?) -> Int32 {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return 0 }

    if (instance != globalStateKeeper!.currInst) { return 0 }

    let fingerprintType = String(cString: type!)
    let fingerPrint1Str = String(cString: fingerPrint1!)
    let fingerPrint2Str = String(cString: fingerPrint2!)
    
    // Check for a match
    if fingerprintType == "SSH" &&
       fingerPrint1Str == globalStateKeeper?.selectedConnection["sshFingerprintSha256"] {
        print ("Found matching saved SHA256 SSH host key fingerprint, continuing.")
        return 1
    } else if fingerprintType == "X509" &&
       fingerPrint1Str == globalStateKeeper?.selectedConnection["x509FingerprintSha256"] &&
       fingerPrint2Str == globalStateKeeper?.selectedConnection["x509FingerprintSha512"] {
       print ("Found matching saved SHA256 and SHA512 X509 key fingerprints, continuing.")
       return 1
    }
    print ("Asking user to verify fingerprints \(String(cString: fingerPrint1!)) and \(String(cString: fingerPrint2!)) of type \(String(cString: type!))")

    let titleStr = String(cString: title!)
    var messageStr = String(cString: message!)
    
    // Check for a mismatch if keys were already set
    if fingerprintType == "SSH" &&
        globalStateKeeper?.selectedConnection["sshFingerprintSha256"] != nil {
        messageStr = "\nWARNING: SSH key of this connection has changed! This could be a man in the middle attack! Do not continue unless you are aware of this change.\n\n" + messageStr
    } else if fingerprintType == "X509" &&
       (globalStateKeeper?.selectedConnection["sshFingerprintSha256"] != nil ||
        globalStateKeeper?.selectedConnection["sshFingerprintSha512"] != nil) {
        messageStr = "\nWARNING: X509 key of this connection has changed! This could be a man in the middle attack! Do not continue unless you are aware of this change.\n\n" + messageStr
    }

    let res = globalStateKeeper?.yesNoResponseRequired(
        title: titleStr, message: messageStr) ?? 0
    
    if res == 1 && fingerprintType == "SSH" {
        globalStateKeeper?.selectedConnection["sshFingerprintSha256"] = fingerPrint1Str
        globalStateKeeper?.saveSettings()
    } else if res == 1 && fingerprintType == "X509" {
        globalStateKeeper?.selectedConnection["x509FingerprintSha256"] = fingerPrint1Str
        globalStateKeeper?.selectedConnection["x509FingerprintSha512"] = fingerPrint2Str
        globalStateKeeper?.saveSettings()
    }
    return res
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

func resize_callback(instance: Int32, cl: UnsafeMutableRawPointer?, fbW: Int32, fbH: Int32) -> Void {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return }

    globalStateKeeper?.fbW = fbW
    globalStateKeeper?.fbH = fbH
    UserInterface {
        autoreleasepool {
            globalStateKeeper?.imageView?.removeFromSuperview()
            globalStateKeeper?.imageView?.image = nil
            globalStateKeeper?.imageView = nil
            let minScale = getMinimumScale(fbW: CGFloat(fbW), fbH: CGFloat(fbH))
            globalStateKeeper?.correctTopSpacingForOrientation()
            let leftSpacing = globalStateKeeper?.leftSpacing ?? 0
            let topSpacing = globalStateKeeper?.topSpacing ?? 0
            globalStateKeeper?.imageView = LongTapDragUIImageView(frame: CGRect(x: leftSpacing, y: topSpacing, width: CGFloat(fbW)*minScale, height: CGFloat(fbH)*minScale), stateKeeper: globalStateKeeper)
            //globalStateKeeper?.imageView?.backgroundColor = UIColor.gray
            globalStateKeeper?.imageView?.enableGestures()
            globalStateKeeper?.imageView?.enableTouch()
            globalWindow!.addSubview(globalStateKeeper!.imageView!)
            globalStateKeeper?.createAndRepositionButtons()
            globalStateKeeper?.addButtons(buttons: globalStateKeeper?.interfaceButtons ?? [:])
            globalStateKeeper?.showConnectedSession()
        }
    }
    globalStateKeeper?.keepSessionRefreshed()
}

func draw(data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) {
    UserInterface {
        autoreleasepool {
            globalStateKeeper?.imageView?.image = UIImage(cgImage: imageFromARGB32Bitmap(pixels: data, withWidth: Int(fbW), withHeight: Int(fbH))!)
        }
    }
}

func update_callback(instance: Int32, data: UnsafeMutablePointer<UInt8>?, fbW: Int32, fbH: Int32, x: Int32, y: Int32, w: Int32, h: Int32) -> Bool {
    if (instance != globalStateKeeper!.currInst) { print("Current instance \(globalStateKeeper!.currInst) discarding call from instance \(instance)") ; return false }
    if (!(globalStateKeeper?.isDrawing ?? false)) {
        print("Not drawing, discard.")
        return false
    }
    
    globalStateKeeper!.frames += 1
    let currentCpuUsage = SystemMonitor.appCpuUsage()
    if (currentCpuUsage > 40.0) {
        //print("CPU usage high, discarding frame, scheduling redraw")
        globalStateKeeper!.rescheduleReDrawTimer(data: data, fbW: fbW, fbH: fbH)
        if (globalStateKeeper!.frames % 10 == 0) {
            //print("Drawing a 10th frame.")
            draw(data: data, fbW: fbW, fbH: fbH, x: x, y: y, w: w, h: h)
        }
    } else {
        //print("Drawing a frame normally.")
        draw(data: data, fbW: fbW, fbH: fbH, x: x, y: y, w: w, h: h)
    }
    return true
}

func imageFromARGB32Bitmap(pixels: UnsafeMutablePointer<UInt8>?, withWidth: Int, withHeight: Int) -> CGImage? {
    guard withWidth > 0 && withHeight > 0 else { return nil }
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue).union(.byteOrder32Big)
    let bitsPerComponent = 8

    guard let context: CGContext = CGContext(data: pixels, width: withWidth, height: withHeight, bitsPerComponent: bitsPerComponent, bytesPerRow: 4*withWidth, space: colorSpace, bitmapInfo: bitmapInfo.rawValue) else {
        print("Could not create CGContext")
        return nil
    }
    return context.makeImage()
    /*
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
     */
}

class VncSession {
    let scene: UIScene, window: UIWindow, stateKeeper: StateKeeper
    var instance: Int
    
    init(scene: UIScene, window: UIWindow, instance: Int, stateKeeper: StateKeeper) {
        print("Initializing VNC Session instance: \(instance)")
        self.scene = scene
        self.window = window
        self.instance = instance
        self.stateKeeper = stateKeeper
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
        }

        let sshAddress = currentConnection["sshAddress"] ?? ""
        let sshPort = currentConnection["sshPort"] ?? ""
        let sshUser = currentConnection["sshUser"] ?? ""
        let sshPass = currentConnection["sshPass"] ?? ""
        let vncPort = currentConnection["port"] ?? ""
        let vncAddress = currentConnection["address"] ?? ""
        let sshPassphrase = currentConnection["sshPassphrase"] ?? ""
        let sshPrivateKey = currentConnection["sshPrivateKey"] ?? ""

        let sshForwardPort = String(arc4random_uniform(30000) + 30000)
        
        var addressAndPort = vncAddress + ":" + vncPort

        if sshAddress != "" {
            Background {
                self.stateKeeper.sshForwardingLock.unlock()
                self.stateKeeper.sshForwardingLock.lock()
                print("Setting up SSH forwarding")
                setupSshPortForward(
                    Int32(self.stateKeeper.currInst),
                    ssh_forward_success,
                    ssh_forward_failure,
                    log_callback,
                    yes_no_dialog_callback,
                    UnsafeMutablePointer<Int8>(mutating: (sshAddress as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPort as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshUser as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPass as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPassphrase as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshPrivateKey as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: ("127.0.0.1" as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (sshForwardPort as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncAddress as NSString).utf8String),
                    UnsafeMutablePointer<Int8>(mutating: (vncPort as NSString).utf8String))
            }
            addressAndPort = "127.0.0.1" + ":" + sshForwardPort
        }
        
        let user = currentConnection["username"] ?? ""
        let pass = currentConnection["password"] ?? ""
        // TODO: Write out CA to a file if keeping it
        //let cert = currentConnection["cert"] ?? ""

        Background {
            self.stateKeeper.yesNoDialogLock.unlock()
            var message = ""
            var continueConnecting = true
            if sshAddress != "" {
                print("Waiting for SSH forwarding to complete successfully")
                // Wait for SSH Tunnel to be established for 60 seconds
                continueConnecting = self.stateKeeper.sshForwardingLock.lock(before: Date(timeIntervalSinceNow: 60))
                if !continueConnecting {
                    message = "Timeout establishing SSH Tunnel"
                } else if (self.stateKeeper.sshForwardingStatus != true) {
                    message = "Failed to establish SSH Tunnel"
                    continueConnecting = false
                } else {
                    print("SSH Tunnel indicated to be successful")
                    self.stateKeeper.sshForwardingLock.unlock()
                }
            }
            if continueConnecting {
                print("Connecting VNC Session in the background...")
                self.stateKeeper.cl = initializeVnc(Int32(self.instance), update_callback, resize_callback, failure_callback_swift, log_callback, lock_write_tls_callback_swift, unlock_write_tls_callback_swift, yes_no_dialog_callback,
                           UnsafeMutablePointer<Int8>(mutating: (addressAndPort as NSString).utf8String),
                           UnsafeMutablePointer<Int8>(mutating: (user as NSString).utf8String),
                           UnsafeMutablePointer<Int8>(mutating: (pass as NSString).utf8String))
                if self.stateKeeper.cl != nil {
                    //let thread = Thread.init(target: self, selector: #selector(self.connectVncSession), object: self.stateKeeper.cl)
                    //thread.start()
                    connectVnc(self.stateKeeper.cl)
                } else {
                    message = "Failed to establish connection to VNC server."
                    print("Error message: \(message)")
                    failure_callback_str(instance: self.instance, message: message)
                }
            } else {
                message = "Error connecting: " + message
                print("Error message: \(message)")
                failure_callback_str(instance: self.instance, message: message)
            }
        }
    }
        
    func disconnect(cl: UnsafeMutableRawPointer?) {
        Background {
            disconnectVnc(cl)
        }
    }
}
