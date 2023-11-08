#pragma once

#include <allio/detail/get_multiplexer.hpp>
#include <allio/detail/handle.hpp>

#include <vsm/utility.hpp>

#include <exec/env.hpp>

namespace allio {

template<typename Multiplexer>
auto via(Multiplexer&& multiplexer)
{
	return exec::write(exec::make_env(exec::with(
		detail::get_multiplexer,
		detail::multiplexer_handle_t<Multiplexer>(vsm_forward(multiplexer)))));
}

} // namespace allio
