#pragma once

#include <allio/detail/handles/datagram_socket_base.hpp>
#include <allio/detail/handles/raw_common_socket_base.hpp>

#include <vsm/platform.h>

namespace allio::detail {

struct raw_datagram_socket_t : datagram_socket_base_t<raw_common_socket_base_t>
{
	using base_type = datagram_socket_base_t<raw_common_socket_base_t>;

	using security_context_type = void;

	static byte_io_limits get_byte_io_limits(native_handle<raw_datagram_socket_t> const& h);

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
