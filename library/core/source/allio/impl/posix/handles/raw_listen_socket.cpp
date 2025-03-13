#include <allio/detail/handles/raw_listen_socket.hpp>

#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

using socket_handle_type = basic_detached_handle<raw_socket_t>;

vsm::result<void> raw_listen_socket_t::listen(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
{
	socket_address_storage address_storage;
	vsm_try(addr, get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(socket_listen(
		socket.get(),
		addr,
		a.backlog));

	h.flags = flags::not_null | set_address_family(addr.addr->sa_family) | flags;
	h.platform_handle = wrap_socket(socket.release());

	return {};
}

vsm::result<socket_handle_type> raw_listen_socket_t::accept(
	native_handle<raw_listen_socket_t> const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);

	void* user_address_storage = nullptr;
	if (a.endpoint)
	{
		vsm_try_assign(user_address_storage, a.endpoint.resize(
			max_address_size,
			max_address_size,
			std::align_val_t(alignof(posix::socket_address_union))));
	}

	socket_address_union local_address_storage;
	void* const address_storage = user_address_storage
		? user_address_storage
		: &local_address_storage;

	socket_address_size_type address_size = vsm::truncating(max_address_size);

	vsm_try_bind((socket, flags), socket_accept(
		unwrap_socket(h.platform_handle),
		static_cast<sockaddr*>(address_storage),
		&address_size,
		a.deadline,
		a.flags));

	return vsm_lazy(socket_handle_type(
		adopt_handle,
		native_handle<raw_socket_t>
		{
			native_handle<platform_object_t>
			{
				native_handle<object_t>
				{
					object_t::flags::not_null
						| flags
						| set_address_family(address_family),
				},
				wrap_handle(socket.release()),
			},
		}));
}

vsm::result<void> raw_listen_socket_t::close(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, close_t> const&)
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
