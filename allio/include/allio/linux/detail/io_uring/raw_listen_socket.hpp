#pragma once

#include <allio/detail/handles/listen_socket.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/detail/socket.hpp>

namespace allio::detail {

template<>
struct async_connector<io_uring_multiplexer, raw_listen_socket_t>
	: io_uring_multiplexer::connector_type
{
};

template<>
struct async_operation<io_uring_multiplexer, raw_listen_socket_t, socket_io::listen_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_listen_socket_t::native_type;
	using C = async_connector_t<M, raw_listen_socket_t>;
	using S = async_operation_t<M, raw_listen_socket_t, socket_io::listen_t>;
	using A = io_parameters_t<raw_listen_socket_t, socket_io::listen_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_listen_socket_t, socket_io::accept_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_listen_socket_t::native_type const;
	using C = async_connector_t<M, raw_listen_socket_t> const;
	using S = async_operation_t<M, raw_listen_socket_t, socket_io::accept_t>;
	using A = io_parameters_t<raw_listen_socket_t, socket_io::accept_t>;
	using R = accept_result<async_handle<raw_listen_socket_t::socket_object_type, basic_multiplexer_handle<M>>>;

	socket_address_storage addr_storage;
	int32_t addr_size;

	static io_result<R> submit(M& m, H const& h, C const& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<R> notify(M& m, H const& h, C const& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_listen_socket_t, close_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_listen_socket_t::native_type;
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
