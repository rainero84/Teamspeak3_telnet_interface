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


    // Notify Client
    std::ostringstream client_info_msg;
    client_info_msg << "ts3.info Server " << server_connection_id << " connected";
    _queue_write(client_info_msg.str());

}

//-----------------------------------------------------------------------------
// Handle connection to server established
void Telnet_interface::handle_server_connecting(uint64 server_connection_id) {

    // Notify Client
    std::ostringstream client_info_msg;
    client_info_msg << "ts3.info Server " << server_connection_id << " connecting";
    _queue_write(client_info_msg.str());
}

//-----------------------------------------------------------------------------
// Handle connection to server terminated
void Telnet_interface::handle_server_disconnected(uint64 server_connection_id) {
    // Notify Client
    std::ostringstream client_info_msg;
    client_info_msg << "ts3.info Server " << server_connection_id << " disconnected";
    _queue_write(client_info_msg.str());
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

//-----------------------------------------------------------------------------
// Listen event is handled. When in the IDLE state, a new server connection
// is started
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

//-----------------------------------------------------------------------------
// Close event is handled. Open sockets are closed, and IDLE state is entered
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

//-----------------------------------------------------------------------------
// Shutdown event is handled. Open sockets are closed, and IDLE state is entered
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

        // Check if the client socket flag is set
        if (FD_ISSET(_client_socket, &read_fds)) {

            // Read data fom client
            char buffer[100];
            int bytes_received = recv(_client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                _ts3Functions.logMessage("Data received from client", LogLevel_DEBUG, "TestPlugin", 0);
                _read_stream.write(buffer, bytes_received);
                _parse_buffer();
            } else {
                // Client has disconnected
                _ts3Functions.logMessage("Client disconnected", LogLevel_INFO, "TestPlugin", 0);
                closesocket(_client_socket);
                _client_socket = INVALID_SOCKET;
                _change_state(TELNET_INTERFACE_STATE_LISTENING);
            }
        } else if (FD_ISSET(_client_socket, &write_fds)) {
            // Data available for client - write
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

        // Handle the specific command category
        if (command_category == "identifier") {
            // Due to limited functionality on the plugin API, it is not
            // possible to add or remove new identities
            _ts3Functions.logMessage("Found identifier command", LogLevel_DEBUG, "TestPlugin", 0);

            if (command_action == "add") {
                _queue_write(command + " not available, as API does not support identity management");
            } else if (command_action == "remove") {
                _queue_write(command + " not available, as API does not support identity management");
            }

        }
        else if (command_category == "servers") {
            // Handles the servers command category.
            // Allows new connections and disconnecting from active connections
            _ts3Functions.logMessage("Found servers command", LogLevel_DEBUG, "TestPlugin", 0);

            // Establish a new connection
            if (command_action == "connect") {
                bool valid = true;
                std::string host;
                line_parser >> host;
                valid &= !host.empty();

                std::string identity;
                line_parser >> identity;
                valid &= !identity.empty();

                std::string nickname;
                line_parser >> nickname;
                valid &= !nickname.empty();

                std::string captureProfile;
                line_parser >> captureProfile;
                if (captureProfile.empty()) {
                    captureProfile = "Default";
                }

                std::string playbackProfile;
                line_parser >> playbackProfile;
                if (playbackProfile.empty()) {
                    playbackProfile = "Default";
                }

                std::string sound_profile;
                line_parser >> sound_profile;
                if (sound_profile.empty()) {
                    sound_profile = "Default Sound Profile (Female)";
                }

                std::string server_password;
                line_parser >> server_password;
                

                if (valid) {
                    uint64 new_server_connection_handler_id = 0;

                    // Start connection
                    int connect_result = _ts3Functions.guiConnect(
                        PLUGIN_CONNECT_TAB_NEW_IF_CURRENT_CONNECTED,
                        "PluginServerTab",       // serverLabel
                        host.c_str(),            // serverAddress
                        server_password.c_str(), // serverPassword
                        nickname.c_str(),        // nickname
                        "",                      // channel
                        "",                      // channelPassword
                        captureProfile.c_str(),  // captureProfile
                        playbackProfile.c_str(), // playbackProfile
                        "Default",               // hotkeyProfile
                        sound_profile.c_str(),   // soundProfile
                        "",                      // userIdentity
                        "",                      // oneTimeKey
                        "",                      // phoneticName
                        &new_server_connection_handler_id
                        );

                    if (_evaluate_result(connect_result)) {
                        _queue_write(command + " ok");

                        std::ostringstream client_info_msg;
                        client_info_msg << "ts3.info New connection to server has ID " << new_server_connection_handler_id;
                        _queue_write(client_info_msg.str());
                    } else {
                        _queue_write(command + " fail");
                    }
                } else {
                    _queue_write(command + " fail");
                }

            } else if (command_action == "disconnect") {
                // Disconnect active server connection
                std::string server_id_str;
                line_parser >> server_id_str;

                uint64 server_id = _active_server_connection;
                if (!server_id_str.empty()) {
                    server_id = atoi(server_id_str.c_str());
                }

                uint64* ids;
                uint64 serverConnectionHandlerID = 0;
                bool found = false;
                if (_ts3Functions.getServerConnectionHandlerList(&ids) == ERROR_ok) {
                    for (int i = 0; ids[i]; i++) {
                        if (ids[i] == server_id) {
                            _ts3Functions.stopConnection(server_id, "Bye");
                            _queue_write(command + " ok");
                            found = true;
                            break;
                        }
                    }
                    _ts3Functions.freeMemory(ids);
                }

                if (!found) {
                    _queue_write(command + " fail. Unknown connection ID");
                }

            } else if (command_action == "list") {
                // List managed connections
                std::ostringstream response;

                uint64* ids;
                uint64 serverConnectionHandlerID = 0;
                char* server_name;
                if (_ts3Functions.getServerConnectionHandlerList(&ids) == ERROR_ok) {
                    for (int i = 0; ids[i]; i++) {

                        if (_evaluate_result(_ts3Functions.getServerVariableAsString(ids[i], VIRTUALSERVER_NAME, &server_name))) {

                            if (_active_server_connection == ids[i]) {
                                response << "[*] ";
                            } else {
                                response << "[ ] ";
                            }
                            response << ids[i] << ":";
                            response << server_name << "\r\n";
                            _ts3Functions.freeMemory(server_name);
                        }
                    }
                    _ts3Functions.freeMemory(ids);

                    _queue_write(response.str());
                }
                

            } else if (command_action == "select") {
                // Disconnect active server connection
                std::string server_id_str;
                line_parser >> server_id_str;

                uint64 server_id = _active_server_connection;
                if (!server_id_str.empty()) {
                    server_id = atoi(server_id_str.c_str());
                }

                uint64* ids;
                uint64 serverConnectionHandlerID = 0;
                bool found = false;
                if (_ts3Functions.getServerConnectionHandlerList(&ids) == ERROR_ok) {
                    for (int i = 0; ids[i]; i++) {
                        if (ids[i] == server_id) {
                            _active_server_connection = server_id;
                            found = true;
                            _queue_write(command + " ok");
                            break;
                        }
                    }
                    _ts3Functions.freeMemory(ids);
                }

                if (!found) {
                    _queue_write(command + " fail. Unknown connection ID");
                }

            }
        } else if (command_category == "messaging") {
            _ts3Functions.logMessage("Found messages command", LogLevel_DEBUG, "TestPlugin", 0);

            if (command_action == "send") {
                std::string message;
                std::getline(line_parser, message);

                if (_evaluate_result(_ts3Functions.requestSendChannelTextMsg(_active_server_connection, message.c_str(), 0, NULL))) {
                    _queue_write(command + " ok");
                } else {
                    _queue_write(command + " fail");
                }

            }

        } else {
            std::string error_str = "ts3.error: ";
            error_str.append(command_action + ": " + command_category + " is not a supported category");
            _queue_write(error_str);
        }
    }
    if (!_read_stream.good()) {
        _read_stream.clear();
        _read_stream.str("");
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
