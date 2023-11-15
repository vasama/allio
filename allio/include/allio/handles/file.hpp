#pragma once

#include <allio/detail/handles/file.hpp>

namespace allio {

using detail::file_t;

namespace blocking { using namespace _file_blocking; }
namespace async { using namespace _file_async; }

#if 0
using file_handle = basic_blocking_handle<detail::_file_handle>;

template<typename Multiplexer>
using basic_file_handle = basic_async_handle<detail::_file_handle, Multiplexer>;


template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_file(path_descriptor const path, P const& args = {})
{
	vsm::result<file_handle> r(vsm::result_value);
	vsm_try_void(r->open(path, args));
	return r;
}

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_unique_file(filesystem_handle const& base, P const& args = {});

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_temporary_file(P const& args = {});

template<parameters<file_handle::open_parameters> P = file_handle::open_parameters::interface>
vsm::result<file_handle> open_anonymous_file(filesystem_handle const& base, P const& args = {});
#endif

} // namespace allio
