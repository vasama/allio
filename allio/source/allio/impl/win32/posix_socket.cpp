#include <allio/impl/posix_socket.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/win32/platform_handle.hpp>
#include <allio/impl/win32/wsa_ex.hpp>

#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

using namespace allio;
using namespace allio::win32;

char const* socket_error_category::name() const noexcept
{
	return "Windows WSA";
}

std::string socket_error_category::message(int const code) const
{
	return std::system_category().message(code);
}

socket_error_category const socket_error_category::instance;


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
				//TODO:
				//WSACleanup();
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
		return vsm::unexpected(get_last_socket_error());
	}

	return {};
}


vsm::result<unique_socket_with_flags> allio::create_socket(int const address_family, common_socket_handle::create_parameters const& args)
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

	if (args.multiplexable)
	{
		flags |= WSA_FLAG_OVERLAPPED;
		handle_flags |= platform_handle::implementation::flags::overlapped;
	}

	SOCKET const socket = WSASocketW(
		address_family,
		SOCK_STREAM,
		protocol,
		nullptr,
		0,
		flags);

	if (socket == INVALID_SOCKET)
	{
		return vsm::unexpected(get_last_socket_error());
	}

	if (args.multiplexable)
	{
		//TODO: Set multiplexable completion modes.
		//handle_flags |= set_multiplexable_completion_modes(socket);
	}

	return vsm::result<unique_socket_with_flags>(vsm::result_value, socket, handle_flags);
}

vsm::result<unique_socket_with_flags> allio::accept_socket(socket_type const listen_socket, socket_address& addr, common_socket_handle::create_parameters const& args)
{
	vsm_try(listen_addr, socket_address::get(listen_socket));
	auto socket_result = create_socket(listen_addr.addr.sa_family, args);
	vsm_try((auto&&, socket), socket_result);

	wsa_accept_address_buffer addr_buffer;
	if (DWORD const error = wsa_accept_ex(listen_socket, socket.socket.get(), addr_buffer, nullptr))
	{
		return vsm::unexpected(static_cast<socket_error>(error));
	}

	static_cast<socket_address_union&>(addr) = addr_buffer.remote;
	addr.size = 0;

	return socket_result;
}

// The layout of WSABUF necessitates a copy.
template<typename T>
static void transform_wsa_buffers(basic_buffers<T> const buffers, WSABUF* const wsa_buffers)
{
	std::transform(buffers.begin(), buffers.end(), wsa_buffers,
		[&](basic_buffer<T> const buffer) -> WSABUF
		{
			return WSABUF
			{
				.len = static_cast<uint32_t>(buffer.size()),
				.buf = (CHAR*)buffer.data(),
			};
		}
	);
}

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
	args.result->address = addr.get_network_address();

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
	vsm_try(addr, socket_address::make(*args.address));

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
