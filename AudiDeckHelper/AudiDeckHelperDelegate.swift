//
//  AudiDeckHelperDelegate.swift
//  AudiDeckHelper
//
//  Implementation of the XPC helper service protocol
//

import Foundation
import CoreAudio

/// Implementation of the AudiDeck helper service
@objc class AudiDeckHelperDelegate: NSObject, AudiDeckHelperProtocol {
    
    // MARK: - Properties
    
    private let driverBundleID = "com.audiorouter.AudiDeck.Driver"
    private let driverPath = "/Library/Audio/Plug-Ins/HAL/AudiDeckDriver.driver"
    private let configPath: URL
    
    // MARK: - Initialization
    
    override init() {
        // Use app group container for shared configuration
        let appGroup = "group.com.audiorouter.shared"
        if let containerURL = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroup) {
            configPath = containerURL.appendingPathComponent("routing_config.json")
        } else {
            // Fallback to Library/Application Support
            let appSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
            let audioRouterDir = appSupport.appendingPathComponent("AudiDeck")
            try? FileManager.default.createDirectory(at: audioRouterDir, withIntermediateDirectories: true)
            configPath = audioRouterDir.appendingPathComponent("routing_config.json")
        }
        
        super.init()
    }
    
    // MARK: - Protocol Implementation
    
    func installDriver(reply: @escaping @Sendable (Bool, String?) -> Void) {
        // Check if driver bundle exists in app bundle
        guard let appBundle = Bundle.main.url(forResource: "AudiDeckDriver", withExtension: "driver") else {
            reply(false, "Driver bundle not found in application")
            return
        }
        
        // Create HAL plugins directory if needed
        let halPluginsDir = URL(fileURLWithPath: "/Library/Audio/Plug-Ins/HAL")
        let destinationURL = halPluginsDir.appendingPathComponent("AudiDeckDriver.driver")
        
        do {
            // Remove existing driver if present
            if FileManager.default.fileExists(atPath: destinationURL.path) {
                try FileManager.default.removeItem(at: destinationURL)
            }
            
            // Copy driver to HAL plugins directory
            try FileManager.default.copyItem(at: appBundle, to: destinationURL)
            
            // Restart Core Audio to load the new driver
            restartCoreAudioDaemon { success, error in
                if success {
                    reply(true, nil)
                } else {
                    reply(false, error)
                }
            }
        } catch {
            reply(false, "Failed to install driver: \(error.localizedDescription)")
        }
    }
    
    func uninstallDriver(reply: @escaping @Sendable (Bool, String?) -> Void) {
        let driverURL = URL(fileURLWithPath: driverPath)
        
        do {
            if FileManager.default.fileExists(atPath: driverPath) {
                try FileManager.default.removeItem(at: driverURL)
            }
            
            // Restart Core Audio
            restartCoreAudioDaemon { success, error in
                reply(success, error)
            }
        } catch {
            reply(false, "Failed to uninstall driver: \(error.localizedDescription)")
        }
    }
    
    func checkDriverStatus(reply: @escaping @Sendable (Int) -> Void) {
        // Check if driver file exists
        let driverExists = FileManager.default.fileExists(atPath: driverPath)
        
        if !driverExists {
            reply(0) // notInstalled
            return
        }
        
        // Check if the driver is loaded by looking for our virtual device
        let deviceIDs = getAllAudioDeviceIDs()
        var driverLoaded = false
        
        for deviceID in deviceIDs {
            if let uid = getDeviceUID(deviceID), uid.contains("AudiDeck") {
                driverLoaded = true
                break
            }
        }
        
        if driverLoaded {
            reply(2) // installed
        } else {
            reply(4) // error - driver exists but not loaded
        }
    }
    
    func setRoutingConfiguration(_ configData: Data, reply: @escaping @Sendable (Bool, String?) -> Void) {
        do {
            try configData.write(to: configPath)
            
            // Notify the driver of configuration change (if it supports it)
            notifyDriverOfConfigChange()
            
            reply(true, nil)
        } catch {
            reply(false, "Failed to save configuration: \(error.localizedDescription)")
        }
    }
    
    func getRoutingConfiguration(reply: @escaping @Sendable (Data?) -> Void) {
        do {
            let data = try Data(contentsOf: configPath)
            reply(data)
        } catch {
            reply(nil)
        }
    }
    
    func setDefaultOutputDevice(_ deviceUID: String, reply: @escaping @Sendable (Bool, String?) -> Void) {
        // Find device ID for the given UID
        guard let deviceID = findDeviceID(forUID: deviceUID) else {
            reply(false, "Device not found: \(deviceUID)")
            return
        }
        
        // Set as default output device
        var mutableDeviceID = deviceID
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDefaultOutputDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        let status = AudioObjectSetPropertyData(
            AudioObjectID(kAudioObjectSystemObject),
            &propertyAddress,
            0,
            nil,
            UInt32(MemoryLayout<AudioDeviceID>.size),
            &mutableDeviceID
        )
        
        if status == noErr {
            reply(true, nil)
        } else {
            reply(false, "Failed to set default device: error \(status)")
        }
    }
    
    func restartCoreAudio(reply: @escaping @Sendable (Bool, String?) -> Void) {
        restartCoreAudioDaemon(reply: reply)
    }
    
    func ping(reply: @escaping @Sendable (Bool) -> Void) {
        reply(true)
    }
    
    // MARK: - Private Helpers
    
    private func restartCoreAudioDaemon(reply: @escaping @Sendable (Bool, String?) -> Void) {
        let task = Process()
        task.executableURL = URL(fileURLWithPath: "/bin/launchctl")
        task.arguments = ["kickstart", "-kp", "system/com.apple.audio.coreaudiod"]
        
        let pipe = Pipe()
        task.standardError = pipe
        
        do {
            try task.run()
            task.waitUntilExit()
            
            if task.terminationStatus == 0 {
                // Give Core Audio time to restart
                DispatchQueue.global().asyncAfter(deadline: .now() + 1.0) {
                    reply(true, nil)
                }
            } else {
                let errorData = pipe.fileHandleForReading.readDataToEndOfFile()
                let errorMessage = String(data: errorData, encoding: .utf8) ?? "Unknown error"
                reply(false, "Failed to restart Core Audio: \(errorMessage)")
            }
        } catch {
            reply(false, "Failed to restart Core Audio: \(error.localizedDescription)")
        }
    }
    
    private func getAllAudioDeviceIDs() -> [AudioDeviceID] {
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDevices,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        var dataSize: UInt32 = 0
        var status = AudioObjectGetPropertyDataSize(
            AudioObjectID(kAudioObjectSystemObject),
            &propertyAddress,
            0,
            nil,
            &dataSize
        )
        
        guard status == noErr else { return [] }
        
        let deviceCount = Int(dataSize) / MemoryLayout<AudioDeviceID>.size
        var deviceIDs = [AudioDeviceID](repeating: 0, count: deviceCount)
        
        status = AudioObjectGetPropertyData(
            AudioObjectID(kAudioObjectSystemObject),
            &propertyAddress,
            0,
            nil,
            &dataSize,
            &deviceIDs
        )
        
        return status == noErr ? deviceIDs : []
    }
    
    private func getDeviceUID(_ deviceID: AudioDeviceID) -> String? {
        var uid: CFString?
        var size = UInt32(MemoryLayout<CFString?>.size)
        
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyDeviceUID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        let status = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nil, &size, &uid)
        return status == noErr ? uid as String? : nil
    }
    
    private func findDeviceID(forUID uid: String) -> AudioDeviceID? {
        let deviceIDs = getAllAudioDeviceIDs()
        
        for deviceID in deviceIDs {
            if let deviceUID = getDeviceUID(deviceID), deviceUID == uid {
                return deviceID
            }
        }
        
        return nil
    }
    
    private func notifyDriverOfConfigChange() {
        // In a real implementation, this would use a custom notification mechanism
        // to inform the driver of configuration changes. This could be:
        // 1. A custom property on the virtual device
        // 2. A shared memory region
        // 3. A Mach port message
        
        // For now, we rely on the driver polling the config file
    }
}

