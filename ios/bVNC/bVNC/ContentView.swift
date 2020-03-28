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

struct MultilineTextView: UIViewRepresentable {
    var placeholder: String
    @Binding var text: String

    var minHeight: CGFloat
    @Binding var calculatedHeight: CGFloat

    init(placeholder: String, text: Binding<String>, minHeight: CGFloat, calculatedHeight: Binding<CGFloat>) {
        self.placeholder = placeholder
        self._text = text
        self.minHeight = minHeight
        self._calculatedHeight = calculatedHeight
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    func makeUIView(context: Context) -> UITextView {
        let textView = UITextView()
        textView.delegate = context.coordinator

        // Decrease priority of content resistance, so content would not push external layout set in SwiftUI
        textView.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        textView.isScrollEnabled = false
        textView.isEditable = true
        textView.isUserInteractionEnabled = true
        textView.backgroundColor = UIColor(white: 0.0, alpha: 0.5)

        // Set the placeholder
        textView.text = placeholder
        textView.textColor = UIColor.lightGray

        return textView
    }

    func updateUIView(_ textView: UITextView, context: Context) {
        if textView.text != self.text {
            textView.text = self.text
        }

        recalculateHeight(view: textView)
    }

    func recalculateHeight(view: UIView) {
        let newSize = view.sizeThatFits(CGSize(width: view.frame.size.width, height: CGFloat.greatestFiniteMagnitude))
        if minHeight < newSize.height && $calculatedHeight.wrappedValue != newSize.height {
            DispatchQueue.main.async {
                self.$calculatedHeight.wrappedValue = newSize.height // !! must be called asynchronously
            }
        } else if minHeight >= newSize.height && $calculatedHeight.wrappedValue != minHeight {
            DispatchQueue.main.async {
                self.$calculatedHeight.wrappedValue = self.minHeight // !! must be called asynchronously
            }
        }
    }

    class Coordinator : NSObject, UITextViewDelegate {

        var parent: MultilineTextView

        init(_ uiTextView: MultilineTextView) {
            self.parent = uiTextView
        }

        func textViewDidChange(_ textView: UITextView) {
            // This is needed for multistage text input (eg. Chinese, Japanese)
            if textView.markedTextRange == nil {
                parent.text = textView.text ?? String()
                parent.recalculateHeight(view: textView)
            }
        }

        func textViewDidBeginEditing(_ textView: UITextView) {
            if textView.textColor == UIColor.lightGray {
                textView.text = nil
                textView.textColor = UIColor.lightGray
            }
        }

        func textViewDidEndEditing(_ textView: UITextView) {
            if textView.text.isEmpty {
                textView.text = parent.placeholder
                textView.textColor = UIColor.lightGray
            }
        }
    }
}

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
                     sshPassphraseText: stateKeeper.selectedConnection["sshPassphrase"] ?? "",
                     sshPrivateKeyText: stateKeeper.selectedConnection["sshPrivateKey"] ?? "",
                     addressText: stateKeeper.selectedConnection["address"] ?? "",
                     portText: stateKeeper.selectedConnection["port"] ?? "5900",
                     usernameText: stateKeeper.selectedConnection["username"] ?? "",
                     passwordText: stateKeeper.selectedConnection["password"] ?? "")
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
    @State var sshPassphraseText: String
    @State var sshPrivateKeyText: String
    @State var addressText: String
    @State var portText: String
    @State var usernameText: String
    @State var passwordText: String
    @State var textHeight: CGFloat = 20
    
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
                        "sshPassphrase": self.sshPassphraseText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "sshPrivateKey": self.sshPrivateKeyText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "address": self.addressText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "port": self.portText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "username": self.usernameText.trimmingCharacters(in: .whitespacesAndNewlines),
                        "password": self.passwordText.trimmingCharacters(in: .whitespacesAndNewlines)]
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
                SecureField("SSH Password", text: $sshPassText).autocapitalization(.none).font(.title)
                SecureField("SSH Passphrase", text: $sshPassphraseText).autocapitalization(.none).font(.title)
                Text("Paste optional SSH Key below").font(.headline)
                MultilineTextView(placeholder: "SSH Key", text: $sshPrivateKeyText, minHeight: self.textHeight, calculatedHeight: $textHeight).frame(minHeight: self.textHeight, maxHeight: self.textHeight)
            }
            VStack {
                Text("Main Connection Parameters").font(.headline)
                TextField("Address", text: $addressText).autocapitalization(.none).font(.title)
                TextField("Port", text: $portText).font(.title)
                TextField("User (if required)", text: $usernameText).autocapitalization(.none).font(.title)
                SecureField("Password", text: $passwordText).font(.title)
                Text("").padding(.bottom, 1000)
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
        ContentViewA(stateKeeper: StateKeeper(), sshAddressText: "", sshPortText: "", sshUserText: "", sshPassText: "", sshPassphraseText: "", sshPrivateKeyText: "", addressText: "", portText: "", usernameText: "", passwordText: "")
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
