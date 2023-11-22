#pragma once

#include <allio/detail/handles/timer.hpp>

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio {

namespace blocking {

using timer_handle = handle<timer_t>;

vsm::result<timer_handle> create_timer(auto&&... args)
{
	vsm::result<timer_handle> r(vsm::result_value);
	vsm_try_void(detail::blocking_io<timer_t::create_t>(
		*r,
		make_io_args<timer_t::create_t>()(vsm_forward(args)...)));
	return r;
}

} // namespace blocking

namespace async {

template<typename MultiplexerHandle>
using basic_timer_handle = handle<timer_t, MultiplexerHandle>;

ex::sender auto create_timer(auto&&... args)
{
	return io_handle_sender<timer_t, timer_t::create_t>(
		make_io_args<timer_t::create_t>()(vsm_forward(args)...));
}

} // namespace async
} // namespace allio