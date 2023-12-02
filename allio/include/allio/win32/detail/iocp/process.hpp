#pragma once

#include <allio/detail/handles/process.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/wait.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, process_t>
	: iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using H = process_t::native_type;
	using C = async_connector_t<M, process_t>;

	static vsm::result<void> attach(M&, H const&, C&)
	{
		// Process objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	static vsm::result<void> detach(M&, H const&, C&)
	{
	}
};

template<>
struct async_operation<iocp_multiplexer, process_t, process_io::wait_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = process_t::native_type const;
	using C = async_connector_t<M, process_t> const;
	using S = async_operation_t<M, process_t, process_io::wait_t>;
	using A = io_parameters_t<process_t, process_io::wait_t>;

	iocp_wait_state wait_state;

	static io_result<process_exit_code> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler);
	static io_result<process_exit_code> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
