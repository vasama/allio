#pragma once

#include <allio/detail/handles/listen_socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, raw_listen_socket_t>
{
};

template<>
struct operation_impl<iocp_multiplexer, raw_listen_socket_t, raw_listen_socket_t::listen_t>
{
	using M = iocp_multiplexer;
	using H = raw_listen_socket_t;
	using N = H::native_type;
	using O = H::listen_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	static io_result2<void> submit(M& m, N& h, C& c, S& s);
	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation_impl<iocp_multiplexer, raw_listen_socket_t, raw_listen_socket_t::accept_t>
{
	using M = iocp_multiplexer;
	using H = raw_listen_socket_t;
	using N = H::native_type;
	using O = H::accept_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using R = accept_result<async_handle<typename H::socket_object, basic_multiplexer_handle<M>>>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	wsa_address_storage<256> address_storage;
	iocp_multiplexer::overlapped overlapped;

	static io_result2<R> submit(M& m, N const& h, C const& c, S& s);
	static io_result2<R> notify(M& m, N const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

} // namespace allio::detail