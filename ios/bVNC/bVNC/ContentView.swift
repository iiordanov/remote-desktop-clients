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
            if stateKeeper.currentPage == "connectionsList" {
                ConnectionsList(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "page1" {
                ContentViewA(
                     stateKeeper: stateKeeper,
                     sshAddressText: stateKeeper.selectedConnection["sshAddress"] ?? "",
                     sshPortText: stateKeeper.selectedConnection["sshPort"] ?? "22",
                     sshUserText: stateKeeper.selectedConnection["sshUser"] ?? "",
                     sshPassText: stateKeeper.selectedConnection["sshPass"] ?? "",
                     addressText: stateKeeper.selectedConnection["address"] ?? "",
                     portText: stateKeeper.selectedConnection["port"] ?? "5900",
                     usernameText: stateKeeper.selectedConnection["username"] ?? "",
                     passwordText: stateKeeper.selectedConnection["password"] ?? "",
                     certText: stateKeeper.selectedConnection["cert"] ?? "")
            } else if stateKeeper.currentPage == "page2" {
                ContentViewB(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "page3" {
                ContentViewC(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "connectedSession" {
                ContentViewD()
            } else if stateKeeper.currentPage == "dismissableErrorMessage" {
                DismissableLogDialog(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "yesNoMessage" {
                YesNoDialog(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "blankPage" {
                BlankPage()
            }
        }
    }
}

struct ConnectionsList : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    func connect(index: Int) {
        self.stateKeeper.connect(index: index)
    }

    func edit(index: Int) {
        self.stateKeeper.editConnection(index: index)
    }
    
    func elementAt(index: Int) -> [String:String] {
        return self.stateKeeper.connections[index]
    }
    
    func buildTitle(i: Int) -> String {
        let connection = self.elementAt(index: i)
        var title = ""
        if connection["sshAddress"] != "" {
            title = "SSH: \(connection["sshAddress"] ?? ""):\(connection["sshPort"] ?? "22")\n"
        }
        title += "VNC: \(connection["address"] ?? ""):\(connection["port"] ?? "5900")"
        return title
    }

    var body: some View {
        ScrollView {
            VStack {
                Button(action: {
                    self.stateKeeper.addNewConnection()
                }) {
                    Text("New Connection")
                    .fontWeight(.bold)
                    .font(.title)
                    .padding(5)
                    .background(Color.gray)
                    .cornerRadius(5)
                    .foregroundColor(.white)
                    .padding(10)
                }

                Text("Tap a connection to connect").font(.headline)
                Text("Long tap a connection to edit").font(.headline)
                ForEach(0 ..< stateKeeper.connections.count) { i in
                    Button(action: {
                    }) {
                        Text(self.buildTitle(i: i))
                            .font(.headline)
                            .padding(5)
                            .background(Color.black)
                            .cornerRadius(5)
                            .foregroundColor(.white)
                            .padding(5)
                            .frame(minWidth: 0, maxWidth: .infinity)
                            .overlay(
                                RoundedRectangle(cornerRadius: 5)
                                    .stroke(Color.white, lineWidth: 4)
                        ).onTapGesture {
                            self.connect(index: i)
                        }.onLongPressGesture {
                            self.edit(index: i)
                        }
                    }.padding(10)
                }
                
                Button(action: {
                    self.stateKeeper.showError(title: "Client Log Messages")
                }) {
                    Text("View Log")
                    .fontWeight(.bold)
                    .font(.title)
                    .padding(5)
                    .background(Color.gray)
                    .cornerRadius(5)
                    .foregroundColor(.white)
                    .padding(10)
                }

            }
        }
    }
}

struct ContentViewA : View {
    
    var settings: UserDefaults = UserDefaults.standard
    @ObservedObject var stateKeeper: StateKeeper
    @State var sshAddressText: String
    @State var sshPortText: String
    @State var sshUserText: String
    @State var sshPassText: String
    @State var addressText: String
    @State var portText: String
    @State var usernameText: String
    @State var passwordText: String
    @State var certText: String

    var body: some View {
        ScrollView {
            VStack {
                HStack {
                Button(action: {
                    self.stateKeeper.currentPage = "page2"
                    let selectedConnection = [
                        "sshAddress": self.sshAddressText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "sshPort": self.sshPortText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "sshUser": self.sshUserText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "sshPass": self.sshPassText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "address": self.addressText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "port": self.portText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "username": self.usernameText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "password": self.passwordText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "cert": self.certText.trimmingCharacters(in: .whitespacesAndNewlines)]
                    self.stateKeeper.saveConnection(connection: selectedConnection)
                }) {
                    Text("Save")
                        .fontWeight(.bold)
                        .font(.title)
                        .padding(5)
                        .background(Color.gray)
                        .cornerRadius(10)
                        .foregroundColor(.white)
                        .padding(10)
                }
                Button(action: {
                    self.stateKeeper.currentPage = "page2"
                    self.stateKeeper.deleteCurrentConnection()
                }) {
                    Text("Delete")
                        .fontWeight(.bold)
                        .font(.title)
                        .padding(5)
                        .background(Color.red)
                        .cornerRadius(10)
                        .foregroundColor(.white)
                        .padding(10)
                }
                }

                Text("Optional SSH Connection Parameters").font(.headline)
                TextField("SSH Server", text: $sshAddressText).autocapitalization(.none).font(.title)
                TextField("SSH Port", text: $sshPortText).autocapitalization(.none).font(.title)
                TextField("SSH User", text: $sshUserText).autocapitalization(.none).font(.title)
                SecureField("SSH Password or Passphrase", text: $sshPassText).autocapitalization(.none).font(.title)
            }
            VStack {
                Text("Main Connection Parameters").font(.headline)
                TextField("Address", text: $addressText).autocapitalization(.none).font(.title)
                TextField("Port", text: $portText).font(.title)
                TextField("User (if required)", text: $usernameText).autocapitalization(.none).font(.title)
                SecureField("Password", text: $passwordText).font(.title)
                TextField("Certificate Authority", text: $certText).padding(.bottom, 500).font(.title)
            }
        }
    }
}

struct ContentViewB : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            Text("Connecting to Server")
            Button(action: {
                self.stateKeeper.disconnect()
            }) {
                Text("Disconnect")
            }
        }
    }
}

struct ContentViewC : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            Text("Disconnecting from Server")
        }
    }
}

struct ContentViewD : View {
    var body: some View {
        Text("")
    }
}

struct BlankPage : View {
    var body: some View {
        Text("")
    }
}

struct DismissableLogDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getTitle() -> String {
        return stateKeeper.title ?? ""
    }

    func getClientLog() -> String {
        return stateKeeper.clientLog
    }
    
    var body: some View {
        VStack {
            ScrollView {
                Text(self.getTitle()).font(.title)
                Text(self.getClientLog()).font(.body)
            }
            Button(action: {
                self.stateKeeper.showConnections()
            }) {
                Text("Dismiss")
                .fontWeight(.bold)
                .font(.title)
                .padding(5)
                .background(Color.gray)
                .cornerRadius(5)
                .foregroundColor(.white)
                .padding(10)
            }
        }
    }
}

struct YesNoDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getTitle() -> String {
        return stateKeeper.title ?? ""
    }
    
    func getMessage() -> String {
        return stateKeeper.message ?? ""
    }
    
    func setResponse(response: Bool) -> Void {
        stateKeeper.setYesNoReponse(response: response,
                                    pageYes: "connectedSession",
                                    pageNo: "connectionsList")
    }

    var body: some View {
        VStack {
            ScrollView {
                Text(self.getTitle()).font(.title)
                Text(self.getMessage()).font(.body)
            }
            HStack {
                Button(action: {
                    self.setResponse(response: false)
                }) {
                    Text("No")
                    .fontWeight(.bold)
                    .font(.title)
                    .padding(5)
                    .background(Color.gray)
                    .cornerRadius(5)
                    .foregroundColor(.white)
                    .padding(10)
                }
                Button(action: {
                    self.setResponse(response: true)
                }) {
                    Text("Yes")
                    .fontWeight(.bold)
                    .font(.title)
                    .padding(5)
                    .background(Color.gray)
                    .cornerRadius(5)
                    .foregroundColor(.white)
                    .padding(10)
                }

            }
        }
    }
}


struct ContentViewA_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewA(stateKeeper: StateKeeper(), sshAddressText: "", sshPortText: "", sshUserText: "", sshPassText: "", addressText: "", portText: "", usernameText: "", passwordText: "", certText: "")
    }
}

struct ContentViewB_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewB(stateKeeper: StateKeeper())
    }
}

struct ContentViewC_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewC(stateKeeper: StateKeeper())
    }
}

struct ContentViewD_Previews : PreviewProvider {
    static var previews: some View {
        ContentViewD()
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView(stateKeeper: StateKeeper())
    }
}
