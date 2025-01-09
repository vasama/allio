#pragma once

#include <allio/detail/handles/datagram_socket.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/linux/detail/socket.hpp>
#include <allio/detail/unique_socket.hpp>

namespace allio::detail {

struct datagram_header_storage
{
	alignas(8) unsigned char storage[56];
};

template<>
struct async_connector<io_uring_multiplexer, raw_datagram_socket_t>
	: io_uring_multiplexer::connector_type
{
};

template<>
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, socket_io::bind_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::bind_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::bind_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, socket_io::receive_from_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::receive_from_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::receive_from_t>;
	using R = receive_result;

	socket_address_storage address_storage;
	datagram_header_storage header_storage;

	static io_result<R> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<R> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, socket_io::send_to_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = raw_datagram_socket_t::native_type;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, socket_io::send_to_t>;
	using A = io_parameters_t<raw_datagram_socket_t, socket_io::send_to_t>;

	socket_address_storage address_storage;
	datagram_header_storage header_storage;

	static io_result<void> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H const& h, C const& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
