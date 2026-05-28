#ifndef OP_CODES_H
#define OP_CODES_H

// ─────────────────────────────────────────────────────────────────────────
// FedSol — Protocol opcodes
//
// Every new connection must start with an AuthMessage (carries password).
// Messages within an established persistent session are already authenticated
// and do not require a password.
// ─────────────────────────────────────────────────────────────────────────

// AuthMessage: client opens a new connection, always includes password
namespace AuthOp
{
    constexpr int HANDSHAKE         = 0;    // Initiate session: auth + architecture
    constexpr int TRAINING_FINISHED = 1;    // Local training round completed
    constexpr int ERROR             = 99;   // Unexpected error on the client side
}

// Message: server -> client, over the persistent connection
namespace ServerOp
{
    constexpr int AUTH_RESPONSE     = 0;    // Handshake response
                                            // content: AUTH_SUCCESSFUL | AUTH_FAILED
                                            //          ARCH_OK | ARCH_INVALID
    constexpr int START_TRAINING    = 1;    // Start training round (model weights follow)
    constexpr int REQUEST_WEIGHTS   = 2;    // Request trained weights from node
    constexpr int SEND_DATASET      = 3;    // Send dataset to node (file follows)
    constexpr int SHUTDOWN          = 4; 
    constexpr int PING              = 5;    // Heartbeat: query current node state
}

// Message: client -> server, over the persistent connection
namespace ClientOp
{
    constexpr int PONG              = 5;    // Heartbeat response to PING
                                            // content: "TRAINING" | "IDLE"
}

#endif