#include <allio/socket_handle.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> common_socket_handle::block_create(network_address_kind const address_kind, create_parameters const& args)
{
	return block<io::socket_create>(*this, args, address_kind);
}


vsm::result<void> stream_socket_handle_base::block_connect(network_address const& address, connect_parameters const& args)
{
	return block<io::socket_connect>(static_cast<stream_socket_handle&>(*this), args, address);
}


vsm::result<packet_read_result> packet_socket_handle_base::block_read(read_buffers const buffers, packet_io_parameters const& args)
{
	return block<io::packet_scatter_read>(static_cast<packet_socket_handle&>(*this), args, buffers);
}

vsm::result<size_t> packet_socket_handle_base::block_write(write_buffers const buffers, network_address const& address, packet_io_parameters const& args)
{
	return block<io::packet_gather_write>(static_cast<packet_socket_handle&>(*this), args, buffers, &address);
}


vsm::result<void> listen_socket_handle_base::block_listen(network_address const& address, listen_parameters const& args)
{
	return block<io::socket_listen>(static_cast<listen_socket_handle&>(*this), args, address);
}

vsm::result<accept_result> listen_socket_handle_base::block_accept(accept_parameters const& args) const
{
	return block<io::socket_accept>(static_cast<listen_socket_handle const&>(*this), args);
}


vsm::result<stream_socket_handle> detail::block_connect(network_address const& address, stream_socket_handle::connect_parameters const& args)
{
	vsm::result<stream_socket_handle> r(vsm::result_value);
	vsm_try_void(r->create(address.kind(), args));
	vsm_try_void(r->connect(address, args));
	return r;
}

vsm::result<listen_socket_handle> detail::block_listen(network_address const& address, listen_socket_handle::listen_parameters const& args)
{
	vsm::result<listen_socket_handle> r(vsm::result_value);
	vsm_try_void(r->create(address.kind(), args));
	vsm_try_void(r->listen(address, args));
	return r;
}
