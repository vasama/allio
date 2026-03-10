#pragma once

#include <allio/linux/path_traits.hpp>

#include <allio/linux/detail/undef.i>

namespace allio {

template<typename Char>
using native_path_traits = linux::path_traits<Char>;

} // namespace allio

#include <allio/linux/detail/undef.i>
