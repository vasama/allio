#pragma once

#include <allio/detail/handles/datagram_socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>
#include <allio/win32/detail/wsa.hpp>

namespace allio::detail {

template<>
struct connector<iocp_multiplexer, raw_datagram_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct operation<iocp_multiplexer, raw_datagram_socket_t, raw_datagram_socket_t::bind_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t;
	using N = H::native_type;
	using O = H::bind_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_datagram_socket_t, raw_datagram_socket_t::send_to_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t;
	using N = H::native_type;
	using O = H::send_to_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;
	
	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<void> submit(M& m, N const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, N const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

template<>
struct operation<iocp_multiplexer, raw_datagram_socket_t, raw_datagram_socket_t::receive_from_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t;
	using N = H::native_type;
	using O = H::receive_from_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;
	using R = receive_result;

	wsa_buffers_storage<8> buffers;
	wsa_address_storage<116> address_storage;
	iocp_multiplexer::overlapped overlapped;

	static io_result<R> submit(M& m, N const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<R> notify(M& m, N const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

} // namespace allio::detail
