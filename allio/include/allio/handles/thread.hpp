#pragma once

#include <allio/detail/handles/thread.hpp>

namespace allio {

using detail::thread_id;
using detail::thread_exit_code;

using detail::thread_t;
using detail::abstract_thread_handle;

namespace blocking { using namespace detail::_thread_b; }
namespace async { using namespace detail::_thread_a; }

namespace this_thread {

thread_id get_id();
blocking::thread_handle const& get_handle();
vsm::result<blocking::thread_handle> open();

} // namespace this_thread
} // namespace allio