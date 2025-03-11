#pragma once

#include <allio/error.hpp>

#include <vsm/result.hpp>

#include <memory>

namespace allio::detail {

template<typename T, size_t Alignment, size_t StorageSize>
struct dynamic_buffer_base
{
	size_t m_storage_size = StorageSize;
	union
	{
		alignas(Alignment) unsigned char m_static[StorageSize];
		T* m_dynamic;
	};
};

template<typename T, size_t Alignment>
struct dynamic_buffer_base<T, Alignment, 0>
{
	size_t m_storage_size = 0;
	T* m_dynamic = nullptr;
};

template<typename T, size_t Alignment, size_t StorageSize>
class basic_dynamic_buffer : dynamic_buffer_base<T, Alignment, StorageSize>
{
	using base = dynamic_buffer_base<T, Alignment, StorageSize>;

public:
	basic_dynamic_buffer() = default;

	basic_dynamic_buffer(basic_dynamic_buffer&& other) noexcept
	{
		base::m_storage_size = other.m_storage_size;
		if (other.m_storage_size > StorageSize)
		{
			base::m_dynamic = other.m_dynamic;
		}
		other.base::m_storage_size = StorageSize;
	}

	basic_dynamic_buffer& operator=(basic_dynamic_buffer&& other) & noexcept
	{
		delete_dynamic();
		base::m_storage_size = other.m_storage_size;
		if (other.m_storage_size > StorageSize)
		{
			base::m_dynamic = other.m_dynamic;
		}
		other.base::m_storage_size = StorageSize;
		return *this;
	}

	~basic_dynamic_buffer()
	{
		delete_dynamic();
	}


	[[nodiscard]] size_t size() const
	{
		return base::m_storage_size / sizeof(T);
	}

	[[nodiscard]] T* data()
	{
		if constexpr (StorageSize != 0)
		{
			if (base::m_storage_size <= StorageSize)
			{
				return std::launder(reinterpret_cast<T*>(base::m_static));
			}
		}
		return base::m_dynamic;
	}

	[[nodiscard]] T const* data() const
	{
		if constexpr (StorageSize != 0)
		{
			if (base::m_storage_size <= StorageSize)
			{
				return std::launder(reinterpret_cast<T const*>(base::m_static));
			}
		}
		return base::m_dynamic;
	}

	[[nodiscard]] vsm::result<T*> reserve(size_t const min_size)
	{
		static_assert(std::is_trivial_v<T>);
		static_assert(StorageSize % sizeof(T) == 0);

		size_t const min_storage_size = min_size * sizeof(T);

		if (min_storage_size <= base::m_storage_size)
		{
			return data();
		}

		if constexpr (StorageSize != 0)
		{
			if (min_storage_size < StorageSize)
			{
				base::m_storage_size = StorageSize;
				return new (base::m_static) T[StorageSize / sizeof(T)];
			}
		}

		//TODO: * Use allio_acquire_storage/allio_release_storage.
		//      * Use RAII to manage the storage during construction.
		void* const block = operator new(min_storage_size, std::nothrow);
		if (block == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
		delete_dynamic();

		T* const data = ::new (block) T[min_size];

		base::m_storage_size = min_storage_size;
		base::m_dynamic = data;

		return data;
	}

private:
	void delete_dynamic()
	{
		if (base::m_storage_size > StorageSize)
		{
			operator delete(base::m_dynamic, base::m_storage_size);
			base::m_storage_size = StorageSize;
		}
	}
};

template<typename T, size_t Capacity = 0>
using dynamic_buffer = basic_dynamic_buffer<T, alignof(T), Capacity * sizeof(T)>;

} // namespace allio::detail
