#include <allio/detail/handles/raw_datagram_socket.hpp>

#include <allio/impl/byte_io_buffers.hpp>
#include <allio/impl/posix/handles/raw_common_socket.hpp>
#include <allio/impl/posix/socket.hpp>
#include <allio/impl/win32/wsa_thread_event.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/numeric.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

byte_io_limits raw_datagram_socket_t::get_byte_io_limits(
	native_handle<raw_datagram_socket_t> const& h)
{
	static constexpr auto max_buffer_count = std::numeric_limits<DWORD>::max();

	return
	{
		.max_buffer_count = max_buffer_count,
		.max_atomic_buffer_count = max_buffer_count,
	};
}

vsm::result<void> raw_datagram_socket_t::bind(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, bind_t> const& a)
{
	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));
	vsm_try(protocol, posix::choose_protocol(addr.addr->sa_family, SOCK_DGRAM));

	vsm_try_bind((socket, flags), posix::create_socket(
		addr.addr->sa_family,
		SOCK_DGRAM,
		protocol,
		a.flags));

	vsm_try_void(posix::socket_bind(socket.get(), addr));

	h.flags = flags::not_null | posix::set_address_family(addr.addr->sa_family) | flags;
	h.platform_handle = posix::wrap_socket(socket.release());

	return {};
}

vsm::result<size_t> raw_datagram_socket_t::receive_from(
	native_handle<raw_datagram_socket_t> const& h,
	io_parameters_t<raw_datagram_socket_t, receive_from_t> const& a)
{
	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	posix::socket_type const socket = posix::unwrap_socket(h.platform_handle);

	int const address_family = posix::get_address_family(h.flags);
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

	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, buffer_storage));

	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSARecvFrom(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::truncating(wsa_buffers.size()),
		&transferred,
		&flags,
		static_cast<sockaddr*>(address_storage),
		&address_size,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
		}

		vsm_try_void(overlapped.wait(
			socket,
			a.deadline,
			&transferred,
			&flags));
	}

	return transferred;
}

vsm::result<void> raw_datagram_socket_t::send_to(
	native_handle<raw_datagram_socket_t> const& h,
	io_parameters_t<raw_datagram_socket_t, send_to_t> const& a)
{
	if (a.deadline != deadline::never() &&
		h.flags[platform_object_t::impl_type::flags::synchronous])
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	vsm_try_void(check_wsa_buffers_size<DWORD>(a.buffers));

	posix::socket_type const socket = posix::unwrap_socket(h.platform_handle);

	posix::socket_address_storage address_storage;
	vsm_try(addr, posix::get_socket_address(a.endpoint, address_storage));

	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(a.buffers, buffer_storage));

	vsm_try(overlapped, wsa_thread_overlapped::get_for(h));

	DWORD transferred;

	if (win32::WSASendTo(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::truncating(wsa_buffers.size()),
		&transferred,
		/* dwFlags: */ 0,
		addr.addr,
		addr.size,
		overlapped,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		if (int const e = WSAGetLastError(); e != WSA_IO_PENDING)
		{
			return vsm::unexpected(allio_error(static_cast<posix::socket_error>(e)));
		}

		DWORD dummy_flags;
		vsm_try_void(overlapped.wait(
			socket,
			a.deadline,
			&transferred,
			&dummy_flags));
	}
	vsm_assert(transferred == get_io_buffers_size(a.buffers));

	return {};
}

vsm::result<void> raw_datagram_socket_t::close(
	native_handle<raw_datagram_socket_t>& h,
	io_parameters_t<raw_datagram_socket_t, close_t> const& a)
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
