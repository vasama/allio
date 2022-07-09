#pragma once

#include <allio/handles/process.hpp>

namespace allio::senders {
inline namespace process {

template<detail::multiplexer_handle_for<process_t> MultiplexerHandle>
using basic_process_handle = detail::sender_handle<process_t, MultiplexerHandle>;

#include <allio/detail/handles/process_io.ipp>

} // inline namespace process
} // namespace allio::senders
