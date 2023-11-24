#pragma once

#include <allio/detail/handles/directory.hpp>

namespace allio {

using detail::directory_entry;
using detail::directory_entry_view;
using detail::directory_stream_view;

using detail::directory_t;
using detail::abstract_directory_handle;

namespace blocking { using namespace detail::_directory_b; }
namespace async { using namespace detail::_directory_a; }


namespace this_process {

vsm::result<size_t> get_current_directory(any_path_buffer buffer);

template<typename Path = path>
vsm::result<Path> get_current_directory()
{
	vsm::result<Path> r(vsm::result_value);
	vsm_try_discard(get_current_directory(*r));
	return r;
}

vsm::result<void> set_current_directory(detail::fs_path path);

vsm::result<blocking::directory_handle> open_current_directory();

} // namespace this_process

} // namespace allio
