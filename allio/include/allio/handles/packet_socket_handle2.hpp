#pragma once

#include <allio/byte_io.hpp>
#include <allio/common_socket_handle.hpp>

namespace allio {

struct packet_read_result
{
	size_t packet_size;
	network_address address;
};

namespace detail {

class packet_socket_handle_impl : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct bind_tag;
	using bind_parameters = no_parameters;


	struct scatter_read_tag;
	struct gather_write_tag;

	using packet_io_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			scatter_read_tag,
			gather_write_tag,
		>
	>;

protected:
	allio_detail_default_lifetime(packet_socket_handle_impl);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<bind_parameters> P = bind_parameters::interface>
		vsm::result<void> bind(network_address const& address, P const& args = {}) &
		{
			return block<bind_tag>(static_cast<H const&>(*this), args, address);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<packet_read_result> read(read_buffers const buffers, P const& args = {}) const
		{
			return block<scatter_read_tag>(static_cast<H const&>(*this), args, buffers);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<packet_read_result> read(read_buffer const buffer, P const& args = {}) const
		{
			return block<scatter_read_tag>(static_cast<H const&>(*this), args, buffer);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<void> write(write_buffers const buffers, network_address const& address, P const& args = {}) const
		{
			return block<gather_write_tag>(static_cast<H const&>(*this), args, buffers, address);
		}
		
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		vsm::result<void> write(write_buffer const buffer, network_address const& address, P const& args = {}) const
		{
			return block<gather_write_tag>(static_cast<H const&>(*this), args, buffer, address);
		}
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<scatter_read_tag> read_async(read_buffers const buffers, P const& args = {}) const
		{
			return basic_sender<scatter_read_tag>(static_cast<H const&>(*this), args, buffers);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<scatter_read_tag> read_async(read_buffer const buffer, P const& args = {}) const
		{
			return basic_sender<scatter_read_tag>(static_cast<H const&>(*this), args, buffer);
		}

		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<gather_write_tag> write_async(write_buffers const buffers, network_address const& address, P const& args = {}) const
		{
			return basic_sender<gather_write_tag>(static_cast<H const&>(*this), args, buffers, address);
		}
		
		template<parameters<packet_io_parameters> P = packet_io_parameters::interface>
		basic_sender<gather_write_tag> write_async(write_buffer const buffer, network_address const& address, P const& args = {}) const
		{
			return basic_sender<gather_write_tag>(static_cast<H const&>(*this), args, buffer, address);
		}
	};
};

} // namespace detail

using packet_socket_handle = basic_handle<detail::packet_socket_handle_impl>;

template<typename Multiplexer>
using basic_packet_socket_handle = basic_async_handle<detail::packet_socket_handle_impl, Multiplexer>;


template<>
struct io_operation_traits<packet_socket_handle::scatter_read_tag>
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
	network_address address;
};

} // namespace allio
