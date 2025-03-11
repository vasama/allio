#include <allio/detail/handles/raw_listen_socket.hpp>

#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa_thread_event.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using socket_handle_type = basic_detached_handle<raw_socket_t>;

vsm::result<void> raw_listen_socket_t::listen(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, listen_t> const& a)
{
	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_STREAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_STREAM,
		protocol,
		a.flags));

	vsm_try_void(posix::socket_listen(
		socket.get(),
		addr,
		a.backlog));

	h.flags = flags::not_null | posix::set_address_family(addr.addr->sa_family) | flags;
	h.platform_handle = posix::wrap_socket(socket.release());

	return {};
}

vsm::result<socket_handle_type> raw_listen_socket_t::accept(
	native_handle<raw_listen_socket_t> const& h,
	io_parameters_t<raw_listen_socket_t, accept_t> const& a)
{
	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	SOCKET const listen_socket = posix::unwrap_socket(h.platform_handle);

	int const address_family = posix::get_address_family(h.flags);
	size_t const max_address_size = posix::get_max_socket_address_size(address_family);
	size_t const min_address_storage_size = max_address_size + 16;
	vsm_assert(min_address_storage_size <= sizeof(wsa_accept_address_storage));

	void* user_address_storage = nullptr;
	if (a.endpoint)
	{
		vsm_try_assign(user_address_storage, a.endpoint.resize(
			max_address_size,
			max_address_size,
			std::align_val_t(alignof(posix::socket_address_union))));
	}

	wsa_accept_address_storage local_address_storage;
	void* const address_storage = user_address_storage
		? user_address_storage
		: &local_address_storage;

	posix::socket_with_flags socket_with_flags;
	auto& [socket, socket_flags] = socket_with_flags;

	if (a.deadline != deadline::never() ||
		!h.flags[platform_object_t::impl_type::flags::synchronous] ||
		vsm::no_flags(a.flags, io_flags::create_synchronous))
	{
		vsm_try_assign(socket_with_flags, posix::create_socket(
			address_family,
			SOCK_STREAM,
			*posix::choose_protocol(address_family, SOCK_STREAM),
			a.flags));

		vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

		DWORD const e = wsa_accept_ex(
			listen_socket,
			socket.get(),
			&local_address_storage,
			vsm::truncating(min_address_storage_size),
			overlapped);

		if (e == WSA_IO_PENDING)
		{
			DWORD transferred;
			DWORD dummy_flags;

			vsm_try_void(overlapped.wait(
				socket.get(),
				a.deadline,
				&transferred,
				&dummy_flags));
		}
		else if (e != 0)
		{
			return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
		}

#if 0
		DWORD transferred = static_cast<DWORD>(-1);
		if (!win32::AcceptEx(
			listen_socket,
			socket.get(),
			/* lpOutputBuffer: */ &local_address_storage,
			/* dwReceiveDataLength: */ 0,
			/* dwLocalAddressLength: */ 0,
			vsm::truncating(min_address_storage_size),
			&transferred,
			overlapped))
		{
			if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
			{
				return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
			}

			DWORD accept_flags;
			vsm_try_void(overlapped.wait(
				socket.get(),
				a.deadline,
				&transferred,
				&accept_flags));
		}
		vsm_assert(transferred == 0);
#endif

		if (user_address_storage != nullptr)
		{
			std::memcpy(user_address_storage, &local_address_storage, max_address_size);
		}
	}
	else
	{
		int addr_size = vsm::truncating(max_address_size);

		SOCKET const new_socket = win32::WSAAccept(
			listen_socket,
			static_cast<sockaddr*>(address_storage),
			&addr_size,
			/* lpfnCondition: */ nullptr,
			/* dwCallbackData: */ 0);

		if (new_socket == INVALID_SOCKET)
		{
			return vsm::unexpected(allio_error(posix::get_last_socket_error()));
		}

		socket.reset(new_socket);
		socket_flags = platform_object_t::impl_type::flags::synchronous;
	}

	socket_flags |= posix::set_address_family(address_family);

	return vsm_lazy(socket_handle_type(
		adopt_handle,
		native_handle<raw_socket_t>
		{
			native_handle<platform_object_t>
			{
				native_handle<object_t>
				{
					object_t::flags::not_null | socket_flags,
				},
				posix::wrap_socket(socket.release()),
			},
		}));
}

vsm::result<void> raw_listen_socket_t::close(
	native_handle<raw_listen_socket_t>& h,
	io_parameters_t<raw_listen_socket_t, close_t> const& a)
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
