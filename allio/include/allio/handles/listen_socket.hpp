#pragma once

#include <allio/deadline.hpp>
#include <allio/detail/handles/listen_socket.hpp>

namespace allio {

using detail::listen_backlog_t;
using detail::listen_backlog;

using detail::listen_socket_t;
using detail::abstract_listen_handle;

using detail::raw_listen_socket_t;
using detail::abstract_raw_listen_handle;

namespace blocking { using namespace detail::_listen_b; }
namespace async { using namespace detail::_listen_a; }

} // namespace allio
