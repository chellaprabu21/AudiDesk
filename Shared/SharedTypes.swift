//
//  SharedTypes.swift
//  AudiDeck
//
//  Shared data types used across all targets
//

import Foundation
import CoreAudio

// MARK: - Audio Device Types

/// Represents an audio output device in the system
public struct AudioOutputDevice: Identifiable, Hashable, Codable, Sendable {
    public let id: AudioDeviceID
    public let uid: String
    public let name: String
    public let manufacturer: String
    public let isVirtual: Bool
    public let isDefault: Bool
    public let channelCount: Int
    public let sampleRate: Double
    
    public init(
        id: AudioDeviceID,
        uid: String,
        name: String,
        manufacturer: String,
        isVirtual: Bool = false,
        isDefault: Bool = false,
        channelCount: Int = 2,
        sampleRate: Double = 48000.0
    ) {
        self.id = id
        self.uid = uid
        self.name = name
        self.manufacturer = manufacturer
        self.isVirtual = isVirtual
        self.isDefault = isDefault
        self.channelCount = channelCount
        self.sampleRate = sampleRate
    }
    
    public func hash(into hasher: inout Hasher) {
        hasher.combine(uid)
    }
    
    public static func == (lhs: AudioOutputDevice, rhs: AudioOutputDevice) -> Bool {
        lhs.uid == rhs.uid
    }
}

// MARK: - Audio App Types

/// Represents an application that is currently producing audio
public struct AudioProducingApp: Identifiable, Hashable, Sendable {
    public let id: pid_t
    public let bundleIdentifier: String
    public let name: String
    public let icon: Data?  // NSImage data for cross-process transfer
    public var volume: Float
    public var isMuted: Bool
    public var outputDeviceUID: String?
    
    public init(
        pid: pid_t,
        bundleIdentifier: String,
        name: String,
        icon: Data? = nil,
        volume: Float = 1.0,
        isMuted: Bool = false,
        outputDeviceUID: String? = nil
    ) {
        self.id = pid
        self.bundleIdentifier = bundleIdentifier
        self.name = name
        self.icon = icon
        self.volume = volume
        self.isMuted = isMuted
        self.outputDeviceUID = outputDeviceUID
    }
    
    public func hash(into hasher: inout Hasher) {
        hasher.combine(bundleIdentifier)
    }
    
    public static func == (lhs: AudioProducingApp, rhs: AudioProducingApp) -> Bool {
        lhs.bundleIdentifier == rhs.bundleIdentifier
    }
}

// MARK: - Routing Configuration

/// Configuration for routing an app's audio to a specific device
public struct RoutingRule: Codable, Hashable, Sendable {
    public let appBundleIdentifier: String
    public let outputDeviceUID: String
    public var volume: Float
    public var isMuted: Bool
    
    public init(
        appBundleIdentifier: String,
        outputDeviceUID: String,
        volume: Float = 1.0,
        isMuted: Bool = false
    ) {
        self.appBundleIdentifier = appBundleIdentifier
        self.outputDeviceUID = outputDeviceUID
        self.volume = volume
        self.isMuted = isMuted
    }
}

/// Complete routing configuration
public struct RoutingConfiguration: Codable, Sendable {
    public var rules: [RoutingRule]
    public var defaultOutputDeviceUID: String?
    public var isEnabled: Bool
    
    public init(
        rules: [RoutingRule] = [],
        defaultOutputDeviceUID: String? = nil,
        isEnabled: Bool = true
    ) {
        self.rules = rules
        self.defaultOutputDeviceUID = defaultOutputDeviceUID
        self.isEnabled = isEnabled
    }
    
    public mutating func setRule(for bundleID: String, deviceUID: String, volume: Float = 1.0, muted: Bool = false) {
        rules.removeAll { $0.appBundleIdentifier == bundleID }
        rules.append(RoutingRule(
            appBundleIdentifier: bundleID,
            outputDeviceUID: deviceUID,
            volume: volume,
            isMuted: muted
        ))
    }
    
    public func rule(for bundleID: String) -> RoutingRule? {
        rules.first { $0.appBundleIdentifier == bundleID }
    }
}

// MARK: - Driver Status

/// Status of the audio driver installation
public enum DriverStatus: Int, Codable, Sendable {
    case notInstalled
    case installing
    case installed
    case needsUpdate
    case error
    
    public var description: String {
        switch self {
        case .notInstalled: return "Not Installed"
        case .installing: return "Installing..."
        case .installed: return "Installed"
        case .needsUpdate: return "Update Available"
        case .error: return "Error"
        }
    }
}

// MARK: - XPC Command Types

/// Commands sent from main app to XPC helper
public enum AudiDeckCommand: Codable, Sendable {
    case installDriver
    case uninstallDriver
    case checkDriverStatus
    case setRouting(RoutingConfiguration)
    case getRouting
    case setDefaultDevice(deviceUID: String)
    case restartAudioEngine
}

/// Responses from XPC helper to main app
public enum AudiDeckResponse: Codable, Sendable {
    case success
    case driverStatus(DriverStatus)
    case routing(RoutingConfiguration)
    case error(String)
}

// MARK: - Error Types

public enum AudiDeckError: Error, LocalizedError, Sendable {
    case deviceNotFound(String)
    case routingFailed(String)
    case driverNotInstalled
    case driverInstallationFailed(String)
    case xpcConnectionFailed
    case coreAudioError(OSStatus)
    case unknown(String)
    
    public var errorDescription: String? {
        switch self {
        case .deviceNotFound(let uid):
            return "Audio device not found: \(uid)"
        case .routingFailed(let reason):
            return "Failed to route audio: \(reason)"
        case .driverNotInstalled:
            return "AudiDeck driver is not installed"
        case .driverInstallationFailed(let reason):
            return "Driver installation failed: \(reason)"
        case .xpcConnectionFailed:
            return "Failed to connect to helper service"
        case .coreAudioError(let status):
            return "Core Audio error: \(status)"
        case .unknown(let message):
            return message
        }
    }
}

