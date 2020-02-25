//
//  ConnectionSettings.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-02-23.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation

class ConnectionSettings {
    let address: String?
    let port: String?
    let username: String?
    let password: String?
    let cert: String?
    
    init() {
        self.address = nil
        self.port = nil
        self.username = nil
        self.password = nil
        self.cert = nil
    }
    
    init(address: String, port: String, username: String, password: String, cert: String) {
        self.address = address
        self.port = port
        self.username = username
        self.password = password
        self.cert = cert
    }
}
