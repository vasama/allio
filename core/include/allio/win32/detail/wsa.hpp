#pragma once

//#include <allio/detail/dynamic_buffer.hpp>
#include <allio/detail/byte_io_buffers.hpp>

struct _WSABUF;

namespace allio::detail {

template<size_t Size>
struct wsa_address_storage
{
	alignas(4) unsigned char storage[Size];
};

#if 0
template<size_t StorageSize>
using _wsa_buffers_storage = basic_dynamic_buffer<
	_WSABUF,
	alignof(void*),
	StorageSize>;

template<size_t Size>
using wsa_buffers_storage = _wsa_buffers_storage<Size * 2 * sizeof(void*)>;
#endif

//TODO: Implement new_io_buffers_storage with SBO.

template<size_t StorageSize = 0>
using _wsa_buffers_storage = new_io_buffers_storage;

template<size_t Size>
using wsa_buffers_storage = new_io_buffers_storage;

} // namespace allio::detail
