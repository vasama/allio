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


static result<void> wsa_startup()
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
		return allio_ERROR(get_last_socket_error());
	}

	return {};
}


result<unique_socket_with_flags> allio::create_socket(int const address_family, common_socket_handle::create_parameters const& args)
{
	allio_TRYV(wsa_startup());

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
		return allio_ERROR(get_last_socket_error());
	}

	if (args.multiplexable)
	{
		//TODO: Set multiplexable completion modes.
		//handle_flags |= set_multiplexable_completion_modes(socket);
	}

	return result<unique_socket_with_flags>(result_value, socket, handle_flags);
}

result<unique_socket_with_flags> allio::accept_socket(socket_type const listen_socket, socket_address& addr, common_socket_handle::create_parameters const& args)
{
	allio_TRY(listen_addr, socket_address::get(listen_socket));
	auto socket_result = create_socket(listen_addr.addr.sa_family, args);
	allio_TRY((auto&&, socket), socket_result);

	wsa_accept_address_buffer addr_buffer;
	if (DWORD const error = wsa_accept_ex(listen_socket, socket.socket.get(), addr_buffer, nullptr))
	{
		return allio_ERROR(static_cast<socket_error>(error));
	}

	static_cast<socket_address_union&>(addr) = addr_buffer.remote;
	addr.size = 0;

	return socket_result;
}


// The layout of WSABUF necessitates a copy.
template<typename T>
static size_t transform_wsa_buffers(basic_buffers<T> const buffers, WSABUF* const wsa_buffers)
{
	size_t packet_size = 0;
	std::transform(buffers.begin(), buffers.end(), wsa_buffers,
		[&](basic_buffer<T> const buffer) -> WSABUF
		{
			packet_size += buffer.size();
			return WSABUF
			{
				.len = static_cast<uint32_t>(buffer.size()),
				.buf = (CHAR*)buffer.data(),
			};
		}
	);
	return packet_size;
}

//TODO: Instead of allocating, perform multiple I/O calls.
result<void> allio::packet_scatter_read(io::parameters_with_result<io::packet_scatter_read> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	SOCKET const socket = unwrap_socket(h.get_platform_handle());

	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;

	size_t packets_transferred = 0;
	for (packet_read_descriptor const& descriptor : args.descriptors)
	{
		auto r = [&]() -> result<void>
		{
			read_buffers const buffers = descriptor.buffers;
			allio_TRY(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));

			(void)transform_wsa_buffers(buffers, wsa_buffers);

			socket_address_union addr;
			//WSACMSGHDR control_header;

			WSAMSG message =
			{
				.name = &addr.addr,
				.namelen = sizeof(addr),
				.lpBuffers = wsa_buffers,
				.dwBufferCount = static_cast<uint32_t>(buffers.size()),
				//.Control =
				//{
				//	.len = sizeof(control_header),
				//	.buf = (CHAR*)&control_header,
				//},
			};

			DWORD transferred;
			if (DWORD const error = wsa_recv_msg(socket, &message, &transferred))
			{
				return allio_ERROR(static_cast<socket_error>(error));
			}

			*descriptor.result =
			{
				.packet_size = transferred,
				.address = addr.get_network_address(),
			};

			return {};
		}();

		if (!r)
		{
			if (packets_transferred != 0)
			{
				break;
			}

			return r;
		}
	}
	*args.result = packets_transferred;

	return {};
}

result<void> allio::packet_gather_write(io::parameters_with_result<io::packet_gather_write> const& args)
{
	packet_socket_handle const& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	SOCKET const socket = unwrap_socket(h.get_platform_handle());

	detail::dynamic_buffer<WSABUF, 64> wsa_buffers_storage;

	size_t packets_transferred = 0;
	for (packet_write_descriptor const& descriptor : args.descriptors)
	{
		auto r = [&]() -> result<void>
		{
			write_buffers const buffers = descriptor.buffers;
			allio_TRY(wsa_buffers, wsa_buffers_storage.reserve(buffers.size()));
			allio_TRY(addr, socket_address::make(*descriptor.address));

			size_t const packet_size = transform_wsa_buffers(buffers, wsa_buffers);

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
				return allio_ERROR(static_cast<socket_error>(error));
			}

			//TODO: Is there any reason why a datagram sendmsg would transfer fewer bytes than requested?
			allio_ASSERT(transferred == packet_size);

			return {};
		}();

		if (!r)
		{
			if (packets_transferred != 0)
			{
				break;
			}

			return r;
		}
	}
	*args.result = packets_transferred;

	return {};
}
