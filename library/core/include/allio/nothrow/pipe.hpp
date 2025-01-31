#pragma once

#include <allio/nothrow/traits.hpp>
#include <allio/handles/pipe.hpp>

namespace allio::nothrow {
inline namespace pipe {

using pipe_handle = traits_type::handle<pipe_t>;
using pipe_handle_pair = detail::basic_pipe_pair<pipe_handle>;

[[nodiscard]] vsm::result<pipe_handle_pair> create_pipe(auto&&... args)
{
	return detail::create_pipe<traits_type>(vsm_forward(args)...);
}

} // inline namespace pipe
} // namespace allio::nothrow
