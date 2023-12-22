#pragma once

#include <allio/detail/handles/raw_listen_socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, raw_listen_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, raw_listen_socket_t, listen_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<raw_listen_socket_t>;
	using C = async_connector_t<M, raw_listen_socket_t>;
	using S = async_operation_t<M, raw_listen_socket_t, listen_t>;
	using A = io_parameters_t<raw_listen_socket_t, listen_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_listen_socket_t, accept_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<raw_listen_socket_t> const;
	using C = async_connector_t<M, raw_listen_socket_t> const;
	using S = async_operation_t<M, raw_listen_socket_t, accept_t>;
	using A = io_parameters_t<raw_listen_socket_t, accept_t>;
	using R = accept_result<basic_attached_handle<raw_socket_t, basic_multiplexer_handle<M>>>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	wsa_address_storage<256> address_storage;
	iocp_multiplexer::overlapped overlapped;

	static io_result<R> submit(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<R> notify(M& m, H const& h, C const& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_listen_socket_t, close_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<raw_listen_socket_t>;
	using C = async_connector_t<M, raw_listen_socket_t>;
	using S = async_operation_t<M, raw_listen_socket_t, close_t>;
	using A = io_parameters_t<raw_listen_socket_t, close_t>;

	static io_result<void> submit(M&, H& h, C const&, S&, A const& a, io_handler<M>&)
	{
		return raw_listen_socket_t::close(h, a);
	}

	static io_result<void> notify(M&, H&, C const&, S&, A const&, M::io_status_type)
	{
		vsm_unreachable();
	}

	static void cancel(M&, H const&, C const&, S&)
	{
	}
};

} // namespace allio::detail
