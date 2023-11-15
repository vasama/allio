#pragma once

#include <allio/detail/handles/event.hpp>
#include <allio/linux/detail/epoll/multiplexer.hpp>

namespace allio::detail {

template<>
struct operation_impl<epoll_multiplexer, event_t, event_t::wait_t>
{
	using M = epoll_multiplexer;
	using H = event_t;
	using O = event_t::wait_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;

	epoll_multiplexer::subscription subscription;

	static io_result2 submit(M& m, H const& h, C const& c, S& s);
	static io_result2 notify(M& m, H const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
