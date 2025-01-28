#pragma once

#include <allio/detail/handle.hpp>
#include <allio/detail/senders/get_multiplexer.hpp>

#include <vsm/utility.hpp>

#include <exec/env.hpp>

namespace allio {

template<typename Multiplexer>
[[nodiscard]] auto via(Multiplexer&& multiplexer)
{
	return exec::write(exec::make_env(exec::with(
		detail::get_multiplexer,
		detail::multiplexer_handle_t<std::remove_cvref_t<Multiplexer>>(vsm_forward(multiplexer)))));
}

} // namespace allio
