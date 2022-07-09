#include "../posix_socket.hpp"

#include "wsa_ex.hpp"

#pragma comment(lib, "ws2_32.lib")

using namespace allio;
using namespace allio::win32;

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
		return allio_ERROR(std::error_code(init.error, wsa_error_category::get()));
	}

	return {};
}

result<unique_socket> allio::create_socket(int const address_family, flags const handle_flags)
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

	if ((handle_flags & allio::flags::multiplexable) != allio::flags::none)
	{
		flags |= WSA_FLAG_OVERLAPPED;
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

	return { result_value, socket };
}

result<unique_socket> allio::accept_socket(socket_type const listen_socket, socket_address& addr, socket_parameters const& create_args)
{
	allio_TRY(listen_addr, socket_address::get(listen_socket));
	auto socket_result = create_socket(listen_addr.addr.sa_family, create_args.handle_flags);
	allio_TRY((auto&&, socket), socket_result);

	wsa_accept_address_buffer addr_buffer;
	if (DWORD const error = wsa_accept_ex(listen_socket, socket.get(), addr_buffer, nullptr))
	{
		return allio_ERROR(std::error_code(error, wsa_error_category::get()));
	}

	static_cast<socket_address_union&>(addr) = addr_buffer.remote;
	addr.size = 0;

	return socket_result;
}
