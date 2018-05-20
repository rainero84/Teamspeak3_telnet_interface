#include "telnet_if.h"
#include "teamspeak/public_errors.h"

#include <ws2tcpip.h>

#include <string>
#include <fstream>

Telnet_interface* Telnet_interface::__telnet_if_singleton = nullptr;

extern struct TS3Functions ts3Functions;

const int TELNET_PORT = 23;
const char* TELNET_PORT_STR = "23";

const char* TEAMSPEAK_CMD_PREFIX = "ts3-";

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
	if (_state == TELNET_INTERFACE_STATE_LISTENING) {
		closesocket(_server_socket);
	} else if (_state == TELNET_INTERFACE_STATE_CONNECTED) {
		closesocket(_client_socket);
		closesocket(_server_socket);
	}
}

//-----------------------------------------------------------------------------
/// Starts the server
bool Telnet_interface::listen() {
    if (_state == TELNET_INTERFACE_STATE_IDLE) {
        WSADATA wsaData;
        int iResult;

        // Initialize Winsock
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            return false;
        }

        struct addrinfo *result = NULL, *ptr = NULL, hints;

        ZeroMemory(&hints, sizeof (hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        // Resolve the local address and port to be used by the server
        int int_result = getaddrinfo(NULL, TELNET_PORT_STR, &hints, &result);
        if (int_result != 0) {
            WSACleanup();
            return false;
        }

        // Assign the server socket
        _server_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (_server_socket == INVALID_SOCKET) {
            freeaddrinfo(result);
            WSACleanup();
            return 1;
        }

        // Setup the TCP listening socket
        int_result = bind(_server_socket, result->ai_addr, (int)result->ai_addrlen);
        if (int_result == SOCKET_ERROR) {
            freeaddrinfo(result);
            closesocket(_server_socket);
            WSACleanup();
            return false;
        }

        // Listen on the socket
        if (::listen(_server_socket, 1) == SOCKET_ERROR) {
            closesocket(_server_socket);
            WSACleanup();
            return false;
        }

        // Change the state - server is now listening
        _change_state(TELNET_INTERFACE_STATE_LISTENING);
        return true;
    }
    else {
        _ts3Functions.logMessage("Interface is already listening for connections", LogLevel_INFO, "TestPlugin", 0);
        return false;
    }
}

//-----------------------------------------------------------------------------
/// Executes the thread
void Telnet_interface::execute() {
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _run_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _run_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _run_TELNET_INTERFACE_STATE_CONNECTED(); break;
    }
}

//-----------------------------------------------------------------------------
/// Changes the current state of the interface
void Telnet_interface::_change_state(Telnet_interface_state state) {
    // Call the appropriate On Exit function
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _on_exit_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _on_exit_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _on_exit_TELNET_INTERFACE_STATE_CONNECTED(); break;
    }

    // Update the state
    _state = state;

    // Call the appropriate On Enter function
    switch (_state) {
    case TELNET_INTERFACE_STATE_IDLE:      _on_enter_TELNET_INTERFACE_STATE_IDLE(); break;
    case TELNET_INTERFACE_STATE_LISTENING: _on_enter_TELNET_INTERFACE_STATE_LISTENING(); break;
    case TELNET_INTERFACE_STATE_CONNECTED: _on_enter_TELNET_INTERFACE_STATE_CONNECTED(); break;
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
        }
        else {
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
            _ts3Functions.logMessage("Client ready for reading!", LogLevel_DEBUG, "TestPlugin", 0);

            char buffer[100];
            int bytes_received = recv(_client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                _ts3Functions.logMessage("Got bytes from client", LogLevel_DEBUG, "TestPlugin", 0);
                _read_stream.write(buffer, bytes_received);
                _parse_buffer();
            } else {
                _ts3Functions.logMessage("Client disconnected", LogLevel_INFO, "TestPlugin", 0);
                closesocket(_client_socket);
                _client_socket = INVALID_SOCKET;
                _change_state(TELNET_INTERFACE_STATE_LISTENING);
            }
        } else if (FD_ISSET(_client_socket, &write_fds)) {
            int buffer_size = _write_stream.tellp() - _write_stream.tellg();
            char* buffer = new char[buffer_size];
            _write_stream.read(buffer, buffer_size);
            int bytes_written = send(_client_socket, buffer, buffer_size, 0);
            delete[] buffer;
        }

    }
}

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
/// Parses the content of the received buffer
void Telnet_interface::_parse_buffer() {
    std::string line;
    std::getline(_read_stream, line);

    std::istringstream line_parser(line);

    // First item of the line, ts3.<category>.<action> <param1> <param2> ... <paramx>
    std::string command;
    line_parser >> command;

    std::istringstream command_parser(command);
    std::string command_prefix;
    std::getline(command_parser, command_prefix, '.');
    
    if (command_prefix == "ts3") {
        // Determine the category
        std::string command_category;
        std::getline(command_parser, command_category, '.');

        // Determine the action
        std::string command_action;
        std::getline(command_parser, command_action, '.');


        if (command_category == "identifier") {
            _ts3Functions.logMessage("Found identifier command", LogLevel_DEBUG, "TestPlugin", 0);

            if (command_action == "add") {
                //_ts3Functions.id
            } else if (command_action == "list") {

            } else if (command_action == "remove") {

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

                uint64 handlerid;

                /*uint64 newServerConnectionHandlerID = 0;
                if (_evaluate_result(_ts3Functions.spawnNewServerConnectionHandler(9987, &newServerConnectionHandlerID))) {
                    if (_evaluate_result(_ts3Functions.startConnection(newServerConnectionHandlerID, identity.c_str(), host.c_str(), 9987, nickname.c_str(), NULL, "", ""))) {
                        _queue_write(command + " ok");
                    } else {
                        _ts3Functions.destroyServerConnectionHandler(newServerConnectionHandlerID);
                        _queue_write(command + " fail");
                    }
                } else {
                    _ts3Functions.destroyServerConnectionHandler(newServerConnectionHandlerID);
                    _queue_write(command + " fail");

                }*/

                _ts3Functions.guiConnect(
                    PLUGIN_CONNECT_TAB_NEW_IF_CURRENT_CONNECTED, 
                    "PluginServerTab", 
                    host.c_str(), 
                    server_password.c_str(),
                    nickname.c_str(), 
                    "", "", "Default", "Default", "Default", "Default",
                    "",
                    "",
                    "",
                    &handlerid
                    );

            } else if (command_action == "disconnect") {

            } else {
                std::string error_str = "ts3.error: ";
                error_str.append(command_action);
                error_str.append(" is not supported in category ");
                error_str.append(command_category);
                _queue_write(error_str);
            }
        } else if (command_category == "messages") {
            _ts3Functions.logMessage("Found messages command", LogLevel_DEBUG, "TestPlugin", 0);

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
