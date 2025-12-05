//
//  Constants.swift
//  AudiDeck
//
//  Shared constants used across all targets
//

import Foundation

public enum AudiDeckConstants {
    // MARK: - Bundle Identifiers
    public static let mainAppBundleID = "com.audiorouter.AudiDeck"
    public static let helperBundleID = "com.audiorouter.AudiDeck.Helper"
    public static let driverBundleID = "com.audiorouter.AudiDeck.Driver"
    
    // MARK: - XPC Service
    public static let xpcServiceName = "com.audiorouter.AudiDeck.Helper"
    public static let xpcMachServiceName = "com.audiorouter.AudiDeck.Helper.mach"
    
    // MARK: - App Group
    public static let appGroupIdentifier = "group.com.audiorouter.shared"
    
    // MARK: - Virtual Device
    public static let virtualDeviceName = "AudiDeck Virtual Device"
    public static let virtualDeviceUID = "AudiDeckVirtualDevice_UID"
    public static let virtualDeviceManufacturer = "AudiDeck"
    public static let virtualDeviceModelUID = "AudiDeckModel_UID"
    
    // MARK: - Audio Configuration
    public static let defaultSampleRate: Double = 48000.0
    public static let defaultBitDepth: UInt32 = 32
    public static let defaultChannelCount: UInt32 = 2
    public static let defaultBufferFrameSize: UInt32 = 512
    
    // MARK: - User Defaults Keys
    public enum UserDefaultsKeys {
        public static let routingConfiguration = "AudiDeckRoutingConfig"
        public static let lastSelectedDevice = "AudiDeckLastSelectedDevice"
        public static let perAppVolumes = "AudiDeckPerAppVolumes"
        public static let isEnabled = "AudiDeckIsEnabled"
        public static let showInDock = "AudiDeckShowInDock"
        public static let launchAtLogin = "AudiDeckLaunchAtLogin"
    }
    
    // MARK: - Notifications
    public enum Notifications {
        public static let deviceListChanged = Notification.Name("AudiDeckDeviceListChanged")
        public static let audioAppsChanged = Notification.Name("AudiDeckAudioAppsChanged")
        public static let routingChanged = Notification.Name("AudiDeckRoutingChanged")
        public static let driverStatusChanged = Notification.Name("AudiDeckDriverStatusChanged")
    }
}

