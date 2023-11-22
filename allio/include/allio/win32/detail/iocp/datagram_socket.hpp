#pragma once

#include <allio/detail/handles/datagram_socket.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>
#include <allio/win32/detail/wsa.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, raw_datagram_socket_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, raw_datagram_socket_t, socket_io::bind_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::bind_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::bind_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_datagram_socket_t, socket_io::receive_from_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::receive_from_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::receive_from_t>;
	using R = receive_result;

	wsa_buffers_storage<8> buffers;
	wsa_address_storage<116> address_storage;
	iocp_multiplexer::overlapped overlapped;

	static io_result<R> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<R> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, raw_datagram_socket_t, socket_io::send_to_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::send_to_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::send_to_t>;

	wsa_buffers_storage<8> buffers;
	iocp_multiplexer::overlapped overlapped;

	static io_result<void> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
