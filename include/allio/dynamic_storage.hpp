#pragma once

#include <allio/storage.hpp>

#include <cstddef>

namespace allio {

namespace detail {

template<size_t LocalCapacity>
struct dynamic_storage_base
{
	size_t m_capacity = 0;
	union
	{
		std::byte* m_remote;
		std::byte m_local alignas(std::max_align_t)[LocalCapacity];
	};

	bool has_remote_storage() const
	{
		return m_capacity > LocalCapacity;
	}

	void* get_storage()
	{
		return m_capacity > LocalCapacity ? m_remote : m_local;
	}
};

template<>
struct dynamic_storage_base<0>
{
	size_t m_capacity = 0;
	std::byte* m_remote;

	bool has_remote_storage() const
	{
		return false;
	}

	void* get_storage()
	{
		return m_remote;
	}
};

} // namespace detail

template<size_t LocalCapacity, typename Allocator = std::allocator<std::byte>>
class small_dynamic_storage
	: detail::dynamic_storage_base<LocalCapacity>
	, Allocator
{
	using base_type = detail::dynamic_storage_base<LocalCapacity>;

public:
	small_dynamic_storage()
	{
	}

	small_dynamic_storage(Allocator const& allocator)
		: Allocator(allocator)
	{
	}

	small_dynamic_storage(storage_requirements const& requirements)
	{
		(void)get(requirements);
	}

	small_dynamic_storage(storage_requirements const& requirements, Allocator const& allocator)
		: Allocator(allocator)
	{
		(void)get(requirements);
	}

	storage_ptr get(storage_requirements const& requirements)
	{
		size_t const min_capacity =
			requirements.alignment > alignof(std::max_align_t)
				? requirements.size + requirements.alignment
				: requirements.size;

		void* const storage =
			min_capacity > this->m_capacity
				? grow_storage(min_capacity)
				: this->get_storage();

		return storage_ptr(storage, this->m_capacity);
	}

private:
	void* grow_storage(size_t const min_capacity)
	{
		if (this->has_remote_storage())
		{
			Allocator::deallocate(this->m_remote, this->m_capacity);
		}

		std::byte* const storage = Allocator::allocate(min_capacity);

		this->m_capacity = min_capacity;
		this->m_remote = storage;

		return storage;
	}
};

template<typename Allocator = std::allocator<std::byte>>
using basic_dynamic_storage = small_dynamic_storage<0, Allocator>;

using dynamic_storage = basic_dynamic_storage<>;

} // namespace allio
