//
//  ConnectionSettings.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-23.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation

class ConnectionSettings {
    let sshAddress: String?
    let sshPort: String?
    let sshUser: String?
    let sshPass: String?
    
    let address: String?
    let port: String?
    let username: String?
    let password: String?
    let cert: String?
    
    init() {
        self.sshAddress = nil
        self.sshPort = nil
        self.sshUser = nil
        self.sshPass = nil
        
        self.address = nil
        self.port = nil
        self.username = nil
        self.password = nil
        self.cert = nil
    }
    
    init(sshAddress: String, sshPort: String, sshUser: String, sshPass: String, address: String, port: String, username: String, password: String, cert: String) {
        self.sshAddress = sshAddress
        self.sshPort = sshPort
        self.sshUser = sshUser
        self.sshPass = sshPass
        
        self.address = address
        self.port = port
        self.username = username
        self.password = password
        self.cert = cert
    }
}
