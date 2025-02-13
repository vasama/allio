#pragma once

#include <allio/detail/handles/pipe.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/byte_io.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, pipe_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, pipe_t, byte_io::stream_read_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<pipe_t>;
	using C = async_connector_t<M, pipe_t>;
	using S = async_operation_t<M, pipe_t, byte_io::stream_read_t>;
	using A = io_parameters_t<pipe_t, byte_io::stream_read_t>;

	iocp_byte_io_state byte_io;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<>
struct async_operation<iocp_multiplexer, pipe_t, byte_io::stream_write_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<pipe_t>;
	using C = async_connector_t<M, pipe_t>;
	using S = async_operation_t<M, pipe_t, byte_io::stream_write_t>;
	using A = io_parameters_t<pipe_t, byte_io::stream_write_t>;

	iocp_byte_io_state byte_io;

	static io_result<size_t> submit(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<size_t> notify(M& m, H const& h, C const& c, S& s, A const& args, io_handler<M>& handler, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

#if 0 //TODO: This is no longer needed. Handles are overlapped by default.
template<>
struct allio::detail::async_operation<
	iocp_multiplexer,
	allio::detail::pipe_pair_t,
	allio::detail::pipe_pair_t::create_pair_t>
	: async_create_pipe_pair<iocp_multiplexer>
{
	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a_ref, io_handler<M>& handler)
	{
		A a = a_ref;

		a.read_pipe.flags |= io_flags::create_non_blocking;
		a.write_pipe.flags |= io_flags::create_non_blocking;

		return async_create_pipe_pair::submit(m, h, c, s, a, handler);
	}
};
#endif

} // namespace allio::detail
