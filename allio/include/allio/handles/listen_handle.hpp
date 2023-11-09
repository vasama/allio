#pragma once

#include <allio/detail/handles/listen_handle.hpp>

namespace allio {

using detail::listen_backlog_t;
using detail::listen_backlog;

using detail::listen_handle_t;
using detail::abstract_listen_handle;

using detail::raw_listen_handle_t;
using detail::abstract_raw_listen_handle;

namespace blocking { using namespace detail::_listen_blocking; }
namespace async { using namespace detail::_listen_async; }

} // namespace allio
