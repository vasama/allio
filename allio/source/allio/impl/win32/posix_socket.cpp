#include <allio/impl/posix/socket.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/byte_io.hpp>
#include <allio/impl/win32/error.hpp>
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


static vsm::result<posix::unique_socket> wsa_socket(
	int const address_family,
	int const type,
	int const protocol,
	DWORD const flags)
{
	SOCKET const socket = WSASocketW(
		address_family,
		type,
		protocol,
		/* lpProtocolInfo: */ nullptr,
		/* group: */ 0,
		flags);

	if (socket == INVALID_SOCKET)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm::result<posix::unique_socket>(vsm::result_value, socket);
}

vsm::result<posix::socket_with_flags> posix::create_socket(
	int const address_family,
	int const type,
	int const protocol,
	io_flags const flags)
{
	vsm_try_void(wsa_init());

	DWORD w_flags = 0;
	handle_flags h_flags = handle_flags::none;

	if (vsm::no_flags(flags, io_flags::create_inheritable))
	{
		w_flags |= WSA_FLAG_NO_HANDLE_INHERIT;
	}

	if (vsm::any_flags(flags, io_flags::create_non_blocking))
	{
		w_flags |= WSA_FLAG_OVERLAPPED;
	}
	else
	{
		h_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	if (vsm::any_flags(flags, io_flags::create_registered_io))
	{
		w_flags |= WSA_FLAG_REGISTERED_IO;
	}

	vsm_try(socket, wsa_socket(
		address_family,
		type,
		protocol,
		w_flags));

	if (vsm::any_flags(flags, io_flags::create_non_blocking))
	{
		h_flags |= set_file_completion_notification_modes((HANDLE)socket.get());
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = h_flags,
	});
}

static vsm::result<posix::unique_socket> wsa_accept(
	posix::socket_type const listen_socket,
	posix::socket_address& addr)
{
	addr.size = sizeof(posix::socket_address_union);

	SOCKET const socket = WSAAccept(
		listen_socket,
		&addr.addr,
		&addr.size,
		/* lpfnCondition: */ nullptr,
		/* dwCallbackData: */ 0);

	if (socket == SOCKET_ERROR)
	{
		return vsm::unexpected(posix::get_last_socket_error());
	}

	return vsm::result<posix::unique_socket>(vsm::result_value, socket);
}

vsm::result<posix::socket_with_flags> posix::socket_accept(
	socket_type const listen_socket,
	socket_address& addr,
	deadline const deadline,
	io_flags const flags)
{
	if (vsm::any_flags(flags, io_flags::create_non_blocking | io_flags::create_registered_io))
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(listen_socket, socket_poll_r, deadline));
	}

	vsm_try(socket, wsa_accept(listen_socket, addr));

	if (vsm::any_flags(flags, io_flags::create_inheritable))
	{
		HANDLE const handle = (HANDLE)socket.get();

		//TODO: Does WSAAccept set inheritable by default?
		if (!SetHandleInformation(
			handle,
			HANDLE_FLAG_INHERIT,
			HANDLE_FLAG_INHERIT))
		{
			return vsm::unexpected(get_last_error());
		}
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = platform_object_t::impl_type::flags::synchronous,
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
