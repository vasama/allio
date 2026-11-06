#include <allio/detail/handles/raw_socket.hpp>

#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

vsm::result<void> raw_socket_t::connect(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, connect_t> const& a)
{
	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(socket_connect(
		socket.get(),
		addr,
		a.deadline));

	h.flags = flags::not_null | set_address_family(addr.addr->sa_family) | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<void> raw_socket_t::disconnect(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, disconnect_t> const& a)
{
	if (a.deadline != deadline::never())
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	return socket_disconnect(unwrap_socket(h.platform_handle), socket_disconnect_rw);
}

vsm::result<void> raw_socket_t::get_endpoint(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, get_endpoint_t> const& a)
{
	posix::socket_address_family_type const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	posix::socket_address_union local_address_storage;
	void* address_storage = &local_address_storage;
	posix::socket_address_size_type address_size = sizeof(local_address_storage);

	if (a.endpoint)
	{
		if (a.endpoint.is_platform_endpoint())
		{
			vsm_try_assign(address_storage, a.endpoint.resize(
				max_address_size,
				max_address_size,
				std::align_val_t(alignof(posix::socket_address_union))));

			address_size = vsm::truncating(max_address_size);
		}
		else if (a.endpoint.kind() != posix::get_address_kind(address_family))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
		else
		{
			//TODO: Implement typed endpoint buffer usage.
			return vsm::unexpected(allio_error(error::unsupported_operation));
		}
	}

	vsm_try(posix::get_socket_address(
		posix::unwrap_socket(h.platform_handle),
		static_cast<sockaddr*>(address_storage),
		&address_size));

	return {};
}

vsm::result<read_result> raw_socket_t::stream_read(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_read_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_r, a.deadline));
	}

	return socket_scatter_read(socket, a.buffers);
}

vsm::result<size_t> raw_socket_t::stream_write(
	native_handle<raw_socket_t> const& h,
	io_parameters_t<raw_socket_t, stream_write_t> const& a)
{
	socket_type const socket = unwrap_socket(h.platform_handle);

	if (a.deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(socket, socket_poll_w, a.deadline));
	}

	return socket_gather_write(socket, a.buffers);
}

vsm::result<void> raw_socket_t::close(
	native_handle<raw_socket_t>& h,
	io_parameters_t<raw_socket_t, close_t> const&)
{
	native_platform_handle const handle = h.platform_handle;
	if (handle != native_platform_handle::null)
	{
		h.platform_handle = native_platform_handle::null;
		posix::close_socket(posix::unwrap_socket(handle));
	}

	h.flags = handle_flags::none;

	return {};
}
