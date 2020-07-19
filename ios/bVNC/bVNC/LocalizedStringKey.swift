//
//  LocalizedStringKey.swift
//  bVNC
//
//  Created by iordan iordanov on 2020-07-19.
//  Copyright © 2020 iordan iordanov. All rights reserved.
//

import Foundation
import Combine
import SwiftUI

extension LocalizedStringKey: Hashable, LosslessStringConvertible {
    public var description: String {
        return ""
    }
    
    public func hash(into hasher: inout Hasher) {
        hasher.combine(String(self))
    }
}
