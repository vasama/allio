#pragma once

#include <memory>

namespace allio::detail {

class mmap_deleter
{
	static constexpr size_t borrow_value = static_cast<size_t>(-1);

	size_t m_size;

public:
	explicit mmap_deleter(size_t const size)
		: m_size(size)
	{
	}

	static mmap_deleter borrow()
	{
		return mmap_deleter(borrow_value);
	}

	void operator()(void const* const addr) const
	{
		if (m_size != borrow_value)
		{
			release(const_cast<void*>(addr), m_size);
		}
	}

private:
	static void release(void* addr, size_t size);
};

template<typename T>
using unique_mmap = std::unique_ptr<T, mmap_deleter>;

using unique_void_mmap = unique_mmap<void>;
using unique_byte_mmap = unique_mmap<std::byte>;

} // namespace allio::detail
