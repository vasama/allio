#pragma once

#include <allio/default_multiplexer.hpp>
#include <allio/detail/io_context.hpp>

#include <vsm/box.hpp>

namespace allio {
namespace detail {

template<typename T>
class inline_ptr
{
	
};

} // namespace detail

using detail::basic_io_context;

/// @brief Default execution context for I/O.
///        Uses an internal @ref default_multiplexer for asynchronous I/O.
///        Uses an internal @ref basic_event_queue for multiplexer aware work scheduling.
using io_context = basic_io_context<inline_ptr<default_multiplexer>>;

} // namespace allio
