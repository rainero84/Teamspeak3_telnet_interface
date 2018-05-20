#include "server_connection.h"

Server_connection::Server_connection(uint64 connection_id, const struct TS3Functions funcs) {
    _server_connection_id = connection_id;
    _ts3Functions = funcs;
    _state = SERVER_STATE_NOT_CONNECTED;
}

void Server_connection::handle_connect_started() {
    _state = SERVER_STATE_CONNECTING;
}

void Server_connection::handle_connection_closed() {
    _state = SERVER_STATE_NOT_CONNECTED;
}

void Server_connection::handle_connection_established() {
    _state = SERVER_STATE_CONNECTED;
}

void Server_connection::close_connection() {
    _ts3Functions.stopConnection(_server_connection_id, "Bye");
}