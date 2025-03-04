#pragma once

#include <allio/error.hpp>
#include <allio/detail/mutable_buffer.hpp>

#include <vsm/result.hpp>
#include <vsm/tag_ptr.hpp>

namespace allio::detail {

[[nodiscard]] inline vsm::allocation get_aligned_storage(
	vsm::allocation const storage,
	std::align_val_t const min_alignment_val)
{
	size_t const min_alignment = static_cast<size_t>(min_alignment_val);
	vsm_assert(std::has_single_bit(min_alignment)); //PRECONDITION

	size_t const ptr_alignment = vsm::memalignment(storage.storage);
	size_t const alignment_off = min_alignment - ptr_alignment;

	if (ptr_alignment >= min_alignment)
	{
		return storage;
	}

	if (alignment_off > storage.size)
	{
		return { nullptr };
	}

	return
	{
		static_cast<std::byte*>(storage.storage) + alignment_off,
		storage.size - alignment_off,
	};
}

class any_aligned_storage_provider
{
	using get_storage_t = vsm::result<vsm::allocation>(
		void* object,
		size_t min_size,
		size_t max_size,
		std::align_val_t min_alignment);

	// True tag indicates pointer to resizable container.
	vsm::incomplete_tag_ptr<void, bool, true> m_data;

	union
	{
		size_t m_size;
		get_storage_t* m_get_storage;
	};

public:
	any_aligned_storage_provider()
		: m_data(nullptr)
		, m_size(static_cast<size_t>(-1))
	{
	}

	explicit any_aligned_storage_provider(void* const data, size_t const size)
		: any_aligned_storage_provider(
			detail::get_aligned_storage(vsm::allocation(data, size), std::align_val_t(2)))
	{
	}

	template<std::ranges::contiguous_range Range>
		requires vsm::byte_type<std::ranges::range_value_t<Range>>
	any_aligned_storage_provider(Range&& range)
		: any_aligned_storage_provider(std::ranges::data(range), std::ranges::size(range))
	{
	}

	template<detail::storage_provider StorageProvider>
	any_aligned_storage_provider(StorageProvider&& storage_provider)
		: m_data(std::addressof(storage_provider))
		, m_get_storage(get_storage_from_storage_provider<vsm::remove_ref_t<StorageProvider>>)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_data == nullptr && m_size == static_cast<size_t>(-1);
	}
	
	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		std::align_val_t const min_alignment = std::align_val_t(alignof(std::max_align_t))) const
	{
		return get_storage(min_size, min_size, min_alignment);
	}

	[[nodiscard]] vsm::result<vsm::allocation> get_storage(
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment = std::align_val_t(alignof(std::max_align_t))) const
	{
		if (m_data.tag())
		{
			return m_get_storage(m_data.ptr(), min_size, max_size, min_alignment);
		}

		if (m_data == nullptr || m_size < min_size)
		{
			return vsm::unexpected(error::no_buffer_space);
		}

		return vsm::allocation(m_data.ptr(), m_size);
	}

private:
	explicit any_aligned_storage_provider(vsm::allocation const storage)
		: m_data(storage.storage)
		, m_size(storage.size)
	{
	}

	template<typename StorageProvider>
	static vsm::result<vsm::allocation> get_storage_from_storage_provider(
		void* const object,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment)
	{
		auto& storage_provider = *static_cast<StorageProvider*>(object);
		return detail::get_storage(storage_provider, min_size, max_size, min_alignment);
	}
};

} // namespace allio::detail
