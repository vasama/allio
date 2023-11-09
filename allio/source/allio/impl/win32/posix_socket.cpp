#include <allio/impl/posix/socket.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/win32/handles/platform_handle.hpp>
#include <allio/impl/win32/wsa_ex.hpp>

#include <vsm/lazy.hpp>

#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

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


static vsm::result<void> wsa_startup()
{
	struct wsa_init
	{
		int error;

		wsa_init(int const error)
			: error(error)
		{
		}

		wsa_init(wsa_init const&) = delete;
		wsa_init& operator=(wsa_init const&) = delete;

		~wsa_init()
		{
			if (error == 0)
			{
				WSACleanup();
			}
		}
	};

	static wsa_init const init = []() -> wsa_init
	{
		WSADATA data;
		return WSAStartup(MAKEWORD(2, 2), &data);
	}();

	if (init.error != 0)
	{
		return vsm::unexpected(static_cast<posix::socket_error>(init.error));
	}

	return {};
}

vsm::result<posix::unique_socket_with_flags> posix::create_socket(
	int const address_family,
	bool const multiplexable)
{
	vsm_try_void(wsa_startup());

	int protocol = 0;

	//TODO: UDP support
	switch (address_family)
	{
	case AF_INET:
	case AF_INET6:
		protocol = IPPROTO_TCP;
		break;
	}

	DWORD flags = WSA_FLAG_NO_HANDLE_INHERIT;
	handle_flags handle_flags = {};

	if (multiplexable)
	{
		flags |= WSA_FLAG_OVERLAPPED;
	}
	else
	{
		handle_flags |= platform_object_t::impl_type::flags::synchronous;
	}

	SOCKET const raw_socket = WSASocketW(
		address_family,
		SOCK_STREAM,
		protocol,
		/* lpProtocolInfo: */ nullptr,
		/* group: */ 0,
		flags);

	if (raw_socket == INVALID_SOCKET)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	unique_socket socket(raw_socket);

	if (multiplexable)
	{
		//TODO: Set multiplexable completion modes.
		//handle_flags |= set_multiplexable_completion_modes(socket);
	}

	return vsm_lazy(unique_socket_with_flags
	{
		.socket = vsm_move(socket),
		.flags = handle_flags,
	});
}

#if 0
template<typename B, std::derived_from<B> D>
static B& base_cast(D& derived)
{
	return derived;
}

template<typename B, std::derived_from<B> D>
static B&& base_cast(D&& derived)
{
	return static_cast<D&&>(derived);
}

template<typename B, std::derived_from<B> D>
static B const& base_cast(D const& derived)
{
	return derived;
}

template<typename B, std::derived_from<B> D>
static B const&& base_cast(D const&& derived)
{
	return static_cast<D const&&>(derived);
}

vsm::result<unique_socket_with_flags> posix::socket_accept(socket_type const listen_socket, socket_address& addr)
{
	vsm_try(listen_addr, socket_address::get(listen_socket));
	auto socket_result = create_socket(listen_addr.addr.sa_family);
	vsm_try((auto&&, socket), socket_result);

	wsa_accept_address_buffer addr_buffer;
	DWORD const error = wsa_accept_ex(
		listen_socket,
		socket.socket.get(),
		addr_buffer,
		/* overlapped: */ nullptr);

	if (error != 0)
	{
		return vsm::unexpected(static_cast<socket_error>(error));
	}

	base_cast<socket_address_union>(addr) = addr_buffer.remote;
	addr.size = 0;

	return socket_result;
}
#endif

vsm::result<posix::socket_poll_mask> posix::socket_poll(socket_type const socket, socket_poll_mask const mask, deadline const deadline)
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

vsm::result<void> posix::socket_set_non_blocking(socket_type const socket, bool const non_blocking)
{
	unsigned long mode = non_blocking ? 1 : 0;
	if (ioctlsocket(socket, FIONBIO, &mode) == socket_error_value)
	{
		return vsm::unexpected(get_last_socket_error());
	}
	return {};
}

// The layout of WSABUF necessitates a copy.
static vsm::result<void> transform_wsa_buffers(untyped_buffers const buffers, WSABUF* const wsa_buffers)
{
	bool size_out_of_range = false;

	std::transform(
		buffers.begin(),
		buffers.end(),
		wsa_buffers,
		[&](untyped_buffer const buffer) -> WSABUF
		{
			if (buffer.size() > std::numeric_limits<ULONG>::max())
			{
				size_out_of_range = true;
			}

			return WSABUF
			{
				.len = static_cast<ULONG>(buffer.size()),
				.buf = static_cast<CHAR*>(const_cast<void*>(buffer.data())),
			};
		}
	);

	if (size_out_of_range)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	return {};
}

vsm::result<size_t> posix::socket_scatter_read(socket_type const socket, untyped_buffers const buffers)
{
	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;
	vsm_try(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));
	vsm_try_void(transform_wsa_buffers(buffers, wsa_buffers));

	DWORD flags = 0;

	DWORD transferred;
	int const r = WSARecv(
		socket,
		wsa_buffers,
		buffers.size(),
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

vsm::result<size_t> posix::socket_gather_write(socket_type const socket, untyped_buffers const buffers)
{
	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;
	vsm_try(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));
	vsm_try_void(transform_wsa_buffers(buffers, wsa_buffers));

	DWORD transferred;
	int const r = WSASend(
		socket,
		wsa_buffers,
		buffers.size(),
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

#if 0
vsm::result<void> allio::packet_scatter_read(io::parameters_with_result<io::packet_scatter_read> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	SOCKET const socket = unwrap_socket(h.get_platform_handle());

	read_buffers const buffers = args.buffers;

	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;
	vsm_try(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));
	transform_wsa_buffers(buffers, wsa_buffers);

	socket_address_union addr;

	WSAMSG message =
	{
		.name = &addr.addr,
		.namelen = sizeof(addr),
		.lpBuffers = wsa_buffers,
		.dwBufferCount = static_cast<uint32_t>(buffers.size()),
	};

	DWORD transferred;
	if (DWORD const error = wsa_recv_msg(socket, &message, &transferred))
	{
		return vsm::unexpected(static_cast<socket_error>(error));
	}
	
	args.result->packet_size = transferred;
	args.result->endpoint = addr.get_network_endpoint();

	return {};
}

vsm::result<void> allio::packet_gather_write(io::parameters_with_result<io::packet_gather_write> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	SOCKET const socket = unwrap_socket(h.get_platform_handle());

	write_buffers const buffers = args.buffers;
	vsm_try(addr, socket_address::make(*args.endpoint));

	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;
	vsm_try(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));
	transform_wsa_buffers(buffers, wsa_buffers);

	WSAMSG message =
	{
		.name = &addr.addr,
		.namelen = addr.size,
		.lpBuffers = wsa_buffers,
		.dwBufferCount = static_cast<uint32_t>(buffers.size()),
	};

	DWORD transferred;
	if (DWORD const error = wsa_send_msg(socket, &message, &transferred))
	{
		return vsm::unexpected(static_cast<socket_error>(error));
	}

	*args.result = transferred;

	return {};
}
#endif
