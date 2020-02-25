//
//  ContentView.swift
//  bVNC
//
//  Created by iordan iordanov on 2019-12-25.
//  Copyright © 2019 iordan iordanov. All rights reserved.
//

import Foundation
import Combine
import SwiftUI

struct ContentView : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            if stateKeeper.currentPage == "page1" {
                ContentViewA(stateKeeper: stateKeeper,
                             addressText: stateKeeper.settings.string(forKey: "address") ?? "",
                             portText: stateKeeper.settings.string(forKey: "port") ?? "",
                             usernameText: stateKeeper.settings.string(forKey: "username") ?? "",
                             passwordText: stateKeeper.settings.string(forKey: "password") ?? "",
                             certText: stateKeeper.settings.string(forKey: "cert") ?? "")
            } else if stateKeeper.currentPage == "page2" {
                ContentViewB(stateKeeper: stateKeeper)
            }
        }
    }
}

struct ContentViewA : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    var settings: UserDefaults = UserDefaults.standard
    @State var addressText: String
    @State var portText: String
    @State var usernameText: String
    @State var passwordText: String
    @State var certText: String

    var body: some View {
        ScrollView {
            VStack {
                Text("Set up VNC Connection")
                TextField("Address", text: $addressText).autocapitalization(.none)
                TextField("Port", text: $portText)
                TextField("User", text: $usernameText).autocapitalization(.none)
                SecureField("Password", text: $passwordText)
                TextField("Certificate Authority", text: $certText)

                Button(action: {
                    self.stateKeeper.currentPage = "page2"
                    self.stateKeeper.connectionSettings = ConnectionSettings(
                        address: self.addressText, port: self.portText, username: self.usernameText, password: self.passwordText, cert: self.certText)
                    
                    self.stateKeeper.settings.set(self.addressText, forKey: "address")
                    self.stateKeeper.settings.set(self.portText, forKey: "port")
                    self.stateKeeper.settings.set(self.usernameText, forKey: "username")
                    self.stateKeeper.settings.set(self.passwordText, forKey: "password")
                    self.stateKeeper.settings.set(self.certText, forKey: "cert")
                    self.stateKeeper.connect()
                }) {
                    Text("Connect")
                }
            }
        }
    }
}

struct ContentViewB : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            Text("Connecting to VNC Server")
            Button(action: {
                self.stateKeeper.disconnect()
            }) {
                Text("Disconnect")
            }
        }
    }
}

struct ContentViewA_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewA(stateKeeper: StateKeeper(), addressText: "", portText: "", usernameText: "", passwordText: "", certText: "")
    }
}

struct ContentViewB_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewB(stateKeeper: StateKeeper())
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView(stateKeeper: StateKeeper())
    }
}
