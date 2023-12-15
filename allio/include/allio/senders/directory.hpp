#pragma once

#include <allio/handles/directory.hpp>

namespace allio::senders {
inline namespace directory {

template<detail::multiplexer_handle_for<directory_t> MultiplexerHandle>
using basic_directory_handle = detail::sender_handle<directory_t, MultiplexerHandle>;

#include <allio/detail/handles/directory_io.ipp>

} // inline namespace directory
} // namespace allio::senders
