#include "server_connection.h"

Server_connection::Server_connection(uint64 connection_id, const struct TS3Functions funcs, std::string hostname) {
    _server_connection_id = connection_id;
    _ts3Functions = funcs;
    _hostname = hostname;
    _state = SERVER_STATE_NOT_CONNECTED;
}

//-----------------------------------------------------------------------------
// Returns the hostname of the server
std::string Server_connection::get_hostname() {
    return _hostname;
}

//-----------------------------------------------------------------------------
// Returns the state of the server
Server_state Server_connection::get_state() {
    return _state;
}

//-----------------------------------------------------------------------------
// Handles event indicating that the connection has started
void Server_connection::handle_connect_started() {
    _state = SERVER_STATE_CONNECTING;
}

//-----------------------------------------------------------------------------
// Handles the event indicating that the connection has closed
void Server_connection::handle_connection_closed() {
    _state = SERVER_STATE_NOT_CONNECTED;
}

//-----------------------------------------------------------------------------
// Handles the event indicating that the connection has been established
void Server_connection::handle_connection_established() {
    _state = SERVER_STATE_CONNECTED;
}

//-----------------------------------------------------------------------------
// Closes the connection to the server
void Server_connection::close_connection() {
    _ts3Functions.stopConnection(_server_connection_id, "Bye");
}