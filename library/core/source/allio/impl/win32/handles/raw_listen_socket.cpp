#include <allio/detail/handles/raw_listen_socket.hpp>

#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa_thread_event.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/lazy.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

using accept_result_type = accept_result<basic_detached_handle<raw_socket_t>>;

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

vsm::result<accept_result_type> raw_listen_socket_t::accept(
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

	void* user_addr_storage = nullptr;
	if (a.endpoint_storage)
	{
		vsm_try_assign(user_addr_storage, a.endpoint_storage.get_storage(
			max_address_size,
			std::align_val_t(alignof(posix::socket_address_union))));
	}

	wsa_accept_address_storage wsa_addr;
	posix::socket_address_union& addr = wsa_addr.remote;

	posix::socket_with_flags socket_with_flags;
	auto& [socket, flags] = socket_with_flags;

	if (a.deadline != deadline::never() ||
		!h.flags[platform_object_t::impl_type::flags::synchronous] ||
		vsm::no_flags(a.flags, io_flags::create_synchronous))
	{
		vsm_try(protocol, posix::choose_protocol(address_family, SOCK_STREAM));

		vsm_try_assign(socket_with_flags, posix::create_socket(
			address_family,
			SOCK_STREAM,
			protocol,
			a.flags));

		vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

		DWORD transferred = static_cast<DWORD>(-1);
		if (!win32::AcceptEx(
			listen_socket,
			socket.get(),
			/* lpOutputBuffer: */ &wsa_addr,
			/* dwReceiveDataLength: */ 0,
			sizeof(wsa_addr.local),
			sizeof(wsa_addr.remote),
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
	}
	else
	{
		int addr_size = sizeof(posix::socket_address_union);

		SOCKET const new_socket = win32::WSAAccept(
			listen_socket,
			&addr.addr,
			&addr_size,
			/* lpfnCondition: */ nullptr,
			/* dwCallbackData: */ 0);

		if (new_socket == INVALID_SOCKET)
		{
			return vsm::unexpected(allio_error(posix::get_last_socket_error()));
		}

		socket.reset(new_socket);
	}

	auto const make_socket_handle = [&]()
	{
		native_handle<raw_socket_t> h = {};
		h.flags = object_t::flags::not_null | flags;
		h.platform_handle = posix::wrap_socket(socket.release());
		return basic_detached_handle<raw_socket_t>(adopt_handle, h);
	};

	auto const get_endpoint = [&]() -> any_endpoint_view
	{
		if (user_addr_storage)
		{
			std::memcpy(user_addr_storage, &wsa_addr.remote.addr, max_address_size);
			return platform_endpoint_view(user_addr_storage, max_address_size);
		}
		else
		{
			return null_endpoint;
		}
	};

	return vsm::result<accept_result_type>(
		vsm::result_value,
		vsm_lazy(make_socket_handle()),
		get_endpoint());
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
