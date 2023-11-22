#pragma once

#include <allio/detail/handles/process.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

#include <allio/win32/detail/iocp/generic_wait.hpp>

namespace allio::detail {

template<>
struct async_connector<iocp_multiplexer, process_handle_t>
	: iocp_multiplexer::connector_type
{
	using M = iocp_multiplexer;
	using H = process_handle_t;
	using C = async_connector_t<M, H>;

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
struct async_operation<iocp_multiplexer, process_handle_t, process_handle_t::wait_t>
	: iocp_multiplexer::operation_type
{
	using M = iocp_multiplexer;
	using H = process_handle_t;
	using O = process_handle_t::wait_t;
	using C = async_connector_t<M, H>;
	using S = async_operation_t<M, H, O>;
	using R = io_result_ref_t<O>;

	wait_operation_state wait_state;

	static io_result submit(M& m, H const& h, C const& c, S& s, R r);
	static io_result notify(M& m, H const& h, C const& c, S& s, R r, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
