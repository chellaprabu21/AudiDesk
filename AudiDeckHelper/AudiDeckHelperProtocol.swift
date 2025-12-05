//
//  AudiDeckHelperProtocol.swift
//  AudiDeckHelper
//
//  Protocol definition for the XPC helper service
//

import Foundation

/// Re-export the shared protocol and types
/// Note: In a real project, this would be part of a shared framework
/// For now, we duplicate the essential protocol definition

@objc public protocol AudiDeckHelperProtocol {
    
    /// Install the audio driver
    func installDriver(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Uninstall the audio driver
    func uninstallDriver(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Check the driver installation status
    func checkDriverStatus(reply: @escaping @Sendable (Int) -> Void)
    
    /// Set the routing configuration
    func setRoutingConfiguration(_ configData: Data, reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Get the current routing configuration
    func getRoutingConfiguration(reply: @escaping @Sendable (Data?) -> Void)
    
    /// Set the default output device
    func setDefaultOutputDevice(_ deviceUID: String, reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Restart Core Audio
    func restartCoreAudio(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Ping to check connectivity
    func ping(reply: @escaping @Sendable (Bool) -> Void)
}

