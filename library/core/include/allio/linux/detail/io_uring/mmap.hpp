#pragma once

#include <allio/linux/detail/mmap.hpp>

namespace allio::detail {

class io_uring_mmap_deleter
{
	static constexpr size_t borrow_value = static_cast<size_t>(-1);

	size_t m_size;

public:
	explicit io_uring_mmap_deleter(size_t const size)
		: m_size(size)
	{
	}

	[[nodiscard]] static io_uring_mmap_deleter borrow()
	{
		return io_uring_mmap_deleter(borrow_value);
	}

	void operator()(void const* const addr) const
	{
		if (m_size != borrow_value)
		{
			close_mmap(const_cast<void*>(addr), m_size);
		}
	}
};

template<typename T>
using unique_io_uring_mmap = std::unique_ptr<T, io_uring_mmap_deleter>;

using unique_io_uring_void_mmap = unique_io_uring_mmap<void>;
using unique_io_uring_byte_mmap = unique_io_uring_mmap<std::byte>;

} // namespace allio::detail
