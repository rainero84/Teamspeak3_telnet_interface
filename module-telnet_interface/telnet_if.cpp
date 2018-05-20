#include "telnet_if.h"
#include "teamspeak/public_errors.h"

#include <ws2tcpip.h>

#include <string>
#include <fstream>

Telnet_interface* Telnet_interface::__telnet_if_singleton = nullptr;

extern struct TS3Functions ts3Functions;

const int TELNET_PORT = 23;
const char* TELNET_PORT_STR = "23";

const char* TEAMSPEAK_CMD_PREFIX = "ts3";

//-----------------------------------------------------------------------------
/// Create instance if no instance exists yet
Telnet_interface* Telnet_interface::create_instance(const struct TS3Functions funcs) {
	if (Telnet_interface::__telnet_if_singleton == nullptr) {
		Telnet_interface::__telnet_if_singleton = new Telnet_interface(funcs);
	}
	return Telnet_interface::__telnet_if_singleton;
}

//-----------------------------------------------------------------------------
/// Returns the instance, nullptr if it doesn't exist
Telnet_interface* Telnet_interface::get_instance() {
	return Telnet_interface::__telnet_if_singleton;
}

//-----------------------------------------------------------------------------
/// Destroys the instance
void Telnet_interface::destroy_instance() {
	if (Telnet_interface::__telnet_if_singleton != nullptr) {
		delete Telnet_interface::__telnet_if_singleton;
		Telnet_interface::__telnet_if_singleton = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Handle connection to server established
void Telnet_interface::handle_server_connected(uint64 server_connection_id) {
    Server_connection_map::iterator iter = _servers.find(server_connection_id);
    if (iter != _servers.end()) {
        iter->second->handle_connection_established();
    }
}

//-----------------------------------------------------------------------------
// Handle connection to server established
void Telnet_interface::handle_server_connecting(uint64 server_connection_id) {
    Server_connection_map::iterator iter = _servers.find(server_connection_id);
    if (iter != _servers.end()) {
        iter->second->handle_connect_started();
    }
}

//-----------------------------------------------------------------------------
// Handle connection to server terminated
void Telnet_interface::handle_server_disconnected(uint64 server_connection_id) {
    Server_connection_map::iterator iter = _servers.find(server_connection_id);
    if (iter != _servers.end()) {
        iter->second->handle_connection_closed();
        delete iter->second;
        _servers.erase(iter);
    }
}

//-----------------------------------------------------------------------------
/// Constructor
Telnet_interface::Telnet_interface(const struct TS3Functions funcs) {

	_state = TELNET_INTERFACE_STATE_IDLE;
    _server_socket = INVALID_SOCKET;
    _client_socket = INVALID_SOCKET;
    _ts3Functions = funcs;
}

//-----------------------------------------------------------------------------
/// Destructor
Telnet_interface::~Telnet_interface() {
    // Delete server instances
    while (!_servers.empty()) {
        delete _servers.begin()->second;
        _servers.erase(_servers.begin());
    }

    // Close sockets
	if (_state == TELNET_INTERFACE_STATE_LISTENING) {
		closesocket(_server_socket);
	} else if (_state == TELNET_INTERFACE_STATE_CONNECTED) {
		closesocket(_client_socket);
		closesocket(_server_socket);
	}
}

//-----------------------------------------------------------------------------
/// Starts the server
void Telnet_interface::event_listen() {
    _pending_external_events.push_back(EXTERNAL_PLUGIN_EVENTS_LISTEN);
}

//-----------------------------------------------------------------------------
/// Closes the client and server connections
void Telnet_interface::event_close() {
    _pending_external_events.push_back(EXTERNAL_PLUGIN_EVENTS_CLOSE);
}

//-----------------------------------------------------------------------------
/// Closes all connections and gets ready to terminate
void Telnet_interface::event_shutdown() {
    _pending_external_events.push_back(EXTERNAL_PLUGIN_EVENTS_SHUTDOWN);
}

//-----------------------------------------------------------------------------
/// Executes the thread
void Telnet_interface::execute() {
    _process_events();
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _run_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _run_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _run_TELNET_INTERFACE_STATE_CONNECTED(); break;
    case TELNET_INTERFACE_STATE_SHUTDOWN:  _run_TELNET_INTERFACE_STATE_SHUTDOWN(); break;
    default:
        // Unhandled state
        break;
    }
}

//-----------------------------------------------------------------------------
/// Determines if the interface is shut down
bool Telnet_interface::execution_complete() {
    return _state == TELNET_INTERFACE_STATE_SHUTDOWN;
}

//-----------------------------------------------------------------------------
/// Process external events
void Telnet_interface::_process_events() {
    while (!_pending_external_events.empty()) {
        switch (*_pending_external_events.begin()) {
        case EXTERNAL_PLUGIN_EVENTS_LISTEN:  _handle_event_listen(); break;
        case EXTERNAL_PLUGIN_EVENTS_CLOSE:   _handle_event_close(); break;
        case EXTERNAL_PLUGIN_EVENTS_SHUTDOWN:_handle_event_shutdown(); break;
        default:
            // Unhandled event
            break;
        }
        _pending_external_events.pop_front();
    }
}

void Telnet_interface::_handle_event_listen() {
    if (_state == TELNET_INTERFACE_STATE_IDLE) {
        WSADATA wsa_data;
        int int_result;

        // Initialize Winsock
        int_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (int_result != 0) {
            _ts3Functions.logMessage("Error starting WSA", LogLevel_WARNING, "TestPlugin", 0);
            return;
        }

        struct addrinfo *addrinfo_result = NULL;
        struct addrinfo hints;

        ZeroMemory(&hints, sizeof (hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        // Resolve the local address and port to be used by the server
        int_result = getaddrinfo(NULL, TELNET_PORT_STR, &hints, &addrinfo_result);
        if (int_result != 0) {
            WSACleanup();
            return;
        }

        // Assign the server socket
        _server_socket = socket(addrinfo_result->ai_family, addrinfo_result->ai_socktype, addrinfo_result->ai_protocol);
        if (_server_socket == INVALID_SOCKET) {
            freeaddrinfo(addrinfo_result);
            WSACleanup();
            return;
        }

        // Setup the TCP listening socket
        int_result = bind(_server_socket, addrinfo_result->ai_addr, (int)addrinfo_result->ai_addrlen);
        if (int_result == SOCKET_ERROR) {
            freeaddrinfo(addrinfo_result);
            closesocket(_server_socket);
            WSACleanup();
            return;
        }

        // Listen on the socket
        if (::listen(_server_socket, 1) == SOCKET_ERROR) {
            closesocket(_server_socket);
            WSACleanup();
            return;
        }

        // Change the state - server is now listening
        _change_state(TELNET_INTERFACE_STATE_LISTENING);
        return;
    } else {
        _ts3Functions.logMessage("Interface is already listening for connections", LogLevel_INFO, "TestPlugin", 0);
        return;
    }
}

void Telnet_interface::_handle_event_close() {
    // Close sockets
    if (_state == TELNET_INTERFACE_STATE_LISTENING) {
        closesocket(_server_socket);
        WSACleanup();
        _change_state(TELNET_INTERFACE_STATE_IDLE);
    } else if (_state == TELNET_INTERFACE_STATE_CONNECTED) {
        closesocket(_client_socket);
        closesocket(_server_socket);
        WSACleanup();
        _change_state(TELNET_INTERFACE_STATE_IDLE);
    }
}

void Telnet_interface::_handle_event_shutdown() {
    // Close sockets
    if (_state == TELNET_INTERFACE_STATE_LISTENING) {
        closesocket(_server_socket);
        WSACleanup();
    } else if (_state == TELNET_INTERFACE_STATE_CONNECTED) {
        closesocket(_client_socket);
        closesocket(_server_socket);
        WSACleanup();
    }
    _change_state(TELNET_INTERFACE_STATE_SHUTDOWN);
}

//-----------------------------------------------------------------------------
/// Changes the current state of the interface
void Telnet_interface::_change_state(Telnet_interface_state state) {
    // Call the appropriate On Exit function
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _on_exit_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _on_exit_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _on_exit_TELNET_INTERFACE_STATE_CONNECTED(); break;
    case TELNET_INTERFACE_STATE_SHUTDOWN:  _on_exit_TELNET_INTERFACE_STATE_SHUTDOWN(); break;
    }

    // Update the state
    _state = state;

    // Call the appropriate On Enter function
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _on_enter_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _on_enter_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _on_enter_TELNET_INTERFACE_STATE_CONNECTED(); break;
    case TELNET_INTERFACE_STATE_SHUTDOWN:  _on_enter_TELNET_INTERFACE_STATE_SHUTDOWN(); break;
    }
}

//-----------------------------------------------------------------------------
/// Enters the IDLE state
void Telnet_interface::_on_enter_TELNET_INTERFACE_STATE_IDLE() {
    _ts3Functions.logMessage("Entering IDLE state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Enters the LISTENING state
void Telnet_interface::_on_enter_TELNET_INTERFACE_STATE_LISTENING() {
    _ts3Functions.logMessage("Entering LISTENING state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Enters the CONNECTED state
void Telnet_interface::_on_enter_TELNET_INTERFACE_STATE_CONNECTED() {
    _ts3Functions.logMessage("Entering CONNECTED state", LogLevel_DEBUG, "TestPlugin", 0);
    _queue_write("Welcome to the TeamSpeak 3 Client Telnet Interface");
}

//-----------------------------------------------------------------------------
/// Enters the SHUTDOWN state
void Telnet_interface::_on_enter_TELNET_INTERFACE_STATE_SHUTDOWN() {
    _ts3Functions.logMessage("Entering SHUTDOWN state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Runs the IDLE state
void Telnet_interface::_run_TELNET_INTERFACE_STATE_IDLE() {}

//-----------------------------------------------------------------------------
/// Runs the LISTENING state
void Telnet_interface::_run_TELNET_INTERFACE_STATE_LISTENING() {
    timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(_server_socket, &readfds);

    if (select(1, &readfds, NULL, NULL, &timeout) > 0) {

        _client_socket = accept(_server_socket, NULL, NULL);
        if (_client_socket == INVALID_SOCKET) {
            _ts3Functions.logMessage("Invalid client socket, closing server socket", LogLevel_INFO, "TestPlugin", 0);
            closesocket(_server_socket);
            WSACleanup();
        } else {
            _ts3Functions.logMessage("Client socket connected!", LogLevel_INFO, "TestPlugin", 0);
            _change_state(TELNET_INTERFACE_STATE_CONNECTED);
        }
    }
}

//-----------------------------------------------------------------------------
/// Runs the CONNECTED state
void Telnet_interface::_run_TELNET_INTERFACE_STATE_CONNECTED() {
    timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    fd_set read_fds, write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_SET(_client_socket, &read_fds);

    if (_write_stream.tellp() > _write_stream.tellg()) {
        FD_SET(_client_socket, &write_fds);
    }

    if (select(1, &read_fds, &write_fds, NULL, &timeout) > 0) {
        if (FD_ISSET(_client_socket, &read_fds)) {

            char buffer[100];
            int bytes_received = recv(_client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                _ts3Functions.logMessage("Data received from client", LogLevel_DEBUG, "TestPlugin", 0);
                _read_stream.write(buffer, bytes_received);
                _parse_buffer();
            } else {
                _ts3Functions.logMessage("Client disconnected", LogLevel_INFO, "TestPlugin", 0);
                closesocket(_client_socket);
                _client_socket = INVALID_SOCKET;
                _change_state(TELNET_INTERFACE_STATE_LISTENING);
            }
        } else if (FD_ISSET(_client_socket, &write_fds)) {
            _ts3Functions.logMessage("Data available for client", LogLevel_DEBUG, "TestPlugin", 0);
            int buffer_size = (int)(_write_stream.tellp() - _write_stream.tellg());
            char* buffer = new char[buffer_size];
            _write_stream.read(buffer, buffer_size);
            int bytes_written = send(_client_socket, buffer, buffer_size, 0);
            delete[] buffer;
        }

    }
}

//-----------------------------------------------------------------------------
// Runs the SHUTDOWN state - nothing to be done here
void Telnet_interface::_run_TELNET_INTERFACE_STATE_SHUTDOWN() {}

//-----------------------------------------------------------------------------
/// Exits the IDLE state
void Telnet_interface::_on_exit_TELNET_INTERFACE_STATE_IDLE() {
    _ts3Functions.logMessage("Exiting IDLE state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Exits the LISTENING state
void Telnet_interface::_on_exit_TELNET_INTERFACE_STATE_LISTENING() {
    _ts3Functions.logMessage("Exiting LISTENING state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Exits the CONNECTED state
void Telnet_interface::_on_exit_TELNET_INTERFACE_STATE_CONNECTED() {
    _ts3Functions.logMessage("Exiting CONNECTED state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Exits the CONNECTED state
void Telnet_interface::_on_exit_TELNET_INTERFACE_STATE_SHUTDOWN() {
    _ts3Functions.logMessage("Exiting SHUTDOWN state", LogLevel_DEBUG, "TestPlugin", 0);
}

//-----------------------------------------------------------------------------
/// Sends a list of supported command to the client
void Telnet_interface::_send_usage_to_client() {
    _queue_write("The TeamSpeak3 interface supports the following commands:");
    _queue_write("ts3.server");
    _queue_write("ts3.messaging");
}

//-----------------------------------------------------------------------------
/// Parses the content of the received buffer
void Telnet_interface::_parse_buffer() {
    std::string line;

    // Get the next line from the read buffer
    std::getline(_read_stream, line);

    // The command is epected to have the following syntax: <command> <param1> <param2> ... <paramx>
    std::istringstream line_parser(line);

    // The command is expected to have the following format: ts3.<category>.<action>
    std::string command;
    line_parser >> command;

    std::istringstream command_parser(command);
    std::string command_prefix;
    std::getline(command_parser, command_prefix, '.');
    
    if (command_prefix == TEAMSPEAK_CMD_PREFIX) {
        // Determine the category
        std::string command_category;
        std::getline(command_parser, command_category, '.');

        // Determine the action
        std::string command_action;
        std::getline(command_parser, command_action, '.');


        if (command_category == "identifier") {
            _ts3Functions.logMessage("Found identifier command", LogLevel_DEBUG, "TestPlugin", 0);

            if (command_action == "add") {
                _queue_write(command + " not available, as API does not support identity management");
            } else if (command_action == "remove") {
                _queue_write(command + " not available, as API does not support identity management");
            }

        }
        else if (command_category == "servers") {
            _ts3Functions.logMessage("Found servers command", LogLevel_DEBUG, "TestPlugin", 0);

            if (command_action == "connect") {
                std::string host;
                line_parser >> host;

                std::string identity;
                line_parser >> identity;

                std::string nickname;
                line_parser >> nickname;

                std::string server_password;
                line_parser >> server_password;
                
                uint64 newServerConnectionHandlerID = 0;

                // Start connection
                int connect_result = _ts3Functions.guiConnect(
                    PLUGIN_CONNECT_TAB_NEW_IF_CURRENT_CONNECTED, 
                    "PluginServerTab",       // serverLabel
                    host.c_str(),            // serverAddress
                    server_password.c_str(), // serverPassword
                    nickname.c_str(),        // nickname
                    "",                      // channel
                    "",                      // channelPassword
                    "Default",               // captureProfile
                    "Default",               // playbackProfile
                    "Default",               // hotkeyProfile
                    "Default",               // soundProfile
                    "",                      // userIdentity
                    "",                      // oneTimeKey
                    "",                      // phoneticName
                    &newServerConnectionHandlerID
                    );
                if (_evaluate_result(connect_result)) {
                    _servers[newServerConnectionHandlerID] = new Server_connection(newServerConnectionHandlerID, _ts3Functions);
                    _queue_write(command + " ok");
                } else {
                    _queue_write(command + " fail");
                }

            } else if (command_action == "disconnect") {

            }
        } else if (command_category == "messaging") {
            _ts3Functions.logMessage("Found messages command", LogLevel_DEBUG, "TestPlugin", 0);

        } else {
            std::string error_str = "ts3.error: ";
            error_str.append(command_action + ": " + command_category + " is not a supported category");
            _queue_write(error_str);
        }
    }
}

//-----------------------------------------------------------------------------
/// Queues data for the client
void Telnet_interface::_queue_write(std::string response) {
    _write_stream.write(">", 1);
    _write_stream.write(response.c_str(), response.length());
    _write_stream.write("\r\n", 2);
}

//-----------------------------------------------------------------------------
/// Checks the result code and logs appropriately
bool Telnet_interface::_evaluate_result(unsigned int result) {
    if (result == ERROR_ok) {
        return true;
    } else {
        char* error_message;
        _ts3Functions.getErrorMessage(result, &error_message);
        _ts3Functions.logMessage(error_message, LogLevel_DEBUG, "TestPlugin", 0);
        _ts3Functions.freeMemory(error_message);
        return false;
    }
}
