#include <allio/linux/detail/epoll/event.hpp>

#include <allio/impl/linux/handles/event.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

using M = epoll_multiplexer;
using H = event_t;
using C = async_connector_t<M, H>;

using wait_t = event_t::wait_t;
using wait_s = async_operation_t<M, H, wait_t>;

io_result operation<M, H, wait_t>::submit(M& m, H const& h, C const& c, wait_s& s, wait_r const r)
{
	using flags_t = epoll_multiplexer::subscription_flags;

	return helper([&]() -> io_result
	{
		flags_t flags = flags_t::read;

		if (is_auto_reset(h))
		{
			vsm_try(was_non_zero, reset_event(unwrap_handle(h.get_platform_handle())));

			if (was_non_zero)
			{
				return std::error_code();
			}

			// In auto reset mode there is no point in notifying multiple
			// subscribers, so the subscription can be made exclusive.
			flags |= flags_t::exclusive;
		}

		// Start polling the event handle.
		vsm_try_void(m.subscribe(c, s.subscription, flags));

		return std::nullopt;
	});
}

io_result operation<M, H, wait_t>::notify(M& m, H const& h, C const& c, wait_s& s, wait_r const r, io_status const status)
{
	return helper([&]() -> io_result
	{
		epoll_multiplexer::subscription_guard guard(m, s.subscription);

		if (is_auto_reset(h))
		{
			vsm_try(was_non_zero, reset_event(unwrap_handle(h.get_platform_handle())));
	
			if (!was_non_zero)
			{
				vsm_try_void(guard.resubscribe());

				return std::nullopt;
			}
		}

		return std::error_code();
	});
}

void operation<M, H, wait_t>::cancel(M& m, H const& h, C const& c, wait_s& s)
{
	m.unsubscribe(c, s.subscription);
}
