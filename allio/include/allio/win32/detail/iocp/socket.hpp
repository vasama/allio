#pragma once

#include <allio/detail/handles/socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>
#include <allio/win32/detail/wsa.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, raw_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, raw_socket_t, socket_io::connect_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t::native_type;
	using C = async_connector_t<M, raw_socket_t>;
	using S = async_operation_t<M, raw_socket_t, socket_io::connect_t>;
	using A = io_parameters_t<raw_socket_t, socket_io::connect_t>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	iocp_multiplexer::overlapped overlapped;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_socket_t, byte_io::stream_read_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t::native_type;
	using C = async_connector_t<M, raw_socket_t>;
	using S = async_operation_t<M, raw_socket_t, byte_io::stream_read_t>;
	using A = io_parameters_t<raw_socket_t, byte_io::stream_read_t>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_socket_t, byte_io::stream_write_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t::native_type;
	using C = async_connector_t<M, raw_socket_t>;
	using S = async_operation_t<M, raw_socket_t, byte_io::stream_write_t>;
	using A = io_parameters_t<raw_socket_t, byte_io::stream_write_t>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_socket_t, close_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t::native_type;
	using C = async_connector_t<M, raw_socket_t>;
	using S = async_operation_t<M, raw_socket_t, close_t>;
	using A = io_parameters_t<raw_socket_t, close_t>;

	static io_result<void> submit(M&, H& h, C const&, S&, A const& a, io_handler<M>&)
	{
		return raw_socket_t::close(h, a);
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
