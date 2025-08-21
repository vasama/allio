#pragma once

#include <allio/blocking/traits.hpp>
#include <allio/handles/standard_stream.hpp>

namespace allio::blocking {
inline namespace standard_stream {

using standard_stream_handle = traits_type::handle<standard_stream_t>;

extern standard_stream_handle cin;
extern standard_stream_handle cout;
extern standard_stream_handle cerr;

} // inline namespace standard_stream
} // namespace allio::blocking
