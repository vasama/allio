#pragma once

#include <allio/detail/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/wait.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, event_t>
{
	using M = iocp_multiplexer;
	using H = event_t;
	using N = H::native_type;
	using C = connector_t<M, H>;

	friend vsm::result<void> tag_invoke(attach_handle_t, M&, N const&, C&)
	{
		// Event objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	friend vsm::result<void> tag_invoke(detach_handle_t, M&, N const&, C&)
	{
		return {};
	}
};

template<>
struct operation_impl<iocp_multiplexer, event_t, event_t::wait_t>
{
	using M = iocp_multiplexer;
	using H = event_t;
	using N = H::native_type;
	using O = H::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	iocp_wait_state wait_state;

	static io_result2<void> submit(M& m, N const& h, C const& c, S& s);
	static io_result2<void> notify(M& m, N const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, N const& h, C const& c, S& s);
};

} // namespace allio::detail
