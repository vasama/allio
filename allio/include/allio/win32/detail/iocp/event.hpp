#pragma once

#include <allio/detail/handles/event.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/wait.hpp>

namespace allio::detail {

template<>
struct connector<iocp_multiplexer, event_t>
	: iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using H = event_t;
	using N = H::native_type;
	using C = connector_t<M, H>;

	static vsm::result<void> attach(M&, N const&, C&)
	{
		// Event objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	static vsm::result<void> detach(M&, N const&, C&)
	{
		return {};
	}
};

template<>
struct operation<iocp_multiplexer, event_t, event_t::wait_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = event_t;
	using O = H::wait_t;
	using N = H::native_type const;
	using C = connector_t<M, H> const;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	iocp_wait_state wait_state;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler);
	static io_result<void> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

} // namespace allio::detail
