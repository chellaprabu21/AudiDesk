//
//  AudiDeckApp.swift
//  AudiDeck
//
//  Main SwiftUI application entry point - Kanban Audio Router
//

import SwiftUI

@main
struct AudiDeckApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    
    var body: some Scene {
        Settings {
            SettingsView()
        }
    }
}

// MARK: - Settings View

struct SettingsView: View {
    @AppStorage("launchAtLogin") private var launchAtLogin = false
    
    var body: some View {
        Form {
            Toggle("Launch at Login", isOn: $launchAtLogin)
        }
        .padding()
        .frame(width: 300, height: 100)
    }
}
