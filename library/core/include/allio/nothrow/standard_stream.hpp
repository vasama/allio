#pragma once

#include <allio/handles/standard_stream.hpp>
#include <allio/nothrow/traits.hpp>

namespace allio::nothrow {
inline namespace standard_stream {

using standard_stream_handle = traits_type::handle<standard_stream_t>;

} // inline namespace standard_stream
} // namespace allio::nothrow
