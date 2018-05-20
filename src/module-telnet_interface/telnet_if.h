#ifndef _TELNET_IF_H_
#define _TELNET_IF_H_

#include <WinSock2.h>

#include <sstream>
#include <map>

#include "ts3_functions.h"

/// States of the interface
enum Telnet_interface_state {
	TELNET_INTERFACE_STATE_IDLE,
	TELNET_INTERFACE_STATE_LISTENING,
	TELNET_INTERFACE_STATE_CONNECTED
};

class Telnet_interface {
public:
	
	//-------------------------------------------------------------------------
	/// Singleton constructor
    static Telnet_interface* create_instance(const struct TS3Functions funcs);

    /// Returns the instance
    static Telnet_interface* get_instance();

    /// Sets the TS3 functions
    //void set_ts3_functions(struct TS3Functions * ts3Functions);
	
	/// Destroys the interface
	static void destroy_instance();
	//-------------------------------------------------------------------------

private:
	// Constrcutor and destructor are private to ensure only a single
	// instance is created
	//-------------------------------------------------------------------------
	/// Private constructor
    Telnet_interface(const struct TS3Functions funcs);

	/// Private destructor
	~Telnet_interface();

	/// Static pointer to single instance
	static Telnet_interface* __telnet_if_singleton;

    // TS3 functions
    struct TS3Functions _ts3Functions;
	//-------------------------------------------------------------------------

public:
	/// Starts the server
	bool listen();

	/// Executes the thread
	void execute();

private:
    /// Changes the current state of the interface
    void _change_state(Telnet_interface_state _state);


    /// Enters the IDLE state
    void _on_enter_TELNET_INTERFACE_STATE_IDLE();

    /// Enters the LISTENING state
    void _on_enter_TELNET_INTERFACE_STATE_LISTENING();

    /// Enters the CONNECTED state
    void _on_enter_TELNET_INTERFACE_STATE_CONNECTED();

    /// Runs the IDLE state
    void _run_TELNET_INTERFACE_STATE_IDLE();

    /// Runs the LISTENING state
    void _run_TELNET_INTERFACE_STATE_LISTENING();

    /// Runs the CONNECTED state
    void _run_TELNET_INTERFACE_STATE_CONNECTED();


    /// Exits the IDLE state
    void _on_exit_TELNET_INTERFACE_STATE_IDLE();

    /// Exits the LISTENING state
    void _on_exit_TELNET_INTERFACE_STATE_LISTENING();

    /// Exits the CONNECTED state
    void _on_exit_TELNET_INTERFACE_STATE_CONNECTED();


    /// Parses the content of the received buffer
    void _parse_buffer();

    /// Queues data for the client
    void _queue_write(std::string response);

    /// Checks the result code and logs appropriately
    bool _evaluate_result(unsigned int result);


private: // Private members

	/// State of the interface
	Telnet_interface_state _state;

	/// Handle of the server socket
	SOCKET _server_socket;

	/// Handle of the client socket
	SOCKET _client_socket;

    /// Stream holding received data
    std::stringstream _read_stream;

    /// Stream holding data to write
    std::stringstream _write_stream;


    // Server Connection Handler ID used for various function calls
    uint64 _serverConnectionHandlerID;

    /// Map of users with their assigned id
    std::map<uint64 /*ID*/, std::string> _users;

    /// Map of server connections with their assigned id
    std::map<uint64 /*ID*/, std::string> _servers;
};

#endif // _TELNET_IF_H