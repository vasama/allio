#pragma once

#include <allio/detail/handles/stream_socket_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct operation_impl<iocp_multiplexer, _stream_socket_handle, _stream_socket_handle::connect_t>
{
	using M = iocp_multiplexer;
	using H = _stream_socket_handle;
	using O = _stream_socket_handle::connect_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using R = io_result_ref_t<O>;

	unique_wrapped_socket socket;
	iocp_multiplexer::overlapped overlapped;

	static io_result submit(M& m, H const& h, C const& c, S& s, R r);
	static io_result notify(M& m, H const& h, C const& c, S& s, R r, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
