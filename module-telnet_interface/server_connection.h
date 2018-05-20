#ifndef _SERVER_CONNECTION_H_
#define _SERVER_CONNECTION_H_

#include <string>

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
    Server_connection(uint64 connection_id, const struct TS3Functions funcs, std::string hostname);

    // Returns the hostname of the server
    std::string get_hostname();

    // Returns the state of the server
    Server_state get_state();

    // Handles event indicating that the connection has started
    void handle_connect_started();

    // Handles the event indicating that the connection has closed
    void handle_connection_closed();

    // Handles the event indicating that the connection has been established
    void handle_connection_established();

    // Closes the connection to the server
    void close_connection();

private:
    // State of the server connection
    Server_state _state;

    // Hostname of the server
    std::string _hostname;

    // ID of the server connection
    uint64 _server_connection_id;

    // TS3 functions
    struct TS3Functions _ts3Functions;
};

#endif //_SERVER_CONNECTION_H_