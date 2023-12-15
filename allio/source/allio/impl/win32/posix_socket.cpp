#include <allio/impl/posix/socket.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/byte_io.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/lazy.hpp>

#include <algorithm>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

char const* posix::socket_error_category::name() const noexcept
{
	return "Windows WSA";
}

std::string posix::socket_error_category::message(int const code) const
{
	return std::system_category().message(code);
}

posix::socket_error_category const posix::socket_error_category::instance;


vsm::result<posix::socket_with_flags> posix::create_socket(
	int const address_family,
	int const type,
	int const protocol,
	socket_flags const flags)
{
	vsm_try_void(wsa_init());

	DWORD w_flags = 0;
	handle_flags h_flags = {};

	if (vsm::no_flags(flags, socket_flags::inheritable))
	{
		w_flags |= WSA_FLAG_NO_HANDLE_INHERIT;
	}

	if (vsm::any_flags(flags, socket_flags::multiplexable))
	{
		w_flags |= WSA_FLAG_OVERLAPPED;
	}
	else
	{
		h_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	if (vsm::any_flags(flags, socket_flags::registered_io))
	{
		w_flags |= WSA_FLAG_REGISTERED_IO;
	}

	SOCKET const raw_socket = WSASocketW(
		address_family,
		type,
		protocol,
		/* lpProtocolInfo: */ nullptr,
		/* group: */ 0,
		w_flags);

	if (raw_socket == INVALID_SOCKET)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	unique_socket socket(raw_socket);

	if (vsm::any_flags(flags, socket_flags::multiplexable))
	{
		//TODO: Set multiplexable completion modes.
		//h_flags |= set_multiplexable_completion_modes(socket);
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = h_flags,
	});
}

vsm::result<posix::socket_poll_mask> posix::socket_poll(
	socket_type const socket,
	socket_poll_mask const mask,
	deadline const deadline)
{
	WSAPOLLFD poll_fd =
	{
		.fd = socket,
		.events = mask,
	};

	int const r = WSAPoll(
		&poll_fd,
		/* fds: */ 1,
		//TODO: WSAPoll timeout
		0);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	if (r == 0)
	{
		return 0;
	}

	vsm_assert(poll_fd.revents & mask);
	return poll_fd.revents;
}

vsm::result<void> posix::socket_set_non_blocking(
	socket_type const socket,
	bool const non_blocking)
{
	unsigned long mode = non_blocking ? 1 : 0;
	if (ioctlsocket(socket, FIONBIO, &mode) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	return {};
}

vsm::result<size_t> posix::socket_scatter_read(
	socket_type const socket,
	read_buffers const buffers)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, buffers));

	DWORD flags = 0;

	DWORD transferred;
	int const r = WSARecv(
		socket,
		wsa_buffers.data(),
		wsa_buffers.size(),
		&transferred,
		&flags,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return transferred;
}

vsm::result<size_t> posix::socket_gather_write(
	socket_type const socket,
	write_buffers const buffers)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, buffers));

	DWORD transferred;
	int const r = WSASend(
		socket,
		wsa_buffers.data(),
		wsa_buffers.size(),
		&transferred,
		/* dwFlags: */ 0,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	return transferred;
}

vsm::result<size_t> posix::socket_receive_from(
	socket_type const socket,
	socket_address& addr,
	read_buffers const buffers)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, buffers));

	DWORD transferred;
	DWORD flags = 0;

	addr.size = sizeof(socket_address_union);

	if (WSARecvFrom(
		socket,
		wsa_buffers.data(),
		wsa_buffers.size(),
		&transferred,
		&flags,
		&addr.addr,
		&addr.size,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	vsm_assert(transferred == get_buffers_size(buffers));

	return transferred;
}

vsm::result<void> posix::socket_send_to(
	socket_type const socket,
	socket_address const& addr,
	write_buffers const buffers)
{
	wsa_buffers_storage<64> buffers_storage;
	vsm_try(wsa_buffers, make_wsa_buffers(buffers_storage, buffers));

	DWORD transferred;
	if (WSASendTo(
		socket,
		wsa_buffers.data(),
		wsa_buffers.size(),
		&transferred,
		/* dwFlags: */ 0,
		&addr.addr,
		addr.size,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine */ nullptr) == SOCKET_ERROR)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	vsm_assert(transferred == get_buffers_size(buffers));

	return {};
}
