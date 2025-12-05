//
//  main.swift
//  AudiDeckHelper
//
//  XPC service entry point
//

import Foundation

/// XPC service delegate
class ServiceDelegate: NSObject, NSXPCListenerDelegate {
    
    func listener(_ listener: NSXPCListener, shouldAcceptNewConnection newConnection: NSXPCConnection) -> Bool {
        // Configure the connection
        newConnection.exportedInterface = NSXPCInterface(with: AudiDeckHelperProtocol.self)
        
        let exportedObject = AudiDeckHelperDelegate()
        newConnection.exportedObject = exportedObject
        
        // Handle connection events
        newConnection.invalidationHandler = {
            // Clean up when connection is invalidated
        }
        
        newConnection.interruptionHandler = {
            // Handle interruption
        }
        
        newConnection.resume()
        return true
    }
}

// Create the listener and delegate
let delegate = ServiceDelegate()
let listener = NSXPCListener.service()
listener.delegate = delegate

// Start the service
listener.resume()

// Run forever
RunLoop.main.run()

