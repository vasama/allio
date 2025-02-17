#pragma once

#include <example-library-abi.hpp>

#include <allio/senders/opaque_object.hpp>

namespace example::library {

using abi::client_context;
using abi::client_handler;
using abi::server_handler;

class server : public server_handler
{
	allio::opaque_handle m_server_handle;

public:
	server();
};

} // namespace example::library
