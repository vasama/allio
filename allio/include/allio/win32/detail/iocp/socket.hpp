#pragma once

#include <allio/detail/handles/socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>
#include <allio/win32/detail/wsa.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, raw_socket_t>
{
};

template<>
struct operation_impl<iocp_multiplexer, raw_socket_t, raw_socket_t::connect_t>
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using N = H::native_type;
	using O = H::connect_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	iocp_multiplexer::overlapped overlapped;

	static io_result2<void> submit(M& m, N& h, C& c, S& s);
	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation_impl<iocp_multiplexer, raw_socket_t, raw_socket_t::read_some_t>
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using N = H::native_type;
	using O = H::read_some_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result2<size_t> submit(M& m, N const& h, C const& c, S& s);
	static io_result2<size_t> notify(M& m, N const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation_impl<iocp_multiplexer, raw_socket_t, raw_socket_t::write_some_t>
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using N = H::native_type;
	using O = H::write_some_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result2<size_t> submit(M& m, N const& h, C const& c, S& s);
	static io_result2<size_t> notify(M& m, N const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

} // namespace allio::detail