//
//  AudiDeckProtocol.swift
//  AudiDeck
//
//  XPC protocol definitions for communication between main app and helper
//

import Foundation

// MARK: - XPC Protocol

/// Protocol for the XPC service that handles privileged operations
@objc public protocol AudiDeckHelperProtocol {
    
    /// Install the audio driver
    /// - Parameter reply: Callback with success status and optional error message
    func installDriver(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Uninstall the audio driver
    /// - Parameter reply: Callback with success status and optional error message
    func uninstallDriver(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Check the current driver installation status
    /// - Parameter reply: Callback with status code (DriverStatus raw value)
    func checkDriverStatus(reply: @escaping @Sendable (Int) -> Void)
    
    /// Set the routing configuration
    /// - Parameters:
    ///   - configData: JSON-encoded RoutingConfiguration
    ///   - reply: Callback with success status and optional error message
    func setRoutingConfiguration(_ configData: Data, reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Get the current routing configuration
    /// - Parameter reply: Callback with JSON-encoded RoutingConfiguration or nil
    func getRoutingConfiguration(reply: @escaping @Sendable (Data?) -> Void)
    
    /// Set the system default output device
    /// - Parameters:
    ///   - deviceUID: The UID of the device to set as default
    ///   - reply: Callback with success status and optional error message
    func setDefaultOutputDevice(_ deviceUID: String, reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Restart the Core Audio daemon
    /// - Parameter reply: Callback with success status and optional error message
    func restartCoreAudio(reply: @escaping @Sendable (Bool, String?) -> Void)
    
    /// Ping to check if service is alive
    /// - Parameter reply: Callback confirming service is responsive
    func ping(reply: @escaping @Sendable (Bool) -> Void)
}

// MARK: - XPC Connection Helper

/// Helper class for establishing XPC connections
public final class XPCConnectionManager: @unchecked Sendable {
    public static let shared = XPCConnectionManager()
    
    private var connection: NSXPCConnection?
    private let connectionLock = NSLock()
    
    private init() {}
    
    /// Get or create an XPC connection to the helper service
    public func getConnection() -> NSXPCConnection {
        connectionLock.lock()
        defer { connectionLock.unlock() }
        
        if let existing = connection, existing.invalidationHandler != nil {
            return existing
        }
        
        let newConnection = NSXPCConnection(serviceName: AudiDeckConstants.xpcServiceName)
        newConnection.remoteObjectInterface = NSXPCInterface(with: AudiDeckHelperProtocol.self)
        
        newConnection.invalidationHandler = { [weak self] in
            self?.connectionLock.lock()
            self?.connection = nil
            self?.connectionLock.unlock()
        }
        
        newConnection.interruptionHandler = { [weak self] in
            // Connection was interrupted, will reconnect on next use
            self?.connectionLock.lock()
            self?.connection = nil
            self?.connectionLock.unlock()
        }
        
        newConnection.resume()
        connection = newConnection
        
        return newConnection
    }
    
    /// Get the remote object proxy for the helper protocol
    public func getProxy() -> AudiDeckHelperProtocol? {
        let conn = getConnection()
        return conn.remoteObjectProxyWithErrorHandler { error in
            print("XPC connection error: \(error)")
        } as? AudiDeckHelperProtocol
    }
    
    /// Invalidate the current connection
    public func invalidate() {
        connectionLock.lock()
        connection?.invalidate()
        connection = nil
        connectionLock.unlock()
    }
}

