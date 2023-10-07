#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/common_socket_handle.hpp>

namespace allio {
namespace detail {

struct packet_read_result
{
	size_t packet_size;
	network_endpoint endpoint;
};

class _packet_socket_handle : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct bind_t;
	using bind_parameters = no_parameters;


	struct scatter_read_t;
	struct gather_write_t;

	using packet_io_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			scatter_read_t,
			gather_write_t
		>
	>;

protected:
	allio_detail_default_lifetime(_packet_socket_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<bind_parameters> P = bind_parameters::interface>
		vsm::result<void> bind(network_endpoint const& endpoint, P const& args = {}) &
		{
			return block<bind_t>(static_cast<H const&>(*this), args, endpoint);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<packet_read_result> read(read_buffers const buffers, P const& args = {}) const
		{
			return block<scatter_read_t>(static_cast<H const&>(*this), args, buffers);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<packet_read_result> read(read_buffer const buffer, P const& args = {}) const
		{
			return block<scatter_read_t>(static_cast<H const&>(*this), args, buffer);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<void> write(write_buffers const buffers, network_endpoint const& endpoint, P const& args = {}) const
		{
			return block<gather_write_t>(static_cast<H const&>(*this), args, buffers, endpoint);
		}
		
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<void> write(write_buffer const buffer, network_endpoint const& endpoint, P const& args = {}) const
		{
			return block<gather_write_t>(static_cast<H const&>(*this), args, buffer, endpoint);
		}
	};

	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<M, H, scatter_read_t> read_async(read_buffers const buffers, P const& args = {}) const
		{
			return basic_sender<M, H, scatter_read_t>(static_cast<H const&>(*this), args, buffers);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<M, H, scatter_read_t> read_async(read_buffer const buffer, P const& args = {}) const
		{
			return basic_sender<M, H, scatter_read_t>(static_cast<H const&>(*this), args, buffer);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<M, H, gather_write_t> write_async(write_buffers const buffers, network_endpoint const& endpoint, P const& args = {}) const
		{
			return basic_sender<M, H, gather_write_t>(static_cast<H const&>(*this), args, buffers, endpoint);
		}
		
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<M, H, gather_write_t> write_async(write_buffer const buffer, network_endpoint const& endpoint, P const& args = {}) const
		{
			return basic_sender<M, H, gather_write_t>(static_cast<H const&>(*this), args, buffer, endpoint);
		}
	};
};

} // namespace detail

template<>
struct io_operation_traits<packet_socket_handle::scatter_read_t>
{
	using handle_type = packet_socket_handle const;
	using result_type = packet_read_result;
	using params_type = packet_socket_handle::packet_io_parameters;

	read_buffers buffers;
};

template<>
struct io_operation_traits<packet_socket_handle::gather_write_at>
{
	using handle_type = packet_socket_handle const;
	using result_type = size_t;
	using params_type = packet_socket_handle::packet_io_parameters;

	write_buffers buffers;
	network_endpoint endpoint;
};

} // namespace allio
