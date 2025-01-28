#pragma once

#include <allio/handles/process.hpp>
#include <allio/nothrow/traits.hpp>

namespace allio::nothrow {
inline namespace processes {

using process_handle = traits_type::handle<process_t>;

} // inline namespace processes

namespace this_process {

using namespace detail::_this_process;

[[nodiscard]] vsm::result<process_handle> open(auto&&... args)
{
	return detail::open_process<traits_type>(get_id(), vsm_forward(args)...);
}

} // namespace this_process
} // namespace allio::nothrow
