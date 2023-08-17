#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/stream_socket_handle.hpp>

namespace allio {
namespace detail {

class _stream_socket_handle : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct connect_t;
	using connect_parameters = deadline_parameters;


	using byte_io_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			connect_t,
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
	
	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		template<parameters<connect_parameters> P = connect_parameters::interface>
		basic_sender<M, H, connect_t> connect_async(network_address const& address, P const& args = {}) &;
	
		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, scatter_read_t> read_async(read_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, scatter_read_t> read_async(read_buffer const buffer, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, gather_write_t> write_async(write_buffers const buffers, P const& args = {}) const;

		template<parameters<byte_io_parameters> P = byte_io_parameters::interface>
		basic_sender<M, H, gather_write_t> write_async(write_buffer const buffer, P const& args = {}) const;
	};
};

} // namespace detail

template<>
struct io_operation_traits<stream_socket_handle::connect_t>
{
	using handle_type = stream_socket_handle;
	using result_type = void;
	using params_type = stream_socket_handle::connect_parameters;

	network_address address;
};

} // namespace allio
