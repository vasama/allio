#pragma once

#include <allio/detail/handles/listen_socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct connector<iocp_multiplexer, raw_listen_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct operation<iocp_multiplexer, raw_listen_socket_t, raw_listen_socket_t::listen_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_listen_socket_t;
	using O = H::listen_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_listen_socket_t, raw_listen_socket_t::accept_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_listen_socket_t;
	using O = H::accept_t;
	using N = H::native_type const;
	using C = connector_t<M, H> const;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;
	using R = accept_result<async_handle<typename H::socket_object_type, basic_multiplexer_handle<M>>>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	wsa_address_storage<256> address_storage;
	iocp_multiplexer::overlapped overlapped;

	static io_result<R> submit(M& m, N const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<R> notify(M& m, N const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_listen_socket_t, raw_listen_socket_t::close_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_listen_socket_t;
	using O = H::close_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	static io_result<void> submit(M& m, N& h, C const& c, S& s, A const& args, io_handler<M>& handler)
	{
		return H::close(h, args);
	}

	static io_result<void> notify(M& m, N& h, C const& c, S& s, A const& args, M::io_status_type status)
	{
		vsm_unreachable();
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
	}
};

} // namespace allio::detail
