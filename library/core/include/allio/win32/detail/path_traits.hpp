#pragma once

#include <allio/win32/path_traits.hpp>

namespace allio {

template<typename Char>
using native_path_traits = win32::path_traits<Char>;

} // namespace allio
