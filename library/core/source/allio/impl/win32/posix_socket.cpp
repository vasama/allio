#include <allio/impl/posix/socket.hpp>

#include <allio/impl/byte_io_buffers.hpp>
#include <allio/impl/error_encoding_impl.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/handles/platform_object.hpp>
#include <allio/impl/win32/wsa.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>

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


static constexpr uint32_t ec_wsa_offset             = 10000;
static constexpr uint32_t ec_wsa_code_mask          = ec::code_mask >> 1;
static constexpr uint32_t ec_wsa_flag               = ec_wsa_code_mask + 1;


template<>
uint32_t ec::encode_error_code(posix::socket_error const e)
{
	uint32_t value = static_cast<uint32_t>(e);
	uint32_t flags = 0;

	if (value >= ec_wsa_offset)
	{
		value -= ec_wsa_offset;
		flags |= ec_wsa_flag;
	}

	return value > ec_wsa_code_mask
		? 0
		: value | flags;
}

template<>
posix::socket_error ec::decode_error_code(uint32_t const e)
{
	return static_cast<posix::socket_error>(
		(e & ec_wsa_code_mask) + (e & ec_wsa_flag ? ec_wsa_offset : 0));
}

template class ec::encoded_error_category<allio_error_encoding, posix::socket_error>;


static vsm::result<posix::unique_socket> wsa_socket(
	int const address_family,
	int const type,
	int const protocol,
	DWORD const flags)
{
	SOCKET const socket = win32::WSASocketW(
		address_family,
		type,
		protocol,
		/* lpProtocolInfo: */ nullptr,
		/* group: */ 0,
		flags);

	if (socket == INVALID_SOCKET)
	{
		return vsm::unexpected(allio_error(posix::get_last_socket_error()));
	}

	return vsm::result<posix::unique_socket>(vsm::result_value, socket);
}

vsm::result<posix::socket_with_flags> posix::create_socket(
	int const address_family,
	int const type,
	int const protocol,
	io_flags const flags)
{
	DWORD w_flags = 0;
	handle_flags h_flags = handle_flags::none;

	if (vsm::no_flags(flags, io_flags::create_inheritable))
	{
		w_flags |= WSA_FLAG_NO_HANDLE_INHERIT;
	}

	if (vsm::any_flags(flags, io_flags::create_synchronous))
	{
		h_flags |= platform_object_t::impl_type::flags::synchronous;
	}
	else
	{
		w_flags |= WSA_FLAG_OVERLAPPED;
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

	if (vsm::no_flags(flags, io_flags::create_synchronous))
	{
		h_flags |= set_file_completion_notification_modes(
			reinterpret_cast<HANDLE>(socket.get()));
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = h_flags,
	});
}

#if 0 //TODO: This is never used?
static vsm::result<posix::unique_socket> wsa_accept(
	posix::socket_type const listen_socket,
	posix::socket_address& addr)
{
	addr.size = sizeof(posix::socket_address_union);

	SOCKET const socket = win32::WSAAccept(
		listen_socket,
		&addr.addr,
		&addr.size,
		/* lpfnCondition: */ nullptr,
		/* dwCallbackData: */ 0);

	if (socket == static_cast<SOCKET>(SOCKET_ERROR))
	{
		return vsm::unexpected(allio_error(posix::get_last_socket_error()));
	}

	return vsm::result<posix::unique_socket>(vsm::result_value, socket);
}

vsm::result<posix::socket_with_flags> posix::socket_accept(
	socket_type const listen_socket,
	socket_address& addr,
	deadline const deadline,
	io_flags const flags)
{
	//TODO: Check for create_synchronous. Accept overlapped handle. Probably requires using
	///     WSAAcceptEx and waiting on an event?
	if (vsm::any_flags(flags, io_flags::create_non_blocking | io_flags::create_registered_io))
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	if (deadline != deadline::never())
	{
		vsm_try_void(socket_poll_or_timeout(listen_socket, socket_poll_r, deadline));
	}

	vsm_try(socket, wsa_accept(listen_socket, addr));

	if (vsm::any_flags(flags, io_flags::create_inheritable))
	{
		HANDLE const handle = reinterpret_cast<HANDLE>(socket.get());

		//TODO: Does WSAAccept set inheritable by default?
		if (!SetHandleInformation(
			handle,
			HANDLE_FLAG_INHERIT,
			HANDLE_FLAG_INHERIT))
		{
			return vsm::unexpected(allio_error(get_last_error()));
		}
	}

	return vsm_lazy(socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = platform_object_t::impl_type::flags::synchronous,
	});
}
#endif

vsm::result<posix::socket_poll_mask> posix::socket_poll(
	socket_type const socket,
	socket_poll_mask const mask,
	deadline const)
{
	WSAPOLLFD poll_fd =
	{
		.fd = socket,
		.events = mask,
	};

	int const r = win32::WSAPoll(
		&poll_fd,
		/* fds: */ 1,
		//TODO: WSAPoll timeout
		0);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	if (r == 0)
	{
		return static_cast<socket_poll_mask>(0);
	}

	vsm_assert((poll_fd.revents & mask) != 0);
	vsm_assert((poll_fd.revents & ~mask) == 0);

	return poll_fd.revents;
}

vsm::result<void> posix::socket_set_non_blocking(
	socket_type const socket,
	bool const non_blocking)
{
	unsigned long mode = non_blocking ? 1 : 0;
	if (ioctlsocket(socket, static_cast<long>(FIONBIO), &mode) == socket_error_value)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}
	return {};
}

vsm::result<size_t> posix::socket_scatter_read(
	socket_type const socket,
	new_read_buffers const buffers)
{
	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(buffers, buffer_storage));

	DWORD flags = 0;

	DWORD transferred;
	int const r = win32::WSARecv(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::saturating(wsa_buffers.size()),
		&transferred,
		&flags,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	if (transferred == 0 && !io_buffers_is_empty(buffers))
	{
		return vsm::unexpected(allio_error(error::end_of_stream));
	}

	return transferred;
}

vsm::result<size_t> posix::socket_gather_write(
	socket_type const socket,
	new_write_buffers const buffers)
{
	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(buffers, buffer_storage));

	DWORD transferred;
	int const r = win32::WSASend(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::saturating(wsa_buffers.size()),
		&transferred,
		/* dwFlags: */ 0,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr);

	if (r == SOCKET_ERROR)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}

	return transferred;
}

vsm::result<size_t> posix::socket_receive_from(
	socket_type const socket,
	sockaddr* const addr,
	socket_address_size_type* const addr_size,
	new_read_buffers const buffers)
{
	vsm_try_void(check_wsa_buffers_size<DWORD>(buffers));

	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(buffers, buffer_storage));

	DWORD transferred;
	DWORD flags = 0;

	if (win32::WSARecvFrom(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::truncating(wsa_buffers.size()),
		&transferred,
		&flags,
		addr,
		addr_size,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine: */ nullptr) == SOCKET_ERROR)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}
	vsm_assert(transferred == get_io_buffers_size(buffers));

	return transferred;
}

vsm::result<void> posix::socket_send_to(
	socket_type const socket,
	socket_address_view const addr,
	new_write_buffers const buffers)
{
	vsm_try_void(check_wsa_buffers_size<DWORD>(buffers));

	dynamic_wsa_buffer_storage buffer_storage;
	vsm_try(wsa_buffers, get_wsa_buffers(buffers, buffer_storage));

	DWORD transferred;
	if (win32::WSASendTo(
		socket,
		const_cast<WSABUF*>(wsa_buffers.data()),
		vsm::truncating(wsa_buffers.size()),
		&transferred,
		/* dwFlags: */ 0,
		addr.addr,
		addr.size,
		/* lpOverlapped: */ nullptr,
		/* lpCompletionRoutine */ nullptr) == SOCKET_ERROR)
	{
		return vsm::unexpected(allio_error(get_last_socket_error()));
	}
	vsm_assert(transferred == get_io_buffers_size(buffers));

	return {};
}
