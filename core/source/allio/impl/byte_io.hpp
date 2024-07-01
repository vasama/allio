#pragma once

#include <allio/byte_io.hpp>

namespace allio {

template<vsm::any_cv_of<std::byte> T>
size_t get_buffers_size(basic_buffers<T> const buffers)
{
	size_t size = 0;
	for (basic_buffer<T> const& buffer : buffers)
	{
		size += buffer.size();
	}
	return size;
}

} // namespace allio
