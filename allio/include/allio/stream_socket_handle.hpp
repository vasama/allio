#pragma once

#include <allio/async_fwd.hpp>
#include <allio/byte_io.hpp>
#include <allio/common_socket_handle.hpp>
#include <allio/detail/api.hpp>

namespace allio {

namespace io {

struct socket_connect;

} // namespace io

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
	vsm::result<void> connect(network_address const& address, Parameters const& args = {})
	{
		return block_connect(address, args);
	}

	template<parameters<stream_socket_handle_base::connect_parameters> Parameters = connect_parameters::interface>
	basic_sender<io::socket_connect> connect_async(network_address const& address, Parameters const& args = {});


	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	vsm::result<void> read(read_buffers const buffers, Parameters const& args = {})
	{
		return block_read(buffers, args);
	}

	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	vsm::result<void> read(read_buffer const buffer, Parameters const& args = {})
	{
		return block_read(read_buffers(&buffer, 1), args);
	}

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_scatter_read> read_async(read_buffers buffers, Parameters const& args = {});

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_scatter_read> read_async(read_buffer buffer, Parameters const& args = {});


	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	vsm::result<void> write(write_buffers const buffers, Parameters const& args = {})
	{
		return block_write(buffers, args);
	}

	template<parameters<byte_io_parameters> Parameters = byte_io_parameters::interface>
	vsm::result<void> write(write_buffer const buffer, Parameters const& args = {})
	{
		return block_write(write_buffers(&buffer, 1), args);
	}

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_gather_write> write_async(write_buffers buffers, Parameters const& args = {});

	template<parameters<stream_socket_handle_base::byte_io_parameters> Parameters = byte_io_parameters::interface>
	basic_sender<io::stream_gather_write> write_async(write_buffer buffer, Parameters const& args = {});

private:
	vsm::result<void> block_connect(network_address const& address, connect_parameters const& args);
	vsm::result<void> block_read(read_buffers buffers, byte_io_parameters const& args);
	vsm::result<void> block_write(write_buffers buffers, byte_io_parameters const& args);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_create> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::socket_connect> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::stream_scatter_read> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::stream_gather_write> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<stream_socket_handle_base>> block_connect(
    network_address const& address,
    stream_socket_handle_base::connect_parameters const& args);

} // namespace detail

using stream_socket_handle = final_handle<detail::stream_socket_handle_base>;

template<parameters<stream_socket_handle::connect_parameters> Parameters = stream_socket_handle::connect_parameters::interface>
vsm::result<stream_socket_handle> connect(network_address const& address, Parameters const& args = {})
{
	return detail::block_connect(address, args);
}


template<>
struct io::parameters<io::socket_connect>
	: stream_socket_handle::connect_parameters
{
	using handle_type = stream_socket_handle;
	using result_type = void;

	network_address address;
};

allio_detail_api extern allio_handle_implementation(stream_socket_handle);

} // namespace allio
