//
//  AudioModels.swift
//  AudiDeck
//
//  Core data models for AudiDeck
//

import Foundation
import SwiftUI
import AppKit
import UniformTypeIdentifiers

// MARK: - Audio Device

struct AudioDevice: Identifiable, Hashable {
    let id: String          // Unique identifier (UID from Core Audio)
    let name: String        // Display name
    let isDefault: Bool
    let icon: String        // SF Symbol name
    
    static let systemDefault = AudioDevice(
        id: "system_default",
        name: "System Default",
        isDefault: true,
        icon: "speaker.wave.3"
    )
    
    // Mock devices for development
    static let mockDevices: [AudioDevice] = [
        systemDefault,
        AudioDevice(id: "builtin_speakers", name: "MacBook Speakers", isDefault: false, icon: "laptopcomputer"),
        AudioDevice(id: "airpods_pro", name: "AirPods Pro", isDefault: false, icon: "airpodspro"),
        AudioDevice(id: "external_dac", name: "USB Audio DAC", isDefault: false, icon: "cable.connector")
    ]
}

// MARK: - Audio Application

struct AudioApp: Identifiable, Hashable {
    let id: String              // Bundle identifier
    let name: String            // Display name
    let icon: NSImage?          // App icon
    var assignedDeviceID: String
    var volume: Float
    var isMuted: Bool
    
    init(
        id: String,
        name: String,
        icon: NSImage? = nil,
        assignedDeviceID: String = AudioDevice.systemDefault.id,
        volume: Float = 1.0,
        isMuted: Bool = false
    ) {
        self.id = id
        self.name = name
        self.icon = icon
        self.assignedDeviceID = assignedDeviceID
        self.volume = volume
        self.isMuted = isMuted
    }
    
    // Mock apps for development
    static let mockApps: [AudioApp] = [
        AudioApp(id: "com.spotify.client", name: "Spotify", icon: NSWorkspace.shared.icon(forFile: "/Applications/Spotify.app")),
        AudioApp(id: "com.apple.Music", name: "Music", icon: NSWorkspace.shared.icon(forFile: "/System/Applications/Music.app")),
        AudioApp(id: "com.google.Chrome", name: "Chrome", icon: NSWorkspace.shared.icon(forFile: "/Applications/Google Chrome.app")),
        AudioApp(id: "com.apple.Safari", name: "Safari", icon: NSWorkspace.shared.icon(forFile: "/Applications/Safari.app")),
        AudioApp(id: "us.zoom.xos", name: "Zoom", icon: NSWorkspace.shared.icon(forFile: "/Applications/zoom.us.app"))
    ]
}

// MARK: - Drag & Drop Transfer

struct AppTransfer: Codable, Transferable {
    let appID: String
    
    static var transferRepresentation: some TransferRepresentation {
        CodableRepresentation(contentType: .data)
    }
}

// MARK: - Routing Command

struct RoutingCommand: Codable {
    let appBundleID: String
    let targetDeviceUID: String
    let volume: Float
    let isMuted: Bool
    let timestamp: Date
    
    init(appBundleID: String, targetDeviceUID: String, volume: Float = 1.0, isMuted: Bool = false) {
        self.appBundleID = appBundleID
        self.targetDeviceUID = targetDeviceUID
        self.volume = volume
        self.isMuted = isMuted
        self.timestamp = Date()
    }
}

