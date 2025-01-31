#pragma once

#include <allio/handles/pipe.hpp>
#include <allio/senders/traits.hpp>

namespace allio::senders {
inline namespace pipe {

template<detail::multiplexer_handle_for<pipe_t> MultiplexerHandle>
using basic_pipe_handle = traits_type::handle<pipe_t, MultiplexerHandle>;

template<detail::multiplexer_handle_for<pipe_t> MultiplexerHandle>
using basic_pipe_pair = detail::basic_pipe_pair<basic_pipe_handle<MultiplexerHandle>>;

[[nodiscard]] detail::ex::sender auto create_pipe(auto&&... args)
{
	return detail::create_pipe<traits_type>(vsm_forward(args)...);
}

} // inline namespace pipe
} // namespace allio::senders
