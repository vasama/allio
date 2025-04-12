#pragma once

#include <allio/blocking/traits.hpp>
#include <allio/handles/process.hpp>
#include <allio/path.hpp>

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

namespace this_process {

using namespace detail::_this_process;

[[nodiscard]] size_t get_executable_path(any_path_buffer const buffer)
{
	return detail::_get_current_executable_path<traits_type>(buffer);
}

template<typename Path = path>
[[nodiscard]] Path get_executable_path()
{
	return detail::_get_current_executable_path<Path, traits_type>();
}

[[nodiscard]] process_handle open(auto&&... args)
{
	return detail::open_process<traits_type>(get_id(), vsm_forward(args)...);
}

} // namespace this_process
} // namespace allio::blocking
