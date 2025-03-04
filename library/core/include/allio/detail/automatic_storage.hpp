#pragma once

#include <allio/detail/mutable_buffer.hpp>
#include <allio/detail/new.hpp>

namespace allio::detail {

template<size_t StorageSize>
struct automatic_storage_base
{
	size_t m_storage_size = StorageSize;
	union
	{
		alignas(std::max_align_t) unsigned char m_static_storage[StorageSize];
		void* m_dynamic_storage;
	};
};

template<>
struct automatic_storage_base<0>
{
	size_t m_storage_size = 0;
	void* m_dynamic_storage;
};

template<size_t StorageSize>
class automatic_storage : automatic_storage_base<StorageSize>
{
	using base = automatic_storage_base<StorageSize>;

public:
	automatic_storage() = default;

	automatic_storage(automatic_storage const&) = delete;
	automatic_storage& operator=(automatic_storage const&) = delete;

	~automatic_storage()
	{
		if (base::m_storage_size > StorageSize)
		{
			detail::release_storage(
				base::m_dynamic_storage,
				base::m_storage_size,
				alignof(std::max_align_t),
				allio_allocation_strategy_automatic);
		}
	}

private:
	friend vsm::result<vsm::allocation> tag_invoke(
		get_storage_t,
		automatic_storage& self,
		size_t const min_size,
		size_t const max_size,
		std::align_val_t const min_alignment) noexcept
	{
		if (self.m_storage_size < min_size)
		{
			auto const new_storage = detail::acquire_storage(
				min_size,
				max_size,
				static_cast<size_t>(alignof(std::max_align_t)),
				allio_allocation_strategy_automatic);

			if (new_storage.storage == nullptr)
			{
				return vsm::unexpected(error::not_enough_memory);
			}

			if (self.m_storage_size > StorageSize)
			{
				detail::release_storage(
					self.m_dynamic_storage,
					self.m_storage_size,
					alignof(std::max_align_t),
					allio_allocation_strategy_automatic);
			}

			self.m_dynamic_storage = new_storage.storage;
			self.m_storage_size = new_storage.size;
		}

		void* const storage = self.m_storage_size <= StorageSize
			? self.m_static_storage
			: self.m_dynamic_storage;

		return vsm::allocation(storage, self.m_storage_size);
	}
};

} // namespace allio::detail
