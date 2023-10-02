#pragma once

#include <allio/detail/handles/directory_handle.hpp>

namespace allio {

using detail::directory_entry;
using detail::directory_entry_view;
using detail::directory_stream_view;
using detail::directory_stream_buffer;


using directory_handle = basic_blocking_handle<detail::_directory_handle>;

template<typename Multiplexer>
using basic_directory_handle = basic_async_handle<detail::_directory_handle, Multiplexer>;


namespace this_process {

vsm::result<size_t> get_current_directory(output_path_ref output);

template<typename Path = path>
vsm::result<Path> get_current_directory()
{
	Path path = {};
	vsm_try_discard(get_current_directory(path));
	return static_cast<Path&&>(path);
}

vsm::result<void> set_current_directory(input_path_view path);

vsm::result<directory_handle> open_current_directory();

} // namespace this_process
} // namespace allio
