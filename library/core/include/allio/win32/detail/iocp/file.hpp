#pragma once

#include <allio/detail/handles/file.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/byte_io.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, file_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, file_t, byte_io::random_read_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<file_t>;
	using C = async_connector_t<M, file_t>;
	using S = async_operation_t<M, file_t, byte_io::random_read_t>;
	using A = io_parameters_t<file_t, byte_io::random_read_t>;

	iocp_byte_io_state byte_io;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, file_t, byte_io::random_write_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<file_t>;
	using C = async_connector_t<M, file_t>;
	using S = async_operation_t<M, file_t, byte_io::random_write_t>;
	using A = io_parameters_t<file_t, byte_io::random_write_t>;

	iocp_byte_io_state byte_io;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
