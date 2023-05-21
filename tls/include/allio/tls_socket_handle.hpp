#pragma once

#include <allio/socket_handle.hpp>

namespace allio {

namespace io {

struct secure_socket_connect;
struct secure_socket_listen;

} // namespace io

namespace detail {

enum class secure_socket_native_handle : uintptr_t {};

class secure_stream_socket_handle_base : public handle
{
	vsm::linear<secure_socket_native_handle> m_native_handle;

	using base_type = handle;

protected:
	constexpr secure_stream_socket_handle_base()
		: base_type(type_of<secure_stream_socket_handle_base>())
	{
	}

	secure_stream_socket_handle_base(secure_stream_socket_handle_base const&) = default;
	secure_stream_socket_handle_base& operator=(secure_stream_socket_handle_base const&) = default;
	~secure_stream_socket_handle_base() = default;
};

class secure_listen_socket_handle_base : public handle
{
	vsm::linear<secure_socket_native_handle> m_native_handle;

	using base_type = handle;

protected:
	constexpr secure_listen_socket_handle_base()
		: base_type(type_of<secure_listen_socket_handle_base>())
	{
	}

	secure_listen_socket_handle_base(secure_listen_socket_handle_base const&) = default;
	secure_listen_socket_handle_base& operator=(secure_listen_socket_handle_base const&) = default;
	~secure_listen_socket_handle_base() = default;
};

class secure_socket_source_handle_base : public handle
{
	vsm::linear<secure_socket_native_handle> m_native_handle;

	using base_type = handle;

public:
	vsm::result<secure_stream_socket_handle_base> connect(network_address const& address);
	vsm::result<secure_listen_socket_handle_base> listen(network_address const& address);

	vsm::result<secure_stream_socket_handle_base> wrap(stream_socket_handle& socket);
	vsm::result<secure_listen_socket_handle_base> wrap(listen_socket_handle& socket);

protected:
	secure_socket_source_handle_base() = default;
	secure_socket_source_handle_base(secure_socket_source_handle_base const&) = default;
	secure_socket_source_handle_base& operator=(secure_socket_source_handle_base const&) = default;
	~secure_socket_source_handle_base() = default;
};

} // namespace detail

} // namespace allio
