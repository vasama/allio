#pragma once

#ifndef allio_detail_socket_api
#	error Include <allio/impl/posix_socket.hpp> instead.
#endif

#include <allio/win32/platform.hpp>

#include <system_error>

#ifndef _WINSOCKAPI_
#	define _WINSOCKAPI_
#endif

#include <win32.h>
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <afunix.h>

namespace allio {

using socket_type = SOCKET;
using socket_address_size_type = int;

static constexpr auto socket_error_value = SOCKET_ERROR;
static constexpr socket_type invalid_socket = INVALID_SOCKET;
static constexpr size_t unix_socket_max_path = UNIX_PATH_MAX;


enum class socket_error : int
{
	none = 0,
};

class socket_error_category : public std::error_category
{
public:
	char const* name() const noexcept override;
	std::string message(int code) const override;

	static socket_error_category const& get()
	{
		return instance;
	}

private:
	static socket_error_category const instance;
};

inline std::error_code make_error_code(socket_error const error)
{
	return std::error_code(static_cast<int>(error), socket_error_category::get());
}

inline socket_error get_last_socket_error()
{
	return static_cast<socket_error>(WSAGetLastError());
}

} // namespace allio

template<>
struct std::is_error_code_enum<allio::socket_error>
{
	static constexpr bool value = true;
};

namespace allio {

inline native_platform_handle wrap_socket(SOCKET const socket)
{
	return win32::wrap_handle<SOCKET>(socket);
}

inline SOCKET unwrap_socket(native_platform_handle const socket)
{
	return win32::unwrap_handle<SOCKET>(socket);
}


inline vsm::result<void> close_socket(socket_type const socket)
{
	if (closesocket(socket))
	{
		return vsm::unexpected(get_last_socket_error());
	}
	return {};
}

} // namespace allio
