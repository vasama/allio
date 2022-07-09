#pragma once

#ifndef allio_CONFIG_CHECK_STORAGE_REQUIREMENTS
#	define allio_CONFIG_CHECK_STORAGE_REQUIREMENTS 1
#endif

#include <algorithm>
#include <span>
#include <type_traits>

namespace allio {

struct storage_requirements
{
	size_t size;
	size_t alignment;

	friend constexpr storage_requirements operator+(storage_requirements const& lhs, storage_requirements const& rhs)
	{
		size_t const alignment = std::max(lhs.alignment, rhs.alignment);
		return { std::max(lhs.size, alignment) + std::max(rhs.size, alignment), alignment };
	}

	friend constexpr storage_requirements operator|(storage_requirements const& lhs, storage_requirements const& rhs)
	{
		return { std::max(lhs.size, rhs.size), std::max(lhs.alignment, rhs.alignment) };
	}
};

template<typename T>
inline constexpr storage_requirements storage_requirements_of = { sizeof(T), alignof(T) };


class storage_ptr
{
	void* m_storage;

#if allio_CONFIG_CHECK_STORAGE_REQUIREMENTS
	size_t m_size;
#endif

public:
	explicit storage_ptr(void* const storage, size_t const size)
		: m_storage(storage)
#if allio_CONFIG_CHECK_STORAGE_REQUIREMENTS
		, m_size(size)
#endif
	{
	}

	void* storage() const
	{
		return m_storage;
	}

	void check_requirements(storage_requirements const& requirements) const
	{
#if allio_CONFIG_DEBUG_STORAGE_REQUIREMENTS
		allio_ASSERT(m_size >= requirements.size);
		allio_ASSERT((reinterpret_cast<uintptr_t>(m_storage) & requirements.alignment - 1) == 0);
#endif
	}
};

} // namespace allio

inline void* operator new(size_t const size, allio::storage_ptr const ptr) noexcept
{
	ptr.check_requirements({ size, 1 });
	return ptr.storage();
}

inline void* operator new(size_t const size, std::align_val_t const alignment, allio::storage_ptr const ptr) noexcept
{
	ptr.check_requirements({ size, static_cast<size_t>(alignment) });
	return ptr.storage();
}

inline void operator delete(void* const block, allio::storage_ptr const ptr) noexcept
{
}
