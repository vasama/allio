#pragma once

#include <allio/detail/handles/opaque_object.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/generic_wait.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, _opaque_handle>
	: iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using H = platform_handle;
	using C = async_connector_t<M, H>;

	static vsm::result<void> attach(M&, H const&, C&)
	{
		// Opaque handles are not associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	static vsm::result<void> detach(M&, H const&, C&)
	{
	}
};

template<>
struct async_operation<iocp_multiplexer, _opaque_handle, _opaque_handle::poll_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using O = _opaque_handle::poll_t;
	using C = async_connector_t<M, H>;
	using S = async_operation_t<M, H, O>;

	wait_operation_state wait_state;

	static io_result submit(M& m, H const& h, C const&, S& s)
	{
		return wait_operation_state::submit(m, h, s.wait_state);
	}

	static io_result notify(M& m, H const& h, C const&, S& s, io_status const status)
	{
		return wait_operation_state::notify(m, h, s.wait_state, status);
	}

	static void cancel(M& m, H const& h, C const&, S& s)
	{
		return wait_operation_state::cancel(m, h, s.wait_state);
	}
};

} // namespace allio::detail
