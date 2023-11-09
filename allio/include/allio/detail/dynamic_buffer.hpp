#pragma once

#include <allio/error.hpp>

#include <vsm/linear.hpp>
#include <vsm/result.hpp>

#include <memory>

namespace allio::detail {

template<typename T, size_t Alignment, size_t StorageSize>
struct dynamic_buffer_base
{
	vsm::linear<size_t, StorageSize> m_storage_size = 0;
	union
	{
		alignas(Alignment) unsigned char m_static[StorageSize];
		struct { T* value; } m_dynamic;
	};
};

template<typename T, size_t Alignment>
struct dynamic_buffer_base<T, Alignment, 0>
{
	vsm::linear<size_t, 0> m_storage_size;
	vsm::linear<T*> m_dynamic;
};

template<typename T, size_t Alignment, size_t StorageSize>
class basic_dynamic_buffer : dynamic_buffer_base<T, Alignment, StorageSize>
{
	using base = dynamic_buffer_base<T, Alignment, StorageSize>;

public:
	basic_dynamic_buffer() = default;

	basic_dynamic_buffer(basic_dynamic_buffer&&) = default;

	basic_dynamic_buffer& operator=(basic_dynamic_buffer&& other) & noexcept
	{
		delete_dynamic();
		static_cast<base&>(*this) = static_cast<base&&>(other);
		return *this;
	}

	~basic_dynamic_buffer()
	{
		delete_dynamic();
	}


	size_t size() const
	{
		return base::m_storage_size.value / sizeof(T);
	}

	T* data()
	{
		if constexpr (StorageSize != 0)
		{
			if (base::m_storage_size.value <= StorageSize)
			{
				return std::launder(reinterpret_cast<T*>(base::m_static));
			}
		}
		return base::m_dynamic.value;
	}

	T const* data() const
	{
		if constexpr (StorageSize != 0)
		{
			if (base::m_storage_size.value <= StorageSize)
			{
				return std::launder(reinterpret_cast<T const*>(base::m_static));
			}
		}
		return base::m_dynamic.value;
	}

	vsm::result<T*> reserve(size_t const min_size)
	{
		static_assert(std::is_trivial_v<T>);
		static_assert(StorageSize % sizeof(T) == 0);

		size_t const min_storage_size = min_size * sizeof(T);

		if (min_storage_size <= base::m_storage_size.value)
		{
			return data();
		}

		if constexpr (StorageSize != 0)
		{
			if (min_storage_size < StorageSize)
			{
				base::m_storage_size.value = StorageSize;
				return new (base::m_static) T[StorageSize / sizeof(T)];
			}
		}

		void* const block = operator new(min_storage_size, std::nothrow);
		if (block == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
		delete_dynamic();

		T* const data = new (block) T[min_size];

		base::m_storage_size.value = min_storage_size;
		base::m_dynamic.value = data;

		return data;
	}

private:
	void delete_dynamic()
	{
		if (base::m_storage_size.value > StorageSize)
		{
			operator delete(base::m_dynamic.value, base::m_storage_size.value);
		}
	}
};

template<typename T, size_t Capacity = 0>
using dynamic_buffer = basic_dynamic_buffer<T, alignof(T), Capacity * sizeof(T)>;

} // namespace allio::detail
