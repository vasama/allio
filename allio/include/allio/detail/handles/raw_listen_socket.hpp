#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/handles/basic_listen_socket.hpp>
#include <allio/detail/handles/raw_socket.hpp>

namespace allio::detail {

struct raw_listen_socket_t : basic_listen_socket_t<platform_object_t, raw_socket_t>
{
	using base_type = basic_listen_socket_t<platform_object_t, raw_socket_t>;

	static vsm::result<void> listen(
		native_handle<raw_listen_socket_t>& h,
		io_parameters_t<raw_listen_socket_t, listen_t> const& a);

	static vsm::result<accept_result<basic_detached_handle<raw_socket_t>>> accept(
		native_handle<raw_listen_socket_t> const& h,
		io_parameters_t<raw_listen_socket_t, accept_t> const& a);

	static vsm::result<void> close(
		native_handle<raw_listen_socket_t>& h,
		io_parameters_t<raw_listen_socket_t, close_t> const& a);
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/raw_listen_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/raw_listen_socket.hpp>
#endif
