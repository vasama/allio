#pragma once

#include <allio/detail/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, _event_handle>
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using C = connector_t<M, H>;

	friend vsm::result<void> tag_invoke(attach_handle_t, M&, H const&, C&)
	{
		// Event objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	friend void tag_invoke(detach_handle_t, M&, H const&, C&)
	{
	}
};

template<>
struct operation_impl<iocp_multiplexer, _event_handle, _event_handle::wait_t>
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using O = _event_handle::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	static io_result submit(M& m, H const& h, C const& c, S& s);
	static io_result notify(M& m, H const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
