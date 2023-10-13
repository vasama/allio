#pragma once

#include <allio/detail/handles/socket_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, raw_socket_handle_t>
{
};

template<>
struct operation_impl<iocp_multiplexer, raw_socket_handle_t, raw_socket_handle_t::connect_t>
{
	using M = iocp_multiplexer;
	using H = raw_socket_handle_t;
	using O = raw_socket_handle_t::connect_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	unique_wrapped_socket socket;
	iocp_multiplexer::overlapped overlapped;

	static io_result2<void> submit(M& m, H& h, C const& c, S& s);
	static io_result2<void> notify(M& m, H& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
