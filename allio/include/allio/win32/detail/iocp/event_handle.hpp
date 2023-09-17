#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

template<>
class handle_impl<iocp_multiplexer, _event_handle>
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using C = handle_impl;

	friend vsm::result<void> tag_invoke(attach_handle_t, M& m, H const& h, C& c);
	friend void tag_invoke(detach_handle_t, M& m, H const& h, C& c, error_handler* e);
};

template<>
class operation_impl<iocp_multiplexer, _event_handle, event_handle::wait_t>
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using C = handle_impl<M, H>;
	using O = H::wait_t;
	using S = operation_state<M, H, O>;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	friend vsm::result<void> tag_invoke(submit_io_t, M& m, H const& h, C const& c, S& s);
	friend std::error_code tag_invoke(reap_io_t, M& m, H const& h, C const& c, S& s);
	friend void tag_invoke(cancel_io_t, M& m, H const& h, C const& c, S& s, error_handler* e);
};

} // namespace allio::detail
