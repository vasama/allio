#pragma once

#include <allio/detail/handles/event.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/wait.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, event_t>
	: iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using H = native_handle<event_t>;
	using C = async_connector_t<M, event_t>;

	static vsm::result<void> attach(M&, H const&, C&)
	{
		// Event objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	static vsm::result<void> detach(M&, H const&, C&)
	{
		return {};
	}
};

template<>
struct async_operation<iocp_multiplexer, event_t, event_t::wait_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = native_handle<event_t> const;
	using C = async_connector_t<M, event_t> const;
	using S = async_operation_t<M, event_t, event_t::wait_t>;
	using A = io_parameters_t<event_t, event_t::wait_t>;

	iocp_wait_state wait_state;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, H& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
