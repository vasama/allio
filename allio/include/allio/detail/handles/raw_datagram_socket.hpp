#pragma once

#include <allio/detail/handles/basic_datagram_socket.hpp>
#include <allio/detail/handles/platform_object.hpp>

#include <vsm/platform.h>

namespace allio::detail {

struct raw_datagram_socket_t : basic_datagram_socket_t<platform_object_t>
{
	using base_type = basic_datagram_socket_t<platform_object_t>;

	static vsm::result<void> bind(
		native_handle<raw_datagram_socket_t>& h,
		io_parameters_t<raw_datagram_socket_t, bind_t> const& args);

	static vsm::result<receive_result> receive_from(
		native_handle<raw_datagram_socket_t> const& h,
		io_parameters_t<raw_datagram_socket_t, receive_from_t> const& args);

	static vsm::result<void> send_to(
		native_handle<raw_datagram_socket_t> const& h,
		io_parameters_t<raw_datagram_socket_t, send_to_t> const& args);

	static vsm::result<void> close(
		native_handle<raw_datagram_socket_t>& h,
		io_parameters_t<raw_datagram_socket_t, close_t> const& args);
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/raw_datagram_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/raw_datagram_socket.hpp>
#endif
