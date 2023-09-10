#pragma once

#include <allio/multiplexer_pointer_handle.hpp>

#include <memory>

namespace allio {

template<typename Multiplexer>
using shared_multiplexer_handle = basic_multiplexer_pointer_handle<std::shared_ptr<Multiplexer>>;

} // namespace allio
