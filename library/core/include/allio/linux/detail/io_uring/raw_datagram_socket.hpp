#pragma once

#include <allio/detail/handles/raw_datagram_socket.hpp>
#include <allio/linux/detail/io_uring/multiplexer.hpp>

#include <allio/detail/byte_io_buffers.hpp>
#include <allio/detail/unique_socket.hpp>
#include <allio/linux/detail/socket.hpp>

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
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, bind_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<raw_datagram_socket_t>;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, bind_t>;
	using A = io_parameters_t<raw_datagram_socket_t, bind_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, receive_from_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<raw_datagram_socket_t>;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, receive_from_t>;
	using A = io_parameters_t<raw_datagram_socket_t, receive_from_t>;
	using R = receive_result;

	socket_address_storage address_storage;
	new_io_buffers_storage buffers_storage;
	datagram_header_storage header_storage;

	static io_result<R> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<R> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<io_uring_multiplexer, raw_datagram_socket_t, send_to_t>
	: io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<raw_datagram_socket_t>;
	using C = async_connector_t<M, raw_datagram_socket_t>;
	using S = async_operation_t<M, raw_datagram_socket_t, send_to_t>;
	using A = io_parameters_t<raw_datagram_socket_t, send_to_t>;

	socket_address_storage address_storage;
	new_io_buffers_storage buffers_storage;
	datagram_header_storage header_storage;

	//TODO: Get rid of this once the address rework is done.
	uint32_t addr_size;

	static io_result<void> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
