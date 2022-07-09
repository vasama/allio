#pragma once

#include <allio/detail/handles/directory.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, directory_t>
	: iocp_multiplexer::connector_type
{
};

template<>
struct async_operation<iocp_multiplexer, directory_t, directory_t::open_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<directory_t>;
	using C = async_connector_t<M, directory_t>;
	using S = async_operation_t<M, directory_t, directory_t::open_t>;
	using A = io_parameters_t<directory_t, directory_t::open_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);

	static io_result<void> notify(M&, H&, C&, S&, A const&, M::io_status_type)
	{
		vsm_unreachable();
	}

	static void cancel(M&, H const&, C const&, S&)
	{
	}
};

template<>
struct async_operation<iocp_multiplexer, directory_t, directory_t::read_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<directory_t> const;
	using C = async_connector_t<M, directory_t> const;
	using S = async_operation_t<M, directory_t, directory_t::read_t>;
	using A = io_parameters_t<directory_t, directory_t::read_t>;
	using R = directory_stream_view;

	iocp_multiplexer::io_status_block io_status_block;

	static io_result<R> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<R> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
