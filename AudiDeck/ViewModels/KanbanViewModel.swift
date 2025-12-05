//
//  KanbanViewModel.swift
//  AudiDeck
//
//  ViewModel for the Kanban audio router
//

import Foundation
import SwiftUI
import AppKit
import Combine
import UniformTypeIdentifiers
import CoreAudio

@MainActor
class KanbanViewModel: ObservableObject {
    
    // MARK: - Published Properties
    
    @Published var devices: [AudioDevice] = []
    @Published var audioApps: [AudioApp] = []
    @Published var masterOutputDeviceID: String = "system_default"
    
    // MARK: - Initialization
    
    init() {
        loadDevices()
        loadAllRunningApps()
    }
    
    // MARK: - Public Methods
    
    /// Get apps assigned to a specific device
    func apps(for device: AudioDevice) -> [AudioApp] {
        audioApps.filter { $0.assignedDeviceID == device.id }
    }
    
    /// Move an app to a different device
    func moveApp(appID: String, to deviceID: String) {
        guard let index = audioApps.firstIndex(where: { $0.id == appID }) else { return }
        
        audioApps[index].assignedDeviceID = deviceID
        
        // Log the routing command
        let command = RoutingCommand(
            appBundleID: appID,
            targetDeviceUID: deviceID
        )
        print("📻 Routing: \(command.appBundleID) → \(command.targetDeviceUID)")
    }
    
    /// Set master output device
    func setMasterOutput(_ deviceID: String) {
        masterOutputDeviceID = deviceID
        print("🔊 Master Output: \(deviceID)")
        
        // In production, this would call Core Audio to change default output
        setSystemDefaultOutput(deviceID)
    }
    
    /// Set volume for an app
    func setVolume(_ volume: Float, for appID: String) {
        guard let index = audioApps.firstIndex(where: { $0.id == appID }) else { return }
        audioApps[index].volume = max(0, min(1, volume))
    }
    
    /// Toggle mute for an app
    func toggleMute(for appID: String) {
        guard let index = audioApps.firstIndex(where: { $0.id == appID }) else { return }
        audioApps[index].isMuted.toggle()
    }
    
    /// Refresh the data
    func refresh() {
        loadDevices()
        loadAllRunningApps()
    }
    
    // MARK: - Device Loading
    
    private func loadDevices() {
        var foundDevices: [AudioDevice] = []
        
        // Get real audio output devices from Core Audio
        let deviceIDs = getAudioDeviceIDs()
        let defaultOutputID = getDefaultOutputDeviceID()
        
        for deviceID in deviceIDs {
            guard hasOutputStreams(deviceID),
                  let name = getDeviceName(deviceID),
                  let uid = getDeviceUID(deviceID) else { continue }
            
            let isDefault = deviceID == defaultOutputID
            let icon = getDeviceIcon(name: name)
            
            let device = AudioDevice(
                id: uid,
                name: name,
                isDefault: isDefault,
                icon: icon
            )
            foundDevices.append(device)
            
            if isDefault {
                masterOutputDeviceID = uid
            }
        }
        
        // Sort with default first
        foundDevices.sort { $0.isDefault && !$1.isDefault }
        
        devices = foundDevices.isEmpty ? AudioDevice.mockDevices : foundDevices
    }
    
    private func getDeviceIcon(name: String) -> String {
        let lowercased = name.lowercased()
        if lowercased.contains("airpods") {
            return "airpodspro"
        } else if lowercased.contains("headphone") {
            return "headphones"
        } else if lowercased.contains("bluetooth") || lowercased.contains("bt") {
            return "wave.3.right"
        } else if lowercased.contains("usb") || lowercased.contains("dac") {
            return "cable.connector"
        } else if lowercased.contains("macbook") || lowercased.contains("built-in") || lowercased.contains("speaker") {
            return "laptopcomputer"
        } else if lowercased.contains("display") || lowercased.contains("hdmi") {
            return "display"
        }
        return "speaker.wave.3"
    }
    
    // MARK: - App Loading
    
    private func loadAllRunningApps() {
        var foundApps: [AudioApp] = []
        let runningApps = NSWorkspace.shared.runningApplications
        
        for app in runningApps {
            guard let bundleID = app.bundleIdentifier,
                  app.activationPolicy == .regular,  // Only regular apps (not background/menu bar)
                  !bundleID.hasPrefix("com.apple.preference"),  // Skip preference panes
                  bundleID != Bundle.main.bundleIdentifier  // Skip ourselves
            else { continue }
            
            // Preserve existing assignment if app was already tracked
            let existingApp = audioApps.first { $0.id == bundleID }
            
            let audioApp = AudioApp(
                id: bundleID,
                name: app.localizedName ?? "Unknown",
                icon: app.icon,
                assignedDeviceID: existingApp?.assignedDeviceID ?? masterOutputDeviceID,
                volume: existingApp?.volume ?? 1.0,
                isMuted: existingApp?.isMuted ?? false
            )
            foundApps.append(audioApp)
        }
        
        // Sort alphabetically
        foundApps.sort { $0.name.localizedCompare($1.name) == .orderedAscending }
        
        audioApps = foundApps
    }
    
    // MARK: - Core Audio Helpers
    
    private func getAudioDeviceIDs() -> [AudioDeviceID] {
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
    
    private func getDefaultOutputDeviceID() -> AudioDeviceID {
        var deviceID: AudioDeviceID = 0
        var size = UInt32(MemoryLayout<AudioDeviceID>.size)
        
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDefaultOutputDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        AudioObjectGetPropertyData(
            AudioObjectID(kAudioObjectSystemObject),
            &propertyAddress,
            0,
            nil,
            &size,
            &deviceID
        )
        
        return deviceID
    }
    
    private func hasOutputStreams(_ deviceID: AudioDeviceID) -> Bool {
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyStreams,
            mScope: kAudioDevicePropertyScopeOutput,
            mElement: kAudioObjectPropertyElementMain
        )
        
        var dataSize: UInt32 = 0
        let status = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nil, &dataSize)
        return status == noErr && dataSize > 0
    }
    
    private func getDeviceName(_ deviceID: AudioDeviceID) -> String? {
        var name: CFString = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyDeviceNameCFString,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        let status = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nil, &size, &name)
        return status == noErr ? name as String : nil
    }
    
    private func getDeviceUID(_ deviceID: AudioDeviceID) -> String? {
        var uid: CFString = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        
        var propertyAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyDeviceUID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        
        let status = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nil, &size, &uid)
        return status == noErr ? uid as String : nil
    }
    
    private func setSystemDefaultOutput(_ deviceUID: String) {
        // Find device ID from UID
        for deviceID in getAudioDeviceIDs() {
            guard let uid = getDeviceUID(deviceID), uid == deviceUID else { continue }
            
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
                print("✅ Set default output to: \(deviceUID)")
                // Refresh to update isDefault flags
                loadDevices()
            } else {
                print("❌ Failed to set default output: \(status)")
            }
            break
        }
    }
}
