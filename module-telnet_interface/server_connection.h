#ifndef _SERVER_CONNECTION_H_
#define _SERVER_CONNECTION_H_

#include "teamspeak/public_definitions.h"

#include "ts3_functions.h"

/// Enumerates the possible states associated with the server connection
enum Server_state {
    SERVER_STATE_NOT_CONNECTED, // Initial state, currently not connected
    SERVER_STATE_CONNECTING,    // Connect requested, awaiting connection
    SERVER_STATE_CONNECTED      // Connection successfully mode
};

class Server_connection {
public:
    // Constructor, sets the connection ID and TS3 functions
    Server_connection(uint64 connection_id, const struct TS3Functions funcs);

    void handle_connect_started();
    void handle_connection_closed();
    void handle_connection_established();
    void close_connection();

private:
    Server_state _state;

    // ID of the server connection
    uint64 _server_connection_id;

    // TS3 functions
    struct TS3Functions _ts3Functions;
};

#endif //_SERVER_CONNECTION_H_