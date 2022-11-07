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

struct accept_result;

struct packet_read_result
{
	size_t packet_size;
	network_address address;
};

struct packet_read_descriptor
{
	read_buffers buffers;
	packet_read_result* result;
};

struct packet_write_descriptor
{
	write_buffers buffers;
	network_address const* address;
};

using packet_read_descriptors = std::span<packet_read_descriptor const>;
using packet_write_descriptors = std::span<packet_write_descriptor const>;


namespace io {

struct socket_create;
struct socket_connect;
struct socket_listen;
struct socket_accept;

struct packet_scatter_read;
struct packet_gather_write;

using packet_scatter_gather = type_list<
	packet_scatter_read,
	packet_gather_write
>;

} // namespace io


class common_socket_handle : public platform_handle
{
public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		platform_handle::async_operations,
		type_list<io::socket_create>
	>;


	template<parameters<create_parameters> Parameters = create_parameters::interface>
	result<void> create(network_address_kind const address_kind, Parameters const& args = {})
	{
		return block_create(address_kind, args);
	}

	template<parameters<common_socket_handle::create_parameters> Parameters = create_parameters::interface>
	basic_sender<io::socket_create> create_async(network_address_kind address_kind, Parameters const& args = {});

protected:
	explicit constexpr common_socket_handle(type_id<common_socket_handle> const type)
		: base_type(type)
	{
	}

	result<void> block_create(network_address_kind address_kind, create_parameters const& args);

	result<void> create_sync(network_address_kind address_kind, create_parameters const& args);
};

namespace detail {

class stream_socket_handle_base : public common_socket_handle
{
	using final_handle_type = final_handle<stream_socket_handle_base>;

public:
	using base_type = common_socket_handle;

	using async_operations = type_list_cat<
		common_socket_handle::async_operations,
		type_list<io::socket_connect>,
		io::stream_scatter_gather
	>;


	using connect_parameters = common_socket_handle::create_parameters;
	using byte_io_parameters = basic_parameters;


	constexpr stream_socket_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<connect_parameters> Parameters = connect_parameters::interface>
	result<void> connect(network_address const& address, Parameters const& args = {})
	{
		return block_connect(address, args);
	}

	template<parameters<stream_socket_handle_base::connect_parameters> Parameters = connect_parameters::interface>
	basic_sender<io::socket_connect> connect_async(network_address const& address, Parameters const& args = {});


	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	result<void> read(read_buffers const buffers, Parameters const& args = {})
	{
		return block_read(buffers, args);
	}

	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	result<void> read(read_buffer const buffer, Parameters const& args = {})
	{
		return block_read(read_buffers(&buffer, 1), args);
	}

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_scatter_read> read_async(read_buffers buffers, Parameters const& args = {});

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_scatter_read> read_async(read_buffer buffer, Parameters const& args = {});


	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	result<void> write(write_buffers const buffers, Parameters const& args = {})
	{
		return block_write(buffers, args);
	}

	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	result<void> write(write_buffer const buffer, Parameters const& args = {})
	{
		return block_write(write_buffers(&buffer, 1), args);
	}

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_gather_write> write_async(write_buffers buffers, Parameters const& args = {});

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_gather_write> write_async(write_buffer buffer, Parameters const& args = {});

private:
	result<void> block_connect(network_address const& address, connect_parameters const& args);
	result<void> block_read(read_buffers buffers, byte_io_parameters const& args);
	result<void> block_write(write_buffers buffers, byte_io_parameters const& args);

protected:
	using base_type::sync_impl;

	static result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::socket_connect> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::stream_scatter_read> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::stream_gather_write> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

class packet_socket_handle_base : public common_socket_handle
{
	using final_handle_type = final_handle<packet_socket_handle_base>;

public:
	using base_type = common_socket_handle;

	using async_operations = type_list_cat<
		common_socket_handle::async_operations,
		io::packet_scatter_gather
	>;


	using packet_io_parameters = basic_parameters;


	constexpr packet_socket_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<packet_read_result> read(read_buffer const buffer, P const& args = {})
	{
		result<packet_read_result> r(result_value);
		packet_read_descriptor const descriptor = { read_buffers(&buffer, 1), &*r };
		allio_TRYD(block_read(block_read(packet_read_descriptors(&descriptor, 1), args)));
		return r;
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<packet_read_result> read(read_buffers const buffers, P const& args = {})
	{
		result<packet_read_result> r(result_value);
		packet_read_descriptor const descriptor = { buffers, &*r };
		allio_TRYD(block_read(block_read(packet_read_descriptors(&descriptor, 1), args)));
		return r;
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<void> read(packet_read_descriptor const descriptor, P const& args = {})
	{
		return discard_value(block_read(packet_read_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<size_t> read(packet_read_descriptors const descriptors, P const& args = {})
	{
		return block_read(descriptors, args);
	}


	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<void> write(network_address const& address, write_buffer const buffer, P const& args)
	{
		packet_write_descriptor const descriptor = { write_buffers(&buffer, 1), address };
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<void> write(network_address const& address, write_buffers const buffers, P const& args)
	{
		packet_write_descriptor const descriptor = { buffers, address };
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<void> write(packet_write_descriptor const descriptor, P const& args = {})
	{
		return discard_value(block_write(packet_write_descriptors(&descriptor, 1), args));
	}

	template<parameters<packet_socket_handle_base::packet_io_parameters> P = packet_io_parameters>
	result<size_t> write(packet_write_descriptors const descriptors, P const& args = {})
	{
		return block_write(descriptors, args);
	}

private:
	result<size_t> block_read(packet_read_descriptors descriptors, packet_io_parameters const& args);
	result<size_t> block_write(packet_write_descriptors descriptors, packet_io_parameters const& args);

protected:
	using base_type::sync_impl;

	static result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::packet_scatter_read> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::packet_gather_write> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

class listen_socket_handle_base : public common_socket_handle
{
	using final_handle_type = final_handle<listen_socket_handle_base>;

public:
	using base_type = common_socket_handle;

	using async_operations = type_list_cat<
		common_socket_handle::async_operations,
		type_list<
			io::socket_listen,
			io::socket_accept
		>
	>;


	#define allio_LISTEN_SOCKET_HANDLE_LISTEN_PARAMETERS(type, data, ...) \
		type(allio::detail::listen_socket_handle_base, listen_parameters) \
		allio_PLATFORM_HANDLE_CREATE_PARAMETERS(__VA_ARGS__, __VA_ARGS__) \
		data(uint32_t, backlog, 0) \

	allio_INTERFACE_PARAMETERS(allio_LISTEN_SOCKET_HANDLE_LISTEN_PARAMETERS);

	using accept_parameters = common_socket_handle::create_parameters;


	constexpr listen_socket_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<listen_parameters> Parameters = listen_parameters::interface>
	result<void> listen(network_address const& address, Parameters const& args = {})
	{
		return block_listen(address, args);
	}

	template<parameters<listen_socket_handle_base::listen_parameters> Parameters = listen_parameters::interface>
	basic_sender<io::socket_listen> listen_async(network_address const& address, Parameters const& args = {});


	template<parameters<accept_parameters> Parameters = accept_parameters::interface>
	result<accept_result> accept(Parameters const& args = {}) const
	{
		return block_accept(args);
	}

	template<parameters<listen_socket_handle_base::accept_parameters> Parameters = accept_parameters::interface>
	basic_sender<io::socket_accept> accept_async(Parameters const& args = {}) const;

private:
	result<void> block_listen(network_address const& address, listen_parameters const& args);
	result<accept_result> block_accept(accept_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::socket_listen> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::socket_accept> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

result<final_handle<stream_socket_handle_base>> block_connect(network_address const& address, stream_socket_handle_base::connect_parameters const& args);
result<final_handle<listen_socket_handle_base>> block_listen(network_address const& address, listen_socket_handle_base::listen_parameters const& args);

} // namespace detail

using stream_socket_handle = final_handle<detail::stream_socket_handle_base>;
using packet_socket_handle = final_handle<detail::packet_socket_handle_base>;
using listen_socket_handle = final_handle<detail::listen_socket_handle_base>;


template<parameters<stream_socket_handle::connect_parameters> Parameters = stream_socket_handle::connect_parameters::interface>
result<stream_socket_handle> connect(network_address const& address, Parameters const& args = {})
{
	return detail::block_connect(address, args);
}

template<parameters<listen_socket_handle::listen_parameters> Parameters = listen_socket_handle::listen_parameters::interface>
result<listen_socket_handle> listen(network_address const& address, Parameters const& args = {})
{
	return detail::block_listen(address, args);
}



struct accept_result
{
	stream_socket_handle socket;
	network_address address;
};


template<>
struct io::parameters<io::socket_create>
	: common_socket_handle::create_parameters
{
	using handle_type = common_socket_handle;
	using result_type = void;

	network_address_kind address_kind;
};

template<>
struct io::parameters<io::socket_connect>
	: stream_socket_handle::connect_parameters
{
	using handle_type = stream_socket_handle;
	using result_type = void;

	network_address address;
};

template<>
struct io::parameters<io::packet_scatter_read>
	: packet_socket_handle::packet_io_parameters
{
	using handle_type = packet_socket_handle const;
	using result_type = size_t;

	packet_read_descriptors descriptors;
};

template<>
struct io::parameters<io::packet_gather_write>
	: packet_socket_handle::packet_io_parameters
{
	using handle_type = packet_socket_handle const;
	using result_type = size_t;

	packet_write_descriptors descriptors;
};

template<>
struct io::parameters<io::socket_listen>
	: listen_socket_handle::listen_parameters
{
	using handle_type = listen_socket_handle;
	using result_type = void;

	network_address address;
};

template<>
struct io::parameters<io::socket_accept>
	: listen_socket_handle::accept_parameters
{
	using handle_type = listen_socket_handle const;
	using result_type = accept_result;
};

allio_API extern allio_HANDLE_IMPLEMENTATION(stream_socket_handle);
allio_API extern allio_HANDLE_IMPLEMENTATION(packet_socket_handle);
allio_API extern allio_HANDLE_IMPLEMENTATION(listen_socket_handle);

} // namespace allio
