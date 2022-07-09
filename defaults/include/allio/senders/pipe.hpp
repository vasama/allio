#pragma once

#include <allio/handles/pipe.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace pipe {

template<detail::multiplexer_handle_for<pipe_t> MultiplexerHandle>
using basic_pipe_handle = detail::sender_handle<pipe_t, MultiplexerHandle>;

template<detail::multiplexer_handle_for<pipe_t> MultiplexerHandle>
using basic_pipe_pair = detail::basic_pipe_pair<basic_pipe_handle<MultiplexerHandle>>;

#include <allio/detail/handles/pipe_io.ipp>

} // inline namespace pipe
} // namespace allio::senders
