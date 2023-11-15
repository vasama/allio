#pragma once

#include <allio/detail/handles/process.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/generic_wait.hpp>

namespace allio::detail {

template<>
struct connector_impl<iocp_multiplexer, process_handle_t>
{
	using M = iocp_multiplexer;
	using H = process_handle_t;
	using C = connector_t<M, H>;

	friend vsm::result<void> tag_invoke(attach_handle_t, M&, H const&, C&)
	{
		// Process objects cannot be associated with an IOCP.
		// Instead waits are performed using wait packet objects.
		return {};
	}

	friend void tag_invoke(detach_handle_t, M&, H const&, C&)
	{
	}
};

template<>
struct operation_impl<iocp_multiplexer, process_handle_t, process_handle_t::wait_t>
{
	using M = iocp_multiplexer;
	using H = process_handle_t;
	using O = process_handle_t::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using R = io_result_ref_t<O>;

	wait_operation_state wait_state;

	static io_result submit(M& m, H const& h, C const& c, S& s, R r);
	static io_result notify(M& m, H const& h, C const& c, S& s, R r, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
