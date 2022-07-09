#pragma once

#include <allio/any_string_buffer.hpp>

namespace allio {

template<detail::character Char>
using path_buffer = string_buffer<Char>;

using any_path_buffer = any_string_buffer;

} // namespace allio
