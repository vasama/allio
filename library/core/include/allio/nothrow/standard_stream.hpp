#pragma once

#include <allio/handles/standard_stream.hpp>
#include <allio/nothrow/traits.hpp>

namespace allio::nothrow {
inline namespace standard_stream {

using standard_stream_handle = traits_type::handle<standard_stream_t>;

extern standard_stream_handle cin;
extern standard_stream_handle cout;
extern standard_stream_handle cerr;

} // inline namespace standard_stream
} // namespace allio::nothrow
