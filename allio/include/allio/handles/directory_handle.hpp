#pragma once

#include <allio/detail/handles/directory.hpp>

namespace allio {

using detail::directory_entry;
using detail::directory_entry_view;
using detail::directory_stream_view;
using detail::directory_stream_buffer;

using detail::directory_t;

namespace this_process {

vsm::result<size_t> get_current_directory(any_path_buffer buffer);

template<typename Path = path>
vsm::result<Path> get_current_directory()
{
	vsm::result<Path> r(vsm::result_value);
	vsm_try_discard(get_current_directory(*r));
	return r;
}

vsm::result<void> set_current_directory(path_descriptor path);

vsm::result<directory_handle> open_current_directory();

} // namespace this_process

namespace blocking { using namespace _directory_blocking; }
namespace async { using namespace _directory_async; }

} // namespace allio
