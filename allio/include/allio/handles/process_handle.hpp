#pragma once

#include <allio/detail/handles/process_handle.hpp>

namespace allio {

using detail::process_id;
using detail::process_exit_code;


using process_handle = basic_blocking_handle<detail::_process_handle>;

template<typename Multiplexer>
using basic_process_handle = basic_async_handle<detail::_process_handle, Multiplexer>;


template<parameters<process_handle::open_parameters> P = process_handle::open_parameters::interface>
vsm::result<process_handle> open_process(process_id const id, P const& args = {})
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->open(id, args));
	return r;
}

template<parameters<process_handle::launch_parameters> P = process_handle::launch_parameters::interface>
vsm::result<process_handle> launch_process(path_descriptor const path, P const& args = {})
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->launch(path, args));
	return r;
}


namespace this_process {

process_handle const& get_handle();

} // namespace this_process
} // namespace allio
