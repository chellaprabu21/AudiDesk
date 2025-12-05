# AudiDeck - macOS Per-App Audio Routing Architecture

## Overview

AudiDeck is a macOS menu bar application that enables per-application audio output device selection and volume control. This document outlines the complete architecture, components, and implementation strategy.

## ⚠️ Important Technical Considerations

### The Challenge of Per-App Audio Routing on macOS

macOS does **not** natively expose per-process audio streams to third-party applications. This is a fundamental architectural limitation that makes true per-app routing extremely challenging. Here's what you need to know:

1. **Core Audio's Design**: Audio streams are mixed at the HAL (Hardware Abstraction Layer) level before reaching output devices. Individual app streams are not exposed.

2. **No Process-Level Audio API**: Unlike Windows (with WASAPI), macOS doesn't provide a documented API to intercept audio per-process.

3. **Available Approaches**:
   - **HAL Plugin (Recommended)**: Creates virtual audio devices that can capture mixed audio
   - **AudioDriverKit (DEXT)**: Modern Apple approach for audio drivers, but still can't isolate per-app streams
   - **Code Injection**: Requires SIP disabled, not viable for distribution
   - **Tap into AudioUnits**: Only works for apps using AudioUnits, not all audio sources

### Our Approach: Virtual Device + Best-Effort Process Detection

This implementation uses a **HAL Audio Plugin** to create virtual audio devices combined with Core Audio APIs for process monitoring. Full per-app isolation is achieved through:

1. Applications voluntarily outputting to specific virtual devices
2. Monitoring which apps are producing audio (via `AudioObjectGetPropertyData` with `kAudioHardwarePropertyProcessIsAudible`)
3. Per-device volume control and routing to physical outputs

---

## Architecture Components

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AudiDeck.app (Main Bundle)                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌──────────────────────┐    ┌──────────────────────┐    ┌────────────────┐ │
│  │   SwiftUI Menu Bar   │    │   Audio Manager      │    │  XPC Client    │ │
│  │   - Popover UI       │◄──►│   - Device Enum      │◄──►│  - Commands    │ │
│  │   - App List         │    │   - Process Monitor  │    │  - Events      │ │
│  │   - Volume Sliders   │    │   - Stream Routing   │    │                │ │
│  └──────────────────────┘    └──────────────────────┘    └───────┬────────┘ │
│                                                                   │          │
└───────────────────────────────────────────────────────────────────┼──────────┘
                                                                    │ XPC
┌───────────────────────────────────────────────────────────────────┼──────────┐
│                         AudiDeckHelper (XPC Service)            │          │
├───────────────────────────────────────────────────────────────────┴──────────┤
│  ┌──────────────────────┐    ┌──────────────────────┐                        │
│  │   XPC Listener       │    │   Privileged Ops     │                        │
│  │   - Protocol Impl    │◄──►│   - Device Control   │                        │
│  │   - Security Check   │    │   - Config Storage   │                        │
│  └──────────────────────┘    └──────────────────────┘                        │
└──────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                    AudiDeckDriver.driver (HAL Plugin)                      │
├──────────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────┐    ┌──────────────────────┐    ┌────────────────┐ │
│  │   Virtual Device     │    │   Audio Engine       │    │  Device Bridge │ │
│  │   - Multi-Output     │◄──►│   - Buffer Mgmt      │◄──►│  - Route to    │ │
│  │   - Per-App Streams  │    │   - Sample Convert   │    │    Physical    │ │
│  └──────────────────────┘    └──────────────────────┘    └────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## File Structure

```
AudiDeck/
├── AudiDeck.xcodeproj/
│   └── project.pbxproj
│
├── AudiDeck/                    # Main App Target
│   ├── App/
│   │   ├── AudiDeckApp.swift   # @main SwiftUI App
│   │   └── AppDelegate.swift        # NSApplicationDelegate for menu bar
│   │
│   ├── Views/
│   │   ├── MenuBarView.swift        # Menu bar icon and popover trigger
│   │   ├── AudioPopoverView.swift   # Main popover content
│   │   ├── AppAudioRowView.swift    # Individual app audio control row
│   │   ├── DevicePickerView.swift   # Output device selector
│   │   └── VolumeSliderView.swift   # Custom volume slider
│   │
│   ├── ViewModels/
│   │   ├── AudiDeckViewModel.swift
│   │   └── AppAudioViewModel.swift
│   │
│   ├── Services/
│   │   ├── AudioDeviceManager.swift     # Core Audio device enumeration
│   │   ├── ProcessAudioMonitor.swift    # Monitor audio-producing apps
│   │   ├── AudioRoutingService.swift    # Route audio between devices
│   │   └── XPCClient.swift              # XPC communication client
│   │
│   ├── Models/
│   │   ├── AudioDevice.swift            # Audio device model
│   │   ├── AudioApp.swift               # Audio-producing app model
│   │   └── RoutingConfiguration.swift   # Routing settings model
│   │
│   ├── Utilities/
│   │   ├── CoreAudioHelpers.swift       # Core Audio utility functions
│   │   └── ProcessHelpers.swift         # Process info utilities
│   │
│   ├── Resources/
│   │   ├── Assets.xcassets/
│   │   └── Localizable.strings
│   │
│   ├── AudiDeck.entitlements
│   └── Info.plist
│
├── AudiDeckHelper/               # XPC Service Target
│   ├── main.swift
│   ├── AudiDeckHelperProtocol.swift
│   ├── AudiDeckHelperDelegate.swift
│   ├── AudiDeckHelper.entitlements
│   └── Info.plist
│
├── AudiDeckDriver/               # HAL Plugin Target (Bundle)
│   ├── AudiDeckDriver.cpp        # Main plugin implementation
│   ├── AudiDeckDevice.cpp        # Virtual device implementation
│   ├── AudiDeckDevice.h
│   ├── AudiDeckStream.cpp        # Audio stream handling
│   ├── AudiDeckStream.h
│   ├── AudiDeckPlugIn.cpp        # Plugin entry point
│   ├── AudiDeckPlugIn.h
│   ├── AudioRingBuffer.cpp          # Lock-free ring buffer
│   ├── AudioRingBuffer.h
│   ├── Info.plist
│   └── AudiDeckDriver.entitlements
│
├── Shared/                          # Shared Code
│   ├── AudiDeckProtocol.swift    # XPC protocol definitions
│   ├── SharedTypes.swift            # Shared data types
│   └── Constants.swift              # App-wide constants
│
└── ARCHITECTURE.md                  # This file
```

---

## Component Details

### 1. Main Application (AudiDeck)

**Purpose**: SwiftUI menu bar app providing the user interface.

**Key Classes**:
- `AudiDeckApp`: Main app entry point, configures menu bar presence
- `AudioDeviceManager`: Enumerates and monitors audio devices using Core Audio
- `ProcessAudioMonitor`: Detects apps currently producing audio
- `XPCClient`: Communicates with privileged helper

### 2. XPC Helper Service (AudiDeckHelper)

**Purpose**: Privileged operations that require elevated permissions.

**Key Responsibilities**:
- Installing/updating the audio driver
- Modifying system audio settings
- Persisting configuration

### 3. HAL Audio Plugin (AudiDeckDriver)

**Purpose**: Virtual audio device that appears in System Preferences.

**Key Features**:
- Registers as a valid audio output device
- Captures audio sent to it
- Routes audio to selected physical output
- Supports multiple virtual "channels" for per-app routing

---

## Core Audio API Usage

### Device Enumeration
```swift
// Get all audio devices
var propertyAddress = AudioObjectPropertyAddress(
    mSelector: kAudioHardwarePropertyDevices,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMain
)
```

### Process Audio Monitoring
```swift
// Check if process is audible (macOS 12+)
var propertyAddress = AudioObjectPropertyAddress(
    mSelector: kAudioHardwarePropertyProcessIsAudible,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMain
)
```

### Default Device Control
```swift
// Set default output device
var propertyAddress = AudioObjectPropertyAddress(
    mSelector: kAudioHardwarePropertyDefaultOutputDevice,
    mScope: kAudioObjectPropertyScopeGlobal,
    mElement: kAudioObjectPropertyElementMain
)
```

---

## Security & Entitlements

### Main App Entitlements
```xml
<key>com.apple.security.app-sandbox</key>
<true/>
<key>com.apple.security.temporary-exception.audio-unit-host</key>
<true/>
<key>com.apple.security.application-groups</key>
<array>
    <string>$(TeamIdentifierPrefix)com.audiorouter.shared</string>
</array>
```

### XPC Service Entitlements
```xml
<key>com.apple.security.app-sandbox</key>
<true/>
<key>com.apple.security.inherit</key>
<true/>
```

### Driver Entitlements
```xml
<key>com.apple.developer.system-extension.install</key>
<true/>
<key>com.apple.developer.driverkit</key>
<true/>
```

---

## Build & Installation

### Prerequisites
- macOS 13.0+ (Ventura or later)
- Xcode 15.0+
- Apple Developer account (for signing)
- System Integrity Protection considerations for driver installation

### Build Steps
1. Open `AudiDeck.xcodeproj` in Xcode
2. Select your development team for all targets
3. Build the main app target
4. Install the HAL plugin to `/Library/Audio/Plug-Ins/HAL/`
5. Restart Core Audio: `sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod`

---

## Limitations & Future Work

### Current Limitations
1. **No True Per-App Isolation**: Due to macOS architecture, we cannot intercept individual app audio streams without system-level modifications
2. **Requires User Action**: Users must manually select virtual devices as output for apps they want to route
3. **Driver Installation**: Requires admin privileges and potentially SIP considerations

### Future Enhancements
- Audio effects processing (EQ, compression)
- Audio recording capabilities
- Multiple simultaneous output routing
- Keyboard shortcuts for quick device switching
- Audio visualization

---

## References

- [Core Audio Overview](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/)
- [Audio Hardware Abstraction Layer](https://developer.apple.com/documentation/coreaudio/audio_hardware_abstraction_layer)
- [AudioDriverKit](https://developer.apple.com/documentation/audiodriverkit)
- [BlackHole Virtual Audio Driver](https://github.com/ExistentialAudio/BlackHole)
- [XPC Services](https://developer.apple.com/documentation/xpc)

