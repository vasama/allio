#pragma once

#include <allio/detail/handles/basic_socket.hpp>
#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

struct raw_socket_t : basic_socket_t<platform_object_t>
{
	using base_type = basic_socket_t<platform_object_t>;

	static vsm::result<void> connect(
		native_handle<raw_socket_t>& h,
		io_parameters_t<raw_socket_t, connect_t> const& args);

	static vsm::result<void> disconnect(
		native_handle<raw_socket_t>& h,
		io_parameters_t<raw_socket_t, disconnect_t> const& args);

	static vsm::result<size_t> stream_read(
		native_handle<raw_socket_t> const& h,
		io_parameters_t<raw_socket_t, stream_read_t> const& args);

	static vsm::result<size_t> stream_write(
		native_handle<raw_socket_t> const& h,
		io_parameters_t<raw_socket_t, stream_write_t> const& args);

	static vsm::result<void> close(
		native_handle<raw_socket_t>& h,
		io_parameters_t<raw_socket_t, close_t> const& args);
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/raw_socket.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/raw_socket.hpp>
#endif
