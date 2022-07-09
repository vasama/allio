#pragma once

#include <allio/handles/file.hpp>

namespace allio::senders {
inline namespace file {

template<detail::multiplexer_handle_for<file_t> MultiplexerHandle>
using basic_file_handle = detail::sender_handle<file_t, MultiplexerHandle>;

#include <allio/detail/handles/file_io.ipp>

} // inline namespace file
} // namespace allio::senders
