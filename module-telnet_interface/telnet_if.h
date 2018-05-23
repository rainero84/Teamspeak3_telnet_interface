/*
* Filenme: telnet_if.h
* Purpose: Defines the Telnet_interface class functions and members
*/
#ifndef _TELNET_IF_H_
#define _TELNET_IF_H_

#include <WinSock2.h>
#include <sstream>
#include <map>
#include <list>

#include "ts3_functions.h"

/// States of the interface
enum Telnet_interface_state {
	TELNET_INTERFACE_STATE_IDLE,
	TELNET_INTERFACE_STATE_LISTENING,
	TELNET_INTERFACE_STATE_CONNECTED,
    TELNET_INTERFACE_STATE_SHUTDOWN
};

/// A list of events generated extrenally
enum External_plugin_events {
    EXTERNAL_PLUGIN_EVENTS_LISTEN,
    EXTERNAL_PLUGIN_EVENTS_CLOSE,
    EXTERNAL_PLUGIN_EVENTS_SHUTDOWN
};

class Telnet_interface {
public:
	
	//-------------------------------------------------------------------------
	/// Singleton constructor
    static Telnet_interface* create_instance(const struct TS3Functions funcs);

    /// Returns the instance
    static Telnet_interface* get_instance();
	
	/// Destroys the interface
	static void destroy_instance();
	//-------------------------------------------------------------------------

    /// Handle connection to server established
    void handle_server_connected(uint64 server_connection_id);

    /// Handles connection to the server being established
    void handle_server_connecting(uint64 server_connection_id);
    
    /// Handle connection to server terminated
    void handle_server_disconnected(uint64 server_connection_id);

    //-------------------------------------------------------------------------

    /// Handles received private text message
    void handle_private_text_message(uint64 server_connection_id, uint64 fromID, const char* from_name, const char* message);

    /// Handles received channel text message
    void handle_channel_text_message(uint64 server_connection_id, uint64 fromID, const char* from_name, const char* message);

    /// Handles received poke
    void handle_poke(uint64 server_connection_id, uint64 fromID, const char* from_name, const char* message);

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
	void event_listen();

    /// Closes the client and server connections
    void event_close();

    /// Closes all connections and gets ready to terminate
    void event_shutdown();

	/// Executes the thread
	void execute();

    /// Determines if the interface is shut down
    bool execution_complete();

private:
    /// Process external events
    void _process_events();

    /// Handles a listen event
    void _handle_event_listen();

    /// Handles a close event
    void _handle_event_close();

    /// Handles a shutdown event
    void _handle_event_shutdown();

    /// Changes the current state of the interface
    void _change_state(Telnet_interface_state _state);


    /// Enters the IDLE state
    void _on_enter_TELNET_INTERFACE_STATE_IDLE();

    /// Enters the LISTENING state
    void _on_enter_TELNET_INTERFACE_STATE_LISTENING();

    /// Enters the CONNECTED state
    void _on_enter_TELNET_INTERFACE_STATE_CONNECTED();

    /// Enters the SHUTDOWN state
    void _on_enter_TELNET_INTERFACE_STATE_SHUTDOWN();


    /// Runs the IDLE state
    void _run_TELNET_INTERFACE_STATE_IDLE();

    /// Runs the LISTENING state
    void _run_TELNET_INTERFACE_STATE_LISTENING();

    /// Runs the CONNECTED state
    void _run_TELNET_INTERFACE_STATE_CONNECTED();

    /// Runs the SHUTDOWN state
    void _run_TELNET_INTERFACE_STATE_SHUTDOWN();


    /// Exits the IDLE state
    void _on_exit_TELNET_INTERFACE_STATE_IDLE();

    /// Exits the LISTENING state
    void _on_exit_TELNET_INTERFACE_STATE_LISTENING();

    /// Exits the CONNECTED state
    void _on_exit_TELNET_INTERFACE_STATE_CONNECTED();

    /// Exits the SHUTDOWN state
    void _on_exit_TELNET_INTERFACE_STATE_SHUTDOWN();


    /// Sends a list of supported command to the client
    void _send_usage_to_client();


    /// Parses the content of the received buffer
    void _parse_buffer();

    /// Queues data for the client
    void _queue_write(std::string response);

    /// Checks the result code and logs appropriately
    bool _evaluate_result(unsigned int result);


private: // Private members

	/// State of the interface
	Telnet_interface_state _state;

    /// Holds a list of externally generated events
    std::list<External_plugin_events> _pending_external_events;

	/// Handle of the server socket
	SOCKET _server_socket;

	/// Handle of the client socket
	SOCKET _client_socket;

    /// Stream holding received data
    std::stringstream _read_stream;

    /// Stream holding data to write
    std::stringstream _write_stream;

    /// Currently selected server ID
    uint64 _active_server_connection;

    /// Currently selected channel
    uint64 _active_server_channel;
};

#endif // _TELNET_IF_H
