#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/detail/linear.hpp>
#include <allio/byte_io.hpp>
#include <allio/network.hpp>
#include <allio/path.hpp>
#include <allio/platform_handle.hpp>

#include <cstdint>

namespace allio {

struct socket_parameters
{
	flags handle_flags = {};
};

struct listen_parameters
{
	uint32_t backlog = 0;
};

struct accept_result;

namespace io {

struct socket;
struct connect;
struct listen;
struct accept;

} // namespace io

namespace detail {

class common_socket_handle_base : public platform_handle
{
public:
	using async_operations = type_list_cat<
		platform_handle::async_operations,
		type_list<io::socket>
	>;

	using platform_handle::platform_handle;

	result<native_handle_type> release_native_handle();
	result<void> set_native_handle(native_handle_type handle);

	result<void> create(network_address_kind address_kind, socket_parameters const& args = {});
	basic_sender<io::socket> create_async(network_address_kind address_kind, socket_parameters const& args = {});

protected:
	result<void> create_sync(network_address_kind address_kind, socket_parameters const& args);
};

class socket_handle_base : public common_socket_handle_base
{
public:
	using async_operations = type_list_cat<
		common_socket_handle_base::async_operations,
		type_list<io::connect>,
		io::stream_scatter_gather
	>;

	using common_socket_handle_base::common_socket_handle_base;

	result<void> connect(network_address const& address);
	basic_sender<io::connect> connect_async(network_address const& address);

	result<void> read(read_buffers buffers);
	result<void> read(read_buffer const buffer)
	{
		return read(read_buffers(&buffer, 1));
	}

	basic_sender<io::stream_scatter_read> read_async(read_buffers buffers);
	basic_sender<io::stream_scatter_read> read_async(read_buffer const buffer);

	result<void> write(write_buffers buffers);
	result<void> write(write_buffer const buffer)
	{
		return write(write_buffers(&buffer, 1));
	}

	basic_sender<io::stream_gather_write> write_async(write_buffers buffers);
	basic_sender<io::stream_gather_write> write_async(write_buffer const buffer);

private:
	result<void> connect_sync(network_address const& address);

	result<void> read_sync(read_buffers buffers);
	result<void> write_sync(read_buffers buffers);
};

class listen_socket_handle_base : public common_socket_handle_base
{
public:
	using async_operations = type_list_cat<
		common_socket_handle_base::async_operations,
		type_list<
			io::listen,
			io::accept
		>
	>;

	using common_socket_handle_base::common_socket_handle_base;

	result<void> listen(network_address const& address, listen_parameters const& args = {});
	basic_sender<io::listen> listen_async(network_address const& address, listen_parameters const& args = {});

	result<accept_result> accept(socket_parameters const& create_args = {}) const;
	basic_sender<io::accept> accept_async(socket_parameters const& create_args = {}) const;

private:
	result<void> listen_sync(network_address const& address, listen_parameters const& args);
	result<accept_result> accept_sync(socket_parameters const& create_args) const;
};

} // namespace detail

using socket_handle = final_handle<detail::socket_handle_base>;
allio_API extern allio_TYPE_ID(socket_handle);

result<socket_handle> connect(network_address const& address, socket_parameters const& create_args = {});


using listen_socket_handle = final_handle<detail::listen_socket_handle_base>;
allio_API extern allio_TYPE_ID(listen_socket_handle);

result<listen_socket_handle> listen(network_address const& address, listen_parameters const& args = {}, socket_parameters const& create_args = {});


struct accept_result
{
	socket_handle socket;
	network_address address;
};


template<>
struct io::parameters<io::socket>
{
	using handle_type = detail::common_socket_handle_base;
	using result_type = void;

	network_address_kind address_kind;
	socket_parameters args;
};

template<>
struct io::parameters<io::connect>
{
	using handle_type = socket_handle;
	using result_type = void;

	network_address address;
};

template<>
struct io::parameters<io::listen>
{
	using handle_type = listen_socket_handle;
	using result_type = void;

	network_address address;
	listen_parameters args;
};

template<>
struct io::parameters<io::accept>
{
	using handle_type = const listen_socket_handle;
	using result_type = accept_result;

	socket_parameters create_args;
};

} // namespace allio
