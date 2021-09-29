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
            } else if stateKeeper.currentPage == "addOrEditConnection" {
                AddOrEditConnectionPage(
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
                     passwordText: stateKeeper.selectedConnection["password"] ?? "",
                     screenShotFile: stateKeeper.selectedConnection["screenShotFile"] ?? UUID().uuidString,
                     allowZooming: Bool(stateKeeper.selectedConnection["allowZooming"] ?? "true") ?? true,
                     allowPanning: Bool(stateKeeper.selectedConnection["allowPanning"] ?? "true") ?? true,
                     showSshTunnelSettings: Bool(stateKeeper.selectedConnection["showSshTunnelSettings"] ?? "false")! || (stateKeeper.selectedConnection["sshAddress"] ?? "") != "")
            } else if stateKeeper.currentPage == "genericProgressPage" {
                ProgressPage(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "connectionInProgress" {
                ConnectionInProgressPage(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "connectedSession" {
                ConnectedSessionPage()
            } else if stateKeeper.currentPage == "dismissableErrorMessage" {
                DismissableLogDialog(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "dismissableMessage" {
                DismissableMessageDialog(stateKeeper: stateKeeper)
            } else if stateKeeper.currentPage == "helpDialog" {
                HelpDialog(stateKeeper: stateKeeper)
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
        if self.stateKeeper.currentPage != "connectionInProgress" {
            self.stateKeeper.currentPage = "connectionInProgress"
            self.stateKeeper.connect(index: index)
        }
    }

    func edit(index: Int) {
        self.stateKeeper.editConnection(index: index)
    }
    
    func elementAt(index: Int) -> [String: String] {
        return self.stateKeeper.connections[index]
    }
    
    func buildTitle(i: Int) -> String {
        let connection = self.elementAt(index: i)
        var title = ""
        if connection["sshAddress"] != "" {
            let user = "\(connection["sshUser"] ?? "")"
            title = "SSH\t\(user)@\(connection["sshAddress"] ?? ""):\(connection["sshPort"] ?? "22")\n"
        }
        title += "VNC\t\(connection["address"] ?? ""):\(connection["port"] ?? "5900")"
        return title
    }
    
    func getSavedImage(named: String) -> UIImage? {
        if let dir = try? FileManager.default.url(for: .documentDirectory, in: .userDomainMask, appropriateFor: nil, create: false) {
            return UIImage(contentsOfFile: URL(fileURLWithPath: dir.absoluteString).appendingPathComponent(named).path)
        }
        return nil
    }

    var body: some View {
        ScrollView {
            VStack {
                HStack() {
                    VStack(alignment: .leading, spacing: 10) {
                        Text(self.stateKeeper.localizedString(for: "APP_NAME"))
                            .frame(width: 80)
                            .lineLimit(1)
                            .allowsTightening(true)
                            .scaledToFit()
                            .minimumScaleFactor(0.5)
                        Text(self.stateKeeper.localizedString(for: "COMPANY_NAME"))
                            .frame(width: 80)
                            .lineLimit(1)
                            .allowsTightening(true)
                            .scaledToFit()
                            .minimumScaleFactor(0.5)
                    }.padding()
                    
                    Button(action: {
                        self.stateKeeper.addNewConnection()
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "plus")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 32, height: 32)
                            Text("NEW_LABEL")
                        }.padding()
                    }
                    Button(action: {
                        self.stateKeeper.showHelp(messages: [ LocalizedStringKey(self.stateKeeper.localizedString(for: "MAIN_HELP_TEXT")) ])
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "info")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 32, height: 32)
                            Text("HELP_LABEL")
                        }.padding()
                    }
                    
                    Button(action: {
                        self.stateKeeper.showLog(title: "SESSION_LOG_LABEL",
                                                 text: self.stateKeeper.clientLog.joined())
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "text.alignleft")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 32, height: 32)
                            Text("LOG_LABEL")
                        }.padding()
                    }
                }.frame(minWidth: 0, maxWidth: .infinity, minHeight: 0, alignment: .topTrailing).padding()

                ForEach(0 ..< stateKeeper.connections.count) { i in
                    Button(action: {
                    }) {
                        VStack {
                            Image(uiImage: self.getSavedImage(named: self.stateKeeper.connections[i]["screenShotFile"]!) ?? UIImage())
                                .resizable()
                                .scaledToFit()
                                .cornerRadius(5)
                                .frame(maxWidth: 600, maxHeight: 200)
                            Text(self.buildTitle(i: i))
                                .font(.headline)
                                .padding(5)
                                .background(Color.black)
                                .cornerRadius(5)
                                .foregroundColor(.white)
                                .padding(5)
                                .frame(height:100)
                        }
                        .padding()
                        .foregroundColor(.white)
                        .background(Color.black)
                        .cornerRadius(5)
                        .overlay(
                            RoundedRectangle(cornerRadius: 5)
                                .stroke(Color.white, lineWidth: 2))
                        .onTapGesture {
                            self.connect(index: i)
                        }.onLongPressGesture {
                            self.edit(index: i)
                        }

                    }.buttonStyle(PlainButtonStyle())
                }
            }
        }
    }
}

struct AddOrEditConnectionPage : View {
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
    @State var screenShotFile: String
    @State var textHeight: CGFloat = 20
    @State var allowZooming: Bool
    @State var allowPanning: Bool
    @State var showSshTunnelSettings: Bool
    
    func retrieveConnectionDetails() -> [String : String] {
        let connection = [
            "sshAddress": self.sshAddressText.trimmingCharacters(in: .whitespacesAndNewlines),
            "sshPort": self.sshPortText.trimmingCharacters(in: .whitespacesAndNewlines),
            "sshUser": self.sshUserText.trimmingCharacters(in: .whitespacesAndNewlines),
            "sshPass": self.sshPassText.trimmingCharacters(in: .whitespacesAndNewlines),
            "sshPassphrase": self.sshPassphraseText.trimmingCharacters(in: .whitespacesAndNewlines),
            "sshPrivateKey": self.sshPrivateKeyText.trimmingCharacters(in: .whitespacesAndNewlines),
            "address": self.addressText.trimmingCharacters(in: .whitespacesAndNewlines),
            "port": self.portText.trimmingCharacters(in: .whitespacesAndNewlines),
            "username": self.usernameText.trimmingCharacters(in: .whitespacesAndNewlines),
            "password": self.passwordText.trimmingCharacters(in: .whitespacesAndNewlines),
            "screenShotFile": self.screenShotFile.trimmingCharacters(in: .whitespacesAndNewlines),
            "allowZooming": String(self.allowZooming),
            "allowPanning": String(self.allowPanning),
            "showSshTunnelSettings": String(self.showSshTunnelSettings)
        ]
        return connection
    }

    var body: some View {
        ScrollView {
            VStack {
                HStack(spacing: 5) {
                    
                    VStack(alignment: .leading, spacing: 10) {
                        Text(self.stateKeeper.localizedString(for: "APP_NAME"))
                            .frame(width: 70)
                            .lineLimit(1)
                            .allowsTightening(true)
                            .scaledToFit()
                            .minimumScaleFactor(0.25)
                        Text(self.stateKeeper.localizedString(for: "COMPANY_NAME"))
                            .frame(width: 70)
                            .lineLimit(1)
                            .allowsTightening(true)
                            .scaledToFit()
                            .minimumScaleFactor(0.25)
                    }.padding()
                    
                    Button(action: {
                        let selectedConnection: [String : String] = self.retrieveConnectionDetails()
                        self.stateKeeper.saveConnection(connection: selectedConnection)
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "folder.badge.plus")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 40, height: 40)
                            Text("SAVE_LABEL")
                                .lineLimit(1)
                                .allowsTightening(true)
                                .scaledToFit()
                                .minimumScaleFactor(0.70)
                        }.padding()
                    }
                    
                    Button(action: {
                        self.stateKeeper.deleteCurrentConnection()
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "trash")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 40, height: 40)
                            Text("DELETE_LABEL")
                                .lineLimit(1)
                                .allowsTightening(true)
                                .scaledToFit()
                                .minimumScaleFactor(0.70)
                        }.padding()
                    }
                    
                    Button(action: {
                        self.stateKeeper.showConnections()
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "arrowshape.turn.up.left")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 40, height: 40)
                            Text("CANCEL_LABEL")
                                .lineLimit(1)
                                .allowsTightening(true)
                                .scaledToFit()
                                .minimumScaleFactor(0.70)
                        }.padding()
                    }

                    Button(action: {
                        var help_messages_list: [LocalizedStringKey] = ["VNC_CONNECTION_SETUP_HELP_TEXT", "UI_SETUP_HELP_TEXT"]
                        if self.stateKeeper.sshAppIds.contains(UIApplication.appId ?? "") {
                            help_messages_list.insert("SSH_CONNECTION_SETUP_HELP_TEXT", at: 0)
                        }
                        self.stateKeeper.cachedConnection = self.retrieveConnectionDetails()
                        self.stateKeeper.showHelp(messages: help_messages_list)
                    }) {
                        VStack(spacing: 10) {
                            Image(systemName: "info")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 40, height: 40)
                            Text("HELP_LABEL")
                                .lineLimit(1)
                                .allowsTightening(true)
                                .scaledToFit()
                                .minimumScaleFactor(0.70)
                        }.padding()
                    }
                }
                
                if self.stateKeeper.sshAppIds.contains(UIApplication.appId ?? "") {
                    VStack {
                        Toggle(isOn: $showSshTunnelSettings) {
                            Text("SHOW_SSH_TUNNEL_SETTINGS_LABEL")
                        }
                    }.padding()
                }

                if self.showSshTunnelSettings {
                VStack {
                    Text("SSH_TUNNEL_LABEL").font(.headline)
                    TextField("SSH_SERVER_LABEL", text: $sshAddressText).autocapitalization(.none).font(.title)
                    TextField("SSH_PORT_LABEL", text: $sshPortText).autocapitalization(.none).font(.title)
                    TextField("SSH_USER_LABEL", text: $sshUserText).autocapitalization(.none).font(.title)
                    SecureField("SSH_PASSWORD_LABEL", text: $sshPassText).autocapitalization(.none).font(.title)
                    SecureField("SSH_PASSPHRASE_LABEL", text: $sshPassphraseText).autocapitalization(.none).font(.title)
                    VStack {
                        Divider()
                        HStack {
                            Text("SSH_KEY_LABEL").font(.headline)
                            Divider()
                            MultilineTextView(placeholder: "", text: $sshPrivateKeyText, minHeight: self.textHeight, calculatedHeight: $textHeight).frame(minHeight: self.textHeight, maxHeight: self.textHeight)
                            Divider()
                        }
                        Divider()
                    }
                }.padding()
                }

                VStack {
                    Text("MAIN_CONNECTION_SETTINGS_LABEL").font(.headline)
                    TextField("ADDRESS_LABEL", text: $addressText).autocapitalization(.none).font(.title)
                    TextField("PORT_LABEL", text: $portText).font(.title)
                    TextField("USER_LABEL", text: $usernameText).autocapitalization(.none).font(.title)
                    SecureField("PASSWORD_LABEL", text: $passwordText).font(.title)
                }.padding()
                
                VStack {
                    Text("USER_INTERFACE_SETTINGS_LABEL").font(.headline)
                    Toggle(isOn: $allowZooming) {
                        Text("ALLOW_DESKTOP_ZOOMING_LABEL")
                    }
                    Toggle(isOn: $allowPanning) {
                        Text("ALLOW_DESKTOP_PANNING_LABEL")
                    }
                    Text("").padding(.bottom, 1000)
                }.padding()
            }
        }
    }
}

struct ConnectionInProgressPage : View {
    
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            Text("CONNECTING_TO_SERVER_LABEL")
            Button(action: {
                self.stateKeeper.lazyDisconnect()
                self.stateKeeper.showConnections()
            }) {
                HStack(spacing: 10) {
                    Image(systemName: "arrowshape.turn.up.left")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 32, height: 32)
                    Text("CANCEL_LABEL")
                }.padding()
            }
        }
    }
}

struct ProgressPage : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getCurrentTransition() -> String {
        return stateKeeper.currentTransition
    }
    
    var body: some View {
        VStack {
            Text(getCurrentTransition())
        }
    }
}

struct ConnectedSessionPage : View {
    var body: some View {
        Text("")
    }
}

struct HelpDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    var body: some View {
        VStack {
            ScrollView {
                Text("HELP_LABEL").font(.title).padding()
                ForEach(self.stateKeeper.localizedMessages, id: \.self) { message in
                    Text(message).font(.body).padding()
                }
                if self.stateKeeper.helpDialogAppIds.contains(UIApplication.appId ?? "") {
                    VStack {
                        Button(action: {
                            UIApplication.shared.open(URL(string: "https://groups.google.com/forum/#!forum/bvnc-ardp-aspice-opaque-remote-desktop-clients")!, options: [:], completionHandler: nil)
                            
                        }) {
                            HStack(spacing: 10) {
                                Image(systemName: "info.circle")
                                    .resizable()
                                    .scaledToFit()
                                    .frame(width: 32, height: 32)
                                Text("SUPPORT_FORUM_LABEL")
                            }.padding()
                        }
                        
                        Button(action: {
                            UIApplication.shared.open(URL(string: "https://github.com/iiordanov/remote-desktop-clients/issues")!, options: [:], completionHandler: nil)
                            
                        }) {
                            HStack(spacing: 10) {
                                Image(systemName: "ant.circle")
                                    .resizable()
                                    .scaledToFit()
                                    .frame(width: 32, height: 32)
                                Text("REPORT_BUG_LABEL")
                            }.padding()
                        }

                        Button(action: {
                            UIApplication.shared.open(URL(string: "https://www.youtube.com/watch?v=16pwo3wwv9w")!, options: [:], completionHandler: nil)
                            
                        }) {
                            HStack(spacing: 10) {
                                Image(systemName: "video.circle")
                                    .resizable()
                                    .scaledToFit()
                                    .frame(width: 32, height: 32)
                                Text("HELP_VIDEOS_LABEL")
                            }.padding()
                        }

                    }
                }
            }
            Button(action: {
                if (self.stateKeeper.cachedConnection.isEmpty) {
                    self.stateKeeper.showConnections()
                }
                else {
                    self.stateKeeper.selectedConnection = self.stateKeeper.cachedConnection
                    self.stateKeeper.addOrEditConnection()
                }
            }) {
                HStack(spacing: 10) {
                    Image(systemName: "arrowshape.turn.up.left")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 32, height: 32)
                    Text("DISMISS_LABEL")
                }.padding()
            }
        }
    }
}


struct BlankPage : View {
    var body: some View {
        Text("")
    }
}

struct DismissableLogDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getTitle() -> LocalizedStringKey {
        return stateKeeper.localizedTitle ?? ""
    }
    
    var body: some View {
        VStack {
            ScrollView {
                Text(self.getTitle()).font(.title)
                Button(action: {
                    self.stateKeeper.showLog(title: "SESSION_LOG_LABEL",
                                             text: self.stateKeeper.clientLog.joined())
                }) {
                    HStack(spacing: 10) {
                        Image(systemName: "text.alignleft")
                            .resizable()
                            .scaledToFit()
                            .frame(width: 32, height: 32)
                        Text("LOG_LABEL")
                    }.padding()
                }
            }
            Button(action: {
                self.stateKeeper.showConnections()
            }) {
                HStack(spacing: 10) {
                    Image(systemName: "arrowshape.turn.up.left")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 32, height: 32)
                    Text("DISMISS_LABEL")
                }.padding()
            }
        }
    }
}

struct DismissableMessageDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getTitle() -> LocalizedStringKey {
        return stateKeeper.localizedTitle ?? ""
    }

    func getMessage() -> String {
        return stateKeeper.message
    }
    
    var body: some View {
        VStack {
            ScrollView {
                Text(self.getTitle()).font(.title).padding()
                Text(self.getMessage()).font(.body).padding()
            }
            HStack {
                Button(action: {
                    let pasteboard = UIPasteboard.general
                    pasteboard.string = self.getMessage()
                }) {
                    HStack(spacing: 10) {
                        Image(systemName: "doc.on.clipboard")
                            .resizable()
                            .scaledToFit()
                            .frame(width: 32, height: 32)
                        Text("COPY_TO_CLIPBOARD_LABEL")
                    }.padding()
                }
                Button(action: {
                    self.stateKeeper.showConnections()
                }) {
                    HStack(spacing: 10) {
                        Image(systemName: "arrowshape.turn.up.left")
                            .resizable()
                            .scaledToFit()
                            .frame(width: 32, height: 32)
                        Text("DISMISS_LABEL")
                    }.padding()
                }
            }
        }
    }
}

struct YesNoDialog : View {
    @ObservedObject var stateKeeper: StateKeeper
    
    func getLocalizedTitle() -> LocalizedStringKey {
        return stateKeeper.localizedTitle ?? ""
    }
    
    func setResponse(response: Bool) -> Void {
        stateKeeper.setYesNoReponse(response: response,
                                    pageYes: "connectedSession",
                                    pageNo: "connectionsList")
    }

    var body: some View {
        VStack {
            Text(self.getLocalizedTitle()).font(.title).padding()
            Divider()
            ScrollView {
                ForEach(self.stateKeeper.localizedMessages, id: \.self) { message in
                    Text(message).font(.body).padding()
                }
                Text(self.stateKeeper.message).font(.body).padding()
            }
            HStack {
                Button(action: {
                    self.setResponse(response: false)
                }) {
                    Text("NO_LABEL")
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
                    Text("YES_LABEL")
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
        AddOrEditConnectionPage(stateKeeper: StateKeeper(), sshAddressText: "", sshPortText: "", sshUserText: "", sshPassText: "", sshPassphraseText: "", sshPrivateKeyText: "", addressText: "", portText: "", usernameText: "", passwordText: "", screenShotFile: "", allowZooming: true, allowPanning: true, showSshTunnelSettings: false)
    }
}

struct ConnectionInProgressPage_Previews : PreviewProvider {
    static var previews: some View {
        ConnectionInProgressPage(stateKeeper: StateKeeper())
    }
}

struct ProgressPage_Previews : PreviewProvider {
    static var previews: some View {
        ProgressPage(stateKeeper: StateKeeper())
    }
}

struct ConnectedSessionPage_Previews : PreviewProvider {
    static var previews: some View {
        ConnectedSessionPage()
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView(stateKeeper: StateKeeper())
    }
}
