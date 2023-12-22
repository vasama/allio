#pragma once

#include <allio/blocking.hpp>
#include <allio/handles/process.hpp>

namespace allio::blocking {
inline namespace processes {

using process_handle = traits_type::handle<process_t>;

[[nodiscard]] process_handle open_process(process_id const id, auto&&... args)
{
	return detail::open_process<traits_type>(id, vsm_forward(args)...);
}

[[nodiscard]] process_handle create_process(detail::fs_path const& path, auto&&... args)
{
	return detail::create_process<traits_type>(path, vsm_forward(args)...);
}

} // inline namespace processes
} // namespace allio::blocking
