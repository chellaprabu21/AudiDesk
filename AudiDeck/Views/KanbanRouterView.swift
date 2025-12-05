//
//  KanbanRouterView.swift
//  AudiDeck
//
//  Main Kanban-style audio routing interface
//

import SwiftUI
import UniformTypeIdentifiers

struct KanbanRouterView: View {
    @StateObject private var viewModel = KanbanViewModel()
    
    var body: some View {
        VStack(spacing: 0) {
            // Header with Master Output
            headerView
            
            // Master Output Selector
            masterOutputSelector
            
            Divider()
            
            // Kanban Board
            ScrollView(.horizontal, showsIndicators: true) {
                HStack(alignment: .top, spacing: 12) {
                    ForEach(viewModel.devices) { device in
                        DeviceColumnView(
                            device: device,
                            apps: viewModel.apps(for: device),
                            onDrop: { appID in
                                viewModel.moveApp(appID: appID, to: device.id)
                            },
                            onVolumeChange: { appID, volume in
                                viewModel.setVolume(volume, for: appID)
                            },
                            onMuteToggle: { appID in
                                viewModel.toggleMute(for: appID)
                            }
                        )
                    }
                }
                .padding()
            }
            
            Divider()
            
            // Footer
            footerView
        }
        .frame(width: 650, height: 500)
        .background(Color(nsColor: .windowBackgroundColor))
    }
    
    // MARK: - Header
    
    private var headerView: some View {
        HStack {
            Image(systemName: "waveform.circle.fill")
                .font(.title2)
                .foregroundStyle(.blue)
            
            Text("AudiDeck")
                .font(.headline)
            
            Spacer()
            
            Button(action: { viewModel.refresh() }) {
                Image(systemName: "arrow.clockwise")
            }
            .buttonStyle(.plain)
            .help("Refresh apps and devices")
            
            Button(action: {
                NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
            }) {
                Image(systemName: "gear")
            }
            .buttonStyle(.plain)
            .help("Settings")
        }
        .padding(.horizontal)
        .padding(.top, 12)
        .padding(.bottom, 8)
    }
    
    // MARK: - Master Output Selector
    
    private var masterOutputSelector: some View {
        HStack {
            Image(systemName: "speaker.wave.3.fill")
                .foregroundStyle(.secondary)
            
            Text("System Output:")
                .font(.subheadline)
                .foregroundStyle(.secondary)
            
            Picker("", selection: $viewModel.masterOutputDeviceID) {
                ForEach(viewModel.devices) { device in
                    HStack {
                        Image(systemName: device.icon)
                        Text(device.name)
                    }
                    .tag(device.id)
                }
            }
            .pickerStyle(.menu)
            .frame(maxWidth: 250)
            .onChange(of: viewModel.masterOutputDeviceID) { _, newValue in
                viewModel.setMasterOutput(newValue)
            }
            
            Spacer()
            
            Text("Drag apps to route audio")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color.primary.opacity(0.03))
    }
    
    // MARK: - Footer
    
    private var footerView: some View {
        HStack {
            Circle()
                .fill(.green)
                .frame(width: 8, height: 8)
            
            Text("Ready")
                .font(.caption)
                .foregroundStyle(.secondary)
            
            Spacer()
            
            Text("\(viewModel.audioApps.count) apps • \(viewModel.devices.count) devices")
                .font(.caption)
                .foregroundStyle(.secondary)
            
            Spacer()
            
            Button("Quit") {
                NSApp.terminate(nil)
            }
            .buttonStyle(.plain)
            .font(.caption)
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
    }
}

// MARK: - Device Column

struct DeviceColumnView: View {
    let device: AudioDevice
    let apps: [AudioApp]
    let onDrop: (String) -> Void
    let onVolumeChange: (String, Float) -> Void
    let onMuteToggle: (String) -> Void
    
    @State private var isTargeted = false
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            // Column Header
            HStack {
                Image(systemName: device.icon)
                    .font(.system(size: 14))
                    .foregroundStyle(device.isDefault ? .blue : .secondary)
                
                VStack(alignment: .leading, spacing: 1) {
                    Text(device.name)
                        .font(.system(size: 12, weight: .semibold))
                        .lineLimit(1)
                    
                    if device.isDefault {
                        Text("System Default")
                            .font(.system(size: 9))
                            .foregroundStyle(.blue)
                    }
                }
                
                Spacer()
                
                Text("\(apps.count)")
                    .font(.caption2)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Color.primary.opacity(0.1))
                    .clipShape(Capsule())
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.primary.opacity(0.05))
            .clipShape(RoundedRectangle(cornerRadius: 8))
            
            // Apps in this column - scrollable
            ScrollView(.vertical, showsIndicators: false) {
                VStack(spacing: 4) {
                    ForEach(apps) { app in
                        AppCardView(
                            app: app,
                            onVolumeChange: { volume in
                                onVolumeChange(app.id, volume)
                            },
                            onMuteToggle: {
                                onMuteToggle(app.id)
                            }
                        )
                    }
                    
                    if apps.isEmpty {
                        emptyColumnPlaceholder
                    }
                }
            }
            .frame(maxHeight: .infinity)
        }
        .frame(width: 150)
        .padding(8)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(isTargeted ? Color.blue.opacity(0.1) : Color.primary.opacity(0.02))
                .strokeBorder(isTargeted ? Color.blue.opacity(0.5) : Color.primary.opacity(0.08), lineWidth: 1)
        )
        .dropDestination(for: String.self) { items, _ in
            guard let appID = items.first else { return false }
            onDrop(appID)
            return true
        } isTargeted: { targeted in
            withAnimation(.easeInOut(duration: 0.2)) {
                isTargeted = targeted
            }
        }
    }
    
    private var emptyColumnPlaceholder: some View {
        VStack(spacing: 6) {
            Image(systemName: "plus.circle.dashed")
                .font(.title3)
                .foregroundStyle(.quaternary)
            
            Text("Drop apps here")
                .font(.caption2)
                .foregroundStyle(.quaternary)
        }
        .frame(maxWidth: .infinity)
        .frame(height: 60)
        .background(Color.primary.opacity(0.02))
        .clipShape(RoundedRectangle(cornerRadius: 6))
    }
}

// MARK: - App Card

struct AppCardView: View {
    let app: AudioApp
    let onVolumeChange: (Float) -> Void
    let onMuteToggle: () -> Void
    
    @State private var isHovered = false
    @State private var localVolume: Float
    
    init(app: AudioApp, onVolumeChange: @escaping (Float) -> Void, onMuteToggle: @escaping () -> Void) {
        self.app = app
        self.onVolumeChange = onVolumeChange
        self.onMuteToggle = onMuteToggle
        self._localVolume = State(initialValue: app.volume)
    }
    
    var body: some View {
        VStack(spacing: 4) {
            HStack(spacing: 6) {
                // App Icon
                if let icon = app.icon {
                    Image(nsImage: icon)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                        .frame(width: 20, height: 20)
                        .clipShape(RoundedRectangle(cornerRadius: 4))
                } else {
                    Image(systemName: "app.fill")
                        .font(.system(size: 16))
                        .foregroundStyle(.secondary)
                        .frame(width: 20, height: 20)
                }
                
                // App Name
                Text(app.name)
                    .font(.system(size: 10, weight: .medium))
                    .lineLimit(1)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                // Mute Button
                Button(action: onMuteToggle) {
                    Image(systemName: app.isMuted ? "speaker.slash.fill" : "speaker.wave.2.fill")
                        .font(.system(size: 9))
                        .foregroundStyle(app.isMuted ? .red : .secondary)
                }
                .buttonStyle(.plain)
            }
            
            // Volume Slider (shown on hover)
            if isHovered {
                Slider(value: Binding(
                    get: { Double(localVolume) },
                    set: { newValue in
                        localVolume = Float(newValue)
                        onVolumeChange(localVolume)
                    }
                ), in: 0...1)
                .controlSize(.mini)
                .transition(.opacity.combined(with: .scale(scale: 0.95)))
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(
            RoundedRectangle(cornerRadius: 6)
                .fill(Color(nsColor: .controlBackgroundColor))
                .shadow(color: .black.opacity(isHovered ? 0.12 : 0.06), radius: isHovered ? 3 : 1, y: 1)
        )
        .onHover { hovering in
            withAnimation(.easeInOut(duration: 0.15)) {
                isHovered = hovering
            }
        }
        .draggable(app.id)
    }
}

// MARK: - Preview

#Preview {
    KanbanRouterView()
        .frame(width: 650, height: 500)
}
