#pragma once

#include <allio/handles/event_handle.hpp>
#include <allio/win32/detail/iocp/multiplexer.hpp>

namespace allio::detail {

#if 0
template<>
class connector_impl<iocp_multiplexer, _event_handle>
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using C = connector_t<M, H>;

	friend vsm::result<void> tag_invoke(attach_handle_t, M& m, H const& h, C& c);
	friend void tag_invoke(detach_handle_t, M& m, H const& h, C& c, error_handler* e);
};
#endif

template<>
struct operation_impl<iocp_multiplexer, _event_handle, _event_handle::wait_t> : iocp_multiplexer::operation
{
	using M = iocp_multiplexer;
	using H = _event_handle;
	using O = _event_handle::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	win32::unique_wait_packet wait_packet;
	iocp_multiplexer::wait_slot wait_slot;

	static vsm::result<void> submit(M& m, H const& h, C const& c, S& s);
	static void cancel(M& m, H const& h, C const& c, S& s, error_handler* e);
	static std::error_code reap(M& m, H const& h, C const& c, S& s, io_result&& result);
};

} // namespace allio::detail
