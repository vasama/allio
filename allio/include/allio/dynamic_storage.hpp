#pragma once

#include <allio/storage.hpp>

#include <vsm/result.hpp>

#include <cstddef>

namespace allio {

namespace detail {

template<size_t LocalCapacity>
struct dynamic_storage_base
{
	size_t m_capacity = 0;
	union
	{
		std::byte* m_remote = nullptr;
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
	std::byte* m_remote = nullptr;

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

#if 0
	small_dynamic_storage(storage_requirements const& requirements)
	{
		(void)get(requirements);
	}

	small_dynamic_storage(storage_requirements const& requirements, Allocator const& allocator)
		: Allocator(allocator)
	{
		(void)get(requirements);
	}
#endif

	small_dynamic_storage(small_dynamic_storage&& src) noexcept
		: Allocator(static_cast<Allocator&&>(src))
	{
		vsm_assert(src.m_capacity == 0);
	}

	small_dynamic_storage& operator=(small_dynamic_storage&& src) & noexcept
	{
		vsm_assert(this->m_capacity == 0 && src.m_capacity == 0);
		static_cast<Allocator&>(*this) = static_cast<Allocator&&>(src);
		return *this;
	}

	~small_dynamic_storage()
	{
		if (this->has_remote_storage())
		{
			Allocator::deallocate(this->m_remote, this->m_capacity);
		}
	}


	storage_ptr get()
	{
		return storage_ptr(this->get_storage(), this->m_capacity);
	}

	vsm::result<storage_ptr> get(storage_requirements const& requirements)
	{
		size_t const min_capacity =
			requirements.alignment > alignof(std::max_align_t)
				? requirements.size + requirements.alignment
				: requirements.size;

		vsm_try(storage, [&]() -> vsm::result<void*>
		{
			if (min_capacity > this->m_capacity)
			{
				return grow_storage(min_capacity);
			}
			return this->get_storage();
		}());

		return storage_ptr(storage, this->m_capacity);
	}

private:
	vsm::result<void*> grow_storage(size_t const min_capacity)
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
