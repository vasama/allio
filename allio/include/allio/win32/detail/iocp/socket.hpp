#pragma once

#include <allio/detail/handles/socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>
#include <allio/win32/detail/wsa.hpp>

#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

template<>
struct connector<iocp_multiplexer, raw_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct operation<iocp_multiplexer, raw_socket_t, raw_socket_t::connect_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using O = H::connect_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	unique_wrapped_socket socket;
	handle_flags socket_flags;
	iocp_multiplexer::overlapped overlapped;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_socket_t, raw_socket_t::read_some_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using O = H::read_some_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<size_t> submit(M& m, N const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, N const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_socket_t, raw_socket_t::write_some_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
	using O = H::write_some_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<size_t> submit(M& m, N const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, N const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_socket_t, raw_socket_t::close_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_socket_t;
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
