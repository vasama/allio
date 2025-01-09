#pragma once

#include <allio/detail/handles/event.hpp>
#include <allio/linux/detail/epoll/multiplexer.hpp>

namespace allio::detail {

template<>
struct async_operation<epoll_multiplexer, event_t, event_t::wait_t>
	: epoll_multiplexer::operation_type
{
	using M = epoll_multiplexer;
	using H = event_t;
	using O = event_t::wait_t;
	using C = async_connector_t<M, H>;
	using S = async_operation_t<M, H, O>;

	epoll_multiplexer::subscription subscription;

	static io_result submit(M& m, H const& h, C const& c, S& s);
	static io_result notify(M& m, H const& h, C const& c, S& s, io_status status);
	static void cancel(M& m, H const& h, C const& c, S& s);
};

} // namespace allio::detail
