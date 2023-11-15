#pragma once

#include <allio/detail/dynamic_buffer.hpp>

struct _WSABUF;

namespace allio::detail {

template<size_t Size>
struct wsa_address_storage
{
	alignas(4) unsigned char storage[Size];
};

template<size_t StorageSize>
using _wsa_buffers_storage = basic_dynamic_buffer<
	_WSABUF,
	alignof(void*),
	StorageSize>;

template<size_t Size>
using wsa_buffers_storage = _wsa_buffers_storage<Size * 2 * sizeof(void*)>;

} // namespace allio::detail
