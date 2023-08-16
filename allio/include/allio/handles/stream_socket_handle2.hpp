#pragma once

#include <allio/byte_io.hpp>
#include <allio/handles/stream_socket_handle2.hpp>

namespace allio {
namespace detail {

class stream_socket_handle_impl : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct connect_tag;
	using connect_parameters = deadline_parameters;


	using byte_io_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			connect_tag,
			stream_scatter_gather_list
		>
	>;

protected:
	allio_detail_default_lifetime(stream_socket_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<connect_parameters> P = connect_parameters::interface>
		vsm::result<void> connect(network_address const& address, P const& args = {}) &;
		
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> read(read_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> read(read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> write(write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		vsm::result<size_t> write(write_buffer const buffer, P const& args = {}) const;
	};
	
	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<connect_parameters> P = connect_parameters::interface>
		basic_sender<connect_tag> connect_async(network_address const& address, P const& args = {}) &;
	
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<scatter_read_tag> read_async(read_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<scatter_read_tag> read_async(read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<gather_write_tag> write_async(write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<gather_write_tag> write_async(write_buffer const buffer, P const& args = {}) const;
	};
};

} // namespace detail

using stream_socket_handle = basic_handle<detail::stream_socket_handle_impl>;

template<typename Multiplexer>
using basic_stream_socket_handle = basic_async_handle<detail::stream_socket_handle_impl, Multiplexer>;


template<parameters<stream_socket_handle::connect_parameters> P = stream_socket_handle::connect_parameters::interface>
vsm::result<stream_socket_handle> connect(network_address const& address, P const& args = {})
{
	vsm::result<stream_socket_handle> r(vsm::result_value);
	vsm_try_void(r->connect(address, args));
	return r;
}

template<typename Multiplexer, parameters<stream_socket_handle::connect_parameters> P = stream_socket_handle::connect_parameters::interface>
auto connect_async(Multiplexer& multiplexer, network_address const& address, P const& args = {});


template<>
struct io_operation_traits<stream_socket_handle::connect_tag>
{
	using handle_type = stream_socket_handle;
	using result_type = void;
	using params_type = stream_socket_handle::connect_parameters;

	network_address address;
};

} // namespace allio
