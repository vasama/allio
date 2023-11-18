#pragma once

#include <allio/byte_io_buffers.hpp>

namespace allio {

template<vsm::any_cv_of<std::byte> T>
struct basic_buffers_t
{
	basic_buffers_storage<T> buffers;
};

using read_buffers_t = basic_buffers_t<std::byte>;
using write_buffers_t = basic_buffers_t<std::byte const>;

} // namespace allio
