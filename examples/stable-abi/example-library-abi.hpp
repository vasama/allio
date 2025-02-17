#pragma once

#include <allio/abi.h>

#include <string_view>

namespace example::library::abi {

class client_context
{
public:
	virtual void send_message(std::string_view message) = 0;

protected:
	client_context() = default;
	client_context(client_context const&) = default;
	client_context& operator=(client_context const&) = default;
	~client_context() = default;
};

class client_handler
{
public:
	virtual void on_message_received(std::string_view message) = 0;
	virtual void on_peer_disconnected() = 0;

protected:
	client_handler() = default;
	client_handler(client_handler const&) = default;
	client_handler& operator=(client_handler const&) = default;
	~client_handler() = default;
};

class server_handler
{
public:
	virtual client_handler& on_new_connection(client_context& context) = 0;

protected:
	server_handler() = default;
	server_handler(server_handler const&) = default;
	server_handler& operator=(server_handler const&) = default;
	~server_handler() = default;
};

allio_abi_object_v1* create_server(server_handler& handler);

} // namespace example::library::abi
