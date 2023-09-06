#pragma once

#include <allio/multiplexer_pointer_handle.hpp>

#include <memory>

namespace allio {

template<typename Multiplexer>
using multiplexer_shared_handle = basic_multiplexer_pointer_handle<std::shared_ptr<Multiplexer>>;

} // namespace allio
