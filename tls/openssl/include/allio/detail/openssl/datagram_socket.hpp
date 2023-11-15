#pragma once

#include <allio/detail/handles/datagram_socket.hpp>

namespace allio::detail {

struct openssl_context;

struct openssl_datagram_socket_t : basic_datagram_socket_t<object_t>
{
	using base_type = basic_datagram_socket_t<object_t>;

	struct native_type : raw_datagram_socket_t::native_type
	{
		openssl_context* context;
	};

	static vsm::result<void> blocking_io(
		close_t,
		native_type& h,
		io_parameters_t<close_t> const& args);

	static vsm::result<void> blocking_io(
		bind_t,
		native_type& h,
		io_parameters_t<bind_t> const& args);

	static vsm::result<void> blocking_io(
		send_to_t,
		native_type const& h,
		io_parameters_t<send_to_t> const& args);

	static vsm::result<void> blocking_io(
		receive_from_t,
		native_type const& h,
		io_parameters_t<receive_from_t> const& args);
};

} // namespace allio::detail
